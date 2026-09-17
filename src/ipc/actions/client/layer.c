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
#include <enact/client.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <ipc/dispatch.h>
#include <ipc/actions/client/layer.h>

/**
 * @brief Raise the client, per @c ipc_client_action_fn's own
 *        contract
 *
 * @note Complexity: @e O(1)
 */
static void s_raise(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_raise(client);
}


/**
 * @brief Lower the client, per @c ipc_client_action_fn's own
 *        contract
 *
 * @note Complexity: @e O(1)
 */
static void s_lower(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_lower(client);
}


/**
 * @brief Move the client to the "always on top" layer, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_layer_above(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_layer_above(client);
}


/**
 * @brief Move the client back to the ordinary layer, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_layer_normal(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_layer_normal(client);
}


/**
 * @brief Move the client to the "always below" layer, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_layer_below(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_layer_below(client);
}


/**
 * @brief Cycle the client through above/normal/below, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_cycle_layer(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_cycle_layer(client);
}


/* Raise the client to the top of its layer */
cJSON *ipc_action_raise_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_raise);
}


/* Lower the client to the bottom of its layer */
cJSON *ipc_action_lower_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_lower);
}


/* Move the client to the "always on top" layer */
cJSON *ipc_action_set_layer_above_client(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_layer_above);
}


/* Move the client back to the ordinary layer */
cJSON *ipc_action_set_layer_normal_client(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_layer_normal);
}


/* Move the client to the "always below" layer */
cJSON *ipc_action_set_layer_below_client(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_layer_below);
}


/* Cycle the client through above/normal/below */
cJSON *ipc_action_cycle_layer_client(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_cycle_layer);
}
