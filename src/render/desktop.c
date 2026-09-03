/**
 * @file render/desktop.c
 *
 * @brief Desktop rendering implementation
 *
 * @note Decoration constants come from @c defs/client.h, icon constants
 *       from @c defs/icon.h; button colors come from the theme passed
 *       to @c s_desktop_titlebar_buttons_draw
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>     /* free, NULL */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/icon.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */
#include <adt/ohtbl.h>  /* Hash table for clients */

/* Menu includes */
#include <menu/cycle.h>

/* Policy includes */
#include <policy/urgency.h>

/* Utils includes */
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>

/* Project includes */
#include <client.h>
#include <logger.h>
#include <render/text.h>
#include <surface.h>
#include <wm.h>

#include <desktop.h>
#include <policy/stacking.h>
#include <config.h>

/* Local includes */
#include <render/desktop.h>
#include <render/icon.h>
#include <render/outdate.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/* Per-screen (not per-desktop) cache of the root window's last
 * solid-color fill, indexed by 'screen_id'.  Every virtual desktop on
 * a given screen shares that one same root window as an X resource, so
 * whether it still shows a particular desktop's configured color
 * has to be tracked per screen too, not per desktop.
 *
 * A field on 'desktop_td' itself would instead let each desktop
 * believe its color remains applied purely because it was
 * the last one THAT desktop painted, even after some other desktop
 * sharing the same root window repainted over it with a different one;
 * and, since a config reload does not reset any of this, leaves that
 * other, now-stale color on screen indefinitely, with no further
 * repaint ever believing there is anything left to fix. */
static bool s_root_bg_applied_once[CONFIG_MAX_SCREENS];
static uint32_t s_root_bg_color_applied[CONFIG_MAX_SCREENS];


/**
 * @brief Property names a wallpaper-setting tool might publish its
 *        own root window background pixmap under
 *
 * - @c ESETROOT_PMAP_ID: legacy 'Esetroot' alias, also used by @c feh
 * - @c _XROOTPMAP_ID: set by @c Esetroot, @c feh, @c nitrogen,
 *   @c hsetroot, and others
 * - @c _XSETROOT_ID: set by @c xsetroot and @c xsetbg
 *
 * Shared between @c s_get_root_background_pixmap, which checks each
 * in turn for a pixmap value, and
 * @c desktop_property_is_background_pixmap, which the @c PropertyNotify
 * handler in handler/focus.c uses to recognize a change to one of
 * them on the root window.
 */
static const char *const s_bg_prop_names[3] = {
    "_XROOTPMAP_ID", "ESETROOT_PMAP_ID", "_XSETROOT_ID"
};

/**
 * @brief Cached, once-resolved atoms for @c s_bg_prop_names
 *
 * None of these ever changes once interned (an atom, once assigned by
 * the X server, is permanent for the life of the connection), so
 * resolving them again on every lookup would be pure waste; resolved
 * lazily by @a s_resolve_bg_atoms on first use, retried on any later
 * call for whichever of the three are still unresolved.
 *
 * @see @a s_resolve_bg_atoms's comment for why a resolution
 *       failure, unlike a success, is not permanent here
 */
static xcb_atom_t s_bg_atoms[3] = {
    XCB_ATOM_NONE, XCB_ATOM_NONE, XCB_ATOM_NONE
};


/**
 * @brief Cached resolution of the root window's background pixmap
 *
 * A well-behaved system sets this once (a wallpaper tool such as
 * feh, nitrogen, or hsetroot runs once at session start) and
 * essentially never changes it again during a normal session, so
 * resolving it fresh on every @a s_get_root_background_pixmap call (up
 * to three property fetches, each a round trip to the X server) would
 * be paying that cost repeatedly for something that stays the same
 * almost every single time.
 *
 * @note Cached here instead, and only re-resolved once
 *       @a desktop_background_pixmap_cache_invalidate says the
 *       underlying property actually changed
 */
static bool s_bg_pixmap_resolved[CONFIG_MAX_SCREENS];
static xcb_pixmap_t s_bg_pixmap_cache[CONFIG_MAX_SCREENS];


/**
 * @brief Resolve @c s_bg_prop_names into @c s_bg_atoms, once
 *
 * @param connection XCB connection used to intern any atom not
 *                   already resolved from a previous call
 *
 * @note Complexity: @e O(1) once resolved; @e O(n) in the number of
 *       candidate properties the first time
 */
static void s_resolve_bg_atoms(xcb_connection_t *connection)
{
    bool any_unresolved = false;

    /* Re-attempts only whichever of the three atoms are still
     * 'XCB_ATOM_NONE', rather than giving up on all three permanently
     * the moment any single attempt is made.
     *
     * 'only_if_exists=true' in 'atom_intern' means a name that does not
     * exist yet on the X server resolves to none, which is correct at
     * that moment, but unlike a successful resolution (an atom, once it
     * exists, is permanent for the life of the connection) that failure
     * is not itself permanent.
     *
     * A wallpaper tool run for the first time after this module's
     * first lookup, before any of these three names had ever been
     * interned by anyone, would otherwise be watched for forever using
     * an atom id that was cached as none before it ever existed. */
    for (size_t i = 0; i < 3u; ++i) {
        if (s_bg_atoms[i] == XCB_ATOM_NONE) {
            any_unresolved = true;
            break;
        }
    }

    if (!any_unresolved) {
        return;
    }

    for (size_t i = 0; i < 3u; ++i) {
        if (s_bg_atoms[i] == XCB_ATOM_NONE) {
            s_bg_atoms[i] = atom_intern(connection, s_bg_prop_names[i],
                    true);
        }
    }
}


