/**
 * @file client.c
 *
 * @brief Client (window) management implementation
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
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy, strlen */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>
#include <utils/safeflg.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <config.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <priority.h>


/**
 * @brief Retrieve the @c WM_NAME property of a window
 *
 * Attempts to fetch the @c WM_NAME atom from the specified window to
 * retrieve the client's human-readable name from the X server.
 *
 * @param connection Pointer to the XCB connection
 * @param window     Window ID to query
 * @param buffer     Destination buffer for the name
 * @param buffer_sz  Size of the destination buffer
 *
 * @return Length of name retrieved on success, 0 otherwise
 *
 * @note Complexity: @e O(1)
 */
static size_t s_client_get_wm_name(xcb_connection_t *connection,
        xcb_window_t window, char *buffer, size_t buffer_sz)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    size_t name_len = 0;

    if (buffer == NULL || buffer_sz == 0) {
        return 0;
    }

    cookie = xcb_get_property(connection, 0, window,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(connection, cookie, NULL);

    if (reply != NULL && reply->value_len > 0) {
        name_len = (reply->value_len < buffer_sz - 1) ?
            reply->value_len : buffer_sz - 1;
        memcpy(buffer, xcb_get_property_value(reply), name_len);
        buffer[name_len] = '\0';
        free(reply);
    } else {
        buffer[0] = '\0';
        if (reply != NULL) {
            free(reply);
        }
    }

    return name_len;
}


/**
 * @brief Retrieve the @c WM_CLASS property of a window
 *
 * Fetches the @c WM_CLASS atom from the window.  The @c WM_CLASS
 * property consists of two null-terminated strings: instance name and
 * class name, both separated by a null terminator.
 *
 * @param connection Pointer to the XCB connection
 * @param window     Window ID to query
 * @param class_buf  Destination buffer for class name
 * @param class_sz   Size of class buffer
 * @param inst_buf   Destination buffer for instance name (may be @c NULL)
 * @param inst_sz    Size of instance buffer (ignored when @p inst_buf
 *                   is @c NULL)
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the property
 *       value
 */
static int s_client_get_wm_class(xcb_connection_t *connection,
        xcb_window_t window, char *class_buf, size_t class_sz,
        char *inst_buf, size_t inst_sz)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    char *value;
    size_t value_len;
    size_t inst_len;
    size_t copy_len;

    if (class_buf == NULL || class_sz == 0) {
        return -1;
    }

    cookie = xcb_get_property(connection, 0, window,
            XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(connection, cookie, NULL);

    class_buf[0] = '\0';
    if (inst_buf != NULL) {
        inst_buf[0] = '\0';
    }

    if (reply != NULL && reply->value_len > 0) {
        value = (char *) xcb_get_property_value(reply);
        value_len = reply->value_len;

        /* 'WM_CLASS' format: "instance\0class\0"
         * Find the first null terminator to separate the two strings */
        inst_len = 0;
        for (size_t i = 0; i < value_len; ++i) {
            if (value[i] == '\0') {
                inst_len = i;
                break;
            }
        }

        /* Copy instance name to the destination buffer if provided and
         * if there is data to copy */
        if (inst_buf != NULL && inst_len > 0) {
            copy_len = (inst_len < inst_sz - 1) ?
                inst_len : inst_sz - 1;
            memcpy(inst_buf, value, copy_len);
            inst_buf[copy_len] = '\0';
        }

        /* Copy class name to the destination buffer.
         * Class starts after instance_name + 1 (skip null terminator) */
        if (inst_len + 1 < value_len) {
            size_t class_len = value_len - inst_len - 1;
            copy_len = (class_len < class_sz - 1) ?
                class_len : class_sz - 1;
            memcpy(class_buf, value + inst_len + 1, copy_len);
            class_buf[copy_len] = '\0';
        }

        free(reply);
        return 0;
    }

    if (reply != NULL) {
        free(reply);
    }

    return -1;
}


