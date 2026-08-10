/**
 * @file cmds/state.c
 *
 * @brief Client state-transition commands: shading, fullscreen, and
 *        decoration toggling
 *
 * Contains operations that change a client's visual state in ways that
 * require XCB geometry manipulation beyond a simple flag update:
 * shade/unshade, fullscreen/unfullscreen, and decoration toggle
 * (including the private helper that builds the frame and titlebar
 * windows).  Focus and visibility operations live in @c cmds/ccmd.c.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdlib.h>     /* free */
#include <time.h>       /* CLOCK_MONOTONIC, clock_gettime */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Default initial values */
#include <defs/client.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <systray.h>
#include <utils/geom.h>
#include <wm.h>

/* Input includes */
#include <input/mouse.h>

/* Local includes */
#include <cmds/ccmd.h>
#include <cmds/layer.h>
#include <cmds/util.h>


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
    int32_t frame_x;
    int32_t frame_y;
    int32_t frame_w;
    int32_t frame_h;
    uint32_t border_color;
    uint32_t bg_color;
    static const xcb_button_index_t s_grab_buttons[] = {
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

    border_color = (client->theme != NULL)
        ? client->theme->window.inactive.border.color
        : 0x999999U;
    bg_color = (client->theme != NULL)
        ? client->theme->window.inactive.color.background
        : 0x000000U;

    frame_x = client->layout.geometry.cur.pos.x - bw;
    frame_y = client->layout.geometry.cur.pos.y - (bw + th);
    frame_w = (int32_t) client->layout.geometry.cur.dim.w + 2 * bw;
    frame_h = (int32_t) client->layout.geometry.cur.dim.h + 2 * bw + th;

    if (frame_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        frame_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
    }
    if (frame_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        frame_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
    }

    wcmd_client_ungrab_buttons(client);

    client->frame = xcb_generate_id(client->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = border_color;
    values[1] = border_color;
    /* 'XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT' is required so the
     * client's own future resize/move attempts on itself are delivered
     * to the window manager as 'ConfigureRequest's instead of being
     * applied directly by the server with no notification at all */
    values[2] = XCB_EVENT_MASK_EXPOSURE             |
                XCB_EVENT_MASK_BUTTON_PRESS         |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY     |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY  |
                XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_POINTER_MOTION;
    xcb_create_window(client->connection,
            XCB_COPY_FROM_PARENT,
            client->frame,
            client->parent_id,
            (int16_t) frame_x, (int16_t) frame_y,
            (uint16_t) frame_w, (uint16_t) frame_h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    client->titlebar = xcb_generate_id(client->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = bg_color;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS;
    xcb_create_window(client->connection,
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
     *  1. From root's SubstructureNotify (event=root, window=content)
     *  2. From the content window's own StructureNotify (event=window,
     *     window=content)
     *
     * Absorb both so focus is not stolen from the active window. */
    client->ignore_unmap += 2u;
    client->ignore_focus_unmap++;

    xcb_reparent_window(client->connection,
            client->window,
            client->frame,
            (int16_t) bw, (int16_t) (bw + th));

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, (const uint32_t[]) {0u});

    for (size_t bi = 0; bi < nb; ++bi) {
        xcb_grab_button(client->connection,
                0,
                client->frame,
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE,
                XCB_GRAB_MODE_SYNC,
                XCB_GRAB_MODE_ASYNC,
                XCB_NONE,
                XCB_NONE,
                (uint8_t) s_grab_buttons[bi],
                XCB_MOD_MASK_ANY);
    }

    xcb_map_window(client->connection, client->frame);
    xcb_map_window(client->connection, client->titlebar);
    xcb_map_window(client->connection, client->window);

    client->layout.geometry.cur.pos.x = frame_x;
    client->layout.geometry.cur.pos.y = frame_y;
    client->layout.geometry.cur.dim.w = (uint16_t) frame_w;
    client->layout.geometry.cur.dim.h = (uint16_t) frame_h;
    client->layout.frame_extents.left = bw;
    client->layout.frame_extents.right = bw;
    client->layout.frame_extents.top = bw + th;
    client->layout.frame_extents.bottom = bw;
    client_set_decoration(client);

    wcmd_publish_frame_extents(client,
            (uint32_t) bw, (uint32_t) bw,
            (uint32_t) (bw + th), (uint32_t) bw);
}


/* Shade client (roll-up), if decorated */
void wcmd_client_shade(client_td *client)
{
    xcb_window_t target;
    uint32_t shaded_h;
    xcb_get_geometry_cookie_t geom_ck;
    xcb_get_geometry_reply_t *geom_r;

    if (client == NULL || !client_is_decorated(client) ||
            client_is_shaded(client) || client_is_fullscreen(client)) {
        return;
    }

    LOGGER_TRACE("Shading client window=0x%x", client->window);

    target = wcmd_target_win(client);

    /* Refresh 'geometry.cur' from the real X11 state right before
     * saving it: an application-driven resize the window manager did
     * not initiate could leave 'geometry.cur' stale, and shading would
     * then save (and unshading would later restore) the wrong height. */
    geom_ck = xcb_get_geometry(client->connection, target);
    geom_r = xcb_get_geometry_reply(client->connection, geom_ck, NULL);
    if (geom_r != NULL) {
        client->layout.geometry.cur.dim.w = geom_r->width;
        client->layout.geometry.cur.dim.h = geom_r->height;
        free(geom_r);
    }

    client_geometry_save(client);

    shaded_h = (uint32_t) (client->layout.frame_extents.top +
                           client->layout.frame_extents.bottom);
    if (shaded_h < WM_MIN_WINDOW_DIMENSION) {
        shaded_h = WM_MIN_WINDOW_DIMENSION;
    }

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) { shaded_h });
    xcb_map_window(client->connection, target);

    /* Absorb both 'UnmapNotify' events generated by the content window
     * unmap: the frame's 'SubstructureNotify' ('event=frame,
     * window=content') and the content window's own 'StructureNotify'
     * ('event=content, window=content').  Both carry 'event->window ==
     * client->window', so both reach the fall-through branch of
     * 'handler_unmap_notify'; only one 'ignore_unmap' token would leave
     * a second event unguarded, causing 'handler_unmap_notify' to
     * wrongly unmap the frame and titlebar and make the shaded titlebar
     * disappear. */
    client->ignore_unmap += 2u;
    xcb_unmap_window(client->connection, client->window);

    client->layout.geometry.cur.dim.h = (uint16_t) shaded_h;
    client_set_shade(client);
    client_sync_decoration_layout(client);
    (void) clock_gettime(CLOCK_MONOTONIC, &client->shade_transition_time);

    wcmd_add_states(client, 1, "_NET_WM_STATE_SHADED");
    wcmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Unshade client (roll-down), if decorated */
void wcmd_client_unshade(client_td *client)
{
    xcb_window_t target;
    uint32_t restored_h;

    if (client == NULL || !client_is_decorated(client) ||
            !client_is_shaded(client)) {
        return;
    }

    LOGGER_TRACE("Unshading client window=0x%x", client->window);

    target = wcmd_target_win(client);

    /* Restore only the height from the saved geometry; keep the current
     * position so that moving the shaded window is honored */
    restored_h = client->layout.geometry.old.dim.h;
    client->layout.geometry.cur.dim.h = restored_h;

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) { restored_h });
    xcb_map_window(client->connection, client->window);

    client_unset_shade(client);
    client_unset_hidden(client);
    (void) clock_gettime(CLOCK_MONOTONIC, &client->shade_transition_time);

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_SHADED",
            "_NET_WM_STATE_HIDDEN");

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/**
 */
