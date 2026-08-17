/**
 * @file ipc/actions/client/basic.h
 *
 * @brief IPC commands mirroring @c cmds/client/basic.h's own actions
 *
 * Every one of these is a thin @a ipc_dispatch_client_action call
 * around the matching @a enact_client_* function: resolve @c client_id,
 * run the one action, report success or the reason it could not be
 * found.
 *
 * @see @c ipc/dispatch.h for that shared mechanism
 *
 * @defgroup ipc_actions_client_basic IPC client lifecycle/flag actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_CLIENT_BASIC_H
#define IPC_ACTIONS_CLIENT_BASIC_H


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/** @c close_client: politely ask the client to close, or destroy its
 *  window directly if it does not support that */
cJSON *ipc_action_close_client(wm_td *wm, const cJSON *args);

/** @c kill_client: forcibly terminate the client's own X connection */
cJSON *ipc_action_kill_client(wm_td *wm, const cJSON *args);

/** @c deiconify_client: restore the client if it was iconified */
cJSON *ipc_action_deiconify_client(wm_td *wm, const cJSON *args);

/** @c focus_client: focus and raise the client */
cJSON *ipc_action_focus_client(wm_td *wm, const cJSON *args);

/** @c unfocus_client: clear input focus from the client, if it had it */
cJSON *ipc_action_unfocus_client(wm_td *wm, const cJSON *args);

/** @c iconify_client: iconify (minimize) the client */
cJSON *ipc_action_iconify_client(wm_td *wm, const cJSON *args);

/** @c hide_client: hide the client without iconifying it */
cJSON *ipc_action_hide_client(wm_td *wm, const cJSON *args);

/** @c unhide_client: undo @c hide_client */
cJSON *ipc_action_unhide_client(wm_td *wm, const cJSON *args);

/** @c pin_client: make the client visible on every desktop */
cJSON *ipc_action_pin_client(wm_td *wm, const cJSON *args);

/** @c unpin_client: undo @c pin_client */
cJSON *ipc_action_unpin_client(wm_td *wm, const cJSON *args);

/** @c toggle_pin_client */
cJSON *ipc_action_toggle_pin_client(wm_td *wm, const cJSON *args);

/** @c urge_client: mark the client urgent */
cJSON *ipc_action_urge_client(wm_td *wm, const cJSON *args);

/** @c unurge_client: undo @c urge_client */
cJSON *ipc_action_unurge_client(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_BASIC_H */
