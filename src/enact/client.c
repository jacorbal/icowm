/**
 * @file enact/client.c
 *
 * @brief Every client-level action this window manager can carry
 *        out, one typed function per action
 *
 * One of the files @c enact/ is made of; see
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* Type includes */
#include <types/direction.h>
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <logger.h>
#include <policy/focus.h>
#include <scratchpad.h>
#include <surface.h>
#include <wm.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Command includes */
#include <cmds/client/flags.h>
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/maximize.h>
#include <cmds/client/meta.h>
#include <cmds/client/move.h>
#include <cmds/client/resize.h>
#include <cmds/client/state.h>
#include <cmds/client/visibility.h>

/* Local includes */
#include <enact.h>
#include <enact/internal.h>
#include <utils/xcb/connection.h>


/**
 * @brief Shared logic for carrying the client to another desktop in
 *        a given compass direction, following it there
 *
 * @param client    Client to move
 * @param surfaces  Full surface list, passed through to
 *                  @c focus_apply
 * @param config    Active configuration, passed through to
 *                  @c focus_apply
 * @param direction Compass direction to move the client in
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's top parent's desktop (see
 *       @a enact_desktop_client_send's comment)
 */
static void s_enact_client_send_to_desktop(client_td *client,
        list_td *surfaces, const config_td *config,
        enum compass_direction_e direction)
{
    surface_td *surface;
    const desktop_td *cur_desktop;
    /* Initialized here for the same reason as every other switch in
     * this project that carries no 'default:': the compiler keeps
     * checking the cases cover the enum, and cannot prove that one of
     * them always runs */
    desktop_td *target_desktop = NULL;
    bool cycle;

    if (client == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    if (surface == NULL) {
        return;
    }

    cur_desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (cur_desktop == NULL) {
        return;
    }

    cycle = (surface->config != NULL)
        ? surface->config->desktops.wrap_at_bounds : true;

    /* No different desktop to move to at all: either genuinely
     * only one exists (restricted-memory mode is always locked to
     * exactly one; see 'surface_action_desktop_add''s doc
     * comment, surface/switch.c), wrapping is disabled and this is
     * already the edgemost one that way, or (north/south only, on a
     * surface with no 'topology.screens.desktops' layout configured
     * at all) there is no second row or column to move to in the
     * first place; is a silent no-op, the same as every other
      keybind here that finds nothing to act on. */
    switch (direction) {
    case COMPASS_NORTH:
        target_desktop = surface_desktop_north(surface,
                cur_desktop->id, cycle);
        break;
    case COMPASS_SOUTH:
        target_desktop = surface_desktop_south(surface,
                cur_desktop->id, cycle);
        break;
    case COMPASS_EAST:
        target_desktop = surface_desktop_east(surface,
                cur_desktop->id, cycle);
        break;
    case COMPASS_WEST:
        target_desktop = surface_desktop_west(surface,
                cur_desktop->id, cycle);
        break;
    }
    if (target_desktop == NULL || target_desktop == cur_desktop) {
        return;
    }

    enact_desktop_client_send(cur_desktop, client, target_desktop);
    enact_surface_desktop_switch(surface, target_desktop->id);

    /* 'enact_surface_desktop_switch' just above, via its
     * 'surface_clients_show', already restored real input focus on
     * its, to whichever client this target desktop's
     * 'client_active_id' still remembered from some earlier,
     * unrelated visit, not this client, freshly arrived on it as
     * of the very call before this one.  Explicitly re-applied here,
     * after the fact, rather than trying to somehow suppress that
     * automatic restore instead: 'client' becomes this desktop's
     * newly active one, genuinely focused, and raised above whatever
     * else that restore just raised in front of it (any client
     * already there before this one arrived stays exactly where it
     * was, simply no longer topmost), matching a plain click or any
     * other deliberate focus request landing on it right after the
     * move, not a stale leftover from before. */
    focus_apply(surfaces, surface, target_desktop, client, true, config);
}


/* Broadcast an IPC event carrying one client's identifying
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


/* Close the client */
void enact_client_close(client_td *client)
{
    ccmd_client_close(client);
    if (client != NULL) {
    }
}


/* Forcibly kill the client's owning connection */
void enact_client_kill(client_td *client)
{
    ccmd_client_kill(client);
    if (client != NULL) {
    }
}


/* Restore the client to its normal state */
void enact_client_restore(client_td *client)
{
    ccmd_client_restore(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_DEICONIFIED);
    }
}


/* Give input focus to the client */
void enact_client_focus(client_td *client)
{
    ccmd_client_focus(client);
    if (client != NULL) {
    }
}


/* Take input focus away from the client */
void enact_client_unfocus(client_td *client)
{
    ccmd_client_unfocus(client);
    if (client != NULL) {
        if (scratchpad_is_client(client) && !client_is_hidden(client)) {
            enact_client_hide(client);
        }
    }
}


