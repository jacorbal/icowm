/**
 * @file cmds/ccmd.c
 *
 * @brief Implementation on executions over clients using the XCB
 *        interface while updating EWMH and ICCCM hints
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>     /* va_arg, va_end, va_start */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* size_t, snprintf */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <actdata.h>
#include <client.h>
#include <desktop.h>
#include <place.h>

/* Local includes */
#include <cmds/ccmd.h>
#include <cmds/util.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/meta.h>


/* Perform the action to close the client */
void wcmd_client_close(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* This destroys the client in 'client->window', but does not
     * deallocates the memory of the client object.  This is intended to
     * be called by the desktop, therefore, it's responsibility of the
     * desktop to execute this action, and then invoke 'client_destroy' */
    xcb_destroy_window(client->connection, client->window);
}


/* Forcibly kill the client's X connection */
void wcmd_client_kill(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* Unlike 'wcmd_client_close' (a request to destroy a single window
     * resource), 'xcb_kill_client' terminates the owning client's
     * ENTIRE connection to the X server.  Meant as a last resort for
     * unresponsive clients that ignore a normal close request. */
    xcb_kill_client(client->connection, client->window);
}


/* Restore a client to its normal state */
void wcmd_client_restore(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_restore(client);

    xcb_map_window(client->connection, client->window);
    if (client->icon_window != 0 && client->is_icon_mapped) {
        xcb_unmap_window(client->connection, client->icon_window);
        client->is_icon_mapped = false;
    }
    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }
    xcb_map_window(client->connection, target);
    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unset_hidden(client);
    client->properties.state = CLIENT_STATE_NORMAL;

    client_geometry_save(client);

    wcmd_rem_states(client, 3,
            "_NET_WM_STATE_HIDDEN",
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
}


/* Focus a client */
void wcmd_client_focus(client_td *client)
{
    if (client == NULL) {
        return;
    }

    xcb_set_input_focus(client->connection, XCB_INPUT_FOCUS_PARENT,
                        client->window, XCB_CURRENT_TIME);
    xcb_map_window(client->connection, client->window);

    if (client->ewmh != NULL) {
        xcb_ewmh_request_change_active_window(client->ewmh,
                (int) client->screen_id,
                client->window, 0,
                XCB_CURRENT_TIME,
                wcmd_active_win(client->ewmh,
                    client->screen_id));
    }
}


/* Unfocus the client */
void wcmd_client_unfocus(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_unfocus(client);

    if (client->ewmh != NULL) {
        xcb_ewmh_request_change_active_window(client->ewmh,
                (int) client->screen_id,
                XCB_NONE, 0,
                XCB_CURRENT_TIME, 0);
    }
}


/* Move client */
void wcmd_client_iconify(client_td *client)
{
    xcb_window_t target;
    uint32_t mask;
    uint32_t values[3];

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    if (client->icon_window == 0) {
        uint16_t icon_h;
        uint16_t screen_w;
        uint16_t screen_h;
        int16_t ix;
        int16_t iy;
        enum config_icon_placement_e policy =
            CONFIG_ICON_PLACEMENT_BOTTOM;

        icon_h = (uint16_t) (WM_ICON_SQUARE_SIZE +
                ((client->theme->icon.is_captioned)
                 ? WM_ICON_CAPTION_HEIGHT
                 : 0u));

        screen_w = 1024u;
        screen_h = 768u;
        if (wcmd_screen_dim(client, &screen_w, &screen_h)) {
            /* dimensions updated */
        }

        if (client->config_base != NULL) {
            policy = client->config_base->icons.placement_policy;
        }

        /* Re-use saved position when the client was already iconified
         * once and manually repositioned by the user */
        if (client->icon_x >= 0 && client->icon_y >= 0) {
            ix = client->icon_x;
            iy = client->icon_y;
        } else {
            place_icon(client, wm_get_client_desktop(client), policy,
                    WM_ICON_SQUARE_SIZE, icon_h,
                    screen_w, screen_h,
                    &ix, &iy);
            client->icon_x = ix;
            client->icon_y = iy;
        }

        client->icon_window = xcb_generate_id(client->connection);
        mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
        values[0] = client->theme->icon.background_color;
        values[1] = client->theme->icon.border_color;
        values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                    XCB_EVENT_MASK_BUTTON_MOTION;
        xcb_create_window(client->connection,
                XCB_COPY_FROM_PARENT,
                client->icon_window,
                client->parent_id,
                ix, iy,
                (uint16_t) WM_ICON_SQUARE_SIZE, icon_h,
                (uint16_t) client->theme->icon.border_width,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                XCB_COPY_FROM_PARENT,
                mask, values);
    } else {
        /* Re-map at the saved position (may have been dragged) */
        xcb_configure_window(client->connection, client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                (const uint32_t[]) {
                    (uint32_t) client->icon_x,
                    (uint32_t) client->icon_y
                });
    }

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }
    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }
    xcb_map_window(client->connection, client->icon_window);
    client->is_icon_mapped = true;

    client_set_hidden(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    /* Iconify per EWMH: window hidden with '_NET_WM_STATE_HIDDEN'.
     * Icon display handled by pager/desktop */

    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    xcb_flush(client->connection);
}


