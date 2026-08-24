/**
 * @file ipc/actions/client/meta.h
 *
 * @brief IPC commands mirroring @c cmds/client/meta.h's own actions
 *
 * Unlike @c ipc/dispatch.h's other client actions, every one of
 * these needs its own string argument alongside @c client_id, so
 * none of them go through its shared wrapper.  Each resolves the
 * client and reads its own argument directly instead.
 *
 * @defgroup ipc_actions_client_meta IPC client metadata actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_CLIENT_META_H
#define IPC_ACTIONS_CLIENT_META_H

/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>



/**
 * @brief @c rename_client: rename the client
 *
 * @param wm   Window manager instance
 * @param args The request object; arguments @c client_id, @c name
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_rename_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c reclass_client: change the client's own @c WM_CLASS
 *
 * @param wm   Window manager instance
 * @param args The request object; arguments @c client_id,
 *             @c class_name, @c instance_name
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_reclass_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c rerole_client: change the client's own @c WM_WINDOW_ROLE
 *
 * @param wm   Window manager instance
 * @param args The request object; arguments @c client_id, @c role
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_rerole_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c set_client_icon: override the client's own icon
 *
 * @param wm   Window manager instance
 * @param args The request object; arguments @c client_id,
 *             @c icon_name
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_set_client_icon(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_META_H */
