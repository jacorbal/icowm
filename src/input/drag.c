/**
 * @file input/drag.c
 *
 * @brief Mouse drag-operation state and implementation
 *
 * Manages the singleton drag state used by the move/resize and
 * icon-drag interactions.  All mutable drag state is @c static in
 * this translation unit; no other module accesses it directly.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/geom.h>

/* Windows & icons policy includes */
#include <policy/focus.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <input/drag.h>


/**
 * @brief Singleton drag state
 */
static struct {
    bool active;
    enum window_operation_e operation;
    client_td *client;
    xcb_window_t drag_window;   /**< Icon window moved, or
                                 *   'XCB_WINDOW_NONE' for normal drag */
    int16_t pointer_start_x;
    int16_t pointer_start_y;
    int32_t client_start_x;
    int32_t client_start_y;
    uint16_t client_start_w;
    uint16_t client_start_h;
    uint32_t screen_w;          /**< Screen width for edge snap */
    uint32_t screen_h;          /**< Screen height for edge snap */
    uint32_t snap;              /**< Snap distance in pixels */
} s_drag = {
    .active = false,
    .operation = CLIENT_OPERATION_IDLE,
    .client = NULL,
    .drag_window = XCB_WINDOW_NONE,
    .pointer_start_x = 0,
    .pointer_start_y = 0,
    .client_start_x = 0,
    .client_start_y = 0,
    .client_start_w = 0,
    .client_start_h = 0,
    .screen_w = 0,
    .screen_h = 0,
    .snap = 0
};


/* Begin a drag operation for a managed client window */
void drag_start(xcb_connection_t *connection,
        xcb_window_t root, client_td *client,
        enum window_operation_e operation,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y,
        uint32_t screen_w, uint32_t screen_h,
        uint32_t snap)
{
    if (connection == NULL || client == NULL) {
        return;
    }

    s_drag.active = true;
    s_drag.client = client;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.operation = operation;
    s_drag.pointer_start_x = root_x;
    s_drag.pointer_start_y = root_y;
    s_drag.client_start_x = client->layout.geometry.cur.pos.x;
    s_drag.client_start_y = client->layout.geometry.cur.pos.y;
    s_drag.client_start_w =
        (uint16_t) client->layout.geometry.cur.dim.w;
    s_drag.client_start_h =
        (uint16_t) client->layout.geometry.cur.dim.h;
    s_drag.screen_w = screen_w;
    s_drag.screen_h = screen_h;
    s_drag.snap = snap;

    client->properties.operation = (uint16_t) operation;

    xcb_grab_pointer(connection,
            0,
            root,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            XCB_NONE,
            event_time);
    xcb_flush(connection);
}


/* Begin a drag operation for an icon window */
void drag_start_icon(xcb_connection_t *connection,
        xcb_window_t root,
        client_td *client,
        int32_t icon_x,
        int32_t icon_y,
        xcb_timestamp_t event_time,
        int16_t root_x,
        int16_t root_y)
{
    if (connection == NULL || client == NULL) {
        return;
    }

    s_drag.active = true;
    s_drag.client = client;
    s_drag.drag_window = client->icon_window;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.pointer_start_x = root_x;
    s_drag.pointer_start_y = root_y;
    s_drag.client_start_x = icon_x;
    s_drag.client_start_y = icon_y;
    s_drag.client_start_w = 0;
    s_drag.client_start_h = 0;

    client->properties.operation = CLIENT_OPERATION_MOVING;

    xcb_grab_pointer(connection,
            0,
            root,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            XCB_NONE,
            event_time);
    xcb_flush(connection);
}


