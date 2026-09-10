/**
 * @file cmds/client/state.c
 *
 * @brief Client state-transition commands: shading, fullscreen, and
 *        decoration toggling
 *
 * Contains operations that change a client's visual state in ways that
 * require XCB geometry manipulation beyond a simple flag update:
 * shade/unshade, fullscreen/unfullscreen, and decoration toggle
 * (including the private helper that builds the frame and titlebar
 * windows).  Focus operations live in @c cmds/client/focus.c;
 * visibility (iconify/hide/unhide) in @c cmds/client/visibility.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free */
#include <time.h>       /* CLOCK_MONOTONIC, clock_gettime */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/client.h>

/* Utils includes */
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Input includes */
#include <input/mouse/hover.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <systray.h>
#include <utils/geom.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/flags.h>
#include <cmds/client/focus.h>
#include <cmds/client/grab.h>
#include <cmds/client/layer.h>
#include <cmds/client/maximize.h>
#include <cmds/client/move.h>
#include <cmds/client/screen.h>
#include <cmds/client/state.h>
#include <cmds/client/visibility.h>
#include <cmds/client/workarea.h>


/**
 * @brief Create frame and titlebar windows for a previously undecorated
 *        client
 *
 * Reparents the client window into a newly created frame, creates the
 * titlebar child window, and updates the client layout fields so
 * rendering and geometry operations use the decorated extents.
 *
 * @param client Pointer to the client
 * @param bw     Border width to apply
 * @param th     Titlebar height to apply
 *
 * @note Complexity: @e O(1)
 */