/* Hide the client (minimize, but not iconify) */
void wcmd_client_hide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }
    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    client_set_hidden(client);

    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Show (unhide) the client */
void wcmd_client_unhide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }
    xcb_map_window(client->connection, target);
    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unset_hidden(client);

    wcmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Shade client (roll-up), if decorated */
void wcmd_client_shade(client_td *client)
{
    if (client == NULL || !client_is_decorated(client)) {
        return;
    }

    client_geometry_save(client);
    client_set_shade(client);

    wcmd_add_states(client, 1, "_NET_WM_STATE_SHADED");
    wcmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");

    xcb_flush(client->connection);
}


/* Unshade client (roll-down), if decorated */
void wcmd_client_unshade(client_td *client)
{
    if (client == NULL || !client_is_decorated(client)) {
        return;
    }

    client_geometry_restore(client);
    client_unset_shade(client);
    client_unset_hidden(client);

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_SHADED",
            "_NET_WM_STATE_HIDDEN");

    xcb_flush(client->connection);
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


/* Set client sticky mode */
void wcmd_client_sticky(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_set_sticky(client);
    wcmd_add_states(client, 1, "_NET_WM_STATE_STICKY");
}

/* Remove client sticky mode */
void wcmd_client_unsticky(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_unset_sticky(client);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_STICKY");
}


/* Toggle stickiness */
void wcmd_client_toggle_sticky(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client_is_sticky(client)) {
        wcmd_client_unsticky(client);
    } else {
        wcmd_client_sticky(client);
    }
}


/* Set full screen mode */
void wcmd_client_fullscreen(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    if (!wcmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    /* Configure window to fill entire screen */
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                0,                  /* X: top-left corner */
                0,                  /* Y: top-left corner */
                (uint32_t) sw,      /* Width: full screen width */
                (uint32_t) sh       /* Height: full screen height */
            });

    client->layout.geometry.cur.pos.x = 0;
    client->layout.geometry.cur.pos.y = 0;
    client->layout.geometry.cur.dim.w = sw;
    client->layout.geometry.cur.dim.h = sh;

    client->properties.state = CLIENT_STATE_FULLSCREEN;

    /* Hide client decorations if decorated (EWMH recommendation) */
    if (client_is_decorated(client)) {
        /* Mark that decorations should be hidden.  This would typically
         * be handled by the theme/decoration system */
    }

    /* Update EWMH states */
    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
    wcmd_add_states(client, 1, "_NET_WM_STATE_FULLSCREEN");

    xcb_flush(client->connection);
}


/* Remove full screen mode */
void wcmd_client_unfullscreen(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_restore(client);

    /* Reconfigure window to restored position and dimensions */
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,
                (uint32_t) client->layout.geometry.cur.pos.y,
                (uint32_t) client->layout.geometry.cur.dim.w,
                (uint32_t) client->layout.geometry.cur.dim.h
            });

    /* Update internal client state */
    client->properties.state = CLIENT_STATE_NORMAL;

    /* Restore client decorations if previously decorated */
    if (client_is_decorated(client)) {
        /* Mark that decorations should be shown again */
    }

    /* Update EWMH states */
    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");

    xcb_flush(client->connection);
}


/* Toggle full screen mode */
void wcmd_client_toggle_fullscreen(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client->properties.state == CLIENT_STATE_FULLSCREEN) {
        wcmd_client_unfullscreen(client);
    } else {
        wcmd_client_fullscreen(client);
    }
}


