/**
 * @file tests/ipc/actions/client/test_geom.c
 *
 * @brief Test battery for the move/resize/monitor/maximize IPC
 *        client actions
 *
 * geom.c mixes two shapes: eight thin ipc_dispatch_client_action
 * wrappers (center, the four move-to-monitor-in-a-direction actions,
 * both single-axis maximize actions, and full maximize), each with
 * its own static callback forwarding into one enact_client_*
 * function; and four entry points with their own extra numeric
 * arguments (move, move-to-a-specific-monitor, move-resize, and
 * resize) that validate those arguments themselves, in the exact
 * order the real source checks them, before ever calling
 * ipc_resolve_client.  This links the real geom.c, the real
 * ipc_dispatch_client_action (dispatch.c), resolve.c, args.c,
 * response.c, and lookup.c, matching tests/ipc/test_dispatch.c's own
 * linking pattern for the shared wrapper.  It also links the real
 * client/state.c for client_gravity_adjust_pos, the one non-enact
 * dependency ipc_action_resize_client pulls in (proven to compile
 * standalone, with no extra libraries, by
 * tests/client/test_gravity.c's own build rule), so its
 * gravity-aware position math is exercised for real rather than
 * stubbed.  Every enact_client_* function these twelve entry points
 * reach is a recording stand-in below, since the real ones
 * (enact/client.c) pull in XCB requests this file has no reason to
 * exercise: every scenario asserts on which stand-in ran, with what
 * arguments, and how many times, never on any X side effect.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <client/state.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>
#include <ipc/actions/client/geom.h>


/** Link-only stand-in for surface_desktop_get (surface.c): lookup.c
 *  as a whole references it, though the id-based fast path
 *  lookup_find_client actually takes never reaches it */
desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    return NULL;
}


/** Link-only stand-in for wm_get_surface_by_id (wm.c): ipc/resolve.c
 *  as a whole references it, from ipc_resolve_surface, which none
 *  of these twelve actions ever calls (only ipc_resolve_client) */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return NULL;
}


/** Call counters, reset by s_build_wm before each scenario */
static int s_call_center;
static int s_call_move_north;
static int s_call_move_south;
static int s_call_move_east;
static int s_call_move_west;
static int s_call_maximize_horz;
static int s_call_maximize_vert;
static int s_call_maximize;
static int s_call_move;
static int s_call_move_to_monitor;
static int s_call_resize;

/** Last client pointer, and last argument or arguments, each
 *  stand-in below actually received */
static client_td *s_last_client;
static struct position_s s_last_move_pos;
static uint32_t s_last_monitor_index;
static struct geometry_s s_last_resize_geom;


/**
 * @brief Recording stand-in for enact_client_center
 *
 * @note Complexity: O(1)
 */
