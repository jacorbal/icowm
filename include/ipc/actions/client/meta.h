/**
 * @file ipc/actions/client/meta.h
 *
 * @brief IPC commands mirroring @c cmds/client/meta.h's own actions
 *
 * Unlike ipc/actions/client/basic.h and its siblings, every one of
 * these needs its own string argument alongside @c "client_id", so
 * none of them go through ipc/dispatch.h's shared wrapper; each
 * resolves the client and reads its own argument directly instead.
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
/** "rename_client": arguments "client_id", "name" */
cJSON *ipc_action_rename_client(wm_td *wm, const cJSON *args);

/** "reclass_client": arguments "client_id", "class_name",
 *  "instance_name" */
cJSON *ipc_action_reclass_client(wm_td *wm, const cJSON *args);

/** "rerole_client": arguments "client_id", "role" */
cJSON *ipc_action_rerole_client(wm_td *wm, const cJSON *args);

/** "set_client_icon": arguments "client_id", "icon_name" */
cJSON *ipc_action_set_client_icon(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_META_H */
