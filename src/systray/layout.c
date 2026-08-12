/**
 * @file systray/layout.c
 *
 * @brief Systray positioning, stacking, and drawing
 *
 * Everything about where the tray window and its docked icons end up
 * on screen, and what gets drawn there: sizing and moving the tray to
 * its configured corner, arranging icons in a row, drawing the clock/
 * battery text next to them, and applying the configured stacking
 * layer.  The text content itself (what the clock or battery status
 * actually says) is read and formatted elsewhere; see
 * @c systray/text.c.
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
#include <string.h>     /* memset */

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <render/text.h>
#include <wm.h>

/* Local includes */
#include <systray/internal.h>


/**
 * @brief Pixel width the tray window needs for the current icon count
 *        and, if enabled, the clock and/or battery status text
 *
 * @return 0 when no icons are docked and neither text item is
 *         enabled (the tray window stays unmapped in that case),
 *         otherwise enough to fit every icon with padding around and
 *         between each, plus the combined text width when any of it
 *         is enabled
 */
static uint16_t s_systray_content_width(void)
{
    uint16_t icons_w = (s_tray.icon_count == 0u) ? 0u
        : (uint16_t) (s_tray.pixmap_pad +
            s_tray.icon_count * (s_tray.pixmap_size + s_tray.pixmap_pad));

    return (uint16_t) (icons_w + systray_text_width());
}


/**
 * @brief Find the topmost currently fullscreen client's own stacking
 *        target, if any
 *
 * @return The last fullscreen client found's frame (or window, if it
 *         has no frame), scanning every desktop on every surface, or
 *         @c XCB_WINDOW_NONE if none is currently fullscreen
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients across every desktop and surface
 */