void enact_client_center(client_td *client)
{
    s_call_center++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_move_monitor_north
 *
 * @note Complexity: O(1)
 */
void enact_client_move_monitor_north(client_td *client)
{
    s_call_move_north++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_move_monitor_south
 *
 * @note Complexity: O(1)
 */
void enact_client_move_monitor_south(client_td *client)
{
    s_call_move_south++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_move_monitor_east
 *
 * @note Complexity: O(1)
 */
void enact_client_move_monitor_east(client_td *client)
{
    s_call_move_east++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_move_monitor_west
 *
 * @note Complexity: O(1)
 */
void enact_client_move_monitor_west(client_td *client)
{
    s_call_move_west++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_maximize_horz
 *
 * @note Complexity: O(1)
 */
void enact_client_maximize_horz(client_td *client)
{
    s_call_maximize_horz++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_maximize_vert
 *
 * @note Complexity: O(1)
 */
void enact_client_maximize_vert(client_td *client)
{
    s_call_maximize_vert++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_maximize
 *
 * @note Complexity: O(1)
 */
void enact_client_maximize(client_td *client)
{
    s_call_maximize++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_move
 *
 * @note Complexity: O(1)
 */
void enact_client_move(client_td *client, struct position_s pos)
{
    s_call_move++;
    s_last_client = client;
    s_last_move_pos = pos;
}


/**
 * @brief Recording stand-in for enact_client_move_to_monitor
 *
 * @note Complexity: O(1)
 */
void enact_client_move_to_monitor(client_td *client,
        uint32_t monitor_index)
{
    s_call_move_to_monitor++;
    s_last_client = client;
    s_last_monitor_index = monitor_index;
}


/**
 * @brief Recording stand-in for enact_client_resize
 *
 * @note Complexity: O(1)
 */
void enact_client_resize(client_td *client, struct geometry_s geom)
{
    s_call_resize++;
    s_last_client = client;
    s_last_resize_geom = geom;
}


static size_t s_id_hash1(const void *key)
{
    return (size_t) ((const client_td *) key)->id;
}


static size_t s_id_hash2(const void *key)
{
    (void) key;
    return 1u;
}


static bool s_id_match(const void *key1, const void *key2)
{
    return ((const client_td *) key1)->id ==
        ((const client_td *) key2)->id;
}


/** Every fixture this file needs, wired up fresh by s_build_wm for
 *  each scenario, and torn down by s_teardown_wm right after */
static wm_td s_wm;
static surface_td s_surface;
static desktop_td s_desktop;
static xcb_screen_t s_screen;
static client_td s_client;


/** Build one surface/desktop/client, holding a single client whose
 *  id is 17, current geometry (100,100)-(200x150), and
 *  'CLIENT_GRAVITY_NORTH_WEST' (a resize no-op, per
 *  client_gravity_adjust_pos's own contract), and hang it off
 *  s_wm.surfaces */
static void s_build_wm(void)
{
    memset(&s_wm, 0, sizeof(s_wm));
    memset(&s_surface, 0, sizeof(s_surface));
    memset(&s_desktop, 0, sizeof(s_desktop));
    memset(&s_screen, 0, sizeof(s_screen));
    memset(&s_client, 0, sizeof(s_client));

    s_client.id = 17u;
    s_client.layout.geometry.cur.pos.x = 100;
    s_client.layout.geometry.cur.pos.y = 100;
    s_client.layout.geometry.cur.dim.w = 200u;
    s_client.layout.geometry.cur.dim.h = 150u;
    s_client.layout.gravity = CLIENT_GRAVITY_NORTH_WEST;
    s_desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(s_desktop.clients, &s_client);
    s_surface.screen = &s_screen;
    s_surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(s_surface.desktops, NULL, &s_desktop);
    s_wm.surfaces = list_init(NULL);
    list_ins_next(s_wm.surfaces, NULL, &s_surface);

    s_call_center = 0;
    s_call_move_north = 0;
    s_call_move_south = 0;
    s_call_move_east = 0;
    s_call_move_west = 0;
    s_call_maximize_horz = 0;
    s_call_maximize_vert = 0;
    s_call_maximize = 0;
    s_call_move = 0;
    s_call_move_to_monitor = 0;
    s_call_resize = 0;
    s_last_client = NULL;
    memset(&s_last_move_pos, 0, sizeof(s_last_move_pos));
    s_last_monitor_index = 0u;
    memset(&s_last_resize_geom, 0, sizeof(s_last_resize_geom));
}


static void s_teardown_wm(void)
{
    list_destroy(s_wm.surfaces);
    cdlist_destroy(s_surface.desktops);
    ohtbl_destroy(s_desktop.clients);
}


/* center_client resolves the client and calls enact_client_center
 * once, reporting success */
static void s_test_center_client(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);

    resp = ipc_action_center_client(&s_wm, args);

    TAP_EQ_INT(s_call_center, 1,
            "center_client calls enact_client_center once");
    TAP_OK(s_last_client == &s_client,
            "enact_client_center received the resolved client");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "center_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* An unresolvable client_id never calls enact_client_center */
static void s_test_center_client_unresolvable(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 999);

    resp = ipc_action_center_client(&s_wm, args);

    TAP_EQ_INT(s_call_center, 0,
            "an unresolvable client_id never calls enact_client_center");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "center_client reports failure");
    TAP_NOT_NULL(cJSON_GetObjectItem(resp, "error"),
            "the failure carries a reason");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* Each of the four move-to-monitor-in-a-direction actions calls its
 * own distinct enact_client_move_monitor_* function, and no other */
static void s_test_move_client_to_monitor_directions(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp_n;
    cJSON *resp_s;
    cJSON *resp_e;
    cJSON *resp_w;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);

    resp_n = ipc_action_move_client_to_monitor_north(&s_wm, args);
    TAP_EQ_INT(s_call_move_north, 1,
            "move_client_to_monitor_north calls"
            " enact_client_move_monitor_north once");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp_n, "ok")),
            "move_client_to_monitor_north reports success");

    resp_s = ipc_action_move_client_to_monitor_south(&s_wm, args);
    TAP_EQ_INT(s_call_move_south, 1,
            "move_client_to_monitor_south calls"
            " enact_client_move_monitor_south once");
    TAP_EQ_INT(s_call_move_north, 1,
            "and leaves the north count unchanged");

    resp_e = ipc_action_move_client_to_monitor_east(&s_wm, args);
    TAP_EQ_INT(s_call_move_east, 1,
            "move_client_to_monitor_east calls"
            " enact_client_move_monitor_east once");

    resp_w = ipc_action_move_client_to_monitor_west(&s_wm, args);
    TAP_EQ_INT(s_call_move_west, 1,
            "move_client_to_monitor_west calls"
            " enact_client_move_monitor_west once");
    TAP_EQ_INT(s_call_move_north + s_call_move_south + s_call_move_east,
            3, "with the other three directions still exactly once"
            " each");

    cJSON_Delete(resp_n);
    cJSON_Delete(resp_s);
    cJSON_Delete(resp_e);
    cJSON_Delete(resp_w);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* maximize_client_horz calls enact_client_maximize_horz only */
static void s_test_maximize_client_horz(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);

    resp = ipc_action_maximize_client_horz(&s_wm, args);

    TAP_EQ_INT(s_call_maximize_horz, 1,
            "maximize_client_horz calls enact_client_maximize_horz"
            " once");
    TAP_EQ_INT(s_call_maximize_vert + s_call_maximize, 0,
            "and neither the vertical nor the full maximize action");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "maximize_client_horz reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* maximize_client_vert calls enact_client_maximize_vert only */
static void s_test_maximize_client_vert(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);

    resp = ipc_action_maximize_client_vert(&s_wm, args);

    TAP_EQ_INT(s_call_maximize_vert, 1,
            "maximize_client_vert calls enact_client_maximize_vert"
            " once");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "maximize_client_vert reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* maximize_client calls enact_client_maximize, distinct from either
 * single-axis maximize action */
static void s_test_maximize_client(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);

    resp = ipc_action_maximize_client(&s_wm, args);

    TAP_EQ_INT(s_call_maximize, 1,
            "maximize_client calls enact_client_maximize once");
    TAP_EQ_INT(s_call_maximize_horz + s_call_maximize_vert, 0,
            "and neither single-axis maximize action");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "maximize_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* move_client resolves the client and calls enact_client_move with
 * exactly the x/y given */
static void s_test_move_client_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "x", 42);
    cJSON_AddNumberToObject(args, "y", -7);

    resp = ipc_action_move_client(&s_wm, args);

    TAP_EQ_INT(s_call_move, 1, "move_client calls enact_client_move once");
    TAP_EQ_INT(s_last_move_pos.x, 42,
            "enact_client_move received the given x verbatim");
    TAP_EQ_INT(s_last_move_pos.y, -7,
            "enact_client_move received the given y verbatim");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "move_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing 'x' is rejected before 'y' is even looked at, and before
 * client resolution: an otherwise-resolvable client_id is present,
 * yet enact_client_move never runs */
static void s_test_move_client_missing_x(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "y", 5);

    resp = ipc_action_move_client(&s_wm, args);

    TAP_EQ_INT(s_call_move, 0,
            "a missing 'x' never calls enact_client_move, even with"
            " a resolvable client_id and a valid 'y'");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'x'",
            "the failure names 'x' rather than 'y'");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A present 'x' but a missing 'y' is rejected with its own distinct
 * message, still before client resolution */
static void s_test_move_client_missing_y(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "x", 5);

    resp = ipc_action_move_client(&s_wm, args);

    TAP_EQ_INT(s_call_move, 0,
            "a missing 'y' never calls enact_client_move");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'y'",
            "the failure names 'y' exactly");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* Valid 'x'/'y' but an unresolvable client_id calls enact_client_move
 * zero times and reports the resolution error instead */
static void s_test_move_client_unresolvable(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 999);
    cJSON_AddNumberToObject(args, "x", 5);
    cJSON_AddNumberToObject(args, "y", 5);

    resp = ipc_action_move_client(&s_wm, args);

    TAP_EQ_INT(s_call_move, 0,
            "an unresolvable client_id never calls enact_client_move,"
            " even with valid 'x'/'y'");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "move_client reports failure");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* move_client_to_monitor resolves the client and calls
 * enact_client_move_to_monitor with the exact monitor_index given */
