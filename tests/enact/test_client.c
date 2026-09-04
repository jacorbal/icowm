/**
 * @file tests/enact/test_client.c
 *
 * @brief Test battery for every client-level action in
 *        enact/client.c
 *
 * Every ccmd_client_* function (cmds/client, one source file per
 * facet) is a controllable, recording stand-in below, following
 * tests/rules/test_apply.c's own
 * established pattern of letting a stub flip the exact state bit a
 * real command would have flipped, so predicate-driven branches
 * (enact_client_toggle_shade and friends, which check client_is_*
 * only after their own ccmd_client_* call returns) stay meaningful
 * without a live X connection.  ipc_broadcast_event (ipc.c) and
 * scratchpad_is_client (scratchpad.c) are likewise controllable
 * stand-ins: the former to capture exactly which IPC_EVENT_* type
 * each action broadcasts without a real IPC transport, the latter to
 * avoid pulling in the whole scratchpad engine for the one boolean
 * enact_client_unfocus itself reads.  wm_get_surface_by_id,
 * surface_desktop_get, surface_desktop_north/south/east/west,
 * enact_desktop_client_send, enact_surface_desktop_switch, and
 * focus_apply are stand-ins too, needed only by the four
 * enact_client_send_to_desktop_* functions, whose own real
 * behavior otherwise needs a live surface's full desktop grid.
 * cJSON is linked for real throughout, so enact_broadcast_client_event
 * and the metadata-carrying actions (rename/reclass/rerole/set_icon)
 * build their own real JSON payload exactly as they do in production,
 * inspected here only through the ipc_broadcast_event stand-in's own
 * captured 'fields' argument.
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
#include <string.h>

/* Type includes */
#include <types/direction.h>
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <harness/tap.h>
#include <ipc.h>
#include <policy/focus.h>
#include <scratchpad.h>
#include <surface.h>
#include <wm.h>


/** Call counters for every ccmd_client_* stand-in below, one field
 *  per command, reset by s_reset before each scenario */
struct s_ccmd_calls_s {
    int close;
    int kill;
    int restore;
    int focus;
    int unfocus;
    int resize;
    int resize_force;
    int move;
    int center;
    int move_to_monitor_north;
    int move_to_monitor_south;
    int move_to_monitor_east;
    int move_to_monitor_west;
    int move_to_monitor;
    int reclass;
    int rerole;
    int rename;
    int maximize;
    int maximize_horz;
    int maximize_vert;
    int iconify;
    int hide;
    int unhide;
    int shade;
    int unshade;
    int toggle_shade;
    int pin;
    int unpin;
    int toggle_pin;
    int fullscreen;
    int unfullscreen;
    int toggle_fullscreen;
    int raise;
    int lower;
    int layer_above;
    int layer_normal;
    int layer_below;
    int cycle_layer;
    int urge;
    int unurge;
    int set_icon;
    int toggle_decorate;
};

static struct s_ccmd_calls_s s_calls;

/** Last position/geometry/monitor-index arguments captured, for the
 *  handful of ccmd_client_* stand-ins that take one */
static struct position_s s_last_move_pos;
static struct geometry_s s_last_resize_geom;
static uint32_t s_last_monitor_index;
static char s_last_reclass_class[64];
static char s_last_reclass_instance[64];
static char s_last_rerole_role[64];
static char s_last_rename_name[64];
static char s_last_set_icon_name[64];

void ccmd_client_close(client_td *client)
{ (void) client; s_calls.close++; }

void ccmd_client_kill(client_td *client)
{ (void) client; s_calls.kill++; }

void ccmd_client_restore(client_td *client)
{ (void) client; s_calls.restore++; }

void ccmd_client_focus(client_td *client)
{ (void) client; s_calls.focus++; }

void ccmd_client_unfocus(client_td *client)
{ (void) client; s_calls.unfocus++; }

void ccmd_client_resize(client_td *client, struct geometry_s geom)
{ (void) client; s_calls.resize++; s_last_resize_geom = geom; }