static void s_client_enable_decoration(client_td *client,
        int32_t bw, int32_t th)
{
    uint32_t mask;
    uint32_t values[3];
    struct geometry_s frame;
    int32_t frame_w;
    int32_t frame_h;
    uint32_t border_color;
    uint32_t bg_color;
    static const xcb_button_t s_grab_buttons[] = {
        XCB_BUTTON_INDEX_1,
        XCB_BUTTON_INDEX_2,
        XCB_BUTTON_INDEX_3,
        6,
        7
    };
    size_t nb = sizeof(s_grab_buttons) / sizeof(s_grab_buttons[0]);

    if (client == NULL || client->parent_id == 0) {
        return;
    }

    border_color = (client->config != NULL)
        ? client->config->theme.window.inactive.border.color
        : 0x999999U;
    bg_color = (client->config != NULL)
        ? client->config->theme.window.inactive.color.background
        : 0x000000U;

    frame.pos.x = client->layout.geometry.cur.pos.x - bw;
    frame.pos.y = client->layout.geometry.cur.pos.y - (bw + th);
    frame_w = (int32_t) client->layout.geometry.cur.dim.w + 2 * bw;
    frame_h = (int32_t) client->layout.geometry.cur.dim.h + 2 * bw + th;

    if (frame_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        frame_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
    }
    if (frame_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        frame_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
    }
    frame.dim.w = (uint32_t) frame_w;
    frame.dim.h = (uint32_t) frame_h;

    ccmd_client_ungrab_buttons(client);

    client->frame = xcb_generate_id(xcb_connection_get());
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = border_color;
    values[1] = border_color;
    /* 'XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT' is required so the
     * client's future resize/move attempts on itself are delivered to
     * the window manager as 'ConfigureRequest's instead of being
     * applied directly by the server with no notification at all */
    values[2] = XCB_EVENT_MASK_EXPOSURE             |
                XCB_EVENT_MASK_BUTTON_PRESS         |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY     |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY  |
                XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_POINTER_MOTION;
    xcb_create_window(xcb_connection_get(),
            XCB_COPY_FROM_PARENT,
            client->frame,
            client->parent_id,
            (int16_t) frame.pos.x, (int16_t) frame.pos.y,
            (uint16_t) frame.dim.w, (uint16_t) frame.dim.h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* A brand-new window, painted with 'border_color' above (always
     * the inactive one, corrected to the active one if warranted by
     * the redraw pass this function's own caller ends with); these
     * two, though, still hold whatever this same client's previous
     * frame (just destroyed, if it had one) last had them set to.
     * Left alone, that stale, coincidentally-matching cache would
     * make 'render_client_decoration_repaint_frame'
     * (render/client/decoration.c) believe this new window already
     * shows the right color and skip painting it for real. */
    client->layout.has_frame_bg = false;
    client->layout.has_titlebar_bg = false;

    client->titlebar = xcb_generate_id(xcb_connection_get());
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = bg_color;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS;
    xcb_create_window(xcb_connection_get(),
            XCB_COPY_FROM_PARENT,
            client->titlebar,
            client->frame,
            (int16_t) bw, (int16_t) bw,
            (uint16_t) client->layout.geometry.cur.dim.w,
            (uint16_t) th,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* Reparenting to an unmapped frame makes the content window
     * non-viewable, which emits two synthetic 'UnmapNotify' events:
     *
     *  1. From root's 'SubstructureNotify' (event=root, window=content)
     *  2. From the content window's 'StructureNotify' (event=window,
     *     window=content)
     *
     * Absorb both so focus is not stolen from the active window. */
    client->ignore.unmap += 2u;
    client->ignore.focus_unmap++;

    xcb_window_reparent(client->window,
            client->frame,
            (int16_t) bw, (int16_t) (bw + th));

    /* Back under a frame of ours, so back in the save set too, or this
     * window manager dying would take the client down along with it
     * instead of handing it back to root (Scheifler and Gettys, 1994,
     * "Inter-Client Communication Conventions Manual", v2.0, §4.1.2). */
    xcb_window_save_set(client->window, true);

    ccmd_client_apply_geometry(client, client->window,
            (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
            0, 0, 0u, 0u, 0u);

    for (size_t bi = 0; bi < nb; ++bi) {
        xcb_grab_button(xcb_connection_get(),
                0,
                client->frame,
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE,
                XCB_GRAB_MODE_SYNC,
                XCB_GRAB_MODE_ASYNC,
                XCB_NONE,
                XCB_NONE,
                s_grab_buttons[bi],
                XCB_MOD_MASK_ANY);
    }

    xcb_window_show(client->frame);
    xcb_window_show(client->titlebar);
    xcb_window_show(client->window);

    client->layout.geometry.cur.pos.x = frame.pos.x;
    client->layout.geometry.cur.pos.y = frame.pos.y;
    client->layout.geometry.cur.dim.w = (uint16_t) frame_w;
    client->layout.geometry.cur.dim.h = (uint16_t) frame_h;
    client->layout.frame_extents.left = bw;
    client->layout.frame_extents.right = bw;
    client->layout.frame_extents.top = bw + th;
    client->layout.frame_extents.bottom = bw;
    client_decorate(client);

    ccmd_publish_frame_extents(client,
            (uint32_t) bw, (uint32_t) bw,
            (uint32_t) (bw + th), (uint32_t) bw);
}


/**
 * @brief Unshades a client window when it is currently shaded
 *
 * @param client Client to unshade if necessary
 *
 * @note Does nothing if @p client is @c NULL or the client is not
 *       shaded
 */
static void s_client_unshade_if_needed(client_td *client)
{
    if (client != NULL && client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }
}


/**
 * @brief Strip a client's decoration and reparent it to the root
 *
 * @param client Client losing its decoration
 * @param bw     Border width the client had while decorated
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_decorate_remove(client_td *client, int32_t bw)
{
    if (client->frame != 0) {
        struct geometry_s inner;
        int32_t inner_w =
            (int32_t) client->layout.geometry.cur.dim.w -
            client->layout.frame_extents.left -
            client->layout.frame_extents.right;
        int32_t inner_h =
            (int32_t) client->layout.geometry.cur.dim.h -
            client->layout.frame_extents.top -
            client->layout.frame_extents.bottom;

        inner.pos.x = client->layout.geometry.cur.pos.x +
            client->layout.frame_extents.left;
        inner.pos.y = client->layout.geometry.cur.pos.y +
            client->layout.frame_extents.top;

        if (inner_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            inner_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }
        if (inner_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            inner_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }

        /* Above, 'inner.pos' keeps the content's top-left corner fixed
         * on screen, correct outright for 'CLIENT_GRAVITY_NORTH_WEST'
         * (the ICCCM default) and 'CLIENT_GRAVITY_STATIC', for which
         * this call is a no-op; for any other gravity a client's
         * 'WM_NORMAL_HINTS' actually requested, this adds whatever
         * further displacement keeps that gravity's anchor fixed
         * instead, given the frame shrinking from its decorated outer
         * size down to this content's, now-undecorated one. */
        client_gravity_adjust_pos(&inner.pos.x, &inner.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h,
                (uint32_t) inner_w, (uint32_t) inner_h,
                client->layout.gravity);

        inner.dim.w = (uint32_t) inner_w;
        inner.dim.h = (uint32_t) inner_h;

        if (client->titlebar != 0) {
            xcb_window_destroy(client->titlebar);
            client->titlebar = 0;
        }

        /* Reparenting generates a synthetic 'UnmapNotify' for the
         * content window.  Absorb it so 'handler_unmap_notify' does not
         * mistake the event for a voluntary hide and does not steal
         * focus from the window. */
        client->ignore.unmap += 2u;
        client->ignore.focus_unmap++;
        xcb_window_reparent(client->window,
                client->parent_id,
                (int16_t) inner.pos.x, (int16_t) inner.pos.y);

        /* No longer a child of a frame of ours, so out of the save set
         * as well: nothing left here for it to be rescued from
         * (Scheifler and Gettys, 1994, "Inter-Client Communication
         * Conventions Manual", v2.0, §4.1.2). */
        xcb_window_save_set(client->window, false);

        ccmd_client_apply_geometry(client, client->window,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT |
                    (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
                inner.pos.x, inner.pos.y,
                inner.dim.w, inner.dim.h, (uint32_t) bw);

        xcb_window_destroy(client->frame);
        client->frame = 0;

        client->layout.geometry.cur.pos.x = inner.pos.x;
        client->layout.geometry.cur.pos.y = inner.pos.y;
        client->layout.geometry.cur.dim.w = (uint16_t) inner_w;
        client->layout.geometry.cur.dim.h = (uint16_t) inner_h;

        /* The window was just reparented out of a frame that no longer
         * exists, so the server is about to place it against the root
         * and report where it landed.  The position asked for above
         * describes the old arrangement and must stop standing as the
         * last word, or 'handler_configure_notify' would take those
         * reports for stale echoes of it and throw them away, leaving
         * what is stored here disagreeing with what is on screen. */
        client->layout.has_requested_pos = false;

        /* Keeps 'client_border_color_apply' (client.c) from seeing
         * a stale 'last_border_width' the moment focus is reapplied
         * a few lines below (via 'ccmd_client_focus'): without this,
         * that call would compare its freshly computed width against
         * whatever this field happened to hold from this same client's
         * last undecorated period (or 'UINT32_MAX' if there never was
         * one), and shift the position it just correctly set above by
         * whatever spurious delta that comparison produces, on every
         * single toggle. */
        client->last_border_width = (uint32_t) bw;
    } else {
        ccmd_client_apply_geometry(client, client->window,
                (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
                0, 0, 0u, 0u, (uint32_t) bw);
        client->last_border_width = (uint32_t) bw;
    }

    client->layout.frame_extents.left = 0;
    client->layout.frame_extents.right = 0;
    client->layout.frame_extents.top = 0;
    client->layout.frame_extents.bottom = 0;
    client_undecorate(client);
    ccmd_client_grab_buttons(client);

    ccmd_publish_frame_extents(client, 0u, 0u, 0u, 0u);
}


/**
 * @brief Give a client its decoration back, frame and titlebar
 *
 * @param client Client regaining its decoration
 * @param bw     Border width to apply once framed
 * @param th     Titlebar height the theme asks for
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_decorate_restore(client_td *client, int32_t bw,
        int32_t th)
{
    if (client->frame == 0) {
        s_client_enable_decoration(client, bw, th);
    } else {
        struct geometry_s frame;
        int32_t frame_w =
            (int32_t) client->layout.geometry.cur.dim.w + 2 * bw;
        int32_t frame_h =
            (int32_t) client->layout.geometry.cur.dim.h + 2 * bw + th;

        frame.pos.x = client->layout.geometry.cur.pos.x - bw;
        frame.pos.y = client->layout.geometry.cur.pos.y - (bw + th);

        if (frame_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            frame_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }

        if (frame_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            frame_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }

        /* Same reasoning as the remove-decoration branch above,
         * mirrored: the baseline 'frame.pos' keeps the content's own
         * top-left corner fixed, correct outright for
         * 'CLIENT_GRAVITY_NORTH_WEST'/'STATIC'; any other gravity gets
         * whatever further displacement keeps its anchor fixed instead,
         * now going from this content's own undecorated size up to the
         * restored frame's, larger one. */
        client_gravity_adjust_pos(&frame.pos.x, &frame.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h,
                (uint32_t) frame_w, (uint32_t) frame_h,
                client->layout.gravity);

        frame.dim.w = (uint32_t) frame_w;
        frame.dim.h = (uint32_t) frame_h;

        ccmd_client_apply_geometry(client, client->frame,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                frame.pos.x, frame.pos.y,
                frame.dim.w, frame.dim.h, 0u);

        ccmd_client_apply_geometry(client, client->window,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT |
                    (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
                bw, bw + th,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h, 0u);

        if (client->titlebar != 0) {
            ccmd_client_apply_geometry(client, client->titlebar,
                    (uint16_t) XCB_CONFIG_WINDOW_X |
                        (uint16_t) XCB_CONFIG_WINDOW_Y |
                        (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                        (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                    bw, bw,
                    client->layout.geometry.cur.dim.w,
                    (uint32_t) th, 0u);
            xcb_window_show(client->titlebar);
        }
        xcb_window_show(client->frame);
        xcb_window_show(client->window);

        client->layout.geometry.cur.pos.x = frame.pos.x;
        client->layout.geometry.cur.pos.y = frame.pos.y;
        client->layout.geometry.cur.dim.w = (uint16_t) frame_w;
        client->layout.geometry.cur.dim.h = (uint16_t) frame_h;

        /* Same reason as when the decoration came off: the window has
         * just been reparented into a frame built here, and what the
         * server reports next describes that new arrangement rather
         * than any request made before it */
        client->layout.has_requested_pos = false;
        client->layout.frame_extents.left = bw;
        client->layout.frame_extents.right = bw;
        client->layout.frame_extents.top = bw + th;
        client->layout.frame_extents.bottom = bw;
        client_decorate(client);

        ccmd_publish_frame_extents(client,
                (uint32_t) bw, (uint32_t) bw,
                (uint32_t) (bw + th), (uint32_t) bw);
    }
}


/**
 * @brief Recompute a maximized client's geometry after the toggle
 *
 * The workarea a maximized client fills does not change, but the frame
 * extents it is measured against just did, so the geometry has to be
 * worked out again rather than kept.
 *
 * @param client Client to recompute
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_decorate_remaximize(client_td *client)
{
    /* A maximized client's geometry, computed just above, only ever
     * grows or shrinks its existing frame in place around whatever
     * position/size that already was (exactly right for an ordinary
     * client, but not for one that was filling the workarea a moment
     * ago).  Decoration changes how much of that area its frame extents
     * eat into, so what it should still fill afterward is the workarea
     * itself, not "whatever it already had, offset by however much
     * bigger or smaller its frame extents just became".
     *
     * Recomputed here instead, against 'ccmd_client_resolve_workarea'
     * (the same resolution 'ccmd_client_maximize' itself already uses),
     * so the client ends up exactly refilling the workarea under its
     * new decorated state, the same as if it had only just been
     * maximized now.
     *
     * Only the axis (or axes) 'client->properties.state' itself
     * actually names gets touched: a client maximized on one axis alone
     * leaves its other axis exactly as the base decorate/ undecorate
     * logic above already placed it, rather than growing it to fill the
     * workarea too and silently turning a horizontal-only or
     * vertical-only maximize into a full one. */
    if (client_is_maximized_any(client)) {
        int32_t mx = 0;
        int32_t my = 0;
        uint16_t sw;
        uint16_t sh;

        if (ccmd_client_resolve_workarea(client, &mx, &my, &sw, &sh)) {
            xcb_window_t target = ccmd_target_win(client);
            bool touch_x = client_is_maximized_horz(client);
            bool touch_y = client_is_maximized_vert(client);
            /* 'ccmd_client_toggle_decorate' always ends by focusing
             * 'client' once this function returns, so its border width
             * right after this toggle is always the active one,
             * regardless of whichever one it had a moment ago. */
            /* 'ignore_frame=false': by this point the decorate/
             * undecorate branch above has already settled, so
             * 'client->frame' now correctly reflects whether one
             * exists; for a now-decorated client this correctly stays
             * zero, since the frame's size (not an additional border
             * atop it) already fills the workarea, matching 'target'
             * being the frame itself just below. */
            uint32_t border = 2u * client_border_width(client, true,
                    false);
            uint16_t mask = 0u;

            sw = (uint16_t) ((sw > border) ? sw - border : 0u);
            sh = (uint16_t) ((sh > border) ? sh - border : 0u);

            if (touch_x) {
                mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH;
                client->layout.geometry.cur.pos.x = mx;
                client->layout.geometry.cur.dim.w = sw;
            }
            if (touch_y) {
                mask |= XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT;
                client->layout.geometry.cur.pos.y = my;
                client->layout.geometry.cur.dim.h = sh;
            }

            if (mask != 0u) {
                ccmd_client_apply_geometry(client, target, mask,
                        client->layout.geometry.cur.pos.x,
                        client->layout.geometry.cur.pos.y,
                        client->layout.geometry.cur.dim.w,
                        client->layout.geometry.cur.dim.h, 0u);
            }

            if (client->frame != 0) {
                client_decoration_layout_sync(client);
            }
        } /* ! if (ccmd_client_resolve_workarea) */
    } /* ! if (client_is_maximized_any) */
}


/* Shade client (roll-up), if decorated */
void ccmd_client_shade(client_td *client)
{
    xcb_window_t target;
    uint32_t shaded_h;
    xcb_get_geometry_cookie_t geom_ck;
    xcb_get_geometry_reply_t *geom_r;

    if (client == NULL || !client_is_decorated(client) ||
            client_is_shaded(client) || client_is_fullscreen(client)) {
        return;
    }

    /* An iconified client's target window is unmapped and its icon
     * window stands in for it; shading it in place here, same as
     * 'ccmd_client_fullscreen' and 'ccmd_client_maximize' (via
     * 's_ccmd_maximize_precheck') already do for the same reason, would
     * map the frame back while the icon window is still up, showing
     * both at once.  Restoring first keeps this consistent with the
     * project's established convention of resolving a conflicting prior
     * state automatically rather than refusing the request outright
     * (the same convention 'ccmd_client_iconify' itself follows for
     * shade and fullscreen on the way in). */
    if (client_is_iconified(client)) {
        ccmd_client_restore(client);
    }

    LOGGER_TRACE("Shading client window=0x%x", client->window);

    target = ccmd_target_win(client);

    /* Refresh 'geometry.cur' from the real X11 state right before
     * saving it.  An application-driven resize the window manager did
     * not initiate could leave 'geometry.cur' stale, and shading would
     * then save, and unshading would later restore, the wrong
     * height. */
    geom_ck = xcb_get_geometry(xcb_connection_get(), target);
    geom_r = xcb_get_geometry_reply(xcb_connection_get(), geom_ck, NULL);
    if (geom_r != NULL) {
        client->layout.geometry.cur.dim.w = geom_r->width;
        client->layout.geometry.cur.dim.h = geom_r->height;
        free(geom_r);
    }

    /* Same guard 'ccmd_client_maximize'/'_horz'/'_vert' ('geom.c') and
     * 'ccmd_client_iconify' already apply.  Skip saving when the client
     * is already maximized (in any of its three variants), so shading
     * a maximized client and then unshading it later restores the
     * maximized size, not the pre-maximize one that
     * 'client->layout.geometry.old' already holds from whenever it was
     * maximized.  An unconditional save here would silently overwrite
     * that with the current (maximized) geometry, losing the true
     * original size no later 'unmaximize' could ever recover, since
     * nothing else remembers it. */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    shaded_h = (uint32_t) (client->layout.frame_extents.top +
                           client->layout.frame_extents.bottom);
    if (shaded_h < WM_MIN_WINDOW_DIMENSION) {
        shaded_h = WM_MIN_WINDOW_DIMENSION;
    }

    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
            0, 0, 0u, shaded_h, 0u);
    xcb_window_show(target);

    /* Absorb both 'UnmapNotify' events generated by the content window
     * unmap: the frame's 'SubstructureNotify' ('event=frame,
     * window=content') and the content window's 'StructureNotify'
     * ('event=content, window=content').  Both carry 'event->window ==
     * client->window', so both reach the fall-through branch of
     * 'handler_unmap_notify'; only one 'ignore_unmap' token would leave
     * a second event unguarded, causing 'handler_unmap_notify' to
     * wrongly unmap the frame and titlebar and make the shaded titlebar
     * disappear. */
    client->ignore.unmap += 2u;
    xcb_window_hide(client->window);

    client->layout.geometry.cur.dim.h = (uint16_t) shaded_h;
    client_shade(client);
    client_decoration_layout_sync(client);
    (void) clock_gettime(CLOCK_MONOTONIC,
            &client->shade_transition_time);

    ccmd_client_sync_states(client);

    wm_request_client_redraw(client);
}


/* Unshade client (roll-down), if decorated */
void ccmd_client_unshade(client_td *client)
{
    xcb_window_t target;
    uint32_t restored_h;
    const desktop_td *own_desktop;

    if (client == NULL || !client_is_decorated(client) ||
            !client_is_shaded(client)) {
        return;
    }

    LOGGER_TRACE("Unshading client window=0x%x", client->window);

    target = ccmd_target_win(client);

    /* 'geometry.old.dim.h' is the pre-maximize height whenever the
     * client is currently maximized on the vertical axis (full or
     * vertical-only): 'ccmd_client_shade' deliberately skips saving
     * over it for a maximized client (see that function's comment),
     * specifically so a later, genuine 'unmaximize' still has the true
     * original height to restore, not whatever height happened to be
     * current at shade time.  That same skip means it is the wrong
     * source here too: restoring it would shrink the window back to its
     * pre-maximize size instead of the maximized one, cutting it off
     * partway down the workarea rather than filling it.  Re-resolving
     * the current workarea/screen height fresh, the exact same way
     * 's_ccmd_client_maximize_dir' ('maximize.c') computes it, is what
     * the vertical axis is actually supposed to be at right now
     * instead. */
    if (client_is_maximized(client) ||
            client_is_maximized_vert(client)) {
        uint16_t sw = 0;
        uint16_t sh = 0;
        const desktop_td *maximize_desktop;
        bool is_active;
        uint32_t border;

        if (!ccmd_client_resolve_workarea(client, NULL, NULL,
                    &sw, &sh) && !ccmd_screen_dim(client, &sw, &sh)) {
            sh = (uint16_t) client->layout.geometry.old.dim.h;
        }
        (void) sw;
        maximize_desktop = wm_get_client_desktop(client);
        is_active = maximize_desktop != NULL &&
            maximize_desktop->client_active_id == client->id;
        border = 2u * client_border_width(client, is_active, false);
        sh = (uint16_t) ((sh > border) ? sh - border : 0u);
        restored_h = sh;
    } else {
        /* Restore only the height from the saved geometry; keep the
         * current position so that moving the shaded window is
         * honored */
        restored_h = client->layout.geometry.old.dim.h;
    }
    client->layout.geometry.cur.dim.h = restored_h;

    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
            0, 0, 0u, restored_h, 0u);
    xcb_window_show(client->window);

    client_unshade(client);
    client_unhide(client);

    /* The content window's real on-screen size was never actually
     * touched while shaded (see 'client_decoration_layout_sync',
     * 'client/geom.c', which now deliberately skips resizing it for
     * a shaded client, matching Openbox's 'frame_adjust_area',
     * 'frame.c'): it already holds its true, correct size, restored
     * with nothing further needed for the common case.  Called here
     * anyway, now that 'client_unshade' just above has genuinely
     * cleared the shaded state, purely as a safety net for the edge
     * case of 'frame_extents' having changed while this client sat
     * shaded (a theme reload changing the border/titlebar size, most
     * plausibly): without this, the content would stay positioned and
     * sized for the frame extents that were in effect before that
     * change, out of sync with the ones actually in force now that the
     * frame is visible again. */
    client_decoration_layout_sync(client);

    (void) clock_gettime(CLOCK_MONOTONIC, &client->shade_transition_time);

    ccmd_client_sync_states(client);

    /* Real X input focus stayed on the frame while shaded (see
     * 'ccmd_client_focus''s comment, 'cmds/client/focus.c', for why):
     * the content window just remapped above is a genuinely different,
     * now-focusable window, and nothing else here moves focus onto it.
     * A client relying on 'WM_HINTS' input=true plus 'WM_TAKE_FOCUS' to
     * know it should redraw itself as focused (e.g., GVim/GTK) can stay
     * stuck showing only its shaded sliver until something else happens
     * to refocus it, since it never receives the real focus hand-off
     * this restores.  Only for the client already active on its
     * desktop, matching the same reasoning 'ccmd_client_fullscreen' and
     * 'ccmd_client_restore' already apply after their equivalent remap:
     * unshading a client that was not the active one should not steal
     * focus from whichever client actually still holds it. */
    own_desktop = wm_get_client_desktop(client);
    if (own_desktop != NULL &&
            own_desktop->client_active_id == client->id) {
        ccmd_client_focus(client);
    }

    wm_request_client_redraw(client);
}


/* Toggle shading */
void ccmd_client_toggle_shade(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    } else {
        ccmd_client_shade(client);
    }
}