/**
 * @brief Retrieve the root window background pixmap if set
 *
 * Queries the root window for one of the standard background pixmap
 * properties (@c _XROOTPMAP_ID, @c ESETROOT_PMAP_ID, @c _XSETROOT_ID)
 * and returns the first non-@c XCB_NONE pixmap found, or @c XCB_NONE if
 * no valid pixmap is present.
 *
 * A cache hit costs nothing beyond returning the cached value, in
 * contrast to a miss, which pays for up to three property fetches, each
 * its round trip to the X server.
 *
 * @param connection XCB connection to the X server
 * @param root       Root window to query for background pixmap
 * @param screen_id  Screen the cache entry belongs to, one entry per
 *                   managed screen
 *
 * @return Root background pixmap, or @c XCB_NONE where none is
 *         available
 *
 * @note Resolved once and cached from then on
 * @note Complexity: @e O(1) on a cache hit; @e O(n) in the number of
 *       candidate properties on a cache miss
 *
 * @see @a s_bg_pixmap_resolved above
 */
static xcb_pixmap_t
    s_get_root_background_pixmap(xcb_connection_t *connection,
            xcb_window_t root, uint32_t screen_id)
{
    if (screen_id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return XCB_NONE;
    }

    if (s_bg_pixmap_resolved[screen_id]) {
        LOGGER_TRACE("Root background pixmap cache hit for screen" \
                " %u: 0x%x", screen_id, s_bg_pixmap_cache[screen_id]);
        return s_bg_pixmap_cache[screen_id];
    }

    if (connection == NULL || root == XCB_WINDOW_NONE) {
        return XCB_NONE;
    }

    s_resolve_bg_atoms(connection);

    for (size_t i = 0; i < 3u; ++i) {
        xcb_get_property_reply_t *reply;
        xcb_pixmap_t pixmap = XCB_NONE;

        if (s_bg_atoms[i] == XCB_ATOM_NONE) {
            continue;
        }

        reply = xcb_get_property_reply(connection,
                xcb_get_property(connection, 0, root, s_bg_atoms[i],
                    XCB_ATOM_PIXMAP, 0, 1), NULL);
        if (reply == NULL) {
            continue;
        }

        if (reply->format == 32 && reply->value_len >= 1 &&
                xcb_get_property_value(reply) != NULL) {
            pixmap = *((xcb_pixmap_t *) xcb_get_property_value(reply));
        }
        free(reply);

        if (pixmap != XCB_NONE) {
            s_bg_pixmap_cache[screen_id] = pixmap;
            s_bg_pixmap_resolved[screen_id] = true;
            return pixmap;
        }
    }

    /* No wallpaper tool has set any of the candidate properties; that
     * is itself a stable outcome worth caching too, not just
     * a successful resolution, so a desktop with no such tool running
     * does not keep paying for this same negative lookup either */
    LOGGER_TRACE("No external root pixmap property found for screen" \
            " %u (checked atoms 0x%x, 0x%x, 0x%x); using configured" \
            " color", screen_id, s_bg_atoms[0], s_bg_atoms[1],
            s_bg_atoms[2]);
    s_bg_pixmap_cache[screen_id] = XCB_NONE;
    s_bg_pixmap_resolved[screen_id] = true;
    return XCB_NONE;
}


/**
 * @brief Color a single titlebar button should be drawn in
 *
 * Pin and layer buttons reflect their state (sticky or non-normal
 * layer) with the active accent color regardless of window focus; every
 * other button reflects window focus instead, the same way the titlebar
 * text itself does.  Maximize and fullscreen fall back to the
 * background color, which makes them effectively invisible, when the
 * client cannot be resized,
 * instead of drawing a button that would do nothing if clicked.
 */
static uint32_t s_titlebar_button_color(
        enum config_titlebar_button_e button, bool is_focused,
        bool is_sticky, bool is_layered, bool can_maximize,
        uint32_t color_active, uint32_t color_inactive,
        uint32_t bg_fill)
{
    if (!can_maximize &&
            (button == CONFIG_TITLEBAR_BUTTON_MAXIMIZE ||
             button == CONFIG_TITLEBAR_BUTTON_FULLSCREEN)) {
        return bg_fill;
    }
    if (button == CONFIG_TITLEBAR_BUTTON_PIN) {
        return (is_sticky) ? color_active : color_inactive;
    }
    if (button == CONFIG_TITLEBAR_BUTTON_LAYER) {
        return (is_layered) ? color_active : color_inactive;
    }

    return (is_focused) ? color_active : color_inactive;
}


/**
 * @brief Draw the buttons configured in @c window.titlebar.buttons on
 *        a titlebar window
 *
 * Draws exactly the buttons in @p left (before the window title) and
 * @p right (after the window title), at the positions
 * @c client_titlebar_layout already computed for them.
 *
 * The position is never recomputed on by this function on its own, so
 * it can never disagree with the click hit-test, which uses the same
 * computed layout.  The fill color for most buttons is taken from
 * @p theme: @c window.active.color.foreground when @p is_focused is
 * @c true, @c window.inactive.color.foreground otherwise; the pin and
 * layer buttons instead reflect their state (sticky/non-normal
 * layer) regardless of focus; maximize and fullscreen fall back to the
 * background color when @p can_maximize is @c false.
 *
 * @param connection   Active XCB connection
 * @param titlebar     XCB window identifier of the titlebar
 * @param btn_y        Y position every button shares, from
 *                     @c client_titlebar_layout
 * @param left         Left-side button layout from
 *                     @c client_titlebar_layout
 * @param left_n       Number of entries in @p left
 * @param right        Right-side button layout from
 *                     @c client_titlebar_layout
 * @param right_n      Number of entries in @p right
 * @param is_focused   Whether the owning client is currently focused
 * @param is_sticky    Whether the owning client has the sticky flag set
 * @param is_layered   Whether the client layer is above or below normal
 * @param can_maximize Whether the maximize button is enabled
 * @param theme        Pointer to the theme providing button colors
 *
 * @note Complexity: @e O(n), where @e n is @p left_n + @p right_n
 */