/**
 * @brief Initialize a new client with the specified parameters
 *
 * Allocates and initializes a new client structure, retrieving its
 * properties from the X server (@c WM_NAME, @c WM_CLASS) and creating
 * an XCB window with the given dimensions and position.  The client is
 * initialized in a hidden state.
 *
 * @param connection Pointer to the XCB connection
 * @param ewmh       Pointer to EWMH connection
 * @param parent_id  Parent window ID (root window of screen)
 * @param w          Width of the client in pixels
 * @param h          Height of the client in pixels
 * @param x          X-coordinate of the client position
 * @param y          Y-coordinate of the client position
 * @param theme      Pointer to the theme configuration
 *
 * @return A pointer to the newly created client structure, or @c NULL
 *         on failure
 *
 * @note Complexity: @e O(1)
 */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        xcb_window_t parent_id,
        uint32_t w, uint32_t h, int32_t x, int32_t y,
        struct config_theme_s *theme)
{
    client_td *client;
    xcb_void_cookie_t create_cookie;
    xcb_generic_error_t *err;
    uint32_t mask;
    uint32_t values[3];
    char wm_name[256];
    char wm_class[256];
    char wm_instance[256];

    LOGGER_TRACE("Initializing client at (%d, %d)" \
            " with size %ux%u",
            x, y, w, h);

    /* Allocate memory for the client structure and verify the
     * allocation was successful before proceeding */
    client = malloc(sizeof(client_td));
    if (client == NULL) {
        LOGGER_ERROR("Failed to allocate memory for client", L_NARG);
        return NULL;
    }

    /* Initialize basic client properties and connections */
    client->connection = connection;
    client->ewmh = ewmh;
    client->parent_id = parent_id;
    client->user_time = 0;
    client->theme = theme;
    client->process.pid = -1;
    client->process.command = NULL;

    /* Set the current geometry, and the "old" as the current one.
     * This allows for saved state when resizing or maximizing */
    client->layout.geometry.cur =
        (struct geometry_s) {.pos = {.x = x, .y = y},
                             .dim = {.w = w, .h = h}};
    client->layout.geometry.old = client->layout.geometry.cur;

    /* Initialize strut and frame extents to zero.
     * These are used for panel and decoration management */
    client->layout.strut_partial.sides =
        (struct sides_s) {0, 0, 0, 0};
    client->layout.strut_partial.start =
        (struct sides_s) {0, 0, 0, 0};
    client->layout.strut_partial.end =
        (struct sides_s) {0, 0, 0, 0};
    client->layout.frame_extents =
        (struct sides_s) {0, 0, 0, 0};

    /* Initialize client properties with sensible defaults */
    client->properties.flags =
        CLIENT_FLAG_HIDDEN | CLIENT_FLAG_FOCUSABLE |
        CLIENT_FLAG_RESIZABLE;
    client->properties.type = CLIENT_TYPE_NORMAL;
    client->properties.state = CLIENT_STATE_NORMAL;
    client->properties.layer = CLIENT_LAYER_NORMAL;
    client->properties.operation = CLIENT_OPERATION_IDLE;
    client->properties.focusing = CLIENT_FOCUSING_UNFOCUSED;
    client->properties.gravity = CLIENT_GRAVITY_NORTH_WEST;

    /* Allocate buffers for client information strings.
     * Each buffer is separately allocated for independent management */
    client->info.name = malloc(256);
    if (client->info.name == NULL) {
        LOGGER_ERROR("Failed to allocate 'name' buffer", L_NARG);
        free(client);
        return NULL;
    }

    client->info.visible_name = malloc(256);
    if (client->info.visible_name == NULL) {
        LOGGER_ERROR("Failed to allocate 'visible_name' buffer", L_NARG);
        free(client->info.name);
        free(client);
        return NULL;
    }

    client->info.role_name = malloc(256);
    if (client->info.role_name == NULL) {
        LOGGER_ERROR("Failed to allocate 'role_name' buffer", L_NARG);
        free(client->info.visible_name);
        free(client->info.name);
        free(client);
        return NULL;
    }

    /* Allocate buffers for window class information */
    client->info.class_name[0] = malloc(256);
    client->info.class_name[1] = malloc(256);
    if (client->info.class_name[0] == NULL ||
            client->info.class_name[1] == NULL) {
        LOGGER_ERROR("Failed to allocate 'class_name' buffers", L_NARG);
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        free(client);
        return NULL;
    }

    /* Allocate buffers for icon information */
    client->icon_info.icon_name = malloc(256);
    client->icon_info.visible_icon_name = malloc(256);
    if (client->icon_info.icon_name == NULL ||
            client->icon_info.visible_icon_name == NULL) {
        LOGGER_ERROR("Failed to allocate icon buffers", L_NARG);
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        safe_free((void **) &client->icon_info.icon_name);
        safe_free((void **) &client->icon_info.visible_icon_name);
        free(client);
        return NULL;
    }

