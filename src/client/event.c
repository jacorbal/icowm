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
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safestr.h>

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


/* Send an event to rename a specified client */
int client_send_event_rename(client_td *client, const char *new_name)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || new_name == NULL) {
        LOGGER_ERROR("Received null pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Renaming client %p to '%s'",
            (void *) client, new_name);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RENAME;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.str.str0 = safe_strdup(new_name);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/* Send an event to change the class of a specified client */
int client_send_event_reclass(client_td *client, const char *new_class)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || new_class == NULL) {
        LOGGER_ERROR("Received null pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Reclassifying client %p to '%s'",
            (void *) client, new_class);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RECLASS;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.str.str0 = safe_strdup(new_class);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
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
    /* NOTE: For decorated clients the outermost positioned window is
     *       the frame; moving the inner client window (which is
     *       reparented INSIDE the frame) would place it at
     *       screen-relative coordinates relative to the frame, making
     *       the content appear shifted. */
    values[0] = (uint32_t) new_x;
    values[1] = (uint32_t) new_y;

    xcb_configure_window(client->connection,
            (client->frame != 0 && client_is_decorated(client))
                ? client->frame : client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(client->connection);

    if (client->properties.operation == CLIENT_OPERATION_MOVING) {
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
        uint32_t new_w, uint32_t new_h)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;
    uint32_t values[2];
    uint32_t req_w;
    uint32_t req_h;

    if (client == NULL) {
        LOGGER_ERROR("Received null client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Resizing client %p to %ux%u",
            (void *) client, new_w, new_h);

    req_w = new_w;
    req_h = new_h;
    client_constrain_size(client, &req_w, &req_h);

    /* Update client's internal geometry */
    client->layout.geometry.cur.dim.w = req_w;
    client->layout.geometry.cur.dim.h = req_h;

    /* Configure the XCB window immediately */
    /* NOTE: For decorated clients the frame must be resized; resizing
     *       only the inner window would leave the decoration at the
     *       wrong size */
    values[0] = req_w;
    values[1] = req_h;

    xcb_configure_window(client->connection,
            (client->frame != 0 && client_is_decorated(client))
                ? client->frame : client->window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            values);
    client_sync_decoration_layout(client);
    xcb_flush(client->connection);

    if (client->properties.operation == CLIENT_OPERATION_RESIZING) {
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
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || icon_name == NULL) {
        LOGGER_ERROR("Received null pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Setting icon for client %p to '%s'",
            (void *) client, icon_name);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_SET_ICON;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.str.str0 = safe_strdup(icon_name);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}
