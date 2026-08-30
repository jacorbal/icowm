/**
 * @file ipc/actions/client/flags.h
 *
 * @brief IPC commands mirroring @c cmds/client/flags.h's own actions
 *
 * Every one of these is a thin @a ipc_dispatch_client_action call
 * around the matching @a enact_client_* function.  Resolve the id,
 * run the one action, report success or the reason it could not be
 * found.
 *
 * @see @c ipc/dispatch.h for that shared mechanism
 *
 * @defgroup ipc_actions_client_flags IPC client pin/urgency flag
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

#ifndef IPC_ACTIONS_CLIENT_FLAGS_H
#define IPC_ACTIONS_CLIENT_FLAGS_H


/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/**
 * @brief @c pin_client: make the client visible on every desktop
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_pin_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c unpin_client: undo @c pin_client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unpin_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c toggle_pin_client: toggle between @c pin_client and
 *        @c unpin_client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_toggle_pin_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c urge_client: mark the client urgent
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_urge_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c unurge_client: undo @c urge_client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unurge_client(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_FLAGS_H */