static xcb_window_t s_systray_find_fullscreen_target(void)
{
    list_td *surfaces;
    xcb_window_t target = XCB_WINDOW_NONE;

    surfaces = wm_get_surfaces();
    if (surfaces == NULL) {
        return XCB_WINDOW_NONE;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;
        cdlist_item_td *dinitial;

        if (surface == NULL || surface->desktops == NULL) {
            continue;
        }
        dnode = cdlist_head(surface->desktops);
        if (dnode == NULL) {
            continue;
        }
        dinitial = dnode;
        do {
            desktop_td *desktop = (desktop_td *) cdlist_data(dnode);
            void *elem;

            if (desktop != NULL && desktop->clients != NULL) {
                ohtbl_foreach(desktop->clients, elem) {
                    client_td *client = (client_td *) elem;

                    if (client->properties.state !=
                            (uint16_t) CLIENT_STATE_FULLSCREEN) {
                        continue;
                    }
                    target = (client->frame != 0)
                        ? client->frame : client->window;
                }
            }
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial);
    }

    return target;
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
 * absolute bottom of the sibling stack out from under any icon that
 * was already there, the same "whichever restacked most recently
 * wins" problem @c ccmd_client_iconify's own explicit stack-below
 * (see @c systray_below_window) handles for the opposite ordering.
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients across every desktop and surface
 */
static void s_systray_push_icons_below(void)
{
    list_td *surfaces;

    surfaces = wm_get_surfaces();
    if (surfaces == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;
        cdlist_item_td *dinitial;

        if (surface == NULL || surface->desktops == NULL) {
            continue;
        }
        dnode = cdlist_head(surface->desktops);
        if (dnode == NULL) {
            continue;
        }
        dinitial = dnode;
        do {
            desktop_td *desktop = (desktop_td *) cdlist_data(dnode);
            void *elem;

            if (desktop != NULL && desktop->clients != NULL) {
                ohtbl_foreach(desktop->clients, elem) {
                    client_td *client = (client_td *) elem;

                    if (client->is_icon_mapped &&
                            client->icon_window != 0u) {
                        xcb_configure_window(s_tray.connection,
                                client->icon_window,
                                XCB_CONFIG_WINDOW_SIBLING |
                                XCB_CONFIG_WINDOW_STACK_MODE,
                                (const uint32_t[]) {
                                s_tray.window, XCB_STACK_MODE_BELOW
                                });
                    }
                }
            }
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial);
    }
}


/* Apply the configured 'systray.layer' stacking rule
 *
 * - CONFIG_SYSTRAY_LAYER_BELOW (the default): stacks the tray window
 *   at the very bottom, behind every client window.
 * - CONFIG_SYSTRAY_LAYER_ABOVE: stacks it at the top, unless a client
 *   is currently fullscreen, in which case it stacks just below that
 *   client instead, so a fullscreen window still covers it; the same
 *   way a taskbar or panel gets covered by a fullscreen window in
 *   most desktop environments, instead of a systray floating above
 *   literally everything regardless of what the user is doing.
 *   Always resolved to its final position in one single
 *   'ConfigureWindow' call (see 's_systray_find_fullscreen_target'
 *   above), never by raising to the top and only then lowering in a
 *   second, separate request, which would flash the tray above
 *   fullscreen content for the brief moment between the two.
 * - CONFIG_SYSTRAY_LAYER_OVERLAY: stacks it at the top and leaves it
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

    if (!s_tray.window_ready || s_tray.connection == NULL) {
        return;
    }

    if (s_tray.layer == CONFIG_SYSTRAY_LAYER_BELOW) {
        xcb_configure_window(s_tray.connection, s_tray.window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const uint32_t[]) { XCB_STACK_MODE_BELOW });
        s_systray_push_icons_below();
        xcb_flush(s_tray.connection);
        return;
    }

    fullscreen_target = (s_tray.layer == CONFIG_SYSTRAY_LAYER_ABOVE)
        ? s_systray_find_fullscreen_target() : XCB_WINDOW_NONE;

    /* A single 'ConfigureWindow' call straight to the final position,
     * rather than unconditionally raising to the very top first and
     * only then lowering below a fullscreen client in a second,
     * separate request: that two-step sequence briefly left the tray
     * stacked above the fullscreen content between the two requests,
     * visible as a flash on every restack (every reflow, and every
     * fullscreen toggle) rather than only when actually needed. */
    if (fullscreen_target != XCB_WINDOW_NONE) {
        xcb_configure_window(s_tray.connection, s_tray.window,
                XCB_CONFIG_WINDOW_SIBLING |
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const uint32_t[]) {
                    fullscreen_target, XCB_STACK_MODE_BELOW
                });
    } else {
        xcb_configure_window(s_tray.connection, s_tray.window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
    }

    xcb_flush(s_tray.connection);
}


/**
 * @brief Resolve the rectangle the tray dock's corner is anchored to
 *
 * Under @c CONFIG_SYSTRAY_MONITOR_SURFACE (the default), returns
 * @c s_tray.surface's own combined dimensions, exactly the previous,
 * always-whole-surface behavior, treated as one virtual monitor
 * spanning it (the same fallback @c s_surface_monitors_fallback uses
 * when RandR itself cannot supply a real monitor list).  Under @c
 * CONFIG_SYSTRAY_MONITOR_PRIMARY, returns whichever monitor RandR
 * reports as primary.  Under @c CONFIG_SYSTRAY_MONITOR_INDEX, returns
 * @c s_tray.monitor.index specifically (out of range falls back to
 * monitor 0, logging a warning).
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


/**
 * @brief Publish (or clear) the tray's own reserved-space strut on
 *        its dock window, and mirror the same values into
 *        's_tray.reserved_strut' for 'systray_get_reserved_strut'
 *
 * Per the specification's own recommendation for a docking area, a
 * taskbar, or a panel, the tray publishes '_NET_WM_STRUT_PARTIAL'
 * -- and, for compatibility with anything that only understands
 * the legacy property, plain
 * '_NET_WM_STRUT' alongside it -- covering the exact strip of screen
 * its own configured corner and current size occupy, so a maximized
 * window (and this window manager's own placement logic, via
 * 'desktop_update_workarea') both leave that strip alone the same
 * way they already do for an external panel or dock, plus
 * 'config.systray.margins' added on top of that strip.  Publishes an
 * all-zero strut instead when 'config.systray.reserve-space' is
 * false, for anyone who would rather windows stayed free to maximize
 * over or under the tray.
 *
 * @param x       Tray's own configured X position (root coordinates)
 * @param y       Tray's own configured Y position (root coordinates)
 * @param w       Tray's own current width, border excluded; zero
 *                clears the strut (the tray itself is unmapped)
 * @param h       Tray's own current height, border excluded
 * @param border2 Total border thickness, both sides combined
 *
 * @note Complexity: @e O(1)
 */
