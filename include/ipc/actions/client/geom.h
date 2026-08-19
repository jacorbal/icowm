/**
 * @file ipc/actions/client/geom.h
 *
 * @brief IPC commands mirroring @c cmds/client/geom.h's own actions
 *
 * @c move_client, @c move_client_to_monitor, @c move_resize_client, and
 * @c resize_client need their own numeric arguments alongside
 * @c client_id, so those four do not go through @c ipc/dispatch.h's
 * shared wrapper; the rest do, the same as
 * @c ipc/actions/client/basic.h's own.
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


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/** @c move_client: arguments @c client_id, @c x, @c y (both signed) */
cJSON *ipc_action_move_client(const wm_td *wm, const cJSON *args);

/** @c center_client: center the client on its current screen */
cJSON *ipc_action_center_client(const wm_td *wm, const cJSON *args);

/** @c move_client_to_monitor: arguments @c client_id, @c monitor_index */
cJSON *ipc_action_move_client_to_monitor(const wm_td *wm, const cJSON *args);

/** @c move_client_to_next_monitor */
cJSON *ipc_action_move_client_to_next_monitor(const wm_td *wm,
        const cJSON *args);

/**
 * @brief @c move_resize_client: arguments @c client_id, @c x, @c y
 *        (both signed, the new top-left corner), @c w, @c h (both
 *        unsigned); moves and resizes together, in one request
 *
 * @see @c resize_client below for one that only ever resizes, leaving
 *      position alone
 */
cJSON *ipc_action_move_resize_client(const wm_td *wm, const cJSON *args);

/**
 * @brief @c resize_client: arguments @c client_id, @c w, @c h (both
 *        unsigned); resizes only, from wherever the client's own
 *        top-left corner already is
 *
 * @see @c move_resize_client above to move and resize together in one
 *      request instead
 */
cJSON *ipc_action_resize_client(const wm_td *wm, const cJSON *args);

/** @c maximize_client_horz */
cJSON *ipc_action_maximize_client_horz(const wm_td *wm, const cJSON *args);

/** @c maximize_client_vert */
cJSON *ipc_action_maximize_client_vert(const wm_td *wm, const cJSON *args);

/** @c maximize_client: both horizontally and vertically */
cJSON *ipc_action_maximize_client(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_GEOM_H */
