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

/* Utils includes */
#include <utils/xcb/connection.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Policy includes */
#include <policy/focus.h>

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
#include <cmds/stage.h>

/* Stage includes */
#include <stage/desktop.h>

/* Project includes */
#include <client.h>
#include <logger.h>
#include <scratchpad.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <enact.h>
#include <enact/client.h>
#include <enact/desktop.h>
#include <enact/stage.h>
#include <enact/internal.h>


/**
 * @brief Shared logic for carrying the client to another desktop in
 *        a given compass direction, following it there
 *
 * @param client    Client to move
 * @param stages    Full stage list, passed through to @c focus_apply
 * @param config    Active configuration, passed through to
 *                  @c focus_apply
 * @param direction Compass direction to move the client in
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       client's top parent's desktop (@a enact_desktop_client_send's
 *       comment)
 */
static void s_enact_client_send_to_desktop(client_td *client,
        list_td *stages, const config_td *config,
        enum compass_direction_e direction)
{
    stage_td *stage;
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

    stage = wm_get_stage_by_id(client->screen_id);
    if (stage == NULL) {
        return;
    }

    cur_desktop = stage_desktop_get(stage, stage->desktop_cur);
    if (cur_desktop == NULL) {
        return;
    }

    cycle = (stage->config != NULL)
        ? stage->config->desktops.wrap_at_bounds : true;

    /* No different desktop to move to at all: either genuinely only one
     * exists (restricted-memory mode is always locked to exactly one;
     * see 'stage_action_desktop_add''s comment, stage/switch.c),
     * wrapping is disabled and this is already the edgemost one that
     * way, or (north/south only, on a stage with no
     * 'topology.screens.desktops' layout configured at all) there is no
     * second row or column to move to in the first place; is a silent
     * no-op, the same as every other
      keybind here that finds nothing to act on. */
    switch (direction) {
    case COMPASS_NORTH:
        target_desktop = stage_desktop_north(stage,
                cur_desktop->id, cycle);
        break;
    case COMPASS_SOUTH:
        target_desktop = stage_desktop_south(stage,
                cur_desktop->id, cycle);
        break;
    case COMPASS_EAST:
        target_desktop = stage_desktop_east(stage,
                cur_desktop->id, cycle);
        break;
    case COMPASS_WEST:
        target_desktop = stage_desktop_west(stage,
                cur_desktop->id, cycle);
        break;
    }
    if (target_desktop == NULL || target_desktop == cur_desktop) {
        return;
    }

    enact_desktop_client_send(cur_desktop, client, target_desktop);
    enact_stage_desktop_switch(stage, target_desktop->id);

    /* 'enact_stage_desktop_switch' just above, via its
     * 'stage_client_show_all', already restored real input focus on
     * its, to whichever client this target desktop's 'client_active_id'
     * still remembered from some earlier, unrelated visit, not this
     * client, freshly arrived on it as of the very call before this
     * one.  Explicitly re-applied here, after the fact, rather than
     * trying to somehow suppress that automatic restore instead:
     * 'client' becomes this desktop's newly active one, genuinely
     * focused, and raised above whatever else that restore just raised
     * in front of it (any client already there before this one arrived
     * stays exactly where it was, simply no longer topmost), matching
     * a plain click or any other deliberate focus request landing on it
     * right after the move, not a stale leftover from before. */
    focus_apply(stages, stage, target_desktop, client, true, config);
}


/**
 * @brief Tell IPC subscribers a client gained or lost a maximized axis
 *
 * Compares the axes it is maximized on now against the ones it was
 * before: gaining one sends @c IPC_EVENT_CLIENT_MAXIMIZE_SET, losing
 * one @c IPC_EVENT_CLIENT_MAXIMIZE_CLEARED, and a call that changed
 * nothing sends neither.
 *
 * @param client   Client that may have changed
 * @param was_horz Whether it was maximized horizontally before
 * @param was_vert Whether it was maximized vertically before
 *
 * @note Complexity: @e O(1)
 */
