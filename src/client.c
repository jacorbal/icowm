/**
 * @file client.c
 *
 * @brief Window structure implementation
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* NULL, free, malloc */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Type includes */
#include <types/pair.h> /* geometry_s */

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <config.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <priority.h>


/* Initialize a new client */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        xcb_window_t parent_id,
        uint32_t w, uint32_t h, int32_t x, int32_t y,
        struct config_theme_s *theme)
{
    xcb_generic_error_t *err;
    xcb_void_cookie_t cookie;
    xcb_screen_t *screen =
        xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    client_td *client;

    client = malloc(sizeof(client_td));
    if (client == NULL) {
        LOGGER_ERROR("Failed to allocate memory for new client",
                L_NARG);
        return NULL;
    }

    client->connection = connection;
    client->parent_id = parent_id;
    client->user_time = 0;

    /* Set the current geometry, and the "old" as the current one */
    client->layout.geometry.cur =
        (struct geometry_s) {.pos = {.x = x, .y = y},
                             .dim = {.w = w, .h = h}};
    client->layout.geometry.old = client->layout.geometry.cur;

    // TODO: Strut & Frame extents

    // TODO: Test this...
    client->properties.flags =
        CLIENT_FLAG_HIDDEN | CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_RESIZABLE;
    client->properties.type = CLIENT_TYPE_NORMAL;
    client->properties.state = CLIENT_STATE_NORMAL;
    client->properties.layer = CLIENT_LAYER_NORMAL;
    client->properties.operation = CLIENT_OPERATION_IDLE;
    client->properties.focusing = CLIENT_FOCUSING_FOCUSED;

    client->process.pid = -1;
    client->process.command = NULL;

    client->theme = theme;

    client->info.name = NULL;              // <-- TODO
    client->info.visible_name = NULL;      // <-- TODO
    client->info.role_name = NULL;         // <-- TODO
    client->info.class_name[0] = NULL;        // <-- TODO
    client->info.class_name[1] = NULL;        // <-- TODO
    client->icon_info.icon_name = NULL;         // <-- TODO
    client->icon_info.visible_icon_name = NULL; // <-- TODO

    /* Create the X client */
    client->window = xcb_generate_id(connection);
    cookie = xcb_create_window(
            connection,
            XCB_COPY_FROM_PARENT,
            client->window,
            parent_id,
            (int16_t) x, (int16_t) y,
            (uint16_t) w, (uint16_t) h,
            (uint16_t) theme->window.general.border_width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            screen->root_visual,
            XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
            (uint32_t[]) {
                screen->white_pixel,
                XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS});

    err = xcb_request_check(connection, cookie);
    if (err) {
        LOGGER_ERROR("Failed to create client", L_NARG);
        free(client);
        return NULL;
    }

    // TODO: This should go in 'client_send_event_create'
    /* Configure client */
    /*
    xcb_icccm_set_wm_name(connection, client->window,
            strlen("Ventana"), "Ventana");
    xcb_icccm_set_wm_class(connection, client->window,
            strlen("my_client"), "my_client",
            strlen("my_class"), "my_class");
    */

    xcb_flush(connection);

    return client;
}


/* Destroy the client and free used memory */
void client_destroy(client_td *client)
{
    LOGGER_TRACE("Deallocating structure for client %#lx ('%s')",
            client->id, client->info.name);
    if (client) {
        if (client->window) {
            xcb_destroy_window(client->connection, client->window);
        }
        free(client);
    }
}


/* Update the content of the client */
void client_update(client_td *client)
{
    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Updating client %#lx ('%s')",
            client->id, client->info.name);

    /* Clear the client */
    xcb_clear_area(client->connection,
            0, client->window,
            0, 0,
            0, 0);

    /* Draw or update the content over the client */
    //  e.g.: draw_content(client);

    /* Optionally, you might want to flush the output buffer */
    //XFlush(client->display);
}


/* Generic client event sending */
int client_send_event(client_td *client,
        enum action_client_e action_client, enum priority_e priority)
{
    event_td *event;
    action_td action;

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = action_client;

    event = event_init((void *) client, NULL, action, priority);

    return eventq_add(event);
}


/* Send event to rename a client */
int client_send_event_rename(client_td *client, const char *new_name)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RENAME;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_client_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_client_init(client, action.object.client);
    data->new_data.str.str0 = safe_strdup(new_name);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);

    return eventq_add(event);
}


/* Send event to change class of a client */
int client_send_event_reclass(client_td *client, const char *new_class)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RECLASS;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_client_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_client_init(client, action.object.client);
    data->new_data.str.str0 = safe_strdup(new_class);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);

    return eventq_add(event);
}


/* Send event to move a client to a new position */
int client_send_event_move(client_td *client,
        int32_t new_x, int32_t new_y)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_MOVE;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_client_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_client_init(client, action.object.client);

    data->new_data.geometry.pos.x = new_x;
    data->new_data.geometry.pos.y = new_y;

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);

    return eventq_add(event);
}


/* Send envent to resize a client to a new position */
int client_send_event_resize(client_td *client,
        uint32_t new_w, uint32_t new_h)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RESIZE;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_client_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_client_init(client, action.object.client);

    data->new_data.geometry.dim.w = new_w;
    data->new_data.geometry.dim.h = new_h;

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);

    return eventq_add(event);
}


/* Send the event to change client icon */
int client_send_event_set_icon(client_td *client, const char *icon_name)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_SET_ICON;

    /* NOTE: Memory for the action data structure 'data' must be freed
     *       with 'action_data_client_destroy' once the event has
     *       finished processing this action to avoid memory leaks */
    data = action_data_client_init(client, action.object.client);
    data->new_data.str.str0 = safe_strdup(icon_name);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);

    return eventq_add(event);
}
