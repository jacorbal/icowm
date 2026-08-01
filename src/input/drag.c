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

/* ADT includes */
#include <adt/cdlist.h>

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
    desktop_td *desktop;
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
    .desktop = NULL,
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


/**
 * @brief Compute the absolute value of a 32-bit signed integer
 *
 * Returns the non-negative magnitude of the given value.
 *
 * @param value Input integer
 *
 * @return Absolute value of @p value
 *
 * @note Complexity: @e O(1)
 */
static int32_t s_drag_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}


/**
 * @brief Select the delta with the smaller absolute magnitude
 *
 * Compares two deltas and returns the one whose absolute value
 * is smaller, preserving its original sign.
 *
 * @param current   Current best delta
 * @param candidate Candidate delta to compare
 *
 * @return The delta with the smaller absolute value
 *
 * @note Complexity: @e O(1)
 */
static int32_t s_drag_closer_delta(int32_t current, int32_t candidate)
{
    if (s_drag_abs_i32(candidate) < s_drag_abs_i32(current)) {
        return candidate;
    }

    return current;
}


/**
 * @brief Check whether two 1-D ranges overlap or are within snap
 *        distance
 *
 * Determines if two intervals either overlap or are closer than a given
 * snapping threshold, allowing near-alignment behavior.
 *
 * @param start_a Start of first range
 * @param end_a   End of first range
 * @param start_b Start of second range
 * @param end_b   End of second range
 * @param snap    Maximum allowed gap for ranges to be considered
 *                "close"
 *
 * @return @c true if ranges overlap or are within @p snap distance,
 *         otherwise @c false
 *
 * @note Complexity: @e O(1)
 */
static bool s_drag_ranges_close(int32_t start_a, int32_t end_a,
        int32_t start_b, int32_t end_b, int32_t snap)
{
    return !(end_a < start_b - snap || end_b < start_a - snap);
}


/**
 * @brief Apply snapping behavior during client movement
 *
 * Adjusts the proposed position of a moving client so it "snaps" to
 * nearby window edges or screen boundaries when within a configurable
 * threshold.  It compares the moving window against other visible,
 * non-iconified clients on the same desktop and computes the smallest
 * adjustment needed to align edges.
 *
 * Snapping is applied independently along both axes and also considers
 * screen edges if available.
 *
 * @param x      Pointer to the proposed X coordinate (updated in place)
 * @param y      Pointer to the proposed Y coordinate (updated in place)
 * @param width  Width of the moving client
 * @param height Height of the moving client
 *
 * @note Requires a valid global @c s_drag context
 * @note Complexity: @e O(n), where @e n is the number of clients in the
 *       stacking list
 */