static void s_enact_broadcast_maximize_change(client_td *client,
        bool was_horz, bool was_vert)
{
    bool is_horz = client_is_maximized_horz(client);
    bool is_vert = client_is_maximized_vert(client);

    if ((is_horz && !was_horz) || (is_vert && !was_vert)) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_MAXIMIZE_SET);
    }
    if ((was_horz && !is_horz) || (was_vert && !is_vert)) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_MAXIMIZE_CLEARED);
    }
}


/* Broadcast an IPC event carrying one client's identifying fields */
void enact_broadcast_client_event(client_td *client, uint64_t type)
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
        cJSON_AddNumberToObject(fields, "stage_id",
                (double) client->screen_id);
    }
    ipc_broadcast_event(type, fields);
}


/* Close the client */
void enact_client_close(client_td *client)
{
    ccmd_client_close(client);
}


/* Forcibly kill the client's owning connection */
void enact_client_kill(client_td *client)
{
    ccmd_client_kill(client);
}


/* Restore the client to its normal state */
void enact_client_restore(client_td *client)
{
    ccmd_client_restore(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_ICONIFY_CLEARED);
    }
}


/* Give input focus to the client */
void enact_client_focus(client_td *client)
{
    stage_td *stage;
    desktop_td *desktop;

    if (client == NULL || client_is_hidden(client) ||
            client_is_iconified(client)) {
        return;
    }

    stage = wm_get_stage_by_id(client->screen_id);
    desktop = wm_get_client_desktop(client);
    if (stage == NULL || desktop == NULL ||
            (desktop->id != stage->desktop_cur &&
             !client_is_pinned(client))) {
        return;
    }

    /* Through the same path a click takes, so the window manager's
     * active client, '_NET_ACTIVE_WINDOW' and the frames all follow the
     * keyboard, and a modal dialog still open over 'client' gets the
     * focus in its place; without the configuration, which is only
     * consulted for raise-on-focus, this never raises it */
    focus_apply(wm_get_stages(), stage, desktop, client, false, NULL);
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
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_RESIZED);
    }
}


/* Resize the client to a specific frame geometry immediately, bypassing
 * any in-flight sync throttling */
void enact_client_resize_force(client_td *client, struct geometry_s geom)
{
    ccmd_client_resize_force(client, geom);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_RESIZED);
    }
}


/* Move the client to a specific position */
void enact_client_move(client_td *client, struct position_s pos)
{
    ccmd_client_move(client, pos);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_MOVED);
    }
}


/* Center the client on its current screen */
void enact_client_center(client_td *client)
{
    ccmd_client_center(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_MOVED);
    }
}


/* Carry the client to the desktop north of the current one, following
 * it there */
void enact_client_send_to_desktop_north(client_td *client,
        list_td *stages, const config_td *config)
{
    s_enact_client_send_to_desktop(client, stages, config,
            COMPASS_NORTH);
}


/* Carry the client to the desktop south of the current one, following
 * it there */
void enact_client_send_to_desktop_south(client_td *client,
        list_td *stages, const config_td *config)
{
    s_enact_client_send_to_desktop(client, stages, config,
            COMPASS_SOUTH);
}


/* Carry the client to the desktop east of the current one, following it
 * there */
void enact_client_send_to_desktop_east(client_td *client,
        list_td *stages, const config_td *config)
{
    s_enact_client_send_to_desktop(client, stages, config,
            COMPASS_EAST);
}


/* Carry the client to the desktop west of the current one, following it
 * there */
void enact_client_send_to_desktop_west(client_td *client,
        list_td *stages, const config_td *config)
{
    s_enact_client_send_to_desktop(client, stages, config,
            COMPASS_WEST);
}


/* Move the client to the monitor north of the current one on its
 * stage */