/* Update the in-progress drag on a motion-notify event */
void drag_update(xcb_connection_t *connection,
        int16_t root_x,
        int16_t root_y)
{
    client_td *client;
    int32_t dx;
    int32_t dy;

    if (connection == NULL || !s_drag.active || s_drag.client == NULL) {
        return;
    }

    client = s_drag.client;
    dx = (int32_t) root_x - (int32_t) s_drag.pointer_start_x;
    dy = (int32_t) root_y - (int32_t) s_drag.pointer_start_y;

    if (s_drag.operation == CLIENT_OPERATION_MOVING &&
            s_drag.drag_window != XCB_WINDOW_NONE &&
            s_drag.drag_window == client->icon_window) {
        int32_t new_x = s_drag.client_start_x + dx;
        int32_t new_y = s_drag.client_start_y + dy;
        uint32_t vals[2];
        vals[0] = (uint32_t) new_x;
        vals[1] = (uint32_t) new_y;
        xcb_configure_window(connection, client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
        xcb_flush(connection);
    } else if (s_drag.operation == CLIENT_OPERATION_MOVING) {
        int32_t new_x = s_drag.client_start_x + dx;
        int32_t new_y = s_drag.client_start_y + dy;

        /* Snap to screen edges when within snap distance */
        if (s_drag.snap > 0 &&
                s_drag.screen_w > 0 && s_drag.screen_h > 0) {
            uint32_t snap = s_drag.snap;
            uint32_t fw = (uint32_t) s_drag.client_start_w;
            uint32_t fh = (uint32_t) s_drag.client_start_h;

            /* Left edge */
            if (new_x >= 0 && (uint32_t) new_x <= snap) {
                new_x = 0;
            }

            /* Top edge */
            if (new_y >= 0 && (uint32_t) new_y <= snap) {
                new_y = 0;
            }

            /* Right edge */
            if (new_x >= 0 &&
                    (uint32_t) new_x + fw <= s_drag.screen_w &&
                    (uint32_t) new_x + fw >=
                    s_drag.screen_w - snap) {
                new_x = (int32_t) (s_drag.screen_w - fw);
            }

            /* Bottom edge */
            if (new_y >= 0 &&
                    (uint32_t) new_y + fh <= s_drag.screen_h &&
                    (uint32_t) new_y + fh >=
                    s_drag.screen_h - snap) {
                new_y = (int32_t) (s_drag.screen_h - fh);
            }
        }

        (void) client_send_event_move(client, new_x, new_y);

    } else if (s_drag.operation == CLIENT_OPERATION_RESIZING) {
        int32_t new_w = (int32_t) s_drag.client_start_w + dx;
        int32_t new_h = (int32_t) s_drag.client_start_h + dy;
        (void) client_send_event_resize(client,
                geom_clamp_dim(new_w),
                geom_clamp_dim(new_h));
    }
}


/* Finish the drag on a button-release event */
void drag_end(xcb_connection_t *connection,
        surface_td *surface,
        desktop_td *desktop,
        int16_t root_x,
        int16_t root_y)
{
    if (!s_drag.active) {
        return;
    }

    if (s_drag.client != NULL) {
        if (s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window == s_drag.client->icon_window) {
            int32_t dx = (int32_t) root_x -
                (int32_t) s_drag.pointer_start_x;
            int32_t dy = (int32_t) root_y -
                (int32_t) s_drag.pointer_start_y;

            if (dx * dx + dy * dy < WM_ICON_DRAG_THRESHOLD) {
                /* Treat as a click: restore and focus */
                client_td *ic = s_drag.client;
                (void) client_send_event_restore(ic);
                if (surface != NULL && desktop != NULL) {
                    focus_apply(NULL, surface, desktop, ic, true, NULL);
                }
            } else {
                s_drag.client->icon_x =
                    (int16_t) (s_drag.client_start_x + dx);
                s_drag.client->icon_y =
                    (int16_t) (s_drag.client_start_y + dy);
            }
        }
        s_drag.client->properties.operation = CLIENT_OPERATION_IDLE;
    }

    s_drag.active = false;
    s_drag.operation = CLIENT_OPERATION_IDLE;
    s_drag.client = NULL;
    s_drag.drag_window = XCB_WINDOW_NONE;

    if (connection != NULL) {
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        xcb_flush(connection);
    }
}


/* Cancel an in-progress drag when the dragged client disappears */
void drag_cancel(xcb_connection_t *connection, const client_td *client)
{
    if (!s_drag.active || s_drag.client != client) {
        return;
    }

    s_drag.active = false;
    s_drag.operation = CLIENT_OPERATION_IDLE;
    s_drag.client = NULL;
    s_drag.drag_window = XCB_WINDOW_NONE;

    if (connection != NULL) {
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        xcb_flush(connection);
    }
}


/* Query whether a drag operation is currently active */
bool drag_is_active(void)
{
    return s_drag.active;
}


/* Return the client currently being dragged, or NULL */
client_td *drag_client(void)
{
    return s_drag.client;
}