static void s_desktop_titlebar_buttons_draw(xcb_connection_t *connection,
        xcb_window_t titlebar, int16_t btn_y,
        const struct titlebar_button_layout_s *left, uint8_t left_n,
        const struct titlebar_button_layout_s *right, uint8_t right_n,
        bool is_focused, bool is_sticky, bool is_layered,
        bool can_maximize, const struct config_theme_s *theme)
{
    xcb_gcontext_t gc;
    uint32_t color;
    xcb_rectangle_t rect;
    uint16_t btn = (uint16_t) WM_DECOR_BTN_SIZE;

    /* Button colors have their dedicated theme entry, independent
     * of the titlebar text foreground, so a theme can style one
     * without the other changing to match.  See
     * 'window.titlebar.buttons.color'. */
    uint32_t color_active = (theme != NULL)
        ? theme->window.titlebar.buttons.color.on
        : 0x000000u;
    uint32_t color_inactive = (theme != NULL)
        ? theme->window.titlebar.buttons.color.off
        : 0xFFFFFFu;
    uint32_t bg_fill = (theme != NULL)
        ? ((is_focused)
            ? theme->window.active.color.background
            : theme->window.inactive.color.background)
        : color_active;

    if (connection == NULL || titlebar == XCB_WINDOW_NONE) {
        return;
    }

    for (uint8_t i = 0u; i < left_n; ++i) {
        color = s_titlebar_button_color(left[i].button, is_focused,
                is_sticky, is_layered, can_maximize, color_active,
                color_inactive, bg_fill);
        gc = xcb_generate_id(connection);
        xcb_create_gc(connection, gc, titlebar,
                XCB_GC_FOREGROUND, &color);
        rect = (xcb_rectangle_t) { left[i].x, btn_y, btn, btn };
        xcb_poly_fill_rectangle(connection, titlebar, gc, 1, &rect);
        xcb_free_gc(connection, gc);
    }

    for (uint8_t i = 0u; i < right_n; ++i) {
        color = s_titlebar_button_color(right[i].button, is_focused,
                is_sticky, is_layered, can_maximize, color_active,
                color_inactive, bg_fill);
        gc = xcb_generate_id(connection);
        xcb_create_gc(connection, gc, titlebar,
                XCB_GC_FOREGROUND, &color);
        rect = (xcb_rectangle_t) { right[i].x, btn_y, btn, btn };
        xcb_poly_fill_rectangle(connection, titlebar, gc, 1, &rect);
        xcb_free_gc(connection, gc);
    }
}


/**
 * @brief Draw a titlebar's text, clipped to the space the buttons leave
 *        available, honoring the theme's chosen alignment
 *
 * A title too wide for the available space is truncated one character
 * at a time until it fits, rather than letting it draw underneath the
 * right-hand buttons.  Whatever ends up actually drawn is kept in sync
 * with @c _NET_WM_VISIBLE_NAME via @a client_sync_visible_name, so a
 * pager showing the same title has a way to know it no longer matches
 * @c _NET_WM_NAME verbatim.
 */
static void s_titlebar_draw_title(xcb_connection_t *connection,
        client_td *client, xcb_window_t titlebar, int16_t title_x,
        uint16_t title_w, int16_t text_y, const char *text,
        enum config_titlebar_alignment_e alignment)
{
    char buf[CONFIG_MAX_LENGTH_NAME];
    uint16_t text_w;
    int16_t draw_x;
    bool can_sync;

    if (connection == NULL || text == NULL || text[0] == '\0' ||
            title_w == 0u) {
        return;
    }

    can_sync = (client != NULL && xcb_ewmh_connection_get() != NULL);

    text_truncate_to_width(buf, sizeof(buf), text, title_w);
    text_w = text_string_measure(buf);

    if (can_sync) {
        client_sync_visible_name(client, client->info.visible_name,
                text, buf, xcb_ewmh_set_wm_visible_name_checked,
                xcb_ewmh_connection_get()->_NET_WM_VISIBLE_NAME);
    }

    if (buf[0] == '\0') {
        return;
    }

    draw_x = title_x;
    if (alignment == CONFIG_TITLEBAR_ALIGN_CENTER && text_w < title_w) {
        draw_x = (int16_t) (title_x + (title_w - text_w) / 2);
    } else if (alignment == CONFIG_TITLEBAR_ALIGN_RIGHT &&
            text_w < title_w) {
        draw_x = (int16_t) (title_x + (title_w - text_w));
    }

    text_draw_string(connection, titlebar, XCB_NONE,
            (struct position_s) { draw_x, text_y }, buf);
}


/**
 * @brief Repaint a client's frame decoration, unless it is
 *        currently forced hidden
 *
 * Shared by @c desktop_render_one_client's full-repaint and
 * focus-only-repaint branches, which otherwise each repeat the exact
 * same @c hide_decoration guard around the same call (see that
 * function's @c hide_decoration for what forces this.  Currently
 * only a fullscreen client that was decorated before going
 * fullscreen).
 *
 * @param connection      XCB connection
 * @param client          Client whose frame decoration to repaint
 * @param is_focused      Whether to use the active or inactive
 *                        color set
 * @param hide_decoration Whether decoration is currently suppressed
 *                        entirely; a no-op when @c true
 * @param theme           Active theme
 *
 * @note Complexity: @e O(1)
 */
static void s_repaint_frame_decoration_unless_hidden(
        xcb_connection_t *connection, const client_td *client,
        bool is_focused, bool hide_decoration,
        const struct config_theme_s *theme)
{
    if (!hide_decoration) {
        desktop_repaint_frame_decoration(connection, client, is_focused,
                theme);
    }
}


