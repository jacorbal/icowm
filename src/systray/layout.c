/**
 * @file systray/layout.c
 *
 * @brief Systray positioning, stacking, and drawing
 *
 * Everything about where the tray window and its docked icons end up on
 * screen, and what gets drawn there.  Sizing and moving the tray to its
 * configured corner, arranging icons in a row, drawing the clock or
 * battery text next to them, and applying the configured stacking
 * layer.  The text content itself (what the clock or battery status
 * actually says) is read and formatted elsewhere.
 *
 * @see @c systray/text.c
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>     /* memcmp, memset, NULL */

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <render/text.h>
#include <wm.h>

/* Local includes */
#include <systray/internal.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/**
 * @brief Pixel width the tray window needs for the current icon count
 *        and, if enabled, the clock and/or battery status text
 *
 * @return @c 0 when no icons are docked and neither text item is
 *         enabled (the tray window stays unmapped in that case),
 *         otherwise enough to fit every icon with padding around and
 *         between each, plus the combined text width when any of it is
 *         enabled
 */
static uint16_t s_systray_content_width(void)
{
    uint16_t icons_w = (s_tray.icon_count == 0u) ? 0u
        : (uint16_t) (s_tray.pixmap_pad +
            s_tray.icon_count * (s_tray.pixmap_size + s_tray.pixmap_pad));

    return (uint16_t) (icons_w + systray_text_width());
}


/**
 * @brief Find the currently fullscreen client the tray's own layer
 *        should duck behind, if any
 *
 * Scoped to @p s_tray.surface's own currently displayed desktop
 * only, the one surface the tray itself actually belongs to and the
 * only desktop whose content can actually be on screen at the same
 * time as the tray: a client fullscreen on some other surface
 * (a different physical monitor's own root window) or on a desktop
 * of @p s_tray.surface that is not the one currently shown is not
 * visible right now, so it has no bearing on where this one tray
 * should stack.
 *
 * @return The fullscreen client's own frame (or plain window, if
 *         undecorated), or @c XCB_WINDOW_NONE if none is fullscreen
 *         on @p s_tray.surface's own current desktop
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p s_tray.surface's own current desktop
 */
static xcb_window_t s_systray_fullscreen_target_find(void)
{
    desktop_td *desktop;
    void *elem;

    if (s_tray.surface == NULL) {
        return XCB_WINDOW_NONE;
    }

    desktop = surface_desktop_get(s_tray.surface,
            s_tray.surface->desktop_cur);
    if (desktop == NULL || desktop->clients == NULL) {
        return XCB_WINDOW_NONE;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *const client = (client_td *) elem;

        if (client_is_fullscreen(client)) {
            return (client->frame != 0) ? client->frame : client->window;
        }
    }

    return XCB_WINDOW_NONE;
}


/**
 * @brief Push every currently mapped icon window below the tray
 *
 * Called right after the tray restacks itself into the 'below' layer,
 * so icons stay "stuck to the desktop" (lower than the tray, even
 * though both are nominally in the same 'below' layer) regardless of
 * restack ordering: the tray's own move to the bottom (an unqualified
 * @c XCB_STACK_MODE_BELOW, since the tray does not otherwise know of
 * any one icon to stack itself relative to) would otherwise claim the
 * absolute bottom of the sibling stack out from under any icon that was
 * already there, the same "whichever restacked most recently wins"
 * problem @a ccmd_client_iconify's own explicit stack-below handles for
 * the opposite ordering.
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients across every desktop and surface
 *
 * @see @a systray_below_window
 */
static void s_systray_icons_push_below(void)
{
    list_td *surfaces;

    surfaces = wm_get_surfaces();
    if (surfaces == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *const surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;
        const cdlist_item_td *dinitial;

        if (surface == NULL || surface->desktops == NULL) {
            continue;
        }
        dnode = cdlist_head(surface->desktops);
        if (dnode == NULL) {
            continue;
        }
        dinitial = dnode;
        do {
            desktop_td *const desktop = (desktop_td *) cdlist_data(dnode);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;
                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const client = (client_td *) elem;

                    if (client->is_icon_mapped &&
                            client->icon_window != 0u) {
                        xcb_window_stack_below(client->icon_window,
                                s_tray.window);
                    }
                }
            }
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial);
    }
}


