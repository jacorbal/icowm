/**
 * @file client/event.c
 *
 * @brief Client event-sender helpers
 *
 * Provides the public @c client_send_event family of functions that
 * construct typed @c event_td objects and enqueue them into the
 * priority event queue.  Separating these from lifecycle code
 * (@c client_init / @c client_manage / @c client_destroy) keeps each
 * module focused on a single concern.
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
#include <utils/safe/safestr.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <priority.h>


/* Generic event sender for a client */
int client_send_event(client_td *client,
        enum action_client_e action_client, enum priority_e priority)
{
    event_td *event;
    action_td action;

    if (client == NULL) {
        LOGGER_ERROR("Received null client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Sending event to client %p: action %d, priority %d",
            (void *) client, action_client, priority);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = action_client;

    event = event_init((void *) client, NULL, action, priority);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event for client", L_NARG);
        return -1;
    }

    return eventq_add(event);
}


/**
 * @brief Enqueue a client action carrying one new string value
 *
 * Shared by @c client_send_event_rename, @c client_send_event_reclass,
 * and @c client_send_event_set_icon below, which only differ in which
 * @c action_client_e to send and the verb their own log message uses.
 *
 * @param client        Target client
 * @param value         New string value; duplicated into the queued
 *                      event's own data
 * @param action_client One of @c ACTION_CLIENT_RENAME/RECLASS/
 *                      SET_ICON
 * @param log_verb      Present-participle verb for the trace log
 *                      message, e.g., @c "Renaming"
 *
 * @return @c 0 on success, @c -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events
 *       in the priority queue
 */