/* Resize the client to a specific frame geometry */
void enact_client_resize(client_td *client, struct geometry_s geom)
{
    ccmd_client_resize(client, geom);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Resize the client to a specific frame geometry immediately,
 * bypassing any in-flight sync throttling */
void enact_client_resize_force(client_td *client, struct geometry_s geom)
{
    ccmd_client_resize_force(client, geom);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Move the client to a specific position */
void enact_client_move(client_td *client, struct position_s pos)
{
    ccmd_client_move(client, pos);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Center the client on its current screen */
void enact_client_center(client_td *client)
{
    ccmd_client_center(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Carry the client to the desktop north of the current one,
 * following it there */
void enact_client_send_to_desktop_north(client_td *client,
        list_td *surfaces, const config_td *config)
{
    s_enact_client_send_to_desktop(client, surfaces, config,
            COMPASS_NORTH);
}


/* Carry the client to the desktop south of the current one,
 * following it there */
void enact_client_send_to_desktop_south(client_td *client,
        list_td *surfaces, const config_td *config)
{
    s_enact_client_send_to_desktop(client, surfaces, config,
            COMPASS_SOUTH);
}


/* Carry the client to the desktop east of the current one,
 * following it there */
void enact_client_send_to_desktop_east(client_td *client,
        list_td *surfaces, const config_td *config)
{
    s_enact_client_send_to_desktop(client, surfaces, config,
            COMPASS_EAST);
}


/* Carry the client to the desktop west of the current one,
 * following it there */
void enact_client_send_to_desktop_west(client_td *client,
        list_td *surfaces, const config_td *config)
{
    s_enact_client_send_to_desktop(client, surfaces, config,
            COMPASS_WEST);
}


/* Move the client to the monitor north of the current one on its
 * surface */
void enact_client_move_monitor_north(client_td *client)
{
    ccmd_client_move_to_monitor_north(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Move the client to the monitor south of the current one on its
 * surface */
void enact_client_move_monitor_south(client_td *client)
{
    ccmd_client_move_to_monitor_south(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Move the client to the monitor east of the current one on its
 * surface */
void enact_client_move_monitor_east(client_td *client)
{
    ccmd_client_move_to_monitor_east(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Move the client to the monitor west of the current one on its
 * surface */
void enact_client_move_monitor_west(client_td *client)
{
    ccmd_client_move_to_monitor_west(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Move the client to a specific monitor index */
void enact_client_move_to_monitor(client_td *client,
        uint32_t monitor_index)
{
    ccmd_client_move_to_monitor(client, monitor_index);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Change the client's 'WM_CLASS' class and instance names */
void enact_client_reclass(client_td *client,
        const char *restrict class_name,
        const char *restrict instance_name)
{
    ccmd_client_reclass(client, class_name, instance_name);
    if (client != NULL) {
        cJSON *fields;


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
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Maximize the client horizontally only */
void enact_client_maximize_horz(client_td *client)
{
    ccmd_client_maximize_horz(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Maximize the client vertically only */
void enact_client_maximize_vert(client_td *client)
{
    ccmd_client_maximize_vert(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Iconify the client */
void enact_client_iconify(client_td *client)
{
    ccmd_client_iconify(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_ICONIFIED);
    }
}


/* Hide the client's window */
void enact_client_hide(client_td *client)
{
    ccmd_client_hide(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_HIDE_SET);
    }
}


/* Show a previously hidden client */
void enact_client_unhide(client_td *client)
{
    ccmd_client_unhide(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_HIDE_CLEARED);
    }
}


/* Shade (roll up) the client */
void enact_client_shade(client_td *client)
{
    ccmd_client_shade(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_SHADE_SET);
    }
}


/* Unshade (roll down) the client */
void enact_client_unshade(client_td *client)
{
    ccmd_client_unshade(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_SHADE_CLEARED);
    }
}


/* Toggle the client's shade status */
void enact_client_toggle_shade(client_td *client)
{
    ccmd_client_toggle_shade(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, client_is_shaded(client)
                ? IPC_EVENT_SHADE_SET : IPC_EVENT_SHADE_CLEARED);
    }
}


/* Set the client's pin mode */
void enact_client_pin(client_td *client)
{
    ccmd_client_pin(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_PIN_SET);
    }
}


/* Remove the client's pin mode */
void enact_client_unpin(client_td *client)
{
    ccmd_client_unpin(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_PIN_CLEARED);
    }
}


/* Toggle the client's pin mode */
void enact_client_toggle_pin(client_td *client)
{
    ccmd_client_toggle_pin(client);
    if (client != NULL) {
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
        enact_broadcast_client_event(client, IPC_EVENT_FULLSCREEN_SET);
    }
}


/* Remove the client's full screen mode */
void enact_client_unfullscreen(client_td *client)
{
    ccmd_client_unfullscreen(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_FULLSCREEN_CLEARED);
    }
}


/* Toggle the client's full screen mode */
void enact_client_toggle_fullscreen(client_td *client)
{
    ccmd_client_toggle_fullscreen(client);
    if (client != NULL) {
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
        enact_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
    }
}


/* Lower the client to the bottom of its layer */
void enact_client_lower(client_td *client)
{
    ccmd_client_lower(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
    }
}


/* Move the client to the always-on-top layer */
void enact_client_layer_above(client_td *client)
{
    ccmd_client_layer_above(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Move the client to the normal layer */
void enact_client_layer_normal(client_td *client)
{
    ccmd_client_layer_normal(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Move the client to the always-on-bottom layer */
void enact_client_layer_below(client_td *client)
{
    ccmd_client_layer_below(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Cycle the client through the normal/above/below layers */
void enact_client_cycle_layer(client_td *client)
{
    ccmd_client_cycle_layer(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Mark the client as urgent */
void enact_client_urge(client_td *client)
{
    ccmd_client_urge(client);
    if (client != NULL) {
    }
}


/* Clear the client's urgency level */
void enact_client_unurge(client_td *client)
{
    ccmd_client_unurge(client);
    if (client != NULL) {
    }
}


/* Set the client's icon name */
void enact_client_set_icon(client_td *client, const char *icon_name)
{
    ccmd_client_set_icon(client, icon_name);
    if (client != NULL) {
        cJSON *fields;


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
        enact_broadcast_client_event(client, client_is_decorated(client)
                ? IPC_EVENT_DECORATION_SET
                : IPC_EVENT_DECORATION_CLEARED);
    }
}