static void s_drag_snap_move(int32_t *x, int32_t *y,
        uint32_t width, uint32_t height)
{
    int32_t snap;
    int32_t right;
    int32_t bottom;

    if (x == NULL || y == NULL || s_drag.snap == 0) {
        return;
    }

    snap = (int32_t) s_drag.snap;
    right = *x + (int32_t) width;
    bottom = *y + (int32_t) height;

    if (s_drag.desktop != NULL && s_drag.desktop->stacking != NULL &&
            cdlist_size(s_drag.desktop->stacking) > 0) {
        cdlist_item_td *node;
        cdlist_item_td *initial;
        int32_t dx = snap;
        int32_t dy = snap;

        node = cdlist_head(s_drag.desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);

                if (other != NULL && other != s_drag.client &&
                        !client_is_hidden(other) &&
                        !client_is_iconified(other)) {
                    int32_t ox = other->layout.geometry.cur.pos.x;
                    int32_t oy = other->layout.geometry.cur.pos.y;
                    int32_t oright = ox +
                        (int32_t) other->layout.geometry.cur.dim.w;
                    int32_t obottom = oy +
                        (int32_t) other->layout.geometry.cur.dim.h;

                    if (s_drag_ranges_close(*y, bottom, oy,
                                obottom, snap)) {
                        dx = s_drag_closer_delta(dx, oright - *x);
                        dx = s_drag_closer_delta(dx, oright - right);
                        dx = s_drag_closer_delta(dx, ox - right);
                        dx = s_drag_closer_delta(dx, ox - *x);
                    }

                    if (s_drag_ranges_close(*x, right, ox,
                                oright, snap)) {
                        dy = s_drag_closer_delta(dy, obottom - *y);
                        dy = s_drag_closer_delta(dy, obottom - bottom);
                        dy = s_drag_closer_delta(dy, oy - bottom);
                        dy = s_drag_closer_delta(dy, oy - *y);
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }

        if (s_drag_abs_i32(dx) <= snap) {
            *x += dx;
            right += dx;
        }

        if (s_drag_abs_i32(dy) <= snap) {
            *y += dy;
            bottom += dy;
        }
    }

    if (s_drag.screen_w > 0 &&
            s_drag_abs_i32(*x) <= snap) {
        right -= *x;
        *x = 0;
    }

    if (s_drag.screen_h > 0 &&
            s_drag_abs_i32(*y) <= snap) {
        bottom -= *y;
        *y = 0;
    }

    if (s_drag.screen_w > 0 &&
            s_drag_abs_i32(right -
                (int32_t) s_drag.screen_w) <= snap) {
        *x = (int32_t) s_drag.screen_w - (int32_t) width;
    }

    if (s_drag.screen_h > 0 &&
            s_drag_abs_i32(bottom -
                (int32_t) s_drag.screen_h) <= snap) {
        *y = (int32_t) s_drag.screen_h - (int32_t) height;
    }
}


/* Begin a drag operation for a managed client window */
void drag_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
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
    s_drag.desktop = desktop;
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
void drag_start_icon(xcb_connection_t *connection, xcb_window_t root,
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
    s_drag.desktop = NULL;
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


/* Snap a resized client against peer windows and screen edges */
static void s_drag_snap_resize(int32_t x, int32_t y,
        uint32_t *width, uint32_t *height)
{
    int32_t snap;
    int32_t right;
    int32_t bottom;

    if (width == NULL || height == NULL || s_drag.snap == 0) {
        return;
    }

    snap = (int32_t) s_drag.snap;
    right = x + (int32_t) *width;
    bottom = y + (int32_t) *height;

    if (s_drag.desktop != NULL && s_drag.desktop->stacking != NULL &&
            cdlist_size(s_drag.desktop->stacking) > 0) {
        cdlist_item_td *node;
        cdlist_item_td *initial;
        int32_t dw = snap;
        int32_t dh = snap;

        node = cdlist_head(s_drag.desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);

                if (other != NULL && other != s_drag.client &&
                        !client_is_hidden(other) &&
                        !client_is_iconified(other)) {
                    int32_t ox = other->layout.geometry.cur.pos.x;
                    int32_t oy = other->layout.geometry.cur.pos.y;
                    int32_t oright = ox +
                        (int32_t) other->layout.geometry.cur.dim.w;
                    int32_t obottom = oy +
                        (int32_t) other->layout.geometry.cur.dim.h;

                    if (s_drag_ranges_close(y, bottom,
                                oy, obottom, snap)) {
                        dw = s_drag_closer_delta(dw, oright - right);
                        dw = s_drag_closer_delta(dw, ox - right);
                    }

                    if (s_drag_ranges_close(x, right,
                                ox, oright, snap)) {
                        dh = s_drag_closer_delta(dh, obottom - bottom);
                        dh = s_drag_closer_delta(dh, oy - bottom);
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }

        if (s_drag_abs_i32(dw) <= snap) {
            *width = geom_clamp_dim((int32_t) *width + dw);
            right = x + (int32_t) *width;
        }

        if (s_drag_abs_i32(dh) <= snap) {
            *height = geom_clamp_dim((int32_t) *height + dh);
            bottom = y + (int32_t) *height;
        }
    }

    if (s_drag.screen_w > 0 &&
            s_drag_abs_i32(right - (int32_t) s_drag.screen_w) <= snap) {
        *width = geom_clamp_dim((int32_t) s_drag.screen_w - x);
    }

    if (s_drag.screen_h > 0 &&
            s_drag_abs_i32(bottom - (int32_t) s_drag.screen_h) <= snap) {
        *height = geom_clamp_dim((int32_t) s_drag.screen_h - y);
    }
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

        s_drag_snap_move(&new_x, &new_y,
                s_drag.client_start_w, s_drag.client_start_h);

        (void) client_send_event_move(client, new_x, new_y);

    } else if (s_drag.operation == CLIENT_OPERATION_RESIZING) {
        uint32_t new_w =
            geom_clamp_dim((int32_t) s_drag.client_start_w + dx);
        uint32_t new_h =
            geom_clamp_dim((int32_t) s_drag.client_start_h + dy);

        s_drag_snap_resize(s_drag.client_start_x, s_drag.client_start_y,
                &new_w, &new_h);

        (void) client_send_event_resize(client, new_w, new_h);
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
        uint32_t final_w = s_drag.client->layout.geometry.cur.dim.w;
        uint32_t final_h = s_drag.client->layout.geometry.cur.dim.h;
        bool finalize_resize =
            s_drag.operation == CLIENT_OPERATION_RESIZING;

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
        if (finalize_resize) {
            (void) client_send_event_resize(s_drag.client,
                    final_w, final_h);
        }
    }

    s_drag.active = false;
    s_drag.operation = CLIENT_OPERATION_IDLE;
    s_drag.client = NULL;
    s_drag.desktop = NULL;
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
    s_drag.desktop = NULL;
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
