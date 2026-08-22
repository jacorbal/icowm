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
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/args.h>
#include <ipc/dispatch.h>
#include <ipc/resolve.h>
#include <ipc/response.h>
#include <ipc/actions/client/geom.h>


/* --- The five that only need "client_id", via the shared wrapper --- */

static void s_center(const wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_center(client);
}

static void s_move_monitor_north(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_move_monitor_north(client);
}

static void s_move_monitor_south(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_move_monitor_south(client);
}

static void s_move_monitor_east(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_move_monitor_east(client);
}

static void s_move_monitor_west(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_move_monitor_west(client);
}

static void s_maximize_horz(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_maximize_horz(client);
}

static void s_maximize_vert(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_maximize_vert(client);
}

static void s_maximize(const wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_maximize(client);
}

cJSON *ipc_action_center_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_center);
}

cJSON *ipc_action_move_client_to_monitor_north(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_move_monitor_north);
}

cJSON *ipc_action_move_client_to_monitor_south(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_move_monitor_south);
}

cJSON *ipc_action_move_client_to_monitor_east(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_move_monitor_east);
}

cJSON *ipc_action_move_client_to_monitor_west(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_move_monitor_west);
}

cJSON *ipc_action_maximize_client_horz(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_maximize_horz);
}

cJSON *ipc_action_maximize_client_vert(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_maximize_vert);
}

cJSON *ipc_action_maximize_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_maximize);
}


/* --- The three with their own extra arguments --- */

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


cJSON *ipc_action_move_client_to_monitor(const wm_td *wm, const cJSON *args)
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


cJSON *ipc_action_resize_client(const wm_td *wm, const cJSON *args)
{
    uint32_t w;
    uint32_t h;
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

    enact_client_resize(client, (struct geometry_s) {
                client->layout.geometry.cur.pos, { w, h } });
    return ipc_response_ok();
}