    client->icon_info.icons = NULL;

    /* Initialize all strings with default empty values */
    snprintf(client->info.name, 255, "Client %p", (void *) client);
    snprintf(client->info.visible_name, 255, "Client %p", (void *) client);
    snprintf(client->info.role_name, 255, "");
    snprintf(client->info.class_name[0], 255, "");
    snprintf(client->info.class_name[1], 255, "");
    snprintf(client->icon_info.icon_name, 255, "");
    snprintf(client->icon_info.visible_icon_name, 255, "");

    /* Create the XCB window that represents this client */
    client->window = xcb_generate_id(connection);

    /* Set window attributes using values from the theme configuration.
     * Background pixel, border pixel, and event mask are theme-aware */
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
           XCB_CW_EVENT_MASK;
    values[0] = theme->window.inactive.background_color;
    values[1] = theme->window.inactive.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_PROPERTY_CHANGE |
                XCB_EVENT_MASK_ENTER_WINDOW |
                XCB_EVENT_MASK_LEAVE_WINDOW |
                XCB_EVENT_MASK_FOCUS_CHANGE;

    /* Create the window with checked cookie to detect immediate errors */
    create_cookie = xcb_create_window_checked(
            connection,
            XCB_COPY_FROM_PARENT,
            client->window,
            parent_id,
            (int16_t) x, (int16_t) y,
            (uint16_t) w, (uint16_t) h,
            (uint16_t) theme->window.general.border_width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    err = xcb_request_check(connection, create_cookie);
    if (err != NULL) {
        LOGGER_ERROR("Failed to create X client window", L_NARG);
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        safe_free((void **) &client->icon_info.icon_name);
        safe_free((void **) &client->icon_info.visible_icon_name);
        free(err);
        free(client);
        return NULL;
    }

    /* Generate unique client identifier based on the address of the
     * allocated client structure */
    client->id = (xcb_window_t) (uintptr_t) client;

    /* Attempt to retrieve 'WM_NAME' property from the X server to
     * populate the client's name field instead of using a default */
    s_client_get_wm_name(connection, parent_id, wm_name,
            sizeof(wm_name));
    if (wm_name[0] != '\0') {
        safe_strncpy(client->info.name, wm_name, 255);
        safe_strncpy(client->info.visible_name, wm_name, 255);
    }

    /* Attempt to retrieve 'WM_CLASS' property from the X server to
     * populate the client's class and instance names */
    s_client_get_wm_class(connection, parent_id,
            wm_class, sizeof(wm_class),
            wm_instance, sizeof(wm_instance));
    if (wm_class[0] != '\0') {
        safe_strncpy(client->info.class_name[1], wm_class, 255);
    }
    if (wm_instance[0] != '\0') {
        safe_strncpy(client->info.class_name[0], wm_instance, 255);
    }

    /*
     * Map the window to make it visible on the screen
     * and flush the output buffer to ensure the request is sent
     */
    xcb_map_window(connection, client->window);
    xcb_flush(connection);

    LOGGER_TRACE("Created X window %#x for client %p" \
            " with name '%s'",
            client->window, (void *) client, client->info.name);

    return client;
}


/**
 * @brief Destroy the specified client and free associated resources
 *
 * Deallocates all memory associated with the client, including the
 * XCB window, all string buffers, and the client structure itself.
 *
 * @param client Pointer to the client structure to be destroyed
 *
 * @note Complexity: @e O(1)
 */
void client_destroy(client_td *client)
{
    if (client == NULL) {
        return;
    }

    LOGGER_DEBUG("Destroying client %p (window %#x, name '%s')",
            (void *) client, client->window, client->info.name);

    /* Destroy the XCB window representation and flush the output buffer
     * to ensure the request is processed */
    if (client->connection != NULL && client->window != 0) {
        xcb_destroy_window(client->connection, client->window);
        xcb_flush(client->connection);
    }

    /* Free all allocated string buffers */
    safe_free((void **) &client->info.name);
    safe_free((void **) &client->info.visible_name);
    safe_free((void **) &client->info.role_name);
    safe_free((void **) &client->info.class_name[0]);
    safe_free((void **) &client->info.class_name[1]);
    safe_free((void **) &client->icon_info.icon_name);
    safe_free((void **) &client->icon_info.visible_icon_name);
    safe_free((void **) &client->icon_info.icons);
    safe_free((void **) &client->process.command);

    /* Free the client structure itself */
    free(client);
}


/**
 * @brief Update the content of the specified client
 *
 * Performs a soft update on the client by refreshing its internal state
 * as needed.  This may include checking for property changes and
 * synchronizing the visual state with the internal representation.
 *
 * @param client Pointer to the client to be updated
 *
 * @note Complexity: @e O(1)
 */
void client_update(client_td *client)
{
    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Updated client %p (window %#x)",
            (void *) client, client->window);
}


/**
 * @brief Generic event sender for a client
 *
 * Creates and sends an event for a specified action on a client.  The
 * event is inserted into the event priority queue for processing by the
 * main event handler.
 *
 * @param client        Pointer to the target client
 * @param action_client Action to be performed on the client
 * @param priority      Priority level of the action
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events
 *       in the priority queue
 */
int client_send_event(client_td *client,
        enum action_client_e action_client, enum priority_e priority)
{
    event_td *event;
    action_td action;

