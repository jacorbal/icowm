/**
 * @file ipc/actions/client/layer.h
 *
 * @brief IPC commands mirroring @c cmds/client/layer.h's own actions
 *
 * @see @c ipc/dispatch.h for the shared shape every one of these
 *      follows
 *
 * @defgroup ipc_actions_client_layer IPC client stacking-order actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_CLIENT_LAYER_H
#define IPC_ACTIONS_CLIENT_LAYER_H

/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>



/**
 * @brief @c raise_client: raise the client to the top of its layer
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_raise_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c lower_client: lower the client to the bottom of its
 *        layer
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_lower_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c set_layer_above_client: move the client to the "always
 *        on top" layer
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_set_layer_above_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c set_layer_normal_client: move the client back to the
 *        ordinary layer
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_set_layer_normal_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c set_layer_below_client: move the client to the "always
 *        below" layer
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_set_layer_below_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c cycle_layer_client: cycle the client through
 *        above/normal/below
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_cycle_layer_client(const wm_td *wm,
        const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_LAYER_H */