static void s_test_move_client_to_monitor_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "monitor_index", 2);

    resp = ipc_action_move_client_to_monitor(&s_wm, args);

    TAP_EQ_INT(s_call_move_to_monitor, 1,
            "move_client_to_monitor calls"
            " enact_client_move_to_monitor once");
    TAP_EQ_INT((int) s_last_monitor_index, 2,
            "enact_client_move_to_monitor received the given"
            " monitor_index verbatim");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "move_client_to_monitor reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing 'monitor_index' is rejected before client resolution */
static void s_test_move_client_to_monitor_missing_index(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);

    resp = ipc_action_move_client_to_monitor(&s_wm, args);

    TAP_EQ_INT(s_call_move_to_monitor, 0,
            "a missing 'monitor_index' never calls"
            " enact_client_move_to_monitor");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'monitor_index'",
            "the failure names 'monitor_index' exactly");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A negative 'monitor_index' fails the uint check the same way a
 * missing one does */
static void s_test_move_client_to_monitor_negative_index(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "monitor_index", -1);

    resp = ipc_action_move_client_to_monitor(&s_wm, args);

    TAP_EQ_INT(s_call_move_to_monitor, 0,
            "a negative 'monitor_index' never calls"
            " enact_client_move_to_monitor");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "move_client_to_monitor reports failure for a negative"
            " 'monitor_index'");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* move_resize_client resolves the client and calls enact_client_resize
 * with exactly the x/y/w/h given, packed as one geometry_s */