/* Raise the client to the top */
void wcmd_client_set_urgent(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_set_urgent(client);
    wcmd_add_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Clear client urgency */
void wcmd_client_clear_urgent(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_unset_urgent(client);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Set icon for the client */
void wcmd_client_toggle_decoration(client_td *client)
{
    int32_t bw;
    int32_t th;

    if (client == NULL || client->frame == 0) {
        return;
    }

    bw = (int32_t) client->theme->window.general.border_width;
    th = (int32_t) client->title_height;

    if (client_is_decorated(client)) {  /* Remove decoration */
         /* Compute inner client geometry from current frame geometry */
        int32_t inner_x = client->layout.geometry.cur.pos.x +
                          client->layout.frame_extents.left;
        int32_t inner_y = client->layout.geometry.cur.pos.y +
                          client->layout.frame_extents.top;
        int32_t inner_w = (int32_t) client->layout.geometry.cur.dim.w -
                          client->layout.frame_extents.left -
                          client->layout.frame_extents.right;
        int32_t inner_h = (int32_t) client->layout.geometry.cur.dim.h -
                          client->layout.frame_extents.top -
                          client->layout.frame_extents.bottom;

        if (inner_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            inner_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }
        if (inner_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            inner_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }

        /* Reposition frame to cover only the client content area */
        xcb_configure_window(client->connection, client->frame,
                XCB_CONFIG_WINDOW_X     |
                XCB_CONFIG_WINDOW_Y     |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    (uint32_t) inner_x, (uint32_t) inner_y,
                    (uint32_t) inner_w, (uint32_t) inner_h
                });

        /* Place client window at (0, 0) within the now-borderless frame */
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_X     |
                XCB_CONFIG_WINDOW_Y     |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    0u, 0u,
                    (uint32_t) inner_w, (uint32_t) inner_h
                });

        if (client->titlebar != 0) {
            xcb_unmap_window(client->connection, client->titlebar);
        }

        client->layout.geometry.cur.pos.x = inner_x;
        client->layout.geometry.cur.pos.y = inner_y;
        client->layout.geometry.cur.dim.w = (uint16_t) inner_w;
        client->layout.geometry.cur.dim.h = (uint16_t) inner_h;
        client->layout.frame_extents.left   = 0;
        client->layout.frame_extents.right  = 0;
        client->layout.frame_extents.top    = 0;
        client->layout.frame_extents.bottom = 0;
        client_unset_decoration(client);
    } else {                            /* Restore decoration */
        /* The frame currently wraps the bare client content; expand it
         * to include the titlebar above and borders on all sides */
        int32_t frame_x = client->layout.geometry.cur.pos.x - bw;
        int32_t frame_y = client->layout.geometry.cur.pos.y - (bw + th);
        int32_t frame_w = (int32_t) client->layout.geometry.cur.dim.w +
                          2 * bw;
        int32_t frame_h = (int32_t) client->layout.geometry.cur.dim.h +
                          2 * bw + th;

        if (frame_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            frame_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }
        if (frame_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            frame_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }

        /* Expand frame to include borders and titlebar */
        xcb_configure_window(client->connection, client->frame,
                XCB_CONFIG_WINDOW_X     |
                XCB_CONFIG_WINDOW_Y     |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    (uint32_t) frame_x, (uint32_t) frame_y,
                    (uint32_t) frame_w, (uint32_t) frame_h
                });

        /* Reposition client window inside frame at '(bw, bw+th)' */
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                (const uint32_t[]) {
                    (uint32_t) bw, (uint32_t) (bw + th)
                });

        /* Configure and map titlebar */
        if (client->titlebar != 0) {
            xcb_configure_window(client->connection, client->titlebar,
                    XCB_CONFIG_WINDOW_X     |
                    XCB_CONFIG_WINDOW_Y     |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT,
                    (const uint32_t[]) {
                        0u, 0u,
                        (uint32_t) frame_w, (uint32_t) (bw + th)
                    });
            xcb_map_window(client->connection, client->titlebar);
        }

        client->layout.geometry.cur.pos.x = frame_x;
        client->layout.geometry.cur.pos.y = frame_y;
        client->layout.geometry.cur.dim.w = (uint16_t) frame_w;
        client->layout.geometry.cur.dim.h = (uint16_t) frame_h;
        client->layout.frame_extents.left = bw;
        client->layout.frame_extents.right = bw;
        client->layout.frame_extents.top = bw + th;
        client->layout.frame_extents.bottom = bw;
        client_set_decoration(client);
    }

    xcb_flush(client->connection);
}