void ccmd_client_resize_force(client_td *client, struct geometry_s geom)
{ (void) client; s_calls.resize_force++; s_last_resize_geom = geom; }

void ccmd_client_move(client_td *client, struct position_s pos)
{ (void) client; s_calls.move++; s_last_move_pos = pos; }

void ccmd_client_center(client_td *client)
{ (void) client; s_calls.center++; }

void ccmd_client_move_to_monitor_north(client_td *client)
{ (void) client; s_calls.move_to_monitor_north++; }

void ccmd_client_move_to_monitor_south(client_td *client)
{ (void) client; s_calls.move_to_monitor_south++; }

void ccmd_client_move_to_monitor_east(client_td *client)
{ (void) client; s_calls.move_to_monitor_east++; }

void ccmd_client_move_to_monitor_west(client_td *client)
{ (void) client; s_calls.move_to_monitor_west++; }

void ccmd_client_move_to_monitor(client_td *client, uint32_t monitor_index)
{
    (void) client;
    s_calls.move_to_monitor++;
    s_last_monitor_index = monitor_index;
}

void ccmd_client_reclass(client_td *client,
        const char *restrict class_name,
        const char *restrict instance_name)
{
    (void) client;
    s_calls.reclass++;
    (void) strncpy(s_last_reclass_class,
            (class_name != NULL) ? class_name : "",
            sizeof(s_last_reclass_class) - 1u);
    (void) strncpy(s_last_reclass_instance,
            (instance_name != NULL) ? instance_name : "",
            sizeof(s_last_reclass_instance) - 1u);
}

void ccmd_client_rerole(client_td *client, const char *role)
{
    (void) client;
    s_calls.rerole++;
    (void) strncpy(s_last_rerole_role, (role != NULL) ? role : "",
            sizeof(s_last_rerole_role) - 1u);
}

void ccmd_client_rename(client_td *client, const char *name)
{
    (void) client;
    s_calls.rename++;
    (void) strncpy(s_last_rename_name, (name != NULL) ? name : "",
            sizeof(s_last_rename_name) - 1u);
}

void ccmd_client_maximize(client_td *client)
{ (void) client; s_calls.maximize++; }

void ccmd_client_maximize_horz(client_td *client)
{ (void) client; s_calls.maximize_horz++; }

void ccmd_client_maximize_vert(client_td *client)
{ (void) client; s_calls.maximize_vert++; }

void ccmd_client_iconify(client_td *client)
{ (void) client; s_calls.iconify++; }

void ccmd_client_hide(client_td *client)
{
    s_calls.hide++;
    if (client != NULL) {
        client->properties.flags |= (uint16_t) CLIENT_FLAG_HIDDEN;
    }
}

void ccmd_client_unhide(client_td *client)
{
    s_calls.unhide++;
    if (client != NULL) {
        client->properties.flags &= (uint16_t) ~CLIENT_FLAG_HIDDEN;
    }
}

void ccmd_client_shade(client_td *client)
{
    s_calls.shade++;
    if (client != NULL) {
        client->properties.flags |= (uint16_t) CLIENT_FLAG_SHADED;
    }
}

void ccmd_client_unshade(client_td *client)
{
    s_calls.unshade++;
    if (client != NULL) {
        client->properties.flags &= (uint16_t) ~CLIENT_FLAG_SHADED;
    }
}

void ccmd_client_toggle_shade(client_td *client)
{
    s_calls.toggle_shade++;
    if (client != NULL) {
        client->properties.flags ^= (uint16_t) CLIENT_FLAG_SHADED;
    }
}

void ccmd_client_pin(client_td *client)
{
    s_calls.pin++;
    if (client != NULL) {
        client->properties.flags |= (uint16_t) CLIENT_FLAG_PIN;
    }
}

void ccmd_client_unpin(client_td *client)
{
    s_calls.unpin++;
    if (client != NULL) {
        client->properties.flags &= (uint16_t) ~CLIENT_FLAG_PIN;
    }
}

