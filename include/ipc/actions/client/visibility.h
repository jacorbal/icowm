/**
 * @file ipc/actions/client/visibility.h
 *
 * @brief IPC commands mirroring @c cmds/client/visibility.h's own
 *        actions
 *
 * Every one of these is a thin @a ipc_dispatch_client_action call
 * around the matching @a enact_client_* function: resolve @c client_id,
 * run the one action, report success or the reason it could not be
 * found.
 *
 * @see @c ipc/dispatch.h for that shared mechanism
 *
 * @defgroup ipc_actions_client_visibility IPC client iconify/hide
 *           actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_CLIENT_VISIBILITY_H
#define IPC_ACTIONS_CLIENT_VISIBILITY_H

/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>



/**
 * @brief @c iconify_client: iconify (minimize) the client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_iconify_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c hide_client: hide the client without iconifying it
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_hide_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c unhide_client: undo @c hide_client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unhide_client(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_VISIBILITY_H */