void enact_client_move_monitor_north(client_td *client)
{
    ccmd_client_move_to_monitor_north(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_MOVED);
    }
}


/* Move the client to the monitor south of the current one on its
 * stage */
void enact_client_move_monitor_south(client_td *client)
{
    ccmd_client_move_to_monitor_south(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_MOVED);
    }
}


/* Move the client to the monitor east of the current one on its
 * stage */
void enact_client_move_monitor_east(client_td *client)
{
    ccmd_client_move_to_monitor_east(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_MOVED);
    }
}


/* Move the client to the monitor west of the current one on its
 * stage */
void enact_client_move_monitor_west(client_td *client)
{
    ccmd_client_move_to_monitor_west(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_MOVED);
    }
}


/* Move the client to a specific monitor index */
void enact_client_move_to_monitor(client_td *client,
        uint32_t monitor_index)
{
    ccmd_client_move_to_monitor(client, monitor_index);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_MOVED);
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
            cJSON_AddNumberToObject(fields, "stage_id",
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
            cJSON_AddNumberToObject(fields, "stage_id",
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
            cJSON_AddNumberToObject(fields, "stage_id",
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
    bool was_horz = (client != NULL) && client_is_maximized_horz(client);
    bool was_vert = (client != NULL) && client_is_maximized_vert(client);

    ccmd_client_maximize(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_RESIZED);
        s_enact_broadcast_maximize_change(client, was_horz, was_vert);
    }
}


/* Maximize the client horizontally only */
void enact_client_maximize_horz(client_td *client)
{
    bool was_horz = (client != NULL) && client_is_maximized_horz(client);
    bool was_vert = (client != NULL) && client_is_maximized_vert(client);

    ccmd_client_maximize_horz(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_RESIZED);
        s_enact_broadcast_maximize_change(client, was_horz, was_vert);
    }
}


/* Maximize the client vertically only */
void enact_client_maximize_vert(client_td *client)
{
    bool was_horz = (client != NULL) && client_is_maximized_horz(client);
    bool was_vert = (client != NULL) && client_is_maximized_vert(client);

    ccmd_client_maximize_vert(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, IPC_EVENT_CLIENT_RESIZED);
        s_enact_broadcast_maximize_change(client, was_horz, was_vert);
    }
}


/* Iconify the client */
void enact_client_iconify(client_td *client)
{
    ccmd_client_iconify(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_ICONIFY_SET);
    }
}


/* Hide the client's window */
void enact_client_hide(client_td *client)
{
    ccmd_client_hide(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_HIDE_SET);
        if (scratchpad_is_client(client)) {
            enact_broadcast_client_event(client,
                    IPC_EVENT_SCRATCHPAD_HIDDEN);
        }
    }
}


/* Show a previously hidden client */
void enact_client_unhide(client_td *client)
{
    ccmd_client_unhide(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_HIDE_CLEARED);
        if (scratchpad_is_client(client)) {
            enact_broadcast_client_event(client,
                    IPC_EVENT_SCRATCHPAD_SHOWN);
        }
    }
}


/* Shade (roll up) the client */
void enact_client_shade(client_td *client)
{
    ccmd_client_shade(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_SHADE_SET);
    }
}


/* Unshade (roll down) the client */
void enact_client_unshade(client_td *client)
{
    ccmd_client_unshade(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_SHADE_CLEARED);
    }
}


/* Toggle the client's shade status */
void enact_client_toggle_shade(client_td *client)
{
    ccmd_client_toggle_shade(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                client_is_shaded(client)
                    ? IPC_EVENT_CLIENT_SHADE_SET
                    : IPC_EVENT_CLIENT_SHADE_CLEARED);
    }
}


/* Set the client's pin mode */
void enact_client_pin(client_td *client)
{
    ccmd_client_pin(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_PIN_SET);
    }
}