void ccmd_client_toggle_pin(client_td *client)
{
    s_calls.toggle_pin++;
    if (client != NULL) {
        client->properties.flags ^= (uint16_t) CLIENT_FLAG_PIN;
    }
}

void ccmd_client_fullscreen(client_td *client)
{
    s_calls.fullscreen++;
    if (client != NULL) {
        client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    }
}

void ccmd_client_unfullscreen(client_td *client)
{
    s_calls.unfullscreen++;
    if (client != NULL) {
        client->properties.state &= (uint16_t) ~CLIENT_STATE_FULLSCREEN;
    }
}

void ccmd_client_toggle_fullscreen(client_td *client)
{
    s_calls.toggle_fullscreen++;
    if (client != NULL) {
        client->properties.state ^= (uint16_t) CLIENT_STATE_FULLSCREEN;
    }
}

void ccmd_client_raise(client_td *client)
{ (void) client; s_calls.raise++; }

void ccmd_client_lower(client_td *client)
{ (void) client; s_calls.lower++; }

void ccmd_client_layer_above(client_td *client)
{ (void) client; s_calls.layer_above++; }

void ccmd_client_layer_normal(client_td *client)
{ (void) client; s_calls.layer_normal++; }

void ccmd_client_layer_below(client_td *client)
{ (void) client; s_calls.layer_below++; }

void ccmd_client_cycle_layer(client_td *client)
{ (void) client; s_calls.cycle_layer++; }

void ccmd_client_urge(client_td *client)
{ (void) client; s_calls.urge++; }

void ccmd_client_unurge(client_td *client)
{ (void) client; s_calls.unurge++; }

void ccmd_client_set_icon(client_td *client, const char *icon_name)
{
    (void) client;
    s_calls.set_icon++;
    (void) strncpy(s_last_set_icon_name,
            (icon_name != NULL) ? icon_name : "",
            sizeof(s_last_set_icon_name) - 1u);
}

void ccmd_client_toggle_decorate(client_td *client)
{
    s_calls.toggle_decorate++;
    if (client != NULL) {
        client->properties.flags ^= (uint16_t) CLIENT_FLAG_DECORATED;
    }
}


/** Controllable stand-in for scratchpad_is_client (scratchpad.c) */
static bool s_scratchpad_is_client_result;

bool scratchpad_is_client(const client_td *client)
{
    (void) client;
    return s_scratchpad_is_client_result;
}


/** Call counter and captured type/fields for the ipc_broadcast_event
 *  stand-in (ipc.c) */
static int s_call_broadcast;
static uint32_t s_last_broadcast_type;
static cJSON *s_last_broadcast_fields;

void ipc_broadcast_event(uint32_t type, cJSON *fields)
{
    s_call_broadcast++;
    s_last_broadcast_type = type;
    cJSON_Delete(s_last_broadcast_fields);
    s_last_broadcast_fields = fields;
}


/** Controllable stand-ins needed only by the four
 *  enact_client_send_to_desktop_* functions */
static surface_td *s_surface_by_id_result;

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_surface_by_id_result;
}


static desktop_td s_cur_desktop;
static desktop_td s_target_desktop;
static bool s_desktop_get_returns_cur;

desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    return s_desktop_get_returns_cur ? &s_cur_desktop : NULL;
}


static desktop_td *s_direction_target;
static int s_call_desktop_north;
static int s_call_desktop_south;
static int s_call_desktop_east;
static int s_call_desktop_west;

desktop_td *surface_desktop_north(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    (void) surface;
    (void) desktop_id;
    (void) cycle;
    s_call_desktop_north++;
    return s_direction_target;
}

desktop_td *surface_desktop_south(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    (void) surface;
    (void) desktop_id;
    (void) cycle;
    s_call_desktop_south++;
    return s_direction_target;
}

desktop_td *surface_desktop_east(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    (void) surface;
    (void) desktop_id;
    (void) cycle;
    s_call_desktop_east++;
    return s_direction_target;
}

