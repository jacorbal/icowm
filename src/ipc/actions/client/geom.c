/**
 * @file ipc/actions/client/geom.c
 *
 * @brief IPC commands mirroring cmds/client/geom.h implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* Type includes */
#include <types/pair.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <enact/client.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/args.h>
#include <ipc/dispatch.h>
#include <ipc/resolve.h>
#include <ipc/response.h>
#include <ipc/actions/client/geom.h>


/* The five that only need 'client_id', via the shared wrapper */

/**
 * @brief Center the client, per @c ipc_client_action_fn's own
 *        contract
 *
 * @note Complexity: @e O(1)
 */
static void s_center(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_center(client);
}


/**
 * @brief Move the client to the monitor north of its current one,
 *        per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_move_monitor_north(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_move_monitor_north(client);
}


/**
 * @brief Move the client to the monitor south of its current one,
 *        per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_move_monitor_south(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_move_monitor_south(client);
}


/**
 * @brief Move the client to the monitor east of its current one, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_move_monitor_east(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_move_monitor_east(client);
}


/**
 * @brief Move the client to the monitor west of its current one, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_move_monitor_west(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_move_monitor_west(client);
}


/**
 * @brief Maximize the client horizontally only, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_maximize_horz(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_maximize_horz(client);
}


/**
 * @brief Maximize the client vertically only, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_maximize_vert(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_maximize_vert(client);
}


/**
 * @brief Maximize the client both horizontally and vertically, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_maximize(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_maximize(client);
}


/* Center the client on its current screen */
cJSON *ipc_action_center_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_center);
}


/* Move the client to the monitor north of its current one */
cJSON *ipc_action_move_client_to_monitor_north(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_move_monitor_north);
}


/* Move the client to the monitor south of its current one */
cJSON *ipc_action_move_client_to_monitor_south(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_move_monitor_south);
}


/* Move the client to the monitor east of its current one */
cJSON *ipc_action_move_client_to_monitor_east(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_move_monitor_east);
}


/* Move the client to the monitor west of its current one */
cJSON *ipc_action_move_client_to_monitor_west(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_move_monitor_west);
}


/* Maximize the client horizontally only */
cJSON *ipc_action_maximize_client_horz(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_maximize_horz);
}


/* Maximize the client vertically only */
cJSON *ipc_action_maximize_client_vert(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_maximize_vert);
}


/* Maximize the client both horizontally and vertically */
cJSON *ipc_action_maximize_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_maximize);
}


/* The three with their own extra arguments */

/* Move the client, keeping its own size */
cJSON *ipc_action_move_client(const wm_td *wm, const cJSON *args)
{
    int32_t x;
    int32_t y;
    client_td *client;
    cJSON *error = NULL;

    if (!ipc_args_get_int(args, "x", &x)) {
        return ipc_response_error("missing or invalid 'x'");
    }
    if (!ipc_args_get_int(args, "y", &y)) {
        return ipc_response_error("missing or invalid 'y'");
    }

    client = ipc_resolve_client(wm, args, NULL, NULL, &error);
    if (client == NULL) {
        return error;
    }

    enact_client_move(client, (struct position_s) { x, y });
    return ipc_response_ok();
}


/* Move the client to a specific monitor */
cJSON *ipc_action_move_client_to_monitor(const wm_td *wm,
        const cJSON *args)
{
    uint32_t monitor_index;
    client_td *client;
    cJSON *error = NULL;

    if (!ipc_args_get_uint(args, "monitor_index", &monitor_index)) {
        return ipc_response_error("missing or invalid 'monitor_index'");
    }

    client = ipc_resolve_client(wm, args, NULL, NULL, &error);
    if (client == NULL) {
        return error;
    }

    enact_client_move_to_monitor(client, monitor_index);
    return ipc_response_ok();
}


/* Move and resize the client together, in one request */
cJSON *ipc_action_move_resize_client(const wm_td *wm, const cJSON *args)
{
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    client_td *client;
    cJSON *error = NULL;

    if (!ipc_args_get_int(args, "x", &x)) {
        return ipc_response_error("missing or invalid 'x'");
    }
    if (!ipc_args_get_int(args, "y", &y)) {
        return ipc_response_error("missing or invalid 'y'");
    }
    if (!ipc_args_get_uint(args, "w", &w)) {
        return ipc_response_error("missing or invalid 'w'");
    }
    if (!ipc_args_get_uint(args, "h", &h)) {
        return ipc_response_error("missing or invalid 'h'");
    }

    client = ipc_resolve_client(wm, args, NULL, NULL, &error);
    if (client == NULL) {
        return error;
    }

    enact_client_resize(client,
            (struct geometry_s) { { x, y }, { w, h } });
    return ipc_response_ok();
}


/* Resize the client, from wherever its own top-left corner already
 * is */
cJSON *ipc_action_resize_client(const wm_td *wm, const cJSON *args)
{
    uint32_t w;
    uint32_t h;
    struct position_s pos;
    client_td *client;
    cJSON *error = NULL;

    if (!ipc_args_get_uint(args, "w", &w)) {
        return ipc_response_error("missing or invalid 'w'");
    }
    if (!ipc_args_get_uint(args, "h", &h)) {
        return ipc_response_error("missing or invalid 'h'");
    }

    client = ipc_resolve_client(wm, args, NULL, NULL, &error);
    if (client == NULL) {
        return error;
    }

    /* No 'x'/'y' in this request at all, unlike the sibling action just
     * above that takes all four: the baseline below keeps the client's
     * own current position exactly as it is, correct outright for
     * 'CLIENT_GRAVITY_NORTH_WEST'/'STATIC', for which this call is
     * a no-op, and gravity-adjusted from there for whatever other
     * gravity the client's own 'WM_NORMAL_HINTS' may have actually
     * requested; the same reasoning, and the same function,
     * 'handler_configure_request' already applies to a client-initiated
     * resize of this identical shape. */
    pos = client->layout.geometry.cur.pos;
    client_gravity_adjust_pos(&pos.x, &pos.y,
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            w, h, client->layout.gravity);

    enact_client_resize(client, (struct geometry_s) { pos, { w, h } });
    return ipc_response_ok();
}
