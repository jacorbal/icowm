/**
 * @file ipc/actions/client/flags.c
 *
 * @brief IPC commands mirroring cmds/client/flags.h implementation
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
#include <ipc/actions/client/flags.h>


/**
 * @brief Pin the client, per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_pin(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_pin(client);
}


/**
 * @brief Unpin the client, per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_unpin(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_unpin(client);
}


/**
 * @brief Toggle the client's pin state, per @c ipc_client_action_fn's
 *        own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_toggle_pin(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_toggle_pin(client);
}


/**
 * @brief Mark the client urgent, per @c ipc_client_action_fn's own
 *        contract
 *
 * @note Complexity: @e O(1)
 */
static void s_set_urgent(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_urge(client);
}


/**
 * @brief Clear the client's urgent mark, per
 *        @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_clear_urgent(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_unurge(client);
}


/**
 * @brief Set the client's urgency, or clear it if it is already set,
 *        per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_toggle_urgent(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    if (client_is_urgent(client)) {
        enact_client_unurge(client);
    } else {
        enact_client_urge(client);
    }
}


/* Make the client visible on every desktop */
cJSON *ipc_action_pin_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_pin);
}


/* Undo 'ipc_action_pin_client' */
cJSON *ipc_action_unpin_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_unpin);
}


/* Toggle the client's pin state */
cJSON *ipc_action_toggle_pin_client(const wm_td *wm,
        const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_toggle_pin);
}


/* Mark the client urgent */
cJSON *ipc_action_urge_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_set_urgent);
}


/* Undo 'ipc_action_urge_client' */
cJSON *ipc_action_unurge_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_clear_urgent);
}


/* Set the client's urgency, or clear it if it is already set */
cJSON *ipc_action_toggle_urge_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_toggle_urgent);
}