/* Draw all clients on a desktop */
/**
 * @brief What rendering one client needs to know about it
 *
 * Gathered so the two paths below, applying an outdated geometry and
 * refreshing decoration colors alone, can each be a function rather
 * than another hundred lines inside
 * @a desktop_render_one_client.  Only what both paths need before
 * they start is carried: the frame extents and the titlebar height
 * are worked out inside each of them, and the theme is reached
 * through @p desktop.
 */
struct s_render_ctx_s {
    desktop_td *desktop;
    client_td *client;
    xcb_window_t target;
    bool is_focused;
    bool hide_decoration;
    bool titlebar_visible;
};


/**
 * @brief Apply a client's geometry, and repaint what it moves
 *
 * Taken when the client is marked outdated, so its position and size
 * are sent to the server and every decoration is laid out against
 * the geometry that results.
 *
 * @param ctx State of the client being rendered
 *
 * @note Complexity: @e O(1)
 */
static void s_render_apply_geometry(struct s_render_ctx_s *ctx)
{
    desktop_td *const desktop = ctx->desktop;
    client_td *const client = ctx->client;
    const xcb_window_t target = ctx->target;
    const bool is_focused = ctx->is_focused;
    const bool hide_decoration = ctx->hide_decoration;
    const bool titlebar_visible = ctx->titlebar_visible;

    /* Configure position and size; only when the client's
     * geometry or decoration changed.  Skipping this for
     * up-to-date clients prevents the server from generating
     * spurious 'ConfigureNotify' and 'Expose' events that cause
     * other windows to unnecessarily redraw, which appears as
     * flicker during keyboard resize of an unrelated client. */
    LOGGER_TRACE("Render pass applying outdated geometry for" \
            " window=0x%x: target=0x%x (%s frame), %ux%u%+d%+d",
            client->window, target,
            (target != client->window) ? "has" : "no",
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y);

    xcb_window_place(target,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y,
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h);
    if (target != client->window) {
        uint16_t top;
        uint16_t bottom;
        uint16_t left;
        uint16_t right;
        uint16_t inner_h;
        uint16_t inner_w;
        uint16_t title_h;

        /* Forced to zero outright for a fullscreen client, rather
         * than trusting 'frame_extents' to already be zero.  This
         * is the exact geometry a click or a losing-focus repaint
         * would otherwise leave stuck at whatever theme padding
         * 'frame_extents' happened to hold, showing the frame's
         * own background (set to the theme's border color by
         * 'desktop_repaint_frame_decoration') through the gap left
         * along the content window's top and left edges.
         * Visually indistinguishable from a real border, though
         * neither an X11 border nor that repaint function was
         * ever actually involved. */
        if (hide_decoration) {
            left = 0;
            right = 0;
            top = 0;
            bottom = 0;
        } else {
            left = (uint16_t) client->layout.frame_extents.left;
            right = (uint16_t) client->layout.frame_extents.right;
            top = (uint16_t) client->layout.frame_extents.top;
            bottom = (uint16_t) client->layout.frame_extents.bottom;
        }
        title_h = (uint16_t) client->title_height;
        inner_w = (client->layout.geometry.cur.dim.w > left + right)
            ? (uint16_t) (client->layout.geometry.cur.dim.w -
                    left - right)
            : 1;
        inner_h = (client->layout.geometry.cur.dim.h > top + bottom)
            ? (uint16_t) (client->layout.geometry.cur.dim.h -
                    top - bottom)
            : 1;

        /* A shaded client's content window is deliberately
         * left unmapped, at whatever geometry it already had
         * (see 'ccmd_client_shade', cmds/client/state.c).  None
         * of the three calls below (repositioning it, telling it
         * about that new position, and prompting it to redraw)
         * are meant for it while shaded, since it is never seen
         * regardless of what geometry it holds.  'left'/'top'/
         * 'inner_w'/'title_h', computed above either way, are
         * still needed below this whole 'if' to position the
         * titlebar correctly even while shaded. */
        if (!client_is_shaded(client)) {
            xcb_window_place(client->window, left, top,
                    inner_w, inner_h);

            /* ICCCM §4.2.3: the 'xcb_configure_window' above
             * positions the inner window relative to the frame
             * (x=left, y=top), so the X server delivers a
             * 'ConfigureNotify' to the client with those
             * frame-relative coordinates.  Override it
             * immediately with a synthetic 'ConfigureNotify'
             * carrying the true screen-relative position so the
             * client's last geometry notification is always
             * correct.  Without this a decorated client sees a
             * frame-relative 'ConfigureNotify' as its final
             * event on every render pass, causing misaligned
             * popups and a content area that appears not to
             * fill the frame until the next user-triggered
             * repaint. */
            client_send_synthetic_configure_notify(
                    xcb_connection_get(), client);

            /* Force a repaint AFTER the synthetic
             * 'ConfigureNotify' so the client always redraws at
             * its correct screen-relative geometry.  Some
             * programs do not redraw on 'ConfigureNotify' alone;
             * this 'Expose' ensures the drawing happens at the
             * right size and position after every render pass,
             * including the initial map and post-resize
             * redraws.  Setting 'exposures=1' causes the X
             * server to generate an 'Expose' event, which
             * arrives in the client's queue after both the
             * 'xcb_configure_window' and the synthetic
             * 'ConfigureNotify' above. */
            xcb_clear_area(xcb_connection_get(), 1,
                    client->window, 0, 0, 0, 0);
        }
        s_repaint_frame_decoration_unless_hidden(xcb_connection_get(),
                client, is_focused, hide_decoration,
                &desktop->config->theme);

        if (titlebar_visible) {
            xcb_window_place(client->titlebar, left,
                    (top > title_h) ? top - title_h : 0,
                    inner_w, title_h);
            desktop_repaint_titlebar_content(
                    xcb_connection_get(), client, is_focused,
                    inner_w, title_h, &desktop->config->theme);
        } else if (client->titlebar != 0) {
            xcb_window_hide(client->titlebar);
        }
    }

    wm_validate_client(client);
}