static void s_client_unshade_if_needed(client_td *client)
{
    if (client != NULL && client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }
}


/* Toggle shading */
void wcmd_client_toggle_shade(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    } else {
        wcmd_client_shade(client);
    }
}


/* Set full screen mode */
void wcmd_client_fullscreen(client_td *client)
{
    int32_t mx = 0;
    int32_t my = 0;
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;
    uint32_t border_width;
    bool was_decorated;
    desktop_td *desktop;
    monitor_td monitor;

    if (client == NULL) {
        return;
    }

    /* Deliberately no 'client_is_resizable' gate here, unlike maximize:
     * fullscreen is a WM-forced override of the client's own preferred
     * geometry, not a user-convenience resize the client's own fixed
     * size hints have any say over.  A DOS-emulation or retro-game
     * window that fixes its own size (min == max in WM_NORMAL_HINTS,
     * clearing CLIENT_FLAG_RESIZABLE; see client/props.c) still needs
     * to enter fullscreen correctly when it requests
     * '_NET_WM_STATE_FULLSCREEN' on its own alt+enter handling, which
     * this check used to silently swallow. */

    LOGGER_TRACE("Entering fullscreen for client window=0x%x",
            client->window);

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    /* Fullscreen deliberately targets the raw monitor rect, not the
     * workarea 'wcmd_client_monitor_workarea' (maximize's own helper)
     * would give: it is meant to cover panels and docks too, not stop
     * at their struts the way maximize does. */
    if (wcmd_client_monitor(client, NULL, &monitor)) {
        mx = monitor.x;
        my = monitor.y;
        sw = geom_clamp_dim((int32_t) monitor.w);
        sh = geom_clamp_dim((int32_t) monitor.h);
    } else if (!wcmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    client_geometry_save(client);
    was_decorated = client_is_decorated(client);
    client->was_decorated_fullscreen = was_decorated;
    target = wcmd_target_win(client);

    if (was_decorated && client->frame != 0) {
        border_width = 0u;
        if (client->titlebar != 0) {
            xcb_unmap_window(client->connection, client->titlebar);
        }
        xcb_configure_window(client->connection, client->frame,
                XCB_CONFIG_WINDOW_BORDER_WIDTH, &border_width);
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_X |
                XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT |
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const uint32_t[]) {
                (uint32_t) mx, (uint32_t) my,
                (uint32_t) sw, (uint32_t) sh,
                0u
                });
        client->layout.frame_extents.left = 0;
        client->layout.frame_extents.right = 0;
        client->layout.frame_extents.top = 0;
        client->layout.frame_extents.bottom = 0;
    }

    /* Resize the visible target to fill screen.  'BORDER_WIDTH' is
     * always included here, not just inside the 'was_decorated' block
     * above: for an undecorated client, 'target' is the client's own
     * window (there is no frame to have already cleared the border
     * on), and it may well have a real border of its own (the theme's
     * border width is applied directly to undecorated windows).
     * Leaving that out only for undecorated clients was exactly why
     * they kept a visible border in fullscreen while decorated ones
     * did not. */
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X      |
            XCB_CONFIG_WINDOW_Y      |
            XCB_CONFIG_WINDOW_WIDTH  |
            XCB_CONFIG_WINDOW_HEIGHT |
            XCB_CONFIG_WINDOW_BORDER_WIDTH,
            (const uint32_t[]) {
                (uint32_t) mx, (uint32_t) my,
                (uint32_t) sw,
                (uint32_t) sh,
                0u
            });

    client->layout.geometry.cur.pos.x = mx;
    client->layout.geometry.cur.pos.y = my;
    client->layout.geometry.cur.dim.w = (uint32_t) sw;
    client->layout.geometry.cur.dim.h = (uint32_t) sh;

    /* ICCCM §4.2.3: applications that render via GL/Vulkan (e.g.,
     * 'mplayer', 'mpv') generally wait for a 'ConfigureNotify' before
     * resizing their rendering surface/viewport, and it must carry the
     * true screen-relative geometry.  The real 'ConfigureNotify' the
     * X server sends for the frame/window configure above already
     * carries that geometry (including the monitor's own origin, not
     * necessarily (0,0), on a surface made of more than one monitor),
     * so this was not strictly required for the client's OWN
     * 'ConfigureNotify'; but the frame reparenting above still delivers
     * one relative to the *frame*, and without an explicit synthetic
     * one afterward some clients only apply the next size they are told
     * about relative to their own last known good state, which can
     * otherwise show as a blank frame until an unrelated event forces
     * a fresh redraw. */
    client_send_synthetic_configure_notify(client->connection, client);

    client->properties.state = CLIENT_STATE_FULLSCREEN;

    wcmd_publish_frame_extents(client, 0u, 0u, 0u, 0u);

    /* Retain focus: keep this client active on its desktop and give it
     * input focus so the window is not lost from the active window
     * tracking when going fullscreen. */
    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        desktop->client_active_id = client->id;
        desktop->focus_dirty = true;
        (void) desktop_action_client_send_front(desktop, client);
        desktop->is_outdated = true;
    }
    wcmd_client_focus(client);

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
    wcmd_add_states(client, 1, "_NET_WM_STATE_FULLSCREEN");

    /* Let the systray reconsider its stacking now that a client just
     * became fullscreen: with 'systray.layer' set to "above" it should
     * drop below this window, the same way a taskbar or panel yields to
     * a fullscreen application everywhere else. */
    systray_restack();

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Remove full screen mode */
void wcmd_client_unfullscreen(client_td *client)
{
    xcb_window_t target;
    uint16_t border_width;
    uint16_t title_height;
    uint16_t inner_w;
    uint16_t inner_h;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Exiting fullscreen for client window=0x%x",
            client->window);

    target = wcmd_target_win(client);
    client_geometry_restore(client);

    border_width = (client->theme != NULL)
        ? (uint16_t) client->theme->window.active.border.width : 0u;

    /* 'BORDER_WIDTH' is included here too, not just inside the
     * 'was_decorated_fullscreen' block below: for an undecorated
     * client, 'target' is its own window and this is the only place
     * its border gets restored at all, since there is no separate
     * frame for a later step to set it on.  Restore the real theme
     * width directly; for a decorated client this 0-or-real value is
     * immediately superseded by the frame's own border configure just
     * below, so it is harmless there, just redundant. */
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X      |
            XCB_CONFIG_WINDOW_Y      |
            XCB_CONFIG_WINDOW_WIDTH  |
            XCB_CONFIG_WINDOW_HEIGHT |
            XCB_CONFIG_WINDOW_BORDER_WIDTH,
            (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,
                (uint32_t) client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h,
                (client->was_decorated_fullscreen)
                    ? 0u : (uint32_t) border_width
            });

    if (client->was_decorated_fullscreen && client->frame != 0) {
        title_height = client->title_height;
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

        client->layout.frame_extents.left = border_width;
        client->layout.frame_extents.right = border_width;
        client->layout.frame_extents.top =
            (uint16_t) (border_width + title_height);
        client->layout.frame_extents.bottom = border_width;

        /* The frame's own X11-native border width must stay 0, always,
         * for a decorated client (see the main render pass in
         * render/desktop.c, which enforces exactly that): the visible
         * border comes from the frame's own size and background color
         * (see frame_extents above), not from an X11-native border.  A
         * non-zero value here would add an extra, unwanted border on
         * top of that until the next full repaint reset it back. */
        xcb_configure_window(client->connection, client->frame,
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const uint32_t[]) { 0u });
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_X |
                XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT |
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const uint32_t[]) {
                    (uint32_t) border_width,
                    (uint32_t) (border_width + title_height),
                    (uint32_t) inner_w,
                    (uint32_t) inner_h,
                0u
                });
        if (client->titlebar != 0) {
            xcb_configure_window(client->connection, client->titlebar,
                    XCB_CONFIG_WINDOW_X |
                    XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT,
                        (const uint32_t[]) {
                        (uint32_t) border_width,
                        (uint32_t) border_width,
                        (uint32_t) inner_w,
                        (uint32_t) title_height
                    });
            xcb_map_window(client->connection, client->titlebar);
        }
    }
    client->was_decorated_fullscreen = false;

    /* Same reasoning as the matching call in 'wcmd_client_fullscreen':
     * make sure the client is told its true screen-relative geometry
     * explicitly, since exiting fullscreen can restore it to any
     * position, not just (0,0), where a decorated client's real
     * 'ConfigureNotify' from the frame reparenting above would be
     * frame-relative instead. */
    client_send_synthetic_configure_notify(client->connection, client);

    client->properties.state = CLIENT_STATE_NORMAL;

    wcmd_publish_frame_extents(client,
            (uint32_t) client->layout.frame_extents.left,
            (uint32_t) client->layout.frame_extents.right,
            (uint32_t) client->layout.frame_extents.top,
            (uint32_t) client->layout.frame_extents.bottom);

    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");

    /* The client that just left fullscreen may have been the one the
     * systray was lowered below; let it reconsider its stacking now
     * that it is gone */
    systray_restack();

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Toggle full screen mode */
void wcmd_client_toggle_fullscreen(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* No 'client_is_resizable' gate; see 'wcmd_client_fullscreen''s own
     * comment for why fullscreen is deliberately exempt. */
    if (client->properties.state == CLIENT_STATE_FULLSCREEN) {
        wcmd_client_unfullscreen(client);
    } else {
        wcmd_client_fullscreen(client);
    }
}