    if (client == NULL) {
        LOGGER_ERROR("Received NULL client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Sending event to client %p: action %d" \
            ", priority %d",
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
 * @brief Send an event to rename a specified client
 *
 * Creates an event to update the name of the client.  The new name is
 * passed as part of the event data for processing by the handler.
 *
 * @param client   Pointer to the client to be renamed
 * @param new_name New name for the client
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_rename(client_td *client, const char *new_name)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || new_name == NULL) {
        LOGGER_ERROR("Received NULL pointer", L_NARG);
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


/**
 * @brief Send an event to change the class of a specified client
 *
 * Creates an event to update the class of the client.  The new class is
 * passed as part of the event data for processing by the handler.
 *
 * @param client    Pointer to the client to be reclassified
 * @param new_class New class for the client
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_reclass(client_td *client, const char *new_class)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || new_class == NULL) {
        LOGGER_ERROR("Received NULL pointer", L_NARG);
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


/**
 * @brief Send event to move the specified client to given coordinates
 *
 * Creates an event to move the client to the specified @e (x, y)
 * position.  The new coordinates are passed as part of the event data.
 *
 * @param client Pointer to the client to be moved
 * @param new_x  New @e x coordinate for the client
 * @param new_y  New @e y coordinate for the client
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_move(client_td *client,
        int32_t new_x, int32_t new_y)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;
    uint32_t values[2];

    if (client == NULL) {
        LOGGER_ERROR("Received NULL client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Moving client %p to (%d, %d)",
            (void *) client, new_x, new_y);

    /* Update client's internal geometry */
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;

    /* Configure the XCB window immediately */
    values[0] = (uint32_t) new_x;
    values[1] = (uint32_t) new_y;

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(client->connection);

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


/**
 * @brief Send an event to resize the specified client
 *
 * Creates an event to resize the client to the specified dimensions.
 * The new width and height are passed as part of the event data.
 *
 * @param client Pointer to the client to be resized
 * @param new_w  New width for the client
 * @param new_h  New height for the client
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_resize(client_td *client,
        uint32_t new_w, uint32_t new_h)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;
    uint32_t values[2];

    if (client == NULL) {
        LOGGER_ERROR("Received NULL client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Resizing client %p to %ux%u",
            (void *) client, new_w, new_h);

    /* Update client's internal geometry */
    client->layout.geometry.cur.dim.w = new_w;
    client->layout.geometry.cur.dim.h = new_h;

    /* Configure the XCB window immediately */
    values[0] = new_w;
    values[1] = new_h;

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            values);
    xcb_flush(client->connection);

    /* Create and queue the event for the action handler */
    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RESIZE;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.geometry.dim.w = new_w;
    data->new_data.geometry.dim.h = new_h;

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/**
 * @brief Send an event to change the icon of a specified client
 *
 * Creates an event to update the icon of the client.  The icon name is
 * passed as part of the event data for processing by the handler.
 *
 * @param client    Pointer to the client for which the icon is to change
 * @param icon_name File path of the new icon
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int client_send_event_set_icon(client_td *client, const char *icon_name)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || icon_name == NULL) {
        LOGGER_ERROR("Received NULL pointer", L_NARG);
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