static void s_test_move_resize_client_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "x", 10);
    cJSON_AddNumberToObject(args, "y", 20);
    cJSON_AddNumberToObject(args, "w", 300);
    cJSON_AddNumberToObject(args, "h", 400);

    resp = ipc_action_move_resize_client(&s_wm, args);

    TAP_EQ_INT(s_call_resize, 1,
            "move_resize_client calls enact_client_resize once");
    TAP_EQ_INT(s_last_resize_geom.pos.x, 10,
            "enact_client_resize received the given x verbatim");
    TAP_EQ_INT(s_last_resize_geom.pos.y, 20,
            "enact_client_resize received the given y verbatim");
    TAP_EQ_INT((int) s_last_resize_geom.dim.w, 300,
            "enact_client_resize received the given w verbatim");
    TAP_EQ_INT((int) s_last_resize_geom.dim.h, 400,
            "enact_client_resize received the given h verbatim");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "move_resize_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* move_resize_client checks 'x', then 'y', then 'w', then 'h', in
 * that exact order, before client resolution: each one missing on
 * its own produces its own distinct message, and none but the first
 * missing field's own check ever runs */
static void s_test_move_resize_client_field_order(void)
{
    cJSON *args_no_x = cJSON_CreateObject();
    cJSON *args_no_y = cJSON_CreateObject();
    cJSON *args_no_w = cJSON_CreateObject();
    cJSON *args_no_h = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();

    cJSON_AddNumberToObject(args_no_x, "client_id", 17);
    cJSON_AddNumberToObject(args_no_x, "y", 1);
    cJSON_AddNumberToObject(args_no_x, "w", 1);
    cJSON_AddNumberToObject(args_no_x, "h", 1);
    resp = ipc_action_move_resize_client(&s_wm, args_no_x);
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'x'",
            "a missing 'x' is reported first, ahead of 'y'/'w'/'h'");
    cJSON_Delete(resp);

    cJSON_AddNumberToObject(args_no_y, "client_id", 17);
    cJSON_AddNumberToObject(args_no_y, "x", 1);
    cJSON_AddNumberToObject(args_no_y, "w", 1);
    cJSON_AddNumberToObject(args_no_y, "h", 1);
    resp = ipc_action_move_resize_client(&s_wm, args_no_y);
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'y'",
            "with 'x' present, a missing 'y' is reported next");
    cJSON_Delete(resp);

    cJSON_AddNumberToObject(args_no_w, "client_id", 17);
    cJSON_AddNumberToObject(args_no_w, "x", 1);
    cJSON_AddNumberToObject(args_no_w, "y", 1);
    cJSON_AddNumberToObject(args_no_w, "h", 1);
    resp = ipc_action_move_resize_client(&s_wm, args_no_w);
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'w'",
            "with 'x'/'y' present, a missing 'w' is reported next");
    cJSON_Delete(resp);

    cJSON_AddNumberToObject(args_no_h, "client_id", 17);
    cJSON_AddNumberToObject(args_no_h, "x", 1);
    cJSON_AddNumberToObject(args_no_h, "y", 1);
    cJSON_AddNumberToObject(args_no_h, "w", 1);
    resp = ipc_action_move_resize_client(&s_wm, args_no_h);
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'h'",
            "with 'x'/'y'/'w' present, a missing 'h' is reported last");
    cJSON_Delete(resp);

    TAP_EQ_INT(s_call_resize, 0,
            "none of these four incomplete requests ever reached"
            " enact_client_resize");

    cJSON_Delete(args_no_x);
    cJSON_Delete(args_no_y);
    cJSON_Delete(args_no_w);
    cJSON_Delete(args_no_h);
    s_teardown_wm();
}


