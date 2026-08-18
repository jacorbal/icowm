/**
 * @file enact/client.c
 *
 * @brief Every client-level action this window manager can carry
 *        out, one typed function per action
 *
 * Split out of what used to be a single, flat @c enact.c; see
 * @c enact/internal.h for why.
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
#include <stddef.h>
#include <stdint.h>

/* Project includes */
#include <client.h>
#include <logger.h>
#include <scratchpad.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Command includes */
#include <cmds/client/basic.h>
#include <cmds/client/geom.h>
#include <cmds/client/layer.h>
#include <cmds/client/meta.h>

/* Local includes */
#include <enact.h>
#include <enact/internal.h>


/* Broadcast an IPC event carrying one client's own identifying
 * fields */
void enact_broadcast_client_event(client_td *client, uint32_t type)
{
    cJSON *fields;

    if (client == NULL) {
        return;
    }

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "client_id",
                (double) client->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) client->desktop_id);
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) client->screen_id);
    }
    ipc_broadcast_event(type, fields);
}


/* 'action_client_e' */

void enact_client_close(client_td *client)
{
    ccmd_client_close(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Forcibly kill the client's owning connection */
void enact_client_kill(client_td *client)
{
    ccmd_client_kill(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Restore the client to its normal state */
void enact_client_restore(client_td *client)
{
    ccmd_client_restore(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_DEICONIFIED);
    }
}


/* Give input focus to the client */
void enact_client_focus(client_td *client)
{
    ccmd_client_focus(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Take input focus away from the client */
void enact_client_unfocus(client_td *client)
{
    ccmd_client_unfocus(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        if (scratchpad_is_client(client) && !client_is_hidden(client)) {
            enact_client_hide(client);
        }
    }
}


/* Resize the client to a specific frame geometry */
void enact_client_resize(client_td *client, int32_t x, int32_t y,
        uint32_t w, uint32_t h)
{
    ccmd_client_resize(client, x, y, w, h);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Move the client to a specific position */
void enact_client_move(client_td *client, int32_t x, int32_t y)
{
    ccmd_client_move(client, x, y);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Center the client on its current screen */
void enact_client_center(client_td *client)
{
    ccmd_client_center(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Move the client to the next monitor on its surface */
void enact_client_move_next_monitor(client_td *client)
{
    ccmd_client_move_to_next_monitor(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Move the client to a specific monitor index */
void enact_client_move_to_monitor(client_td *client,
        uint32_t monitor_index)
{
    ccmd_client_move_to_monitor(client, monitor_index);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Change the client's 'WM_CLASS' class and instance names */
void enact_client_reclass(client_td *client, const char *class_name,
        const char *instance_name)
{
    ccmd_client_reclass(client, class_name, instance_name);
    if (client != NULL) {
        cJSON *fields;

        xcb_flush(client->connection);

        fields = cJSON_CreateObject();
        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) client->desktop_id);
            cJSON_AddNumberToObject(fields, "surface_id",
                    (double) client->screen_id);
            cJSON_AddStringToObject(fields, "class_name",
                    (class_name != NULL) ? class_name : "");
            cJSON_AddStringToObject(fields, "instance_name",
                    (instance_name != NULL) ? instance_name : "");
        }
        ipc_broadcast_event(IPC_EVENT_CLIENT_RECLASSED, fields);
    }
}


/* Change the client's 'WM_WINDOW_ROLE' */
void enact_client_rerole(client_td *client, const char *role)
{
    ccmd_client_rerole(client, role);
    if (client != NULL) {
        cJSON *fields;

        xcb_flush(client->connection);

        fields = cJSON_CreateObject();
        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) client->desktop_id);
            cJSON_AddNumberToObject(fields, "surface_id",
                    (double) client->screen_id);
            cJSON_AddStringToObject(fields, "role",
                    (role != NULL) ? role : "");
        }
        ipc_broadcast_event(IPC_EVENT_CLIENT_REROLED, fields);
    }
}


/* Rename the client's window title */
void enact_client_rename(client_td *client, const char *name)
{
    ccmd_client_rename(client, name);
    if (client != NULL) {
        cJSON *fields;

        xcb_flush(client->connection);

        fields = cJSON_CreateObject();
        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) client->desktop_id);
            cJSON_AddNumberToObject(fields, "surface_id",
                    (double) client->screen_id);
            cJSON_AddStringToObject(fields, "name",
                    (name != NULL) ? name : "");
        }
        ipc_broadcast_event(IPC_EVENT_CLIENT_RENAMED, fields);
    }
}


/* Maximize the client both horizontally and vertically */
void enact_client_maximize(client_td *client)
{
    ccmd_client_maximize(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Maximize the client horizontally only */
void enact_client_maximize_horz(client_td *client)
{
    ccmd_client_maximize_horz(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Maximize the client vertically only */
void enact_client_maximize_vert(client_td *client)
{
    ccmd_client_maximize_vert(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Iconify the client */
void enact_client_iconify(client_td *client)
{
    ccmd_client_iconify(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_ICONIFIED);
    }
}


/* Hide the client's window */
void enact_client_hide(client_td *client)
{
    ccmd_client_hide(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_HIDE_SET);
    }
}


/* Show a previously hidden client */
void enact_client_unhide(client_td *client)
{
    ccmd_client_unhide(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_HIDE_CLEARED);
    }
}


/* Shade (roll up) the client */
void enact_client_shade(client_td *client)
{
    ccmd_client_shade(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_SHADE_SET);
    }
}


/* Unshade (roll down) the client */
void enact_client_unshade(client_td *client)
{
    ccmd_client_unshade(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_SHADE_CLEARED);
    }
}


/* Toggle the client's shade status */
void enact_client_toggle_shade(client_td *client)
{
    ccmd_client_toggle_shade(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, client_is_shaded(client)
                ? IPC_EVENT_SHADE_SET : IPC_EVENT_SHADE_CLEARED);
    }
}


/* Set the client's pin mode */
void enact_client_pin(client_td *client)
{
    ccmd_client_pin(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_PIN_SET);
    }
}


/* Remove the client's pin mode */
void enact_client_unpin(client_td *client)
{
    ccmd_client_unpin(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_PIN_CLEARED);
    }
}


/* Toggle the client's pin mode */
void enact_client_toggle_pin(client_td *client)
{
    ccmd_client_toggle_pin(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, client_is_pinned(client)
                ? IPC_EVENT_PIN_SET
                : IPC_EVENT_PIN_CLEARED);
    }
}