/**
 * @brief Publish (or clear) the tray's own reserved-space strut on its
 *        dock window, and mirror the same values into
 *        @a s_tray.reserved_strut for @a systray_get_reserved_strut
 *
 * Per the specification's own recommendation for a docking area,
 * a taskbar, or a panel, the tray publishes @c _NET_WM_STRUT_PARTIAL
 * (and, for compatibility with anything that only understands the
 * legacy property, plain @c _NET_WM_STRUT alongside it) covering the
 * exact strip of screen its own configured corner and current size
 * occupy, so a maximized window (and this window manager's own
 * placement logic, via @a desktop_update_workarea) both leave that
 * strip alone the same way they already do for an external panel or
 * dock, plus @p config.systray.margins added on top of that strip.
 *
 * Publishes an all-zero strut instead when
 * @p config.systray.reserve-space is @c false, for anyone who would
 * rather windows stayed free to maximize over or under the tray.
 *
 * @param geom    Tray's own current rectangle (root coordinates); a
 *                zero dimension clears the strut (the tray itself
 *                is unmapped)
 * @param border2 Total border thickness, both sides combined
 *
 * @note Complexity: @e O(1)
 */
static bool s_systray_strut_update(struct geometry_s geom,
        int32_t border2)
{
    const struct strut_partial_s previous = s_tray.reserved_strut;
    xcb_ewmh_wm_strut_partial_t partial;

    memset(&partial, 0, sizeof(partial));

    /* 'reserve_space == false' leaves 'partial' at the all-zero shape
     * 'memset' above already set it to, an explicit '{0, 0, 0, 0}'
     * strut, published and stored exactly like any other, rather than
     * simply skipping the publish/store below.  A caller that reads
     * 's_tray.reserved_strut' should see "reserves nothing" the same
     * way it would for a real client with no strut of its own, not
     * a stale value left over from whenever reservation was last
     * enabled. */
    if (s_tray.reserve_space && s_tray.surface != NULL &&
            geom.dim.w > 0u && geom.dim.h > 0u) {
        switch (s_tray.position) {
            case CONFIG_SYSTRAY_POSITION_TOP_LEFT:
            case CONFIG_SYSTRAY_POSITION_TOP_RIGHT:
                partial.top = (uint32_t) (geom.pos.y +
                        (int32_t) geom.dim.h + border2);
                partial.top_start_x = (uint32_t) geom.pos.x;
                partial.top_end_x = (uint32_t) (geom.pos.x +
                        (int32_t) geom.dim.w + border2);
                break;

            case CONFIG_SYSTRAY_POSITION_BOTTOM_LEFT:
            case CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT: {
                uint32_t y_u = (uint32_t) geom.pos.y;
                uint32_t screen_h = s_tray.surface->properties.dim.h;

                partial.bottom = (screen_h > y_u)
                    ? screen_h - y_u : 0u;
                partial.bottom_start_x = (uint32_t) geom.pos.x;
                partial.bottom_end_x = (uint32_t) (geom.pos.x +
                        (int32_t) geom.dim.w + border2);
                break;
            }
        }

        /* 'config.systray.margins': added on top of whatever the switch
         * above just computed from the tray's own actual geometry, the
         * same way 'config_desktop_s''s own 'margins' adds on top of
         * a client's published strut in 'desktop_update_workarea'; not
         * restricted to the edge the tray currently docks at
         * (left/right add to a screen side the tray itself never
         * reserves on its own), left with no start/end range of their
         * own to honor (0..0), so they apply along the whole edge
         * unconditionally, exactly like 'config_desktop_s''s own
         * margins do. */
        partial.top += s_tray.strut_margins.top;
        partial.right += s_tray.strut_margins.right;
        partial.bottom += s_tray.strut_margins.bottom;
        partial.left += s_tray.strut_margins.left;
    }

    if (xcb_ewmh_connection_get() != NULL && s_tray.window != XCB_WINDOW_NONE) {
        (void) xcb_ewmh_set_wm_strut_partial(xcb_ewmh_connection_get(), s_tray.window,
                partial);
        (void) xcb_ewmh_set_wm_strut(xcb_ewmh_connection_get(), s_tray.window,
                partial.left, partial.right, partial.top,
                partial.bottom);
    }

    s_tray.reserved_strut.sides.left = (int32_t) partial.left;
    s_tray.reserved_strut.sides.right = (int32_t) partial.right;
    s_tray.reserved_strut.sides.top = (int32_t) partial.top;
    s_tray.reserved_strut.sides.bottom = (int32_t) partial.bottom;
    s_tray.reserved_strut.start.left = (int32_t) partial.left_start_y;
    s_tray.reserved_strut.start.right = (int32_t) partial.right_start_y;
    s_tray.reserved_strut.start.top = (int32_t) partial.top_start_x;
    s_tray.reserved_strut.start.bottom =
        (int32_t) partial.bottom_start_x;
    s_tray.reserved_strut.end.left = (int32_t) partial.left_end_y;
    s_tray.reserved_strut.end.right = (int32_t) partial.right_end_y;
    s_tray.reserved_strut.end.top = (int32_t) partial.top_end_x;
    s_tray.reserved_strut.end.bottom = (int32_t) partial.bottom_end_x;

    return memcmp(&previous, &s_tray.reserved_strut,
            sizeof(previous)) != 0;
}