/* resize_client, with the client at gravity 'CLIENT_GRAVITY_NORTH_WEST'
 * (a client_gravity_adjust_pos no-op per its own contract), keeps the
 * client's existing (x, y) untouched and only changes (w, h) */
static void s_test_resize_client_north_west_keeps_pos(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "w", 500);
    cJSON_AddNumberToObject(args, "h", 600);

    resp = ipc_action_resize_client(&s_wm, args);

    TAP_EQ_INT(s_call_resize, 1,
            "resize_client calls enact_client_resize once");
    TAP_EQ_INT(s_last_resize_geom.pos.x, 100,
            "at CLIENT_GRAVITY_NORTH_WEST the existing x (100) is"
            " left untouched");
    TAP_EQ_INT(s_last_resize_geom.pos.y, 100,
            "and the existing y (100) is left untouched too");
    TAP_EQ_INT((int) s_last_resize_geom.dim.w, 500,
            "enact_client_resize received the given w verbatim");
    TAP_EQ_INT((int) s_last_resize_geom.dim.h, 600,
            "enact_client_resize received the given h verbatim");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "resize_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* The same resize_client request, with the client's gravity set to
 * 'CLIENT_GRAVITY_SOUTH_EAST' instead, keeps the frame's bottom-right
 * corner fixed rather than its top-left: shrinking from 200x150 down
 * to 100x75 must shift (x, y) forward by exactly the shrink on each
 * axis, per client_gravity_adjust_pos's own real, linked-in math,
 * not a stub */
static void s_test_resize_client_south_east_shifts_pos(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    s_client.layout.gravity = CLIENT_GRAVITY_SOUTH_EAST;
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "w", 100);
    cJSON_AddNumberToObject(args, "h", 75);

    resp = ipc_action_resize_client(&s_wm, args);

    TAP_EQ_INT(s_call_resize, 1,
            "resize_client calls enact_client_resize once");
    TAP_EQ_INT(s_last_resize_geom.pos.x, 200,
            "at CLIENT_GRAVITY_SOUTH_EAST, shrinking w from 200 to"
            " 100 shifts x forward by the 100 lost");
    TAP_EQ_INT(s_last_resize_geom.pos.y, 175,
            "and shrinking h from 150 to 75 shifts y forward by the"
            " 75 lost");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "resize_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing 'w' is rejected before 'h', and before client resolution;
 * resize_client never even reads the client's own gravity in that
 * case */
static void s_test_resize_client_missing_w(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "h", 50);

    resp = ipc_action_resize_client(&s_wm, args);

    TAP_EQ_INT(s_call_resize, 0,
            "a missing 'w' never calls enact_client_resize");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'w'",
            "the failure names 'w' rather than 'h'");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A present 'w' but a missing 'h' is rejected with its own distinct
 * message */
static void s_test_resize_client_missing_h(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 17);
    cJSON_AddNumberToObject(args, "w", 50);

    resp = ipc_action_resize_client(&s_wm, args);

    TAP_EQ_INT(s_call_resize, 0,
            "a missing 'h' never calls enact_client_resize");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'h'",
            "the failure names 'h' exactly");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


int main(void)
{
    TAP_PLAN(63);

    s_test_center_client();
    s_test_center_client_unresolvable();
    s_test_move_client_to_monitor_directions();
    s_test_maximize_client_horz();
    s_test_maximize_client_vert();
    s_test_maximize_client();
    s_test_move_client_ok();
    s_test_move_client_missing_x();
    s_test_move_client_missing_y();
    s_test_move_client_unresolvable();
    s_test_move_client_to_monitor_ok();
    s_test_move_client_to_monitor_missing_index();
    s_test_move_client_to_monitor_negative_index();
    s_test_move_resize_client_ok();
    s_test_move_resize_client_field_order();
    s_test_resize_client_north_west_keeps_pos();
    s_test_resize_client_south_east_shifts_pos();
    s_test_resize_client_missing_w();
    s_test_resize_client_missing_h();

    return TAP_DONE();
}