/* Remove the client's pin mode */
void enact_client_unpin(client_td *client)
{
    ccmd_client_unpin(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_PIN_CLEARED);
    }
}


/* Toggle the client's pin mode */
void enact_client_toggle_pin(client_td *client)
{
    ccmd_client_toggle_pin(client);
    if (client != NULL) {
        enact_broadcast_client_event(client, client_is_pinned(client)
                ? IPC_EVENT_CLIENT_PIN_SET
                : IPC_EVENT_CLIENT_PIN_CLEARED);
    }
}


/* Set the client's sticky mode.  No IPC event to broadcast here, unlike
 * its pin counterpart above: every bit of the IPC_EVENT_* mask is
 * already in use, with none free for a new sticky pair */
void enact_client_stick(client_td *client)
{
    ccmd_client_stick(client);
}


/* Remove the client's sticky mode.  Same reasoning as its setter above
 * for why there is no IPC event to broadcast */
void enact_client_unstick(client_td *client)
{
    ccmd_client_unstick(client);
}


/* Toggle the client's sticky mode.  No IPC event to broadcast here,
 * unlike its pin counterpart above: every bit of the IPC_EVENT_* mask
 * is already in use, with none free for a new sticky pair */
void enact_client_toggle_stick(client_td *client)
{
    ccmd_client_toggle_stick(client);
}


/* Move the client to a given page of its desktop's viewport */
void enact_client_send_to_page(stage_td *stage, client_td *client,
        uint32_t col, uint32_t row)
{
    scmd_stage_viewport_client_send_to_page(stage, client, col, row);
}


/* Set the client to full screen mode */
void enact_client_fullscreen(client_td *client)
{
    ccmd_client_fullscreen(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_FULLSCREEN_SET);
    }
}


/* Remove the client's full screen mode */
void enact_client_unfullscreen(client_td *client)
{
    ccmd_client_unfullscreen(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_FULLSCREEN_CLEARED);
    }
}


/* Toggle the client's full screen mode */
void enact_client_toggle_fullscreen(client_td *client)
{
    ccmd_client_toggle_fullscreen(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                client_is_fullscreen(client)
                    ? IPC_EVENT_CLIENT_FULLSCREEN_SET
                    : IPC_EVENT_CLIENT_FULLSCREEN_CLEARED);
    }
}


/* Raise the client to the top of its layer */
void enact_client_raise(client_td *client)
{
    ccmd_client_raise(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_STACKING_CHANGED);
    }
}


/* Lower the client to the bottom of its layer */
void enact_client_lower(client_td *client)
{
    ccmd_client_lower(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_STACKING_CHANGED);
    }
}


/* Move the client to the always-on-top layer */
void enact_client_layer_above(client_td *client)
{
    ccmd_client_layer_above(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_LAYER_CHANGED);
    }
}


/* Move the client to the normal layer */
void enact_client_layer_normal(client_td *client)
{
    ccmd_client_layer_normal(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_LAYER_CHANGED);
    }
}


/* Move the client to the always-on-bottom layer */
void enact_client_layer_below(client_td *client)
{
    ccmd_client_layer_below(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_LAYER_CHANGED);
    }
}


/* Cycle the client through the normal/above/below layers */
void enact_client_cycle_layer(client_td *client)
{
    ccmd_client_cycle_layer(client);
    if (client != NULL) {
        enact_broadcast_client_event(client,
                IPC_EVENT_CLIENT_LAYER_CHANGED);
    }
}


/* Mark the client as urgent */
void enact_client_urge(client_td *client)
{
    ccmd_client_urge(client);
}


/* Clear the client's urgency level */
void enact_client_unurge(client_td *client)
{
    ccmd_client_unurge(client);
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
            cJSON_AddNumberToObject(fields, "stage_id",
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
                ? IPC_EVENT_CLIENT_DECORATION_SET
                : IPC_EVENT_CLIENT_DECORATION_CLEARED);
    }
}