/**
 * @brief Resolve the rectangle the tray dock's corner is anchored to
 *
 * - Under @c CONFIG_SYSTRAY_MONITOR_SURFACE (the default), returns
 *   @p s_tray.surface's own combined dimensions, exactly the previous,
 *   always-whole-surface behavior, treated as one virtual monitor
 *   spanning it (the same fallback @a s_surface_monitors_fallback uses
 *   when RandR itself cannot supply a real monitor list).
 * 
 * - Under @c CONFIG_SYSTRAY_MONITOR_PRIMARY, returns whichever monitor
 *   RandR reports as primary.  Under @c CONFIG_SYSTRAY_MONITOR_INDEX,
 *   returns @p s_tray.monitor.index specifically (out of range falls
 *   back to monitor 0, logging a warning).
 *
 * @return The resolved anchor monitor
 *
 * @note Complexity: @e O(1)
 */
static monitor_td s_systray_anchor_rect(void)
{
    monitor_td rect = {.x = 0, .y = 0,
        .w = s_tray.surface->properties.dim.w,
        .h = s_tray.surface->properties.dim.h};

    if (s_tray.monitor.anchor == CONFIG_SYSTRAY_MONITOR_PRIMARY) {
        return surface_primary_monitor(s_tray.surface);
    }

    if (s_tray.monitor.anchor == CONFIG_SYSTRAY_MONITOR_INDEX) {
        uint32_t idx = s_tray.monitor.index;

        if (s_tray.surface->monitor_count == 0u) {
            return rect;
        }
        if (idx >= s_tray.surface->monitor_count) {
            LOGGER_WARNING("Systray targets monitor %u, which does" \
                    " not exist on surface %u (%u monitor(s));" \
                    " falling back to monitor 0", s_tray.monitor.index,
                    s_tray.surface->id, s_tray.surface->monitor_count);
            idx = 0u;
        }
        return s_tray.surface->monitors[idx];
    }

    return rect;
}


/* Apply the configured 'systray.layer' stacking rule
 *
 * - 'CONFIG_SYSTRAY_LAYER_BELOW' (the default): stacks the tray window
 *   at the very bottom, behind every client window.
 * - 'CONFIG_SYSTRAY_LAYER_ABOVE': stacks it at the top, unless a client
 *   is currently fullscreen, in which case it stacks just below that
 *   client instead, so a fullscreen window still covers it; the same
 *   way a taskbar or panel gets covered by a fullscreen window in most
 *   desktop environments, instead of a systray floating above literally
 *   everything regardless of what the user is doing.  Always resolved
 *   to its final position in one single 'ConfigureWindow' call (cfr.
 *   's_systray_fullscreen_target_find' above), never by raising to the
 *   top and only then lowering in a second, separate request, which
 *   would flash the tray above fullscreen content for the brief moment
 *   between the two.
 * - 'CONFIG_SYSTRAY_LAYER_OVERLAY': stacks it at the top and leaves it
 *   there unconditionally, even over fullscreen windows.
 *
 * Safe to call whenever the tray's stacking might need reconsidering:
 * after every reflow, and whenever any client enters or exits
 * fullscreen (see 'ccmd_client_fullscreen' and
 * 'ccmd_client_unfullscreen', which call the public 'systray_restack'
 * wrapper in systray.c). */
