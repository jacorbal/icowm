/**
 * @file ipc/actions/client/state.h
 *
 * @brief IPC commands mirroring @c cmds/client/state.h's own actions
 *
 * @see @c ipc/actions/client/basic.h for the shared shape every one of
 *      these follows
 *
 * @defgroup ipc_actions_client_state IPC client shade/fullscreen/decoration
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


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/** @c shade_client: roll the client up into just its own titlebar */
cJSON *ipc_action_shade_client(const wm_td *wm, const cJSON *args);

/** @c unshade_client: undo @c shade_client */
cJSON *ipc_action_unshade_client(const wm_td *wm, const cJSON *args);

/** @c toggle_shade_client */
cJSON *ipc_action_toggle_shade_client(const wm_td *wm, const cJSON *args);

/** @c fullscreen_client */
cJSON *ipc_action_fullscreen_client(const wm_td *wm, const cJSON *args);

/** @c unfullscreen_client: undo @c fullscreen_client */
cJSON *ipc_action_unfullscreen_client(const wm_td *wm, const cJSON *args);

/** @c toggle_fullscreen_client */
cJSON *ipc_action_toggle_fullscreen_client(const wm_td *wm, const cJSON *args);

/** @c toggle_decorate_client: show or hide the client's own titlebar
 *  and border */
cJSON *ipc_action_toggle_decorate_client(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_STATE_H */