/* Set full screen mode */
void ccmd_client_fullscreen(client_td *client)
{
    int32_t mx = 0;
    int32_t my = 0;
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;
    bool was_decorated;
    desktop_td *desktop;
    monitor_td monitor;

    if (client == NULL) {
        return;
    }

    /* Restore first if iconified, the same reasoning as
     * 'ccmd_client_shade''s identical guard just above.  An iconified
     * client's target window is unmapped, and entering fullscreen here
     * would map it back while the icon window is still up. */
    if (client_is_iconified(client)) {
        ccmd_client_restore(client);
    }

    /* Deliberately no 'client_is_resizable' gate here, unlike maximize.
     * Fullscreen is a WM-forced override of the client's preferred
     * geometry, not a user-convenience resize the client's fixed size
     * hints have any say over.  A DOS-emulation or retro-game window
     * that fixes its size (min == max in WM_NORMAL_HINTS, clearing
     * CLIENT_FLAG_RESIZABLE; see client/props.c) still needs to enter
     * fullscreen correctly when it requests '_NET_WM_STATE_FULLSCREEN'
     * on its own alt+enter handling, which a stricter check here would
     * silently swallow. */

    /* A modal is refused all the same, for a reason of its own rather
     * than anything to do with size hints: it exists to be answered
     * while the window it belongs to stays in view, and filling the
     * screen puts that window out of sight with no way back to it that
     * does not go through the modal first */
    if (client_is_modal(client)) {
        return;
    }

    LOGGER_TRACE("Entering fullscreen for client window=0x%x",
            client->window);

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    /* Fullscreen deliberately targets the raw monitor rect, not the
     * workarea 'ccmd_client_resolve_workarea' would give.  It is meant
     * to cover panels and docks too, not stop at their struts the way
     * maximize does. */
    if (ccmd_client_monitor(client, NULL, &monitor)) {
        mx = monitor.x;
        my = monitor.y;
        sw = geom_dim_clamp((int32_t) monitor.w);
        sh = geom_dim_clamp((int32_t) monitor.h);
    } else if (!ccmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    /* Same guard 'ccmd_client_maximize'/'_horz'/'_vert' (geom.c) and
     * 'ccmd_client_iconify' already apply, for the exact same reason:
     * skip saving when the client is already maximized, so entering
     * fullscreen and then leaving it later restores the maximized size,
     * not the pre-maximize one 'client->layout.geometry.old' already
     * holds; an unconditional save here would overwrite it with the
     * current (maximized) geometry instead, permanently losing the true
     * original size a later 'unmaximize' needs.  The equivalent case
     * for shade is already handled above, via the 'ccmd_client_unshade'
     * call this function already makes before ever reaching here. */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    was_decorated = client_is_decorated(client);
    client->was_decorated_fullscreen = was_decorated;
    target = ccmd_target_win(client);

    /* Resized to fill the screen FIRST, before the content window below
     * (when there is a separate one, i.e., 'target' is the frame).  The
     * reverse order leaves a real, if brief, window between the two
     * separate 'ConfigureWindow' requests where the content window
     * already had its fullscreen size while its parent frame still had
     * its old, smaller one, which X11 clips a child window to
     * regardless of what size the child itself was just given.
     * A fast-redrawing client (e.g., 'xterm') never showed it,
     * redrawing its content well before a human could perceive the gap.
     *
     * A client buffering its rendering (e.g., a GL/Vulkan video player
     * like 'mpv', already special-cased below for exactly this kind of
     * timing sensitivity) could catch that intermediate geometry and
     * paint a frame reflecting it, leaving the frame's background (set
     * to the theme's border color by
     * 'render_client_decoration_repaint_frame',
     * 'render/client/decoration.c') visible through the gap along the
     * content's top and left edges until its next redraw happened to
     * catch up (visually indistinguishable from a real border, though
     * neither an X11 border nor that repaint function was ever actually
     * involved).  Configuring the parent first removes the gap
     * outright: the child is never given a size its parent does not
     * already accommodate, however briefly. */
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y |
                (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                (uint16_t) XCB_CONFIG_WINDOW_HEIGHT |
                (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
            mx, my, sw, sh, 0u);

    if (was_decorated && client->frame != 0) {
        if (client->titlebar != 0) {
            xcb_window_hide(client->titlebar);
        }
        ccmd_client_apply_geometry(client, client->window,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT |
                    (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
                mx, my, sw, sh, 0u);
        client->layout.frame_extents.left = 0;
        client->layout.frame_extents.right = 0;
        client->layout.frame_extents.top = 0;
        client->layout.frame_extents.bottom = 0;
    }

    client->layout.geometry.cur.pos.x = mx;
    client->layout.geometry.cur.pos.y = my;
    client->layout.geometry.cur.dim.w = (uint32_t) sw;
    client->layout.geometry.cur.dim.h = (uint32_t) sh;

    /* ICCCM §4.2.3: applications that render via GL/Vulkan (e.g.,
     * 'mplayer', 'mpv') generally wait for a 'ConfigureNotify' before
     * resizing their rendering surface/viewport, and it must carry the
     * true screen-relative geometry.
     *
     * The real 'ConfigureNotify' the X server sends for the
     * frame/window configure above already carries that geometry
     * (including the monitor's origin, not necessarily (0,0), on
     * a surface made of more than one monitor), so this was not
     * strictly required for the client's OWN 'ConfigureNotify'; but the
     * frame reparenting above still delivers one relative to the
     * *frame*, and without an explicit synthetic one afterward some
     * clients only apply the next size they are told about relative to
     * their last known good state, which can otherwise show as a blank
     * frame until an unrelated event forces a fresh redraw. */
    client_send_synthetic_configure_notify(xcb_connection_get(), client);

    client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    (void) clock_gettime(CLOCK_MONOTONIC,
            &client->fullscreen_transition_time);

    ccmd_publish_frame_extents(client, 0u, 0u, 0u, 0u);

    /* Retain focus: keep this client active on its desktop and give it
     * input focus so the window is not lost from the active window
     * tracking when going fullscreen. */
    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        desktop->client_active_id = client->id;
        desktop->is_focus_dirty = true;
        (void) desktop_action_client_send_front(desktop, client);
        desktop->is_outdated = true;
    }
    ccmd_client_focus(client);

    ccmd_client_sync_states(client);

    /* Now genuinely fullscreen and focused both.  Stack it above every
     * other client on this desktop, including every other ABOVE-layer
     * one, right away rather than leaving it to whatever future
     * stacking-order pass happens to run next; see
     * 'ccmd_desktop_enforce_layers''s comment on this. */
    if (desktop != NULL) {
        ccmd_desktop_enforce_layers(desktop);
    }

    /* Let the systray reconsider its stacking now that a client just
     * became fullscreen: with 'systray.layer' set to "above" it should
     * drop below this window, the same way a taskbar or panel yields to
     * a fullscreen application everywhere else. */
    systray_restack();

    wm_request_client_redraw(client);
}


/* Remove full screen mode */
void ccmd_client_unfullscreen(client_td *client)
{
    xcb_window_t target;
    uint32_t border_width;
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Exiting fullscreen for client window=0x%x",
            client->window);

    target = ccmd_target_win(client);
    client_geometry_restore(client);

    /* Folds any still-maximized axis into 'layout.geometry.cur' right
     * now, before anything below ever applies it for real: without
     * this, the block below would configure the client to the restored
     * (pre-maximize) geometry the line above just set, tell it so with
     * a synthetic 'ConfigureNotify', and then, further down, correct
     * that with a second real configure and a second synthetic notify
     * for whichever axis is still maximized, once
     * 'ccmd_client_refill_maximized' used to run.  Two contradictory
     * geometries and two notifications for what is a single logical
     * change is wasted work at best, and at worst a visible flicker to
     * the restored, un-maximized size for one frame before snapping to
     * the correct one, which every client below configures against and
     * gets told about exactly once now instead. */
    (void) ccmd_client_refill_maximized_geometry(client);

    /* 'ignore_frame=true': 'client->frame' is already non-zero here (it
     * persists across the whole fullscreen cycle, never destroyed or
     * recreated), but 'layout.frame_extents' does not yet reflect that:
     * it was zeroed out by 'ccmd_client_fullscreen' on entry and is
     * exactly what the block below is about to (re)establish.  Without
     * this, 'client_border_width''s 'frame != 0' guard (correct for
     * every other caller, where the frame already does account for it)
     * returns 0 here unconditionally, collapsing 'inner_w'/'inner_h'
     * below to the frame's full size and 'frame_extents.left'/'.right'
     * to 0 right along with it.  The border theme color never
     * disappears, there is simply no frame pixel width left for it to
     * occupy, the client's content drawn flush against the frame's
     * outer edge instead. */
    border_width = client_border_width(client, true, true);

    /* Configured BEFORE the frame/target itself shrinks further down,
     * for the same reason 'ccmd_client_fullscreen' now configures its
     * own outer target before the inner content window.  The reverse
     * order leaves a real, if brief, window between the two separate
     * 'ConfigureWindow' requests where the frame already has its
     * smaller, restored size while the content window still had its
     * old, larger fullscreen one, which X11 clips a child window to
     * regardless of what size the child itself still claims.  A smaller
     * child always fits within a still-larger parent with no clipping
     * at all, so configuring the child first here removes that gap
     * outright, the same as configuring the parent first does for the
     * opposite (shrinking a parent that would otherwise still be
     * smaller than an as-yet-unshrunk child) case there. */
    if (client->was_decorated_fullscreen && client->frame != 0) {
        uint16_t title_height;
        uint16_t inner_w;
        uint16_t inner_h;

        title_height = (uint16_t) client->title_height;
        inner_w = (client->layout.geometry.cur.dim.w >
                (uint16_t) (border_width * 2u))
            ? (uint16_t) (client->layout.geometry.cur.dim.w -
                    (uint16_t) (border_width * 2u))
            : (uint16_t) WM_MIN_WINDOW_DIMENSION;
        inner_h = (client->layout.geometry.cur.dim.h >
                (uint16_t) (border_width * 2u + title_height))
            ? (uint16_t) (client->layout.geometry.cur.dim.h -
                    (uint16_t) (border_width * 2u + title_height))
            : (uint16_t) WM_MIN_WINDOW_DIMENSION;

        client->layout.frame_extents.left = (int32_t) border_width;
        client->layout.frame_extents.right = (int32_t) border_width;
        client->layout.frame_extents.top =
            (int32_t) (border_width + title_height);
        client->layout.frame_extents.bottom = (int32_t) border_width;

        /* The frame's X11-native border width must stay 0, always, for
         * a decorated client (see the main render pass in
         * render/desktop.c, which enforces exactly that).  The visible
         * border comes from the frame's size and background color (see
         * 'frame_extents' above), and not from an X11-native border.
         * A non-zero value here would add an unwanted one on top of
         * that until the next full repaint reset it back. */
        ccmd_client_apply_geometry(client, client->frame,
                (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
                0, 0, 0u, 0u, 0u);
        ccmd_client_apply_geometry(client, client->window,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT |
                    (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (int32_t) border_width,
                (int32_t) (border_width + title_height),
                inner_w, inner_h, 0u);
        if (client->titlebar != 0) {
            ccmd_client_apply_geometry(client, client->titlebar,
                    (uint16_t) XCB_CONFIG_WINDOW_X |
                        (uint16_t) XCB_CONFIG_WINDOW_Y |
                        (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                        (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                    (int32_t) border_width, (int32_t) border_width,
                    inner_w, title_height, 0u);
            xcb_window_show(client->titlebar);
        }
    }

    /* 'BORDER_WIDTH' is included here too, not just inside the
     * 'was_decorated_fullscreen' block above.  For an undecorated
     * client, 'target' is its window and this is the only place its
     * border gets restored at all, since there is no separate frame for
     * an earlier step to already have set it on. */
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y |
                (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                (uint16_t) XCB_CONFIG_WINDOW_HEIGHT |
                (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y,
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            (client->was_decorated_fullscreen) ? 0u : border_width);

    client->was_decorated_fullscreen = false;

    /* Same reasoning as the matching call in 'ccmd_client_fullscreen':
     * make sure the client is told its true screen-relative geometry
     * explicitly, since exiting fullscreen can restore it to any
     * position, not just (0,0), where a decorated client's real
     * 'ConfigureNotify' from the frame reparenting above would be
     * frame-relative instead. */
    client_send_synthetic_configure_notify(xcb_connection_get(),
            client);

    /* Only the full screen bit is cleared.  EWMH keeps
     * '_NET_WM_STATE_FULLSCREEN' independent of the maximized states,
     * so a window maximized before going full screen is maximized still
     * on leaving it, having never asked for that to be forgotten; and
     * one that was merely normal has no other bit to keep. */
    client->properties.state &= (uint16_t) ~CLIENT_STATE_FULLSCREEN;
    (void) clock_gettime(CLOCK_MONOTONIC,
            &client->fullscreen_transition_time);

    /* The workarea may well have moved while the client was covering
     * it, a panel having appeared or a monitor changed, so a restored
     * maximization is measured against the workarea as it is now
     * rather than the geometry saved on the way in.  Already folded
     * into 'layout.geometry.cur' and applied for real, once, right
     * after 'client_geometry_restore' above: nothing left to do here. */

    ccmd_publish_frame_extents(client,
            (uint32_t) client->layout.frame_extents.left,
            (uint32_t) client->layout.frame_extents.right,
            (uint32_t) client->layout.frame_extents.top,
            (uint32_t) client->layout.frame_extents.bottom);

    ccmd_client_sync_states(client);

    /* No longer fullscreen, so the forced-above stacking
     * 'ccmd_desktop_enforce_layers' gives a focused fullscreen client
     * no longer applies to it either way; re-run it now so it settles
     * straight back into its real layer group rather than waiting on
     * whatever future stacking-order pass happens to run next. */
    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        ccmd_desktop_enforce_layers(desktop);
    }

    /* The client that just left fullscreen may have been the one the
     * systray was lowered below; let it reconsider its stacking now
     * that it is gone */
    systray_restack();

    wm_request_client_redraw(client);
}


/* Toggle full screen mode */
void ccmd_client_toggle_fullscreen(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* No 'client_is_resizable' gate; see 'ccmd_client_fullscreen''s
     * comment for why fullscreen is deliberately exempt. */
    if (client_is_fullscreen(client)) {
        ccmd_client_unfullscreen(client);
    } else {
        ccmd_client_fullscreen(client);
    }
}


/* Toggle window decoration on or off */
void ccmd_client_toggle_decorate(client_td *client)
{
    int32_t bw;
    int32_t th;
    desktop_td *desktop;

    if (client == NULL || client_is_fullscreen(client) ||
            client_is_locked(client)) {
        return;
    }

    LOGGER_TRACE("Toggling decoration for client window=0x%x" \
            " (currently decorated=%d)", client->window,
            (int) client_is_decorated(client));

    /* A resize-cursor poll target (see 'mouse_hover_poll_tick' in
     * input/mouse/hover.h) tracked for either of this client's windows
     * would otherwise keep polling and re-applying a cursor to
     * whichever one it was tracking before this toggle, oblivious to
     * decoration having just changed underneath it.  If it was tracking
     * the client's window because it was undecorated when hover-polling
     * started, and this toggle adds a frame, the two would fight over
     * the client window's cursor from then on, one correctly following
     * the new frame's border and the other still polling the client
     * window directly on a stale assumption. */
    mouse_hover_poll_clear(client->window);
    if (client->frame != 0) {
        mouse_hover_poll_clear(client->frame);
    }

    /* 'ignore_frame=true': computed once here, before this toggle's
     * direction (remove or restore) is even decided below, and used by
     * both; whichever one runs, this represents the border width the
     * client/frame is being configured to, not one its current framing
     * state (about to change either way) already accounts for.  Same
     * reasoning as 'ccmd_client_unfullscreen''s identical call. */
    bw = (int32_t) client_border_width(client, true, true);
    th = (int32_t) client->title_height;
    desktop = wm_get_client_desktop(client);

    s_client_unshade_if_needed(client);

    if (client_is_decorated(client)) {  /* Remove decoration */
        s_ccmd_decorate_remove(client, bw);
    } else {                            /* Restore decoration */
        s_ccmd_decorate_restore(client, bw, th);
    }

    s_ccmd_decorate_remaximize(client);

    if (desktop != NULL) {
        desktop->client_active_id = client->id;
        desktop->is_focus_dirty = true;
        (void) desktop_action_client_send_front(desktop, client);
        desktop->is_outdated = true;
    }

    ccmd_client_raise(client);
    ccmd_client_focus(client);

    /* Toggling decoration changes whether resize/move/decoration
     * actions actually make sense (an undecorated client's border
     * cannot be dragged to resize it, for one), so the client's
     * '_NET_WM_ALLOWED_ACTIONS' needs republishing here, the same as
     * every other place this project's capabilities genuinely change
     * out from under a client. */
    ccmd_client_update_allowed_actions(client);

    wm_request_client_redraw(client);
}
