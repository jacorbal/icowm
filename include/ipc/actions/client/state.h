/**
 * @file ipc/actions/client/state.h
 *
 * @brief IPC commands mirroring @c cmds/client/state.h's own actions
 *
 * @see @c ipc/dispatch.h for the shared shape every one of these
 *      follows
 *
 * @defgroup ipc_actions_client_state IPC client shade/fullscreen/
 *           decoration
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_CLIENT_STATE_H
#define IPC_ACTIONS_CLIENT_STATE_H


/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/**
 * @brief @c shade_client: roll the client up into just its own
 *        titlebar
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_shade_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c unshade_client: undo @c shade_client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unshade_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c toggle_shade_client: toggle between @c shade_client and
 *        @c unshade_client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_toggle_shade_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c fullscreen_client: make the client fill the whole screen
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_fullscreen_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c unfullscreen_client: undo @c fullscreen_client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_unfullscreen_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c toggle_fullscreen_client: toggle between
 *        @c fullscreen_client and @c unfullscreen_client
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_toggle_fullscreen_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c toggle_decorate_client: show or hide the client's own
 *        titlebar and border
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_toggle_decorate_client(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c decorate_client: show the client's own titlebar and border
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_decorate_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c undecorate_client: hide the client's own titlebar and
 *        border
 *
 * @param wm   Window manager instance
 * @param args The request object; argument @c client_id
 *
 * @return The standard success or failure response
 */
cJSON *ipc_action_undecorate_client(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_STATE_H */
