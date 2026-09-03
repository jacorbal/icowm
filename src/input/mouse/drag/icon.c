/**
 * @file input/mouse/drag/icon.c
 *
 * @brief Everything specific to dragging an icon window
 *
 * One of the files @c input/mouse/drag/ is made of.  Kept apart from
 * @c input/mouse/drag/overlay.c even though
 * @a drag_icon_sync_active_visual and @a drag_icon_height touch the
 * overlay window: icon
 * dragging merely reuses that window, and neither function is about
 * the overlay itself.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <render/icon.h>

/* Local includes */
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/overlay.h>


/* Begin a drag operation for an icon window */
void drag_icon_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        struct position_s icon_pos,
        xcb_timestamp_t event_time,
        struct position_s root_pos,
        struct dimensions_s screen_dim)
{
    xcb_grab_pointer_cookie_t grab_cookie;
    xcb_grab_pointer_reply_t *grab_reply;

    if (connection == NULL || client == NULL) {
        return;
    }

    drag_overlay_hide(connection);
    s_drag.is_active = true;
    s_drag.client = client;
    s_drag.desktop = desktop;
    s_drag.drag_window = client->icon_window;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.pointer_start_x = (int16_t) root_pos.x;
    s_drag.pointer_start_y = (int16_t) root_pos.y;
    s_drag.client_start.pos = icon_pos;
    s_drag.client_start.dim.w = 0;
    s_drag.client_start.dim.h = 0;
    s_drag.client_cur.pos = icon_pos;
    /* Icon drags are always solid, regardless of 'windows.solid-drag':
     * moving just the small icon window live is cheap enough on its
     * own that the outline machinery would add complexity for no
     * real benefit here; see 'drag_end''s comment on this same
     * exclusion. */
    s_drag.is_solid_drag = true;
    s_drag.screen_w = screen_dim.w;
    s_drag.screen_h = screen_dim.h;
    s_drag.was_icon_mapped = client->is_icon_mapped;
    s_drag.is_anchor_right = false;
    s_drag.is_anchor_bottom = false;
    s_drag.is_resize_w = false;
    s_drag.is_resize_h = false;
    s_drag.has_last_pos = false;
    s_drag.is_warp_pending = false;

    client->properties.operation = CLIENT_OPERATION_MOVING;
    client->is_icon_mapped = true;
    /* Repainted at once, and forced: the icon has just become the
     * picked-up one, and waiting for the next render pass would show
     * it unselected for a frame first. */
    ri_render_client_icon(client, true, true);

    grab_cookie = xcb_grab_pointer(connection,
            0,
            root,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            XCB_NONE,
            event_time);
    grab_reply = xcb_grab_pointer_reply(connection, grab_cookie, NULL);

    if (grab_reply == NULL ||
            grab_reply->status != XCB_GRAB_STATUS_SUCCESS) {
        LOGGER_WARNING("xcb_grab_pointer failed for icon drag start," \
                " status=%d",
                (grab_reply != NULL) ? (int) grab_reply->status : -1);
        free(grab_reply);
        s_drag.is_active = false;
        return;
    }
    free(grab_reply);
}


/* Query whether the active drag is on an icon window */
bool drag_is_icon_drag(void)
{
    return s_drag.is_active &&
        s_drag.drag_window != XCB_WINDOW_NONE &&
        s_drag.client != NULL &&
        s_drag.drag_window == s_drag.client->icon_window;
}


/* Return the full icon-window height for a dragged client */
uint16_t drag_icon_height(const client_td *client)
{
    if (client == NULL || client->config == NULL) {
        return (uint16_t) WM_ICON_SQUARE_SIZE;
    }

    return (uint16_t) (WM_ICON_SQUARE_SIZE +
            ((client->config->theme.icon.is_captioned)
                ? WM_ICON_CAPTION_HEIGHT
                : 0u));
}
