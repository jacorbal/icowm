/**
 * @file ipc/actions/client/layer.c
 *
 * @brief IPC commands mirroring cmds/client/layer.h implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/dispatch.h>
#include <ipc/actions/client/layer.h>


static void s_raise(const wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_raise(client);
}

static void s_lower(const wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_lower(client);
}

static void s_layer_above(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_layer_above(client);
}

static void s_layer_normal(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_layer_normal(client);
}

static void s_layer_below(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_layer_below(client);
}

static void s_cycle_layer(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_cycle_layer(client);
}


cJSON *ipc_action_raise_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_raise);
}

cJSON *ipc_action_lower_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_lower);
}

cJSON *ipc_action_set_layer_above_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_layer_above);
}

cJSON *ipc_action_set_layer_normal_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_layer_normal);
}

cJSON *ipc_action_set_layer_below_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_layer_below);
}

cJSON *ipc_action_cycle_layer_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_cycle_layer);
}