/* Set the client to full screen mode */
void enact_client_fullscreen(client_td *client)
{
    ccmd_client_fullscreen(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_FULLSCREEN_SET);
    }
}


/* Remove the client's full screen mode */
void enact_client_unfullscreen(client_td *client)
{
    ccmd_client_unfullscreen(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_FULLSCREEN_CLEARED);
    }
}


/* Toggle the client's full screen mode */
void enact_client_toggle_fullscreen(client_td *client)
{
    ccmd_client_toggle_fullscreen(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, client_is_fullscreen(client)
                ? IPC_EVENT_FULLSCREEN_SET
                : IPC_EVENT_FULLSCREEN_CLEARED);
    }
}


/* Raise the client to the top of its layer */
void enact_client_raise(client_td *client)
{
    ccmd_client_raise(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
    }
}


/* Lower the client to the bottom of its layer */
void enact_client_lower(client_td *client)
{
    ccmd_client_lower(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
    }
}


/* Move the client to the always-on-top layer */
void enact_client_layer_above(client_td *client)
{
    ccmd_client_layer_above(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Move the client to the normal layer */
void enact_client_layer_normal(client_td *client)
{
    ccmd_client_layer_normal(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Move the client to the always-on-bottom layer */
void enact_client_layer_below(client_td *client)
{
    ccmd_client_layer_below(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Cycle the client through the normal/above/below layers */
void enact_client_cycle_layer(client_td *client)
{
    ccmd_client_cycle_layer(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Mark the client as urgent */
void enact_client_urge(client_td *client)
{
    ccmd_client_urge(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Clear the client's urgency level */
void enact_client_unurge(client_td *client)
{
    ccmd_client_unurge(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Set the client's icon name */
void enact_client_set_icon(client_td *client, const char *icon_name)
{
    ccmd_client_set_icon(client, icon_name);
    if (client != NULL) {
        cJSON *fields;

        xcb_flush(client->connection);

        fields = cJSON_CreateObject();
        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) client->desktop_id);
            cJSON_AddNumberToObject(fields, "surface_id",
                    (double) client->screen_id);
            cJSON_AddStringToObject(fields, "icon_name",
                    (icon_name != NULL) ? icon_name : "");
        }
        ipc_broadcast_event(IPC_EVENT_CLIENT_ICON_CHANGED, fields);
    }
}


/* Toggle the client's decoration */
void enact_client_toggle_decorate(client_td *client)
{
    ccmd_client_toggle_decorate(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        enact_broadcast_client_event(client, client_is_decorated(client)
                ? IPC_EVENT_DECORATION_SET
                : IPC_EVENT_DECORATION_CLEARED);
    }
}