desktop_td *surface_desktop_west(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    (void) surface;
    (void) desktop_id;
    (void) cycle;
    s_call_desktop_west++;
    return s_direction_target;
}


static int s_call_desktop_client_send;
static int s_call_surface_desktop_switch;
static int s_call_focus_apply;

void enact_desktop_client_send(const desktop_td *desktop,
        client_td *client, desktop_td *target)
{
    (void) desktop;
    (void) client;
    (void) target;
    s_call_desktop_client_send++;
}

void enact_surface_desktop_switch(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    s_call_surface_desktop_switch++;
}

void focus_apply(list_td *surfaces, surface_td *surface,
        desktop_td *desktop, client_td *client,
        bool raise, const config_td *cfg)
{
    (void) surfaces;
    (void) surface;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) cfg;
    s_call_focus_apply++;
}


/** The one fixture client every scenario shares, and reset between
 *  them */
static client_td s_client;


static void s_reset(void)
{
    memset(&s_calls, 0, sizeof(s_calls));
    memset(&s_client, 0, sizeof(s_client));
    memset(&s_last_move_pos, 0, sizeof(s_last_move_pos));
    memset(&s_last_resize_geom, 0, sizeof(s_last_resize_geom));
    s_last_monitor_index = 0u;
    s_last_reclass_class[0] = '\0';
    s_last_reclass_instance[0] = '\0';
    s_last_rerole_role[0] = '\0';
    s_last_rename_name[0] = '\0';
    s_last_set_icon_name[0] = '\0';

    s_scratchpad_is_client_result = false;

    s_call_broadcast = 0;
    s_last_broadcast_type = 0u;
    cJSON_Delete(s_last_broadcast_fields);
    s_last_broadcast_fields = NULL;

    s_surface_by_id_result = NULL;
    memset(&s_cur_desktop, 0, sizeof(s_cur_desktop));
    memset(&s_target_desktop, 0, sizeof(s_target_desktop));
    s_cur_desktop.id = 1u;
    s_target_desktop.id = 2u;
    s_desktop_get_returns_cur = false;
    s_direction_target = NULL;
    s_call_desktop_north = 0;
    s_call_desktop_south = 0;
    s_call_desktop_east = 0;
    s_call_desktop_west = 0;
    s_call_desktop_client_send = 0;
    s_call_surface_desktop_switch = 0;
    s_call_focus_apply = 0;
}


/* enact_client_close/kill/focus/raise/lower/urge/unurge: every
 * broadcast-free action forwards straight to its own ccmd_client_*
 * call, and never touches ipc_broadcast_event at all */
static void s_test_broadcast_free_actions(void)
{
    s_reset();
    enact_client_close(&s_client);
    TAP_EQ_INT(s_calls.close, 1, "enact_client_close calls"
            " ccmd_client_close exactly once");

    enact_client_kill(&s_client);
    TAP_EQ_INT(s_calls.kill, 1, "enact_client_kill calls"
            " ccmd_client_kill exactly once");

    enact_client_focus(&s_client);
    TAP_EQ_INT(s_calls.focus, 1, "enact_client_focus calls"
            " ccmd_client_focus exactly once");

    enact_client_raise(&s_client);
    enact_client_lower(&s_client);
    TAP_EQ_INT(s_call_broadcast, 2,
            "raise and lower each still broadcast their own event");

    s_call_broadcast = 0;
    enact_client_urge(&s_client);
    enact_client_unurge(&s_client);
    TAP_EQ_INT(s_calls.urge, 1, "enact_client_urge calls"
            " ccmd_client_urge exactly once");
    TAP_EQ_INT(s_calls.unurge, 1, "enact_client_unurge calls"
            " ccmd_client_unurge exactly once");
    TAP_EQ_INT(s_call_broadcast, 0,
            "neither urge nor unurge ever broadcasts an IPC event");
}