/**
 * @brief Refresh a client's decoration without touching its geometry
 *
 * Taken when the geometry has not changed but the colors have, after
 * a focus change or an urgency blink, so only what is painted needs
 * redoing.
 *
 * @param ctx State of the client being rendered
 *
 * @note Complexity: @e O(1)
 */
static void s_render_refresh_decoration(struct s_render_ctx_s *ctx)
{
    desktop_td *const desktop = ctx->desktop;
    client_td *const client = ctx->client;
    const bool is_focused = ctx->is_focused;
    const bool hide_decoration = ctx->hide_decoration;
    const bool titlebar_visible = ctx->titlebar_visible;
    uint16_t left;
    uint16_t right;
    uint16_t inner_w;
    uint16_t title_h;

    /* The client geometry has not changed; only refresh the
     * focus-sensitive decoration colors (border and titlebar
     * background/text) when the active client actually changed,
     * or when this specific client is urgent (its attention
     * blink alternates these same colors on every phase change,
     * the same reasoning 'ri_render_client_icon', render/icon.c,
     * already applies via its '!client_is_urgent' skip-check
     * condition, just expressed the other way around here).
     * Skipping this repaint otherwise avoids spurious
     * 'xcb_clear_area + text-draw' calls on every render pass
     * during resize, which was the source of the desktop-wide
     * flickering visible on all non-resized windows. */
    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    title_h = (uint16_t) client->title_height;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w -
                left - right)
        : 1;

    /* Skipped entirely, not just recolored, for the same reason
     * the full-repaint branch above never gives a fullscreen
     * client a border in the first place (see its comment by
     * 'client_is_fullscreen' there): a focus change alone must
     * not paint one back in over fullscreen content just because
     * this lighter branch only meant to refresh existing colors,
     * not decide from scratch whether a border belongs here at
     * all. */
    s_repaint_frame_decoration_unless_hidden(xcb_connection_get(),
            client, is_focused, hide_decoration,
            &desktop->config->theme);

    if (titlebar_visible) {
        desktop_repaint_titlebar_content(xcb_connection_get(),
                client, is_focused, inner_w, title_h,
                &desktop->config->theme);
    } else if (client->titlebar != 0) {
        xcb_window_hide(client->titlebar);
    }
}


/**
 * @brief What @a s_desktop_render_client_visit needs beyond the client
 */
struct s_desktop_render_ctx_s {
    desktop_td *desktop;    /**< Desktop being rendered */
    int client_count;       /**< How many have been rendered */
    bool is_current;        /**< Whether it is the visible one */
};


/**
 * @brief Render one client, or its icon when it is iconified
 *
 * @param client Client reached by the walk
 * @param data   Pointer to this walk's render context
 *
 * @note A plainly hidden client is drawn neither way, being unmapped
 *       and having no icon standing in for it
 * @note Complexity: @e O(1)
 */
static void s_desktop_render_client_visit(client_td *client, void *data)
{
    struct s_desktop_render_ctx_s *const render_ctx = data;

    if (client == NULL || render_ctx == NULL) {
        return;
    }

    render_ctx->client_count++;

    /* Keep icon windows visible only for iconified clients.  Plain
     * hidden windows must stay fully unmapped. */
    if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
        if (client_is_iconified(client)) {
            ri_render_client_icon(client, render_ctx->is_current,
                    false);
        }
        return;
    }

    desktop_render_one_client(render_ctx->desktop, client,
            render_ctx->is_current);
}


/**
 * @brief Draw all clients on a desktop
 *
 * Iterates through all clients in the desktop's stacking list and
 * configures their geometry.  Windows are only mapped (made visible)
 * when @p is_current is @c true; for a desktop that is not the one
 * currently displayed on its surface, only geometry/stacking is updated
 * so that a stale full-render pass (triggered by an unrelated
 * @p is_outdated flag, e.g., after moving/resizing a client) cannot
 * undo an explicit @a surface_clients_hide and make a client reappear
 * on top of the desktop the user actually switched to.
 *
 * @param desktop    Pointer to the desktop to draw
 * @param is_current Whether @p desktop is the surface's currently
 *                   displayed desktop; when @c false, clients are not
 *                   (re-)mapped, only their geometry is updated
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to draw clients
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
static int s_desktop_render_clients(desktop_td *desktop, bool is_current)
{
    struct s_desktop_render_ctx_s render_ctx;
    int client_count = 0;
    size_t stacking_size;

    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    stacking_size = (size_t) stacking_count(desktop);
    LOGGER_DEBUG("Rendering %zu client(s) from stacking list" \
            " on desktop %u ('%s')",
            stacking_size, desktop->id, desktop->name);

    /* If no clients, return early */
    if (stacking_size == 0) {
        LOGGER_TRACE("No clients to render on desktop %u ('%s')",
                desktop->id, desktop->name);
        return 0;
    }

    render_ctx.desktop = desktop;
    render_ctx.is_current = is_current;
    render_ctx.client_count = 0;
    stacking_walk(desktop, s_desktop_render_client_visit, &render_ctx);
    client_count = render_ctx.client_count;

    LOGGER_DEBUG("Successfully rendered %d clients" \
            " on desktop %u ('%s')",
            client_count, desktop->id, desktop->name);

    return 0;
}