void systray_layout_restack(void)
{
    xcb_window_t fullscreen_target;

    if (!s_tray.is_window_ready || xcb_connection_get() == NULL) {
        return;
    }

    if (s_tray.layer == CONFIG_SYSTRAY_LAYER_BELOW) {
        if (s_tray.is_stacking_known &&
                s_tray.stacked_layer == s_tray.layer) {
            return;
        }
        xcb_window_lower(s_tray.window);
        s_tray.stacked_against = XCB_WINDOW_NONE;
        s_tray.stacked_layer = s_tray.layer;
        s_tray.is_stacking_known = true;
        s_systray_icons_push_below();
        return;
    }

    fullscreen_target = (s_tray.layer == CONFIG_SYSTRAY_LAYER_ABOVE)
        ? s_systray_fullscreen_target_find() : XCB_WINDOW_NONE;

    /* A single 'ConfigureWindow' call straight to the final position,
     * rather than unconditionally raising to the very top first and
     * only then lowering below a fullscreen client in a second,
     * separate request: that two-step sequence briefly left the tray
     * stacked above the fullscreen content between the two requests,
     * visible as a flash on every restack (every reflow, and every
     * fullscreen toggle) rather than only when actually needed. */
    /* Skipped when the tray already sits where this would put it: the
     * server answers a restack by exposing whatever the move
     * uncovered, those exposures reach the tray, and the tray reflows
     * and restacks again.  Asking only when the answer would differ
     * is what stops that from feeding itself. */
    if (s_tray.is_stacking_known &&
            s_tray.stacked_layer == s_tray.layer &&
            s_tray.stacked_against == fullscreen_target) {
        return;
    }

    if (fullscreen_target != XCB_WINDOW_NONE) {
        xcb_window_stack_below(s_tray.window, fullscreen_target);
    } else {
        xcb_window_raise(s_tray.window);
    }
    s_tray.stacked_against = fullscreen_target;
    s_tray.stacked_layer = s_tray.layer;
    s_tray.is_stacking_known = true;

}


/* Reposition the tray window and lay out its docked icons
 *
 * Unmaps the tray window while empty (nothing docked and neither the
 * clock nor the battery text enabled) or while the tray is not
 * currently active at all (disabled by configuration; see 'is_active''s
 * comment in 'include/systray/internal.h'), so it never shows on screen
 * in either case; otherwise sizes and moves it to the configured corner
 * of 's_tray.surface' and arranges icons in a single horizontal row
 * inside it, in 's_tray.icons' order (see 'systray_protocol_dock' in
 * 'systray/protocol.c' for how that order is maintained per the 'order'
 * policy). */
