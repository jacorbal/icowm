/**
 * @file ipc/actions/client/visibility.c
 *
 * @brief IPC commands mirroring cmds/client/visibility.h
 *        implementation
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
#include <ipc/actions/client/visibility.h>


/**
 * @brief Iconify the client, per @c ipc_client_action_fn's own
 *        contract
 *
 * @note Complexity: @e O(1)
 */
static void s_iconify(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_iconify(client);
}


/**
 * @brief Hide the client, per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_hide(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_hide(client);
}


/**
 * @brief Unhide the client, per @c ipc_client_action_fn's own
 *        contract
 *
 * @note Complexity: @e O(1)
 */
static void s_unhide(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_unhide(client);
}


/* Iconify (minimize) the client */
cJSON *ipc_action_iconify_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_iconify);
}


/* Hide the client without iconifying it */
cJSON *ipc_action_hide_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_hide);
}


/* Undo 'ipc_action_hide_client' */
cJSON *ipc_action_unhide_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_unhide);
}