/**
 * @brief Render, position, and decorate a single already-non-hidden
 *        client during a stacking-order render pass
 *
 * Applies the client's border width (only when it actually
 * changed, to avoid needless server round trips), maps or unmaps its
 * frame/titlebar/content window as appropriate for whether @p desktop
 * is the surface's currently displayed one, and either reconfigures
 * its full geometry and decoration (when @c is_outdated) or, more
 * cheaply, only refreshes focus-sensitive decoration colors (when
 * only @p desktop's @c is_focus_dirty changed).  See the caller's
 * own stacking-order iteration for how this fits into a full render
 * pass.
 *
 * Public (not @c static) so @c policy/urgency.c can repaint one
 * specific urgent client directly on its own blink-phase change,
 * without forcing a full-desktop @c desktop_render_full pass (and
 * every other client on it repainting along with it) just to update
 * one client's titlebar colors.
 *
 * @param desktop    Desktop the client belongs to
 * @param client     Client to render; assumed non-@c NULL and not
 *                   currently hidden
 * @param is_current Whether @p desktop is the surface's currently
 *                   displayed desktop
 *
 * @note Complexity: @e O(1)
 */
void desktop_render_one_client(desktop_td *desktop,
        client_td *client, bool is_current)
{
    struct s_render_ctx_s ctx = {0};
    xcb_window_t target;
    bool is_focused;
    bool hide_decoration;
    bool titlebar_visible;
    uint32_t border_width;

    is_focused = (desktop->client_active_id == client->id);

    /* Swapped for one half of the blink cycle when this client is
     * urgent (see 'policy/urgency.h'): every later use of
     * 'is_focused' below (border, background, font, and text colors)
     * already follows from this one flip, so the titlebar reads as
     * attention-grabbing without a second, separate code path. */
    if (client_is_urgent(client) && urgency_blink_is_on()) {
        is_focused = !is_focused;
    }

    hide_decoration =
        (client_is_fullscreen(client) &&
         client->was_decorated_fullscreen);

    /* Computed once here rather than re-spelled out as 'client->
     * titlebar != 0 && !hide_decoration' at each of the three spots
     * below (the initial map, the full outdated repaint, and the
     * lighter focus-only repaint) that all need to draw exactly the
     * same distinction between "this client currently has a
     * titlebar to show at all" and "decoration is being suppressed
     * right now" (fullscreen). */
    titlebar_visible = (client->titlebar != 0 && !hide_decoration);

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    if (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK ||
            client->properties.type ==
                (uint16_t) CLIENT_TYPE_NOTIFICATION) {
        /* Dock and notification windows must never have a WM border */
        border_width = 0u;
    } else if (client_is_fullscreen(client)) {
        /* A fullscreen client is undecorated (see 'hide_decoration'
         * above), so it falls to the same 'not decorated' branch a
         * plain undecorated window would, which would otherwise
         * apply the theme's regular window border directly to its
         * own raw window: a border painted over video or other
         * fullscreen content, not just an unwanted frame around an
         * ordinary window. */
        border_width = 0u;
    } else if (client_is_decorated(client) && client->frame != 0) {
        border_width = 0u;
    } else {
        border_width = client_border_width(client, is_focused, false);
    }

    /* Only actually send the request when the value would change:
     * an unconditional 'ConfigureWindow' here on every render pass
     * for every client (needed so an icon-cycle selection border
     * appears/disappears promptly) was extra server round-trip
     * traffic for the common case where nothing about this
     * particular client changed at all (e.g., a keyboard resize of
     * one window previously still re-sent border width for every
     * other window on the desktop each time). */
    if (border_width != client->last_border_width) {
        xcb_window_set_border(target, border_width);
        client->last_border_width = border_width;
    }

    /* An undecorated client shows its focus through this border and
     * nothing else, a decorated one through the frame repainted just
     * above, so both now follow from 'is_focused' in the same pass.
     * Leaving the color to whoever changes the focus instead means
     * a path that forgets leaves a window still wearing the active
     * border after another has taken the focus
     * from it, where a decorated window would have corrected itself
     * on the next pass.  Skipped when unchanged, as the width is. */
    if (!client_is_decorated(client) || client->frame == 0) {
        client_border_color_apply(client, is_focused);
    }

    /* Map the window to make it visible.
     * Only do this when 'desktop' is the surface's currently
     * displayed desktop.
     *
     * Its also invoked as part of a general
     * 'surface_render_all_desktops' refresh pass whenever ANY
     * desktop's 'is_outdated' flag is set (e.g., after moving or
     * resizing a client, which marks its desktop outdated).
     *
     * If that pass unconditionally mapped clients on a desktop that
     * is not currently shown, it could race with (and undo) an
     * explicit 'surface_clients_hide' issued by a desktop switch,
     * making a client reappear on top of the desktop the user just
     * switched to.
     *
     * Visibility of non-current desktops must be governed solely by
     * 'surface_clients_hide'/'surface_clients_show' */
    if (is_current) {
        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_window_hide(client->icon_window);
            client->is_icon_mapped = false;
        }
        if (titlebar_visible) {
            xcb_window_show(client->titlebar);
        } else if (client->titlebar != 0) {
            xcb_window_hide(client->titlebar);
        }
        xcb_window_show(target);

        /* Do not re-map the content window for shaded clients.  The
         * shade operation explicitly unmaps it, and mapping it here
         * would undo the shade and prevent the titlebar-only view
         * from being painted correctly, especially for inactive
         * windows that receive no 'FocusOut'-triggered repaint */
        if (target != client->window && !client_is_shaded(client)) {
            xcb_window_show(client->window);
        }
    }

    ctx.desktop = desktop;
    ctx.client = client;
    ctx.target = target;
    ctx.is_focused = is_focused;
    ctx.hide_decoration = hide_decoration;
    ctx.titlebar_visible = titlebar_visible;

    if (client->is_outdated) {
        s_render_apply_geometry(&ctx);
    } else if (target != client->window &&
            (desktop->is_focus_dirty || client_is_urgent(client))) {
        s_render_refresh_decoration(&ctx);
    }

    LOGGER_TRACE("Rendered client 0x%08x with" \
                 " geometry (%ux%u%+u%+u)",
            client->id,
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y);
}