void systray_layout_reflow(void)
{
    bool is_strut_changed;
    uint16_t w;
    uint16_t h;
    uint16_t text_w;
    uint16_t icons_base_x;
    uint16_t icon_y;
    int16_t x = 0;
    int16_t y = 0;
    int32_t border2;
    monitor_td anchor;

    if (!s_tray.is_window_ready || s_tray.surface == NULL) {
        return;
    }

    if (!s_tray.is_active ||
            (s_tray.icon_count == 0u && !s_tray.clock_enabled &&
                !s_tray.battery_enabled)) {
        xcb_window_hide(s_tray.window);
        (void) s_systray_strut_update((struct geometry_s) {
                    { 0, 0 }, { 0u, 0u } }, 0);
        return;
    }

    h = s_tray.height;
    text_w = systray_text_width();
    w = s_systray_content_width();
    if (w == 0u) {
        xcb_window_hide(s_tray.window);
        (void) s_systray_strut_update((struct geometry_s) {
                    { 0, 0 }, { 0u, 0u } }, 0);
        return;
    }

    /* An X11 border is drawn entirely outside a window's own width and
     * height (the X/Y a window is configured at mark the outer corner,
     * before the border), so the tray's true on-screen footprint is
     * 'w + 2 * border_width' wide and 'h + 2 * border_width' tall, not
     * just 'w' by 'h'.  Right/bottom-anchored positions have to
     * subtract that extra span or the tray pokes out past the screen
     * edge by exactly that amount. */
    border2 = (s_tray.theme != NULL)
        ? (int32_t) (2u * s_tray.theme->systray.style.border.width) : 0;

    anchor = s_systray_anchor_rect();

    switch (s_tray.position) {
        case CONFIG_SYSTRAY_POSITION_TOP_LEFT:
            x = (int16_t) anchor.x;
            y = (int16_t) anchor.y;
            break;

        case CONFIG_SYSTRAY_POSITION_BOTTOM_LEFT:
            x = (int16_t) anchor.x;
            y = (int16_t) (anchor.y + (int32_t) anchor.h -
                    (int32_t) h - border2);
            break;

        case CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT:
            x = (int16_t) (anchor.x + (int32_t) anchor.w -
                    (int32_t) w - border2);
            y = (int16_t) (anchor.y + (int32_t) anchor.h -
                    (int32_t) h - border2);
            break;

        case CONFIG_SYSTRAY_POSITION_TOP_RIGHT:
            x = (int16_t) (anchor.x + (int32_t) anchor.w -
                    (int32_t) w - border2);
            y = (int16_t) anchor.y;
            break;
    }

    xcb_window_place(s_tray.window, x, y, w, h);
    is_strut_changed = s_systray_strut_update((struct geometry_s) {
                { x, y }, { w, h } }, border2);

    /* Icons sit after the text block when it is on the left, or right
     * at the tray's own left edge otherwise (text block on the right,
     * or nothing enabled). */
    icons_base_x = (text_w > 0u &&
            s_tray.text_position == CONFIG_SYSTRAY_TEXT_LEFT)
        ? text_w : 0u;
    icon_y = (h > (uint16_t) s_tray.pixmap_size)
        ? (uint16_t) ((h - s_tray.pixmap_size) / 2u) : 0u;

    for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
        uint32_t icon_pos[2];
        uint32_t stride = (uint32_t)
            (s_tray.pixmap_size + s_tray.pixmap_pad);

        /* Every operand here is a non-negative 'uint16_t' to begin
         * with.  Plain 'uint16_t' arithmetic still promotes to 'int'
         * before the addition though, which trips '-Wsign-conversion'
         * on assignment to the 'uint32_t' array below.  The explicit
         * casts keep every step of the arithmetic in 'uint32_t'
         * instead. */
        icon_pos[0] = (uint32_t) icons_base_x +
            (uint32_t) s_tray.pixmap_pad + (uint32_t) i * stride;
        icon_pos[1] = icon_y;
        xcb_window_move(s_tray.icons[i].window,
                (int32_t) icon_pos[0], (int32_t) icon_pos[1]);
    }

    xcb_window_show(s_tray.window);

    if (text_w > 0u && s_tray.theme != NULL) {
        uint16_t icons_w = (uint16_t) (w - text_w);
        int16_t block_x = (s_tray.text_position == CONFIG_SYSTRAY_TEXT_LEFT)
            ? 0 : (int16_t) icons_w;
        int16_t ascent;
        int16_t descent;
        int16_t item_h;
        int16_t item_y = 0;
        int16_t pen_x;

        xcb_clear_area(xcb_connection_get(), 0, s_tray.window,
                block_x, 0, text_w, h);
        (void) text_renderer_use_font(xcb_connection_get(),
                s_tray.theme->systray.style.font);
        text_renderer_set_color(s_tray.theme->systray.style.color.foreground,
                s_tray.theme->systray.style.color.background);

        /* 'text_draw_string' takes the baseline, not the top of the
         * text, so each alignment has to add the font's own ascent (see
         * 'text_font_ascent') to whatever pixel the top of the text
         * should land on.  Every item in 'text_order' shares one font,
         * so this is computed once and reused for each. */
        ascent = text_font_ascent();
        descent = text_font_descent();
        item_h = (int16_t) (ascent + descent);

        switch (s_tray.text_valign) {
            case CONFIG_SYSTRAY_TEXT_VALIGN_TOP:
                item_y = (int16_t) ((int32_t) s_tray.pixmap_pad + ascent);
                break;

            case CONFIG_SYSTRAY_TEXT_VALIGN_BOTTOM:
                item_y = (int16_t) ((int32_t) h -
                        (int32_t) s_tray.pixmap_pad - descent);
                break;

            case CONFIG_SYSTRAY_TEXT_VALIGN_CENTER:
                item_y = (int16_t) (((h > (uint16_t) item_h)
                        ? (int32_t) (h - (uint16_t) item_h) / 2 : 0) +
                        ascent);
                break;
        }

        pen_x = (int16_t) (block_x + (int16_t) s_tray.pixmap_pad);
        for (uint8_t i = 0u; i < s_tray.text_order_count; ++i) {
            bool enabled = false;
            const char *text = systray_text_for_item(
                    s_tray.text_order[i], &enabled);

            if (!enabled || text[0] == '\0') {
                continue;
            }

            text_draw_string(xcb_connection_get(), s_tray.window, XCB_NONE,
                    (struct position_s) { pen_x, item_y }, text);
            pen_x = (int16_t) (pen_x +
                    (int16_t) text_string_measure(text) +
                    (int16_t) s_tray.text_gap);
        }
    }


    systray_layout_restack();

    /* Only when the strut actually changed does what every desktop on
     * this same surface considers its own available 'workarea' change
     * with it.  A reflow that republished the same strut, as every
     * repaint of the tray does, has nothing to recompute: an icon
     * dragged across the tray exposes it hundreds of times a second,
     * and each of those was walking every client of every desktop to
     * arrive back at the numbers already there.
     *
     * When it did change, it is recomputed here rather than left for
     * whatever unrelated trigger happens to call this next, the same
     * reasoning 'wm_action_config_reload' already applies to a changed
     * 'desktops.margins' (see its comment in wm/actions.c). */
    if (is_strut_changed) {
        surface_refresh_workareas(s_tray.surface);
    }
}