/* enact_client_restore: calls ccmd_client_restore, then broadcasts
 * IPC_EVENT_CLIENT_DEICONIFIED */
static void s_test_restore_broadcasts_deiconified(void)
{
    s_reset();
    enact_client_restore(&s_client);
    TAP_EQ_INT(s_calls.restore, 1,
            "enact_client_restore calls ccmd_client_restore exactly once");
    TAP_EQ_INT((int) s_last_broadcast_type,
            (int) IPC_EVENT_CLIENT_DEICONIFIED,
            "the broadcast event type is IPC_EVENT_CLIENT_DEICONIFIED");
}


/* Every remaining call-then-broadcast action forwards to its own
 * ccmd_client_* call and broadcasts the exact IPC_EVENT_* type
 * documented for it */
static void s_test_call_then_fixed_broadcast_actions(void)
{
    s_reset();
    enact_client_resize(&s_client, (struct geometry_s) { { 1, 2 },
            { 3, 4 } });
    TAP_EQ_INT(s_calls.resize, 1, "enact_client_resize calls"
            " ccmd_client_resize exactly once");
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_WINDOW_RESIZED,
            "resize broadcasts IPC_EVENT_WINDOW_RESIZED");

    s_reset();
    enact_client_resize_force(&s_client, (struct geometry_s) { { 0, 0 },
            { 0, 0 } });
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_WINDOW_RESIZED,
            "resize_force also broadcasts IPC_EVENT_WINDOW_RESIZED");

    s_reset();
    enact_client_move(&s_client, (struct position_s) { 5, 6 });
    TAP_EQ_INT(s_last_move_pos.x, 5,
            "the exact x position given is forwarded to"
            " ccmd_client_move");
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_WINDOW_MOVED,
            "move broadcasts IPC_EVENT_WINDOW_MOVED");

    s_reset();
    enact_client_center(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_WINDOW_MOVED,
            "center also broadcasts IPC_EVENT_WINDOW_MOVED");

    s_reset();
    enact_client_move_monitor_north(&s_client);
    enact_client_move_monitor_south(&s_client);
    enact_client_move_monitor_east(&s_client);
    enact_client_move_monitor_west(&s_client);
    TAP_EQ_INT(s_calls.move_to_monitor_north +
            s_calls.move_to_monitor_south +
            s_calls.move_to_monitor_east +
            s_calls.move_to_monitor_west, 4,
            "each of the four move_monitor directions calls its own"
            " ccmd_client_move_to_monitor_* exactly once");
    TAP_EQ_INT(s_call_broadcast, 4,
            "each of the four also broadcasts IPC_EVENT_WINDOW_MOVED"
            " once");

    s_reset();
    enact_client_move_to_monitor(&s_client, 3u);
    TAP_EQ_INT((int) s_last_monitor_index, 3,
            "the exact monitor index given is forwarded");
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_WINDOW_MOVED,
            "move_to_monitor also broadcasts IPC_EVENT_WINDOW_MOVED");

    s_reset();
    enact_client_maximize(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_WINDOW_RESIZED,
            "maximize broadcasts IPC_EVENT_WINDOW_RESIZED");

    s_reset();
    enact_client_maximize_horz(&s_client);
    enact_client_maximize_vert(&s_client);
    TAP_EQ_INT(s_calls.maximize_horz + s_calls.maximize_vert, 2,
            "maximize_horz and maximize_vert each call their own"
            " ccmd_client_* exactly once");

    s_reset();
    enact_client_iconify(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_CLIENT_ICONIFIED,
            "iconify broadcasts IPC_EVENT_CLIENT_ICONIFIED");

    s_reset();
    enact_client_hide(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_HIDE_SET,
            "hide broadcasts IPC_EVENT_HIDE_SET");
    TAP_OK(client_is_hidden(&s_client) != 0,
            "hide leaves the client's own hidden flag set");

    enact_client_unhide(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_HIDE_CLEARED,
            "unhide broadcasts IPC_EVENT_HIDE_CLEARED");
    TAP_OK(client_is_hidden(&s_client) == 0,
            "unhide clears the client's own hidden flag");

    s_reset();
    enact_client_shade(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_SHADE_SET,
            "shade broadcasts IPC_EVENT_SHADE_SET");
    enact_client_unshade(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_SHADE_CLEARED,
            "unshade broadcasts IPC_EVENT_SHADE_CLEARED");

    s_reset();
    enact_client_pin(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_PIN_SET,
            "pin broadcasts IPC_EVENT_PIN_SET");
    enact_client_unpin(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_PIN_CLEARED,
            "unpin broadcasts IPC_EVENT_PIN_CLEARED");

    s_reset();
    enact_client_fullscreen(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_FULLSCREEN_SET,
            "fullscreen broadcasts IPC_EVENT_FULLSCREEN_SET");
    enact_client_unfullscreen(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type,
            (int) IPC_EVENT_FULLSCREEN_CLEARED,
            "unfullscreen broadcasts IPC_EVENT_FULLSCREEN_CLEARED");

    s_reset();
    enact_client_layer_above(&s_client);
    enact_client_layer_normal(&s_client);
    enact_client_layer_below(&s_client);
    enact_client_cycle_layer(&s_client);
    TAP_EQ_INT(s_call_broadcast, 4,
            "all four layer actions broadcast IPC_EVENT_LAYER_CHANGED,"
            " one each");
}