/**
 * @brief Repaint a titlebar's background, text and buttons
 *
 * @param connection XCB connection
 * @param client     Client whose titlebar is repainted
 * @param is_focused Whether the client currently holds focus
 * @param inner_w    Width available inside the frame
 * @param title_h    Height of the titlebar itself
 * @param theme      Theme the colors and fonts come from
 *
 * @note Complexity: @e O(n), where @e n is the number of buttons
 */
void desktop_repaint_titlebar_content(xcb_connection_t *connection,
        client_td *client, bool is_focused, uint16_t inner_w,
        uint16_t title_h, const struct config_theme_s *theme)
{
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;
    int16_t text_y;
    bool can_maximize;
    bool hide_pin;
    surface_td *surface;

    if (connection == NULL || client == NULL || theme == NULL ||
            client->titlebar == 0) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    hide_pin = surface != NULL && surface->desktop_count <= 1u;

    xcb_change_window_attributes(connection,
            client->titlebar, XCB_CW_BACK_PIXEL,
            (const uint32_t[]) {
                (is_focused)
                    ? theme->window.active.color.background
                    : theme->window.inactive.color.background
            });
    xcb_clear_area(connection, 0, client->titlebar, 0, 0, 0, 0);

    (void) text_renderer_use_font(connection,
            (is_focused)
                ? theme->window.active.font
                : theme->window.inactive.font);
    text_renderer_set_color(
            (is_focused)
                ? theme->window.active.color.foreground
                : theme->window.inactive.color.foreground,
            (is_focused)
                ? theme->window.active.color.background
                : theme->window.inactive.color.background);

    client_titlebar_layout(theme, inner_w, title_h, hide_pin, left,
            &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    /* Vertically centered against the titlebar's font ascent and
     * descent, the same way 'client_titlebar_layout' above already
     * centers 'btn_y' against the button size, rather than a fixed
     * pixel offset from the bottom: a fixed offset only happens to
     * look centered for whichever font it was tuned against, and
     * drifts visibly off-center for any other (a restricted-memory
     * session's plain X core font included, since that swap
     * changes the font's ascent/descent without this titlebar's
     * own height changing to match). */
    text_y = (int16_t) (((int16_t) title_h -
                (int16_t) (text_font_ascent() + text_font_descent())) / 2 +
            text_font_ascent());
    s_titlebar_draw_title(connection, client, client->titlebar,
            title_x, title_w, text_y, client->info.name,
            theme->window.titlebar.alignment);

    can_maximize = !client_is_fullscreen(client) &&
        (bool) client_is_maximizable(client);
    s_desktop_titlebar_buttons_draw(connection, client->titlebar,
            btn_y, left, left_n, right, right_n, is_focused,
            (bool) client_is_pinned(client),
            (client->properties.layer != CLIENT_LAYER_NORMAL),
            can_maximize, theme);
}


/**
 * @brief Repaint the frame background, border and corner resize
 *        grips
 *
 * @param connection       XCB connection
 * @param client           Client whose frame is repainted
 * @param use_active_style Whether the active colors apply
 * @param theme            Theme the colors come from
 *
 * @note Complexity: @e O(1)
 */
void desktop_repaint_frame_decoration(xcb_connection_t *connection,
        const client_td *client, bool use_active_style,
        const struct config_theme_s *theme)
{
    uint8_t opacity_percent;

    if (connection == NULL || client == NULL || client->frame == 0 ||
            theme == NULL || !client_is_decorated(client)) {
        return;
    }

    xcb_change_window_attributes(connection, client->frame,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
                (use_active_style)
                    ? theme->window.active.border.color
                    : theme->window.inactive.border.color,
                (use_active_style)
                    ? theme->window.active.border.color
                    : theme->window.inactive.border.color
            });

    if (use_active_style) {
        opacity_percent = (client->opacity_override.is_set_active)
            ? client->opacity_override.active
            : theme->window.active.opacity;
    } else {
        opacity_percent = (client->opacity_override.is_set_inactive)
            ? client->opacity_override.inactive
            : theme->window.inactive.opacity;
    }
    atom_set_window_opacity(connection, client->frame,
            config_theme_opacity_to_raw(opacity_percent));
    xcb_clear_area(connection, 0, client->frame, 0, 0, 0, 0);
}


/* Invalidate the cached root window background pixmap on every
 * screen: cheap and always correct, even though only one screen's
 * own property actually changed, since which one that was is not
 * known at this call site (handler/focus.c, a generic PropertyNotify
 * handler not otherwise concerned with which screen a client's
 * root belongs to) and this only ever runs on the comparatively rare
 * event of an external wallpaper tool actually changing something,
 * not on every render pass. */
void desktop_background_pixmap_cache_invalidate(void)
{
    for (size_t i = 0; i < (size_t) CONFIG_MAX_SCREENS; ++i) {
        s_bg_pixmap_resolved[i] = false;
        s_bg_pixmap_cache[i] = XCB_NONE;
    }
}


/* Recognize whether an atom is one of the background pixmap properties
 * this module watches */
bool desktop_property_is_background_pixmap(xcb_connection_t *connection,
        xcb_atom_t atom)
{
    if (atom == XCB_ATOM_NONE) {
        return false;
    }

    s_resolve_bg_atoms(connection);

    for (size_t i = 0; i < 3u; ++i) {
        if (s_bg_atoms[i] != XCB_ATOM_NONE && s_bg_atoms[i] == atom) {
            return true;
        }
    }

    return false;
}


