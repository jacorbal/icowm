/**
 * @file ipc/actions/client/geom.h
 *
 * @brief IPC commands mirroring @c cmds/client/geom.h's own actions
 *
 * @c move_client, @c move_client_to_monitor, @c move_resize_client, and
 * @c resize_client need their own numeric arguments alongside
 * @c client_id, so those four do not go through @c ipc/dispatch.h's
 * shared wrapper; the rest do, the same as @c ipc/dispatch.h's own.
 *
 * @defgroup ipc_actions_client_geom IPC client position/size actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_CLIENT_GEOM_H
#define IPC_ACTIONS_CLIENT_GEOM_H


/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/**
 * @brief @c move_client: move the client, keeping its own size
 *
 * @param wm   Window manager instance
 * @param args The request object; arguments @c client_id, @c x,
 *             @c y (both signed)
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_move_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c center_client: center the client on its current screen
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_center_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c move_client_to_monitor: move the client to a specific
 *        monitor
 *
 * @param wm   Window manager instance
 * @param args The request object; arguments @c client_id,
 *             @c monitor_index
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_move_client_to_monitor(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c move_client_to_monitor_north: move the client to the
 *        monitor north of its current one
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_move_client_to_monitor_north(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c move_client_to_monitor_south: move the client to the
 *        monitor south of its current one
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_move_client_to_monitor_south(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c move_client_to_monitor_east: move the client to the
 *        monitor east of its current one
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_move_client_to_monitor_east(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c move_client_to_monitor_west: move the client to the
 *        monitor west of its current one
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_move_client_to_monitor_west(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c move_resize_client: move and resize the client together,
 *        in one request
 *
 * @param wm   Window manager instance
 * @param args The request object; arguments @c client_id, @c x,
 *             @c y (both signed, the new top-left corner), @c w,
 *             @c h (both unsigned)
 *
 * @return The standard success or failure response
 *
 * @see @c resize_client below for one that only ever resizes,
 *      leaving position alone
 */
cJSON *ipc_action_move_resize_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c resize_client: resize the client, from wherever its own
 *        top-left corner already is
 *
 * @param wm   Window manager instance
 * @param args The request object; arguments @c client_id, @c w,
 *             @c h (both unsigned)
 *
 * @return The standard success or failure response
 *
 * @see @c move_resize_client above to move and resize together in
 *      one request instead
 */
cJSON *ipc_action_resize_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c maximize_client_horz: maximize the client horizontally
 *        only
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_maximize_client_horz(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c maximize_client_vert: maximize the client vertically
 *        only
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_maximize_client_vert(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c maximize_client: maximize the client both horizontally
 *        and vertically
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_maximize_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c unmaximize_client_horz: restore the client's width, if it is
 *        maximized horizontally
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unmaximize_client_horz(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c unmaximize_client_vert: restore the client's height, if it is
 *        maximized vertically
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unmaximize_client_vert(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c unmaximize_client: restore the client from being maximized on
 *        either axis
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unmaximize_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c toggle_maximize_client_horz: maximize the client horizontally, or
 *        restore its width if it already is
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_toggle_maximize_client_horz(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c toggle_maximize_client_vert: maximize the client vertically, or
 *        restore its height if it already is
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_toggle_maximize_client_vert(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c toggle_maximize_client: maximize the client both ways, or restore
 *        it if it already is
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_toggle_maximize_client(const wm_td *wm,
        const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_GEOM_H */