/* Toggle window decoration on or off */
void wcmd_client_toggle_decoration(client_td *client)
{
    int32_t bw;
    int32_t th;
    desktop_td *desktop;
    bool keep_focus;

    if (client == NULL || client_is_fullscreen(client)) {
        return;
    }

    LOGGER_TRACE("Toggling decoration for client window=0x%x" \
            " (currently decorated=%d)", client->window,
            (int) client_is_decorated(client));

    /* A resize-cursor poll target (see 'mouse_hover_poll_tick' in
     * input/mouse.h) tracked for either of this client's windows
     * would otherwise keep polling and re-applying a cursor to
     * whichever one it was tracking before this toggle, oblivious to
     * decoration having just changed underneath it: if it was
     * tracking the client's own window because it was undecorated
     * when hover-polling started, and this toggle adds a frame, the
     * two would fight over the client window's cursor from then on,
     * one correctly following the new frame's own border and the
     * other still polling the client window directly on a stale
     * assumption. */
    mouse_hover_poll_clear(client->window);
    if (client->frame != 0) {
        mouse_hover_poll_clear(client->frame);
    }

    bw = (client->theme != NULL)
        ? (int32_t) client->theme->window.active.border.width
        : 0;
    th = (int32_t) client->title_height;
    desktop = wm_get_client_desktop(client);
    keep_focus = true;

    s_client_unshade_if_needed(client);

    if (client_is_decorated(client)) {  /* Remove decoration */
        if (client->frame != 0) {
            int32_t inner_x = client->layout.geometry.cur.pos.x +
                client->layout.frame_extents.left;
            int32_t inner_y = client->layout.geometry.cur.pos.y +
                client->layout.frame_extents.top;
            int32_t inner_w =
                (int32_t) client->layout.geometry.cur.dim.w -
                client->layout.frame_extents.left -
                client->layout.frame_extents.right;
            int32_t inner_h =
                (int32_t) client->layout.geometry.cur.dim.h -
                client->layout.frame_extents.top -
                client->layout.frame_extents.bottom;

            if (inner_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
                inner_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
            }
            if (inner_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
                inner_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
            }

            if (client->titlebar != 0) {
                xcb_destroy_window(client->connection, client->titlebar);
                client->titlebar = 0;
            }

            /* Reparenting generates a synthetic 'UnmapNotify' for the
             * content window.  Absorb it so 'handler_unmap_notify' does
             * not mistake the event for a voluntary hide and does not
             * steal focus from the window. */
            client->ignore_unmap += 2u;
            client->ignore_focus_unmap++;
            xcb_reparent_window(client->connection,
                    client->window,
                    client->parent_id,
                    (int16_t) inner_x, (int16_t) inner_y);

            xcb_configure_window(client->connection, client->window,
                    XCB_CONFIG_WINDOW_X     |
                    XCB_CONFIG_WINDOW_Y     |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT |
                    XCB_CONFIG_WINDOW_BORDER_WIDTH,
                    (const uint32_t[]) {
                        (uint32_t) inner_x, (uint32_t) inner_y,
                        (uint32_t) inner_w, (uint32_t) inner_h,
                        (uint32_t) bw
                    });

            xcb_destroy_window(client->connection, client->frame);
            client->frame = 0;

            client->layout.geometry.cur.pos.x = inner_x;
            client->layout.geometry.cur.pos.y = inner_y;
            client->layout.geometry.cur.dim.w = (uint16_t) inner_w;
            client->layout.geometry.cur.dim.h = (uint16_t) inner_h;
        } else {
            xcb_configure_window(client->connection, client->window,
                    XCB_CONFIG_WINDOW_BORDER_WIDTH,
                    (const uint32_t[]) { (uint32_t) bw });
        }

        client->layout.frame_extents.left = 0;
        client->layout.frame_extents.right = 0;
        client->layout.frame_extents.top = 0;
        client->layout.frame_extents.bottom = 0;
        client_unset_decoration(client);
        wcmd_client_grab_buttons(client);

        wcmd_publish_frame_extents(client, 0u, 0u, 0u, 0u);
    } else {                            /* Restore decoration */
        if (client->frame == 0) {
            s_client_enable_decoration(client, bw, th);
        } else {
            int32_t frame_x =
                client->layout.geometry.cur.pos.x - bw;
            int32_t frame_y =
                client->layout.geometry.cur.pos.y - (bw + th);
            int32_t frame_w =
                (int32_t) client->layout.geometry.cur.dim.w + 2 * bw;
            int32_t frame_h =
                (int32_t) client->layout.geometry.cur.dim.h + 2 * bw + th;

            if (frame_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
                frame_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
            }

            if (frame_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
                frame_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
            }

            xcb_configure_window(client->connection, client->frame,
                    XCB_CONFIG_WINDOW_X     |
                    XCB_CONFIG_WINDOW_Y     |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT,
                    (const uint32_t[]) {
                        (uint32_t) frame_x, (uint32_t) frame_y,
                        (uint32_t) frame_w, (uint32_t) frame_h
                    });

            xcb_configure_window(client->connection, client->window,
                    XCB_CONFIG_WINDOW_X |
                    XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT |
                    XCB_CONFIG_WINDOW_BORDER_WIDTH,
                    (const uint32_t[]) {
                        (uint32_t) bw, (uint32_t) (bw + th),
                        (uint32_t) client->layout.geometry.cur.dim.w,
                        (uint32_t) client->layout.geometry.cur.dim.h,
                        0u
                    });

            if (client->titlebar != 0) {
                xcb_configure_window(client->connection, client->titlebar,
                        XCB_CONFIG_WINDOW_X     |
                        XCB_CONFIG_WINDOW_Y     |
                        XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT,
                        (const uint32_t[]) {
                            (uint32_t) bw, (uint32_t) bw,
                            (uint32_t) client->layout.geometry.cur.dim.w,
                            (uint32_t) th
                        });
                xcb_map_window(client->connection, client->titlebar);
            }
            xcb_map_window(client->connection, client->frame);
            xcb_map_window(client->connection, client->window);

            client->layout.geometry.cur.pos.x = frame_x;
            client->layout.geometry.cur.pos.y = frame_y;
            client->layout.geometry.cur.dim.w = (uint16_t) frame_w;
            client->layout.geometry.cur.dim.h = (uint16_t) frame_h;
            client->layout.frame_extents.left = bw;
            client->layout.frame_extents.right = bw;
            client->layout.frame_extents.top = bw + th;
            client->layout.frame_extents.bottom = bw;
            client_set_decoration(client);

            wcmd_publish_frame_extents(client,
                    (uint32_t) bw, (uint32_t) bw,
                    (uint32_t) (bw + th), (uint32_t) bw);
        }
    }

    if (keep_focus) {
        if (desktop != NULL) {
            desktop->client_active_id = client->id;
            desktop->focus_dirty = true;
            (void) desktop_action_client_send_front(desktop, client);
            desktop->is_outdated = true;
        }

        wcmd_client_raise(client);
        wcmd_client_focus(client);
    }

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}