/* enact_client_toggle_shade/toggle_pin/toggle_fullscreen: the
 * broadcast event chosen depends on the client's own state after
 * the underlying ccmd_client_toggle_* call flips it */
static void s_test_toggle_actions_pick_event_by_resulting_state(void)
{
    s_reset();
    enact_client_toggle_shade(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_SHADE_SET,
            "toggling shade on an unshaded client broadcasts"
            " IPC_EVENT_SHADE_SET");
    enact_client_toggle_shade(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_SHADE_CLEARED,
            "toggling shade again broadcasts IPC_EVENT_SHADE_CLEARED");

    s_reset();
    enact_client_toggle_pin(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_PIN_SET,
            "toggling pin on an unpinned client broadcasts"
            " IPC_EVENT_PIN_SET");
    enact_client_toggle_pin(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_PIN_CLEARED,
            "toggling pin again broadcasts IPC_EVENT_PIN_CLEARED");

    s_reset();
    enact_client_toggle_fullscreen(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_FULLSCREEN_SET,
            "toggling fullscreen on a windowed client broadcasts"
            " IPC_EVENT_FULLSCREEN_SET");
    enact_client_toggle_fullscreen(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type,
            (int) IPC_EVENT_FULLSCREEN_CLEARED,
            "toggling fullscreen again broadcasts"
            " IPC_EVENT_FULLSCREEN_CLEARED");

    s_reset();
    enact_client_toggle_decorate(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type,
            (int) IPC_EVENT_DECORATION_SET,
            "toggling decoration on an undecorated client broadcasts"
            " IPC_EVENT_DECORATION_SET");
    enact_client_toggle_decorate(&s_client);
    TAP_EQ_INT((int) s_last_broadcast_type,
            (int) IPC_EVENT_DECORATION_CLEARED,
            "toggling decoration again broadcasts"
            " IPC_EVENT_DECORATION_CLEARED");
}


/* enact_client_reclass/rerole/rename/set_icon: each forwards to its
 * own ccmd_client_* call, then broadcasts an event carrying the
 * exact string field given */