static int s_client_send_str_event(client_td *client, const char *value,
        enum action_client_e action_client, const char *log_verb)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || value == NULL) {
        LOGGER_ERROR("Received null pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("%s client %p to '%s'", log_verb, (void *) client, value);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = action_client;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.str.str0 = safe_strdup(value);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/* Send an event to rename a specified client */
int client_send_event_rename(client_td *client, const char *new_name)
{
    return s_client_send_str_event(client, new_name,
            ACTION_CLIENT_RENAME, "Renaming");
}


/* Send an event to change the class of a specified client */
int client_send_event_reclass(client_td *client, const char *new_class)
{
    return s_client_send_str_event(client, new_class,
            ACTION_CLIENT_RECLASS, "Reclassifying");
}


/* Send event to move the specified client to given coordinates */
int client_send_event_move(client_td *client,
        int32_t new_x, int32_t new_y)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;
    uint32_t values[2];

    if (client == NULL) {
        LOGGER_ERROR("Received null client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Moving client %p to (%d, %d)",
            (void *) client, new_x, new_y);

    /* Update client's internal geometry */
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;

    /* Configure the XCB window immediately */
    /* For decorated clients the outermost positioned window is the
     * frame; moving the inner client window (which is reparented INSIDE
     * the frame) would place it at screen-relative coordinates relative
     * to the frame, making the content appear shifted */
    values[0] = (uint32_t) new_x;
    values[1] = (uint32_t) new_y;

    xcb_configure_window(client->connection,
            (client->frame != 0 && client_is_decorated(client))
                ? client->frame : client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(client->connection);

    if (client->properties.operation == CLIENT_OPERATION_MOVING ||
            client->properties.operation == CLIENT_OPERATION_RESIZING) {
        return 0;
    }

    /* Create and queue the event for the action handler */
    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_MOVE;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.geometry.pos.x = new_x;
    data->new_data.geometry.pos.y = new_y;

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/* Send an event to resize the specified client */
int client_send_event_resize(client_td *client,
        int32_t new_x, int32_t new_y, uint32_t new_w, uint32_t new_h)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;
    uint32_t values[4];
    uint32_t req_w;
    uint32_t req_h;
    uint16_t mask;
    int32_t req_x;
    int32_t req_y;
    int32_t old_x;
    int32_t old_y;
    bool interactive_resize;
    uint32_t fe_l;
    uint32_t fe_r;
    uint32_t fe_t;
    uint32_t fe_b;
    bool w_changed;
    bool h_changed;
    uint32_t snap_w;
    uint32_t snap_h;

    if (client == NULL) {
        LOGGER_ERROR("Received null client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Resizing client %p to %ux%u",
            (void *) client, new_w, new_h);

    old_x = client->layout.geometry.cur.pos.x;
    old_y = client->layout.geometry.cur.pos.y;
    req_x = new_x;
    req_y = new_y;

    /* ICCCM §4.1.2.3: size hints (min/max/increment) are defined for
     * the inner (content) window, not for the WM-added frame.
     * Convert the incoming frame dimensions to inner dimensions before
     * applying the constraints, then convert back so the rest of the
     * function always works in frame space.  For undecorated clients
     * the frame extents are all zero, so the conversion is a no-op.
     *
     * Important: only apply increment snapping to the axis that
     * actually changed relative to the current stored size.  Applying
     * increments to the unchanged axis would snap an off-grid dimension
     * (set by the application itself via 'ConfigureRequest') down to
     * the nearest lower multiple, causing the window to shrink on every
     * keypress that only moves the other edge (e.g., pressing
     * 'RESIZE_RIGHT' would shrink the height if program's height is not
     * on the WM's grid). */
    fe_l = (uint32_t) client->layout.frame_extents.left;
    fe_r = (uint32_t) client->layout.frame_extents.right;
    fe_t = (uint32_t) client->layout.frame_extents.top;
    fe_b = (uint32_t) client->layout.frame_extents.bottom;
    req_w = (new_w > fe_l + fe_r) ? new_w - fe_l - fe_r : 0u;
    req_h = (new_h > fe_t + fe_b) ? new_h - fe_t - fe_b : 0u;
    client_constrain_size(client, &req_w, &req_h);

    w_changed = (new_w != client->layout.geometry.cur.dim.w);
    h_changed = (new_h != client->layout.geometry.cur.dim.h);
    snap_w = req_w;
    snap_h = req_h;
    client_constrain_size(client, &snap_w, &snap_h);
    if (w_changed) {
        req_w = snap_w;
    }
    if (h_changed) {
        req_h = snap_h;
    }

    req_w += fe_l + fe_r;
    req_h += fe_t + fe_b;

    interactive_resize =
        client->properties.operation == CLIENT_OPERATION_RESIZING;

    if (interactive_resize) {
        /* During interactive mouse resizing, apply the geometry
         * immediately so the user sees live feedback */
        client->layout.geometry.cur.pos.x = req_x;
        client->layout.geometry.cur.pos.y = req_y;
        client->layout.geometry.cur.dim.w = req_w;
        client->layout.geometry.cur.dim.h = req_h;
        mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[0] = req_w;
        values[1] = req_h;
        if (req_x != old_x || req_y != old_y) {
            mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
            values[0] = (uint32_t) req_x;
            values[1] = (uint32_t) req_y;
            values[2] = req_w;
            values[3] = req_h;
        }
        xcb_configure_window(client->connection,
                (client->frame != 0 && client_is_decorated(client))
                ? client->frame : client->window,
                mask,
                values);
        client_sync_decoration_layout(client);

        /* For undecorated clients 'client_sync_decoration_layout' is
         * a no-op, so the content window does not receive
         * a 'xcb_clear_area' call there.  Force a repaint with
         * 'exposures=1' so the X server generates an 'Expose' event and
         * the client redraws the newly exposed region immediately
         * rather than leaving stale content until the next
         * user-triggered event. */
        if (client->frame == 0 || !client_is_decorated(client)) {
            xcb_clear_area(client->connection, 1,
                    client->window, 0, 0, 0, 0);
        }

        /* Send a synthetic 'ConfigureNotify' so applications that use
         * size increments can recompute their internal layout during
         * live mouse-resize feedback */
        client_send_synthetic_configure_notify(client->connection,
        client);

        xcb_flush(client->connection);

        return 0;
    }

    /* Create and queue the event for the action handler */
    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RESIZE;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.geometry.pos.x = req_x;
    data->new_data.geometry.pos.y = req_y;
    data->new_data.geometry.dim.w = req_w;
    data->new_data.geometry.dim.h = req_h;

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/* Send an event to change the icon of a specified client */
int client_send_event_set_icon(client_td *client, const char *icon_name)
{
    return s_client_send_str_event(client, icon_name,
            ACTION_CLIENT_SET_ICON, "Setting icon for");
}
