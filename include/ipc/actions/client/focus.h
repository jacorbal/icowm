/**
 * @file ipc/actions/client/focus.h
 *
 * @brief IPC commands mirroring @c cmds/client/focus.h's own actions
 *
 * Every one of these is a thin @a ipc_dispatch_client_action call
 * around the matching @a enact_client_* function: resolve @c client_id,
 * run the one action, report success or the reason it could not be
 * found.
 *
 * @see @c ipc/dispatch.h for that shared mechanism
 *
 * @defgroup ipc_actions_client_focus IPC client close/kill/restore/
 *           focus actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_CLIENT_FOCUS_H
#define IPC_ACTIONS_CLIENT_FOCUS_H

/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>



/**
 * @brief @c close_client: politely ask the client to close, or
 *        destroy its window directly if it does not support that
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_close_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c kill_client: forcibly terminate the client's own X
 *        connection
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_kill_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c deiconify_client: restore the client if it was iconified
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_deiconify_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c focus_client: focus and raise the client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_focus_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c unfocus_client: clear input focus from the client, if it
 *        had it
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unfocus_client(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_FOCUS_H */