/* Draw the background of a desktop */
int desktop_render_background(desktop_td *desktop)
{
    xcb_screen_t *screen;
    uint32_t values[2];
    xcb_pixmap_t root_pixmap;

    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    if (desktop->screen_id >= (uint32_t) CONFIG_MAX_SCREENS) {
        LOGGER_ERROR("Desktop %u ('%s') has an out-of-range screen_id" \
                " %u; cannot render its background",
                desktop->id, desktop->name, desktop->screen_id);
        return 1;
    }

    LOGGER_TRACE("Rendering background for desktop %u ('%s')" \
            " with color #%06x",
            desktop->id, desktop->name, desktop->background.bg.color);

    /* O(1): reuses the pointer 'desktop_init' (desktop.c) already
     * resolved once for this desktop, rather than re-walking every
     * screen from scratch (O(n)) on every single repaint */
    screen = desktop->screen;
    if (screen == NULL) {
        LOGGER_ERROR("Could not get screen for background rendering",
                L_NARG);
        return 1;
    }

    root_pixmap = s_get_root_background_pixmap(xcb_connection_get(),
            screen->root, desktop->screen_id);
    if (root_pixmap != XCB_NONE) {
        /* An external tool ('xsetbg', 'xsetroot', 'nitrogen', 'feh',
         * &c.) painted the root window and recorded the pixmap ID in
         * a well-known atom.  Record that fact so that subsequent
         * repaints do not overwrite the wallpaper with our color.
         */
        /* NOTE: Deliberately do NOT set 'XCB_CW_BACK_PIXMAP' on the
         *       root window to this pixmap.  Many setters free the
         *       pixmap after drawing (the pixels persist in the root
         *       drawable), so referencing it via 'XCB_CW_BACK_PIXMAP'
         *       would cause X to use a freed resource on the next
         *       'xcb_clear_area', and leading to a 'BadPixmap' or
         *       'BadDrawable' X error and an abrupt crash. */
        desktop->background.use_root_pixmap = true;
        /* So that if the WM ever owns the background again later (the
         * external pixmap atom disappears), the solid-color path below
         * always re-applies at least once even if that color happens to
         * equal whatever it last applied before this external pixmap
         * appeared.  Otherwise this screen's shared root window would
         * be left showing the stale external wallpaper under the
         * mistaken belief that the WM's color was already correctly
         * in place. */
        s_root_bg_applied_once[desktop->screen_id] = false;
        LOGGER_TRACE("External root pixmap 0x%x detected for" \
                " desktop %u ('%s'); skipping color fill",
                root_pixmap, desktop->id, desktop->name);
        return 0;
    }

    if (desktop->background.use_root_pixmap) {
        /* No pixmap atom found this time, but an external tool
         * previously painted the root window.  The pixels are still
         * there; leave the root window untouched so the wallpaper
         * remains visible. */
        LOGGER_TRACE("Preserving previous external background for" \
                " desktop %u ('%s')", desktop->id, desktop->name);
        return 0;
    }

    /* No external background detected and the window manager owns the
     * background: apply the configured color and clear the root window
     * to make it visible.  Only actually do so when the color changed
     * since the last time this ran, or on the very first pass.  This
     * function runs on every 'is_current' full-desktop render (every
     * client gaining focus marks its desktop 'is_outdated', not
     * just an actual background change), so without this check every
     * such render would repeat the same full-screen
     * 'xcb_change_window_attributes' + 'xcb_clear_area' for a color
     * that never actually changed.  Checked and updated per screen
     * (see 's_root_bg_applied_once' above), not per desktop.  Every
     * desktop sharing this screen's one root window can otherwise
     * repaint over whichever color another one on the same screen
     * applied, without either ever detecting that the color actually
     * showing has changed since its last render. */
    if (s_root_bg_applied_once[desktop->screen_id] &&
            s_root_bg_color_applied[desktop->screen_id] ==
                desktop->background.bg.color) {
        return 0;
    }

    values[0] = XCB_BACK_PIXMAP_NONE;
    values[1] = desktop->background.bg.color;
    xcb_change_window_attributes(xcb_connection_get(), screen->root,
            XCB_CW_BACK_PIXMAP | XCB_CW_BACK_PIXEL, values);
    xcb_clear_area(xcb_connection_get(), 0, screen->root, 0, 0,
            screen->width_in_pixels, screen->height_in_pixels);
    s_root_bg_applied_once[desktop->screen_id] = true;
    s_root_bg_color_applied[desktop->screen_id] =
        desktop->background.bg.color;

    LOGGER_TRACE("Background rendered for desktop %u ('%s')",
            desktop->id, desktop->name);

    return 0;
}


/* Full desktop render */
int desktop_render_full(desktop_td *desktop, bool is_current)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_TRACE("Fully rendering desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Draw background, but only for the desktop currently shown on
     * screen: a non-current desktop's background is never
     * actually visible (the surface-level repaint that calls this,
     * in render/surface.c, re-applies the current desktop's
     * background again right after every desktop in the list has
     * been rendered, specifically because earlier ones painting
     * theirs would otherwise overwrite it on the one shared root
     * window), so painting it here for a desktop nobody can see would
     * just be work immediately thrown away. */
    if (is_current && desktop_render_background(desktop) != 0) {
        LOGGER_ERROR("Failed to render background on" \
                 " desktop %u ('%s')", desktop->id, desktop->name);
        return 1;
    }

    /* Draw all clients */
    if (s_desktop_render_clients(desktop, is_current) != 0) {
        LOGGER_ERROR("Failed to render clients", L_NARG);
        return 1;
    }

    /* Mark desktop as up-to-date */
    wm_validate_desktop(desktop);

    /* Every client just had its chance, in the loop above, to compare
     * itself against 'is_focus_dirty' and refresh its decoration
     * colors if the active client changed since the last full render.
     * Clearing it here consumes that signal so the next pass (e.g., a
     * later resize of one otherwise-unrelated client, with focus
     * unchanged since) does not see it still set and re-trigger the
     * exact spurious 'xcb_clear_area + text-draw' repaint on every
     * other window this flag exists to avoid.  See its comment in
     * 'desktop.h' ("since last render pass") and the 'is_focus_dirty'
     * branch in 'desktop_render_one_client' above. */
    desktop->is_focus_dirty = false;

    /* NOTE: Do NOT flush here!  Let the surface handle the flushing */

    return 0;
}
