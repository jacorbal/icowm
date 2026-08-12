/**
 * @file ipc/actions/client/basic.h
 *
 * @brief IPC commands mirroring @c cmds/client/basic.h's own actions
 *
 * Every one of these is a thin @c ipc_dispatch_client_action call
 * around the matching @c enact_client_* function: resolve @c
 * "client_id", run the one action, report success or the reason it
 * could not be found.  See ipc/dispatch.h for that shared mechanism.
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
/** "close_client": politely ask the client to close, or destroy its
 *  window directly if it does not support that (see manual.md 5.3) */
cJSON *ipc_action_close_client(wm_td *wm, const cJSON *args);

/** "kill_client": forcibly terminate the client's own X connection */
cJSON *ipc_action_kill_client(wm_td *wm, const cJSON *args);

/** "deiconify_client": restore the client if it was iconified */
cJSON *ipc_action_deiconify_client(wm_td *wm, const cJSON *args);

/** "focus_client": focus and raise the client */
cJSON *ipc_action_focus_client(wm_td *wm, const cJSON *args);

/** "unfocus_client": clear input focus from the client, if it had it */
cJSON *ipc_action_unfocus_client(wm_td *wm, const cJSON *args);

/** "iconify_client": iconify (minimize) the client */
cJSON *ipc_action_iconify_client(wm_td *wm, const cJSON *args);

/** "hide_client": hide the client without iconifying it */
cJSON *ipc_action_hide_client(wm_td *wm, const cJSON *args);

/** "unhide_client": undo "hide_client" */
cJSON *ipc_action_unhide_client(wm_td *wm, const cJSON *args);

/** "sticky_client": make the client visible on every desktop */
cJSON *ipc_action_sticky_client(wm_td *wm, const cJSON *args);

/** "unsticky_client": undo "sticky_client" */
cJSON *ipc_action_unsticky_client(wm_td *wm, const cJSON *args);

/** "toggle_sticky_client" */
cJSON *ipc_action_toggle_sticky_client(wm_td *wm, const cJSON *args);

/** "set_urgent_client": mark the client urgent */
cJSON *ipc_action_set_urgent_client(wm_td *wm, const cJSON *args);

/** "clear_urgent_client": undo "set_urgent_client" */
cJSON *ipc_action_clear_urgent_client(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_CLIENT_BASIC_H */