static void s_test_metadata_actions_carry_their_own_field(void)
{
    cJSON *field;

    s_reset();
    enact_client_reclass(&s_client, "Firefox", "firefox");
    TAP_EQ_STR(s_last_reclass_class, "Firefox",
            "reclass forwards the exact class_name given");
    TAP_EQ_STR(s_last_reclass_instance, "firefox",
            "reclass forwards the exact instance_name given");
    TAP_EQ_INT((int) s_last_broadcast_type,
            (int) IPC_EVENT_CLIENT_RECLASSED,
            "reclass broadcasts IPC_EVENT_CLIENT_RECLASSED");
    field = cJSON_GetObjectItem(s_last_broadcast_fields, "class_name");
    TAP_EQ_STR(cJSON_GetStringValue(field), "Firefox",
            "the broadcast fields carry the exact class_name given");

    s_reset();
    enact_client_rerole(&s_client, "browser");
    TAP_EQ_STR(s_last_rerole_role, "browser",
            "rerole forwards the exact role given");
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_CLIENT_REROLED,
            "rerole broadcasts IPC_EVENT_CLIENT_REROLED");

    s_reset();
    enact_client_rename(&s_client, "New Title");
    TAP_EQ_STR(s_last_rename_name, "New Title",
            "rename forwards the exact name given");
    TAP_EQ_INT((int) s_last_broadcast_type, (int) IPC_EVENT_CLIENT_RENAMED,
            "rename broadcasts IPC_EVENT_CLIENT_RENAMED");

    s_reset();
    enact_client_set_icon(&s_client, "/path/icon.png");
    TAP_EQ_STR(s_last_set_icon_name, "/path/icon.png",
            "set_icon forwards the exact icon_name given");
    TAP_EQ_INT((int) s_last_broadcast_type,
            (int) IPC_EVENT_CLIENT_ICON_CHANGED,
            "set_icon broadcasts IPC_EVENT_CLIENT_ICON_CHANGED");
}


/* Every action broadcasting through enact_broadcast_client_event
 * skips the broadcast entirely for a NULL client, since there would
 * be no client_id/desktop_id/surface_id to report; the underlying
 * ccmd_client_* call itself is still made (that function's own job
 * is deciding whether to act, not this file's; every ccmd_client_*
 * stand-in above already tolerates a NULL client on its own) */
static void s_test_null_client_never_broadcasts(void)
{
    s_reset();
    enact_client_restore(NULL);
    enact_client_resize(NULL, (struct geometry_s) { { 0, 0 }, { 0, 0 } });
    enact_client_iconify(NULL);
    enact_client_toggle_shade(NULL);
    enact_client_reclass(NULL, "x", "y");
    TAP_EQ_INT(s_call_broadcast, 0,
            "a NULL client never reaches ipc_broadcast_event, on any"
            " of these five representative actions");
}


/* enact_client_unfocus: a plain client is unfocused with no side
 * effect beyond the underlying ccmd_client_unfocus call */
static void s_test_unfocus_plain_client(void)
{
    s_reset();
    enact_client_unfocus(&s_client);
    TAP_EQ_INT(s_calls.unfocus, 1,
            "enact_client_unfocus calls ccmd_client_unfocus exactly"
            " once");
    TAP_EQ_INT(s_calls.hide, 0,
            "a non-scratchpad client is never hidden as a side effect");
}


/* enact_client_unfocus: the scratchpad client is hidden as a side
 * effect, but only when it is not hidden already */
static void s_test_unfocus_scratchpad_client_hides_once(void)
{
    s_reset();
    s_scratchpad_is_client_result = true;

    enact_client_unfocus(&s_client);
    TAP_EQ_INT(s_calls.hide, 1,
            "an unhidden scratchpad client is hidden exactly once"
            " on unfocus");

    s_calls.hide = 0;
    enact_client_unfocus(&s_client);
    TAP_EQ_INT(s_calls.hide, 0,
            "an already-hidden scratchpad client is never hidden"
            " again on a second unfocus");
}


/* enact_client_send_to_desktop_*: a NULL client is a silent no-op,
 * never reaching wm_get_surface_by_id at all */
static void s_test_send_to_desktop_null_client_is_noop(void)
{
    int fake_surfaces_storage;
    list_td *const fake_surfaces = (list_td *) &fake_surfaces_storage;

    s_reset();
    enact_client_send_to_desktop_north(NULL, fake_surfaces, NULL);
    TAP_EQ_INT(s_call_surface_desktop_switch, 0,
            "a NULL client never reaches enact_surface_desktop_switch"
            " at all");
}


