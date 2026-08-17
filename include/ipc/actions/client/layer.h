/**
 * @file ipc/actions/client/layer.h
 *
 * @brief IPC commands mirroring @c cmds/client/layer.h's own actions
 *
 * @see @c ipc/actions/client/basic.h for the shared shape every one of
 *      these follows
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


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/** @c raise_client */
cJSON *ipc_action_raise_client(wm_td *wm, const cJSON *args);

/** @c lower_client */
cJSON *ipc_action_lower_client(wm_td *wm, const cJSON *args);

/** @c set_layer_above_client: move the client to the "always on top" layer */
cJSON *ipc_action_set_layer_above_client(wm_td *wm, const cJSON *args);

/** @c set_layer_normal_client: move the client back to the ordinary layer */
cJSON *ipc_action_set_layer_normal_client(wm_td *wm, const cJSON *args);

/** @c set_layer_below_client: move the client to the "always below" layer */
cJSON *ipc_action_set_layer_below_client(wm_td *wm, const cJSON *args);

/** @c cycle_layer_client: cycle the client through above/normal/below */
cJSON *ipc_action_cycle_layer_client(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_LAYER_H */
