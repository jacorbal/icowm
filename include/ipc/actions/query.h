/**
 * @file ipc/actions/query.h
 *
 * @brief The read-only IPC commands: version, listings, focus status
 *
 * Unlike every other file under @c ipc/actions/, none of these mirror
 * a single @a enact_* function; they read state that already exists
 * rather than acting on anything.
 *
 * @defgroup ipc_actions_query IPC read-only queries
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_QUERY_H
#define IPC_ACTIONS_QUERY_H

/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>



/** @c get_version: no arguments */
cJSON *ipc_action_get_version(const wm_td *wm, const cJSON *args);

/** @c list_desktops: no arguments */
cJSON *ipc_action_list_desktops(const wm_td *wm, const cJSON *args);

/** @c list_clients: no arguments */
cJSON *ipc_action_list_clients(const wm_td *wm, const cJSON *args);

/** @c get_focused: no arguments */
cJSON *ipc_action_get_focused(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_QUERY_H */
