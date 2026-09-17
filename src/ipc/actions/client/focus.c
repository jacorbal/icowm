/**
 * @file ipc/actions/client/focus.c
 *
 * @brief IPC commands mirroring cmds/client/focus.h implementation
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
#include <ipc/actions/client/focus.h>


/**
 * @brief Close the client, per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_close(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_close(client);
}


/**
 * @brief Kill the client, per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_kill(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_kill(client);
}


/**
 * @brief Restore the client, per @c ipc_client_action_fn's own
 *        contract
 *
 * @note Complexity: @e O(1)
 */
static void s_restore(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_restore(client);
}


/**
 * @brief Focus the client, per @c ipc_client_action_fn's own contract
 *
 * @note Complexity: @e O(1)
 */
static void s_focus(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_focus(client);
}


/**
 * @brief Unfocus the client, per @c ipc_client_action_fn's own
 *        contract
 *
 * @note Complexity: @e O(1)
 */
static void s_unfocus(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    (void) wm; (void) stage; (void) desktop;
    enact_client_unfocus(client);
}


/* Politely ask the client to close, or destroy its window directly if
 * it does not support that */
cJSON *ipc_action_close_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_close);
}


/* Forcibly terminate the client's own X connection */
cJSON *ipc_action_kill_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_kill);
}


/* Restore the client if it was iconified */
cJSON *ipc_action_deiconify_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_restore);
}


/* Focus and raise the client */
cJSON *ipc_action_focus_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_focus);
}


/* Clear input focus from the client, if it had it */
cJSON *ipc_action_unfocus_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_unfocus);
}
