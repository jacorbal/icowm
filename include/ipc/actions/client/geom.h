/**
 * @file ipc/actions/client/geom.h
 *
 * @brief IPC commands mirroring @c cmds/client/geom.h's own actions
 *
 * @c move_client, @c move_client_to_monitor, @c move_resize_client,
 * and @c resize_client need their own numeric arguments alongside
 * @c "client_id", so those four do not go through ipc/dispatch.h's
 * shared wrapper; the rest do, the same as ipc/actions/client/
 * basic.h's own.
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
/** "move_client": arguments "client_id", "x", "y" (both signed) */
cJSON *ipc_action_move_client(wm_td *wm, const cJSON *args);

/** "center_client": center the client on its current screen */
cJSON *ipc_action_center_client(wm_td *wm, const cJSON *args);

/** "move_client_to_monitor": arguments "client_id", "monitor_index" */
cJSON *ipc_action_move_client_to_monitor(wm_td *wm, const cJSON *args);

/** "move_client_to_next_monitor" */
cJSON *ipc_action_move_client_to_next_monitor(wm_td *wm, const cJSON *args);

/** "move_resize_client": arguments "client_id", "x", "y" (both
 *  signed, the new top-left corner), "w", "h" (both unsigned) --
 *  moves and resizes together, in one request.  See @c resize_client
 *  below for one that only ever resizes, leaving position alone. */
cJSON *ipc_action_move_resize_client(wm_td *wm, const cJSON *args);

/** "resize_client": arguments "client_id", "w", "h" (both unsigned)
 *  -- resizes only, from wherever the client's own top-left corner
 *  already is; see @c move_resize_client above to move and resize
 *  together in one request instead. */
cJSON *ipc_action_resize_client(wm_td *wm, const cJSON *args);

/** "maximize_client_horz" */
cJSON *ipc_action_maximize_client_horz(wm_td *wm, const cJSON *args);

/** "maximize_client_vert" */
cJSON *ipc_action_maximize_client_vert(wm_td *wm, const cJSON *args);

/** "maximize_client": both horizontally and vertically */
cJSON *ipc_action_maximize_client(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_GEOM_H */