/* enact_client_send_to_desktop_*: an unresolvable surface is a
 * silent no-op */
static void s_test_send_to_desktop_no_surface_is_noop(void)
{
    int fake_surfaces_storage;
    list_td *const fake_surfaces = (list_td *) &fake_surfaces_storage;

    s_reset();
    s_surface_by_id_result = NULL;

    enact_client_send_to_desktop_north(&s_client, fake_surfaces, NULL);
    TAP_EQ_INT(s_call_desktop_client_send, 0,
            "an unresolvable surface never reaches"
            " enact_desktop_client_send");
}


/* enact_client_send_to_desktop_*: no different desktop in that
 * direction (the direction lookup returns the same desktop already
 * current, or NULL) is a silent no-op */
static void s_test_send_to_desktop_no_target_is_noop(void)
{
    surface_td surface;
    int fake_surfaces_storage;
    list_td *const fake_surfaces = (list_td *) &fake_surfaces_storage;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_surface_by_id_result = &surface;
    s_desktop_get_returns_cur = true;
    s_direction_target = &s_cur_desktop;

    enact_client_send_to_desktop_east(&s_client, fake_surfaces, NULL);
    TAP_EQ_INT(s_call_desktop_client_send, 0,
            "a direction lookup returning the already-current desktop"
            " never sends the client anywhere");

    s_direction_target = NULL;
    enact_client_send_to_desktop_west(&s_client, fake_surfaces, NULL);
    TAP_EQ_INT(s_call_desktop_client_send, 0,
            "a direction lookup returning NULL never sends the client"
            " anywhere either");
}


/* enact_client_send_to_desktop_*: a genuinely different target
 * desktop sends the client, switches the surface to it, and
 * re-applies focus, once each, for every one of the four compass
 * directions */
static void s_test_send_to_desktop_moves_and_follows(void)
{
    surface_td surface;
    int fake_surfaces_storage;
    list_td *const fake_surfaces = (list_td *) &fake_surfaces_storage;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_surface_by_id_result = &surface;
    s_desktop_get_returns_cur = true;
    s_direction_target = &s_target_desktop;

    enact_client_send_to_desktop_north(&s_client, fake_surfaces, NULL);
    enact_client_send_to_desktop_south(&s_client, fake_surfaces, NULL);
    enact_client_send_to_desktop_east(&s_client, fake_surfaces, NULL);
    enact_client_send_to_desktop_west(&s_client, fake_surfaces, NULL);

    TAP_EQ_INT(s_call_desktop_north + s_call_desktop_south +
            s_call_desktop_east + s_call_desktop_west, 4,
            "all four compass directions each query their own"
            " surface_desktop_* function exactly once");
    TAP_EQ_INT(s_call_desktop_client_send, 4,
            "a genuinely different target desktop sends the client"
            " on every one of the four calls");
    TAP_EQ_INT(s_call_surface_desktop_switch, 4,
            "and switches the surface to that target desktop every"
            " time as well");
    TAP_EQ_INT(s_call_focus_apply, 4,
            "and re-applies focus onto the client every time too");
}


int main(void)
{
    TAP_PLAN(64);

    s_test_broadcast_free_actions();
    s_test_restore_broadcasts_deiconified();
    s_test_call_then_fixed_broadcast_actions();
    s_test_toggle_actions_pick_event_by_resulting_state();
    s_test_metadata_actions_carry_their_own_field();
    s_test_null_client_never_broadcasts();
    s_test_unfocus_plain_client();
    s_test_unfocus_scratchpad_client_hides_once();
    s_test_send_to_desktop_null_client_is_noop();
    s_test_send_to_desktop_no_surface_is_noop();
    s_test_send_to_desktop_no_target_is_noop();
    s_test_send_to_desktop_moves_and_follows();

    cJSON_Delete(s_last_broadcast_fields);

    return TAP_DONE();
}
