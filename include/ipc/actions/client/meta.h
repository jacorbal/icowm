/**
 * @file ipc/actions/client/meta.h
 *
 * @brief IPC commands mirroring @c cmds/client/meta.h's own actions
 *
 * Unlike @c ipc/actions/client/basic.h and its siblings, every one of
 * these needs its own string argument alongside @c client_id, so none
 * of them go through @c ipc/dispatch.h's shared wrapper.  Each resolves
 * the client and reads its own argument directly instead.
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


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/** @c rename_client: arguments @c client_id, @c name */
cJSON *ipc_action_rename_client(wm_td *wm, const cJSON *args);

/** @c reclass_client: arguments @c client_id, @c class_name,
 *  @c instance_name */
cJSON *ipc_action_reclass_client(wm_td *wm, const cJSON *args);

/** @c rerole_client: arguments @c client_id, @c role */
cJSON *ipc_action_rerole_client(wm_td *wm, const cJSON *args);

/** @c set_client_icon: arguments @c client_id, @c icon_name */
cJSON *ipc_action_set_client_icon(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_META_H */