static void s_systray_update_strut(int16_t x, int16_t y, uint16_t w,
        uint16_t h, int32_t border2)
{
    xcb_ewmh_wm_strut_partial_t partial;
    uint32_t screen_h;

    memset(&partial, 0, sizeof(partial));

    /* 'reserve_space == false' leaves 'partial' at the all-zero shape
     * 'memset' above already set it to -- an explicit '{0, 0, 0, 0}'
     * strut, published and stored exactly like any other, rather than
     * simply skipping the publish/store below: a caller that reads
     * 's_tray.reserved_strut' should see "reserves nothing" the same
     * way it would for a real client with no strut of its own, not a
     * stale value left over from whenever reservation was last
     * enabled. */
    if (s_tray.reserve_space && s_tray.surface != NULL &&
            w > 0u && h > 0u) {
        screen_h = s_tray.surface->properties.dim.h;

        switch (s_tray.position) {
            case CONFIG_SYSTRAY_POSITION_TOP_LEFT:
            case CONFIG_SYSTRAY_POSITION_TOP_RIGHT:
                partial.top = (uint32_t) ((int32_t) y + (int32_t) h +
                        border2);
                partial.top_start_x = (uint32_t) x;
                partial.top_end_x = (uint32_t) ((int32_t) x +
                        (int32_t) w + border2);
                break;

            case CONFIG_SYSTRAY_POSITION_BOTTOM_LEFT:
            case CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT: {
                uint32_t y_u = (uint32_t) y;

                partial.bottom = (screen_h > y_u)
                    ? screen_h - y_u : 0u;
                partial.bottom_start_x = (uint32_t) x;
                partial.bottom_end_x = (uint32_t) ((int32_t) x +
                        (int32_t) w + border2);
                break;
            }
        }

        /* 'config.systray.margins': added on top of whatever the
         * switch above just computed from the tray's own actual
         * geometry, the same way 'config_desktop_s''s own 'margins'
         * adds on top of a client's published strut in
         * 'desktop_update_workarea' -- not restricted to the edge the
         * tray currently docks at (left/right add to a screen side
         * the tray itself never reserves on its own), left with no
         * start/end range of their own to honor (0..0), so they apply
         * along the whole edge unconditionally, exactly like
         * 'config_desktop_s''s own margins do. */
        partial.top += s_tray.strut_margins.top;
        partial.right += s_tray.strut_margins.right;
        partial.bottom += s_tray.strut_margins.bottom;
        partial.left += s_tray.strut_margins.left;
    }

    if (s_tray.ewmh != NULL && s_tray.window != XCB_WINDOW_NONE) {
        (void) xcb_ewmh_set_wm_strut_partial(s_tray.ewmh, s_tray.window,
                partial);
        (void) xcb_ewmh_set_wm_strut(s_tray.ewmh, s_tray.window,
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
}


/* Reposition the tray window and lay out its docked icons
 *
 * Unmaps the tray window while empty (nothing docked and neither the
 * clock nor the battery text enabled) or while the tray is not
 * currently active at all (disabled by configuration; see
 * 'is_active''s own doc comment in include/systray/internal.h), so it
 * never shows on screen in either case; otherwise sizes and moves it
 * to the configured corner of
 * 's_tray.surface' and arranges icons in a single horizontal row
 * inside it, in 's_tray.icons' order (see 'systray_protocol_dock' in
 * systray/protocol.c for how that order is maintained per the
 * 'order' policy). */
void systray_layout_reflow(void)
{
    uint16_t w;
    uint16_t h;
    uint16_t text_w;
    uint16_t icons_base_x;
    uint16_t icon_y;
    int16_t x = 0;
    int16_t y = 0;
    int32_t border2;
    uint32_t geom_values[4];
    monitor_td anchor;

    if (!s_tray.window_ready || s_tray.surface == NULL) {
        return;
    }

    if (!s_tray.is_active ||
            (s_tray.icon_count == 0u && !s_tray.clock_enabled &&
                !s_tray.battery_enabled)) {
        xcb_unmap_window(s_tray.connection, s_tray.window);
        s_systray_update_strut(0, 0, 0u, 0u, 0);
        xcb_flush(s_tray.connection);
        return;
    }

    h = s_tray.height;
    text_w = systray_text_width();
    w = s_systray_content_width();
    if (w == 0u) {
        xcb_unmap_window(s_tray.connection, s_tray.window);
        s_systray_update_strut(0, 0, 0u, 0u, 0);
        xcb_flush(s_tray.connection);
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

    geom_values[0] = (uint32_t) x;
    geom_values[1] = (uint32_t) y;
    geom_values[2] = w;
    geom_values[3] = h;
    xcb_configure_window(s_tray.connection, s_tray.window,
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT,
            geom_values);
    s_systray_update_strut(x, y, w, h, border2);

    /* Icons sit after the text block when it is on the left, or right
     * at the tray's own left edge otherwise (text block on the
     * right, or nothing enabled). */
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
         * before the addition though, which trips
         * '-Wsign-conversion' on assignment to the 'uint32_t' array
         * below.  The explicit casts keep every step of the
         * arithmetic in 'uint32_t' instead. */
        icon_pos[0] = (uint32_t) icons_base_x +
            (uint32_t) s_tray.pixmap_pad + (uint32_t) i * stride;
        icon_pos[1] = icon_y;
        xcb_configure_window(s_tray.connection, s_tray.icons[i].window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, icon_pos);
    }

    xcb_map_window(s_tray.connection, s_tray.window);

    if (text_w > 0u && s_tray.theme != NULL) {
        uint16_t icons_w = (uint16_t) (w - text_w);
        int16_t block_x = (s_tray.text_position == CONFIG_SYSTRAY_TEXT_LEFT)
            ? 0 : (int16_t) icons_w;
        int16_t ascent;
        int16_t descent;
        int16_t item_h;
        int16_t item_y = 0;
        int16_t pen_x;

        xcb_clear_area(s_tray.connection, 0, s_tray.window,
                block_x, 0, text_w, h);
        text_renderer_init(s_tray.connection,
                s_tray.theme->systray.style.font);
        text_renderer_set_color(s_tray.theme->systray.style.color.foreground,
                s_tray.theme->systray.style.color.background);

        /* 'text_draw_string' takes the baseline, not the top of the
         * text, so each alignment has to add the font's own ascent
         * (see 'text_font_ascent') to whatever pixel the top of the
         * text should land on.  Every item in 'text_order' shares one
         * font, so this is computed once and reused for each. */
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

            text_draw_string(s_tray.connection, s_tray.window, XCB_NONE,
                    pen_x, item_y, text);
            pen_x = (int16_t) (pen_x +
                    (int16_t) text_measure_string(text) +
                    (int16_t) s_tray.text_gap);
        }
    }

    xcb_flush(s_tray.connection);

    systray_layout_restack();

    /* The strut just published (or cleared) above changes what every
     * desktop on this same surface considers its own available
     * 'workarea' -- recomputed here rather than left for whatever
     * unrelated trigger happens to call this next, the same reasoning
     * 'wm_action_config_reload' already applies to a changed
     * 'desktops.margins' (see its own comment in wm/actions.c). */
    surface_refresh_workareas(s_tray.surface);
}
