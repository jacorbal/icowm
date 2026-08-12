/**
 * @file ipc/actions/desktop.h
 *
 * @brief IPC commands mirroring @c enact.h's own @c enact_desktop_*
 *        and @c enact_client_*-on-a-desktop actions
 *
 * @c enact_desktop_client_add and @c enact_desktop_client_remove
 * are deliberately not exposed here: they are internal bookkeeping
 * (adding or removing a client from a desktop's own tracking) used
 * while mapping or unmapping a window, not a user-facing action on
 * their own; calling either directly over IPC, detached from the
 * window (re)parenting it is normally paired with, could leave
 * IcoWM's own internal state inconsistent with what is actually on
 * screen.  @c enact_desktop_cycle_clients_active/_prev/_icons_next/
 * _icons_prev are not exposed either: each opens the interactive
 * cycle menu, which then expects further keyboard or mouse input to
 * actually pick something, not something a fire-and-forget socket
 * command can usefully drive.
 *
 * @defgroup ipc_actions_desktop IPC desktop-scoped actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_DESKTOP_H
#define IPC_ACTIONS_DESKTOP_H


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/** "set_desktop_background": arguments "desktop_id" (required),
 *  "surface_id" (optional), "color" (required, a packed 0xRRGGBB
 *  value) */
cJSON *ipc_action_set_desktop_background(wm_td *wm, const cJSON *args);

/** "show_desktop": arguments "desktop_id" (required), "surface_id"
 *  (optional), "show" (required boolean) */
cJSON *ipc_action_show_desktop(wm_td *wm, const cJSON *args);

/** "send_client_to_desktop": arguments "client_id",
 *  "target_desktop_id" (on the client's own current surface) */
cJSON *ipc_action_send_client_to_desktop(wm_td *wm, const cJSON *args);

/** "send_client_to_front": raise the client to the front of its own
 *  desktop's window stack; argument "client_id" */
cJSON *ipc_action_send_client_to_front(wm_td *wm, const cJSON *args);

/** "send_client_to_back": send the client to the back of its own
 *  desktop's window stack; argument "client_id" */
cJSON *ipc_action_send_client_to_back(wm_td *wm, const cJSON *args);

/** "iconify_all": arguments "desktop_id" (optional; the resolved
 *  surface's own current desktop otherwise), "surface_id" (optional) */
cJSON *ipc_action_iconify_all(wm_td *wm, const cJSON *args);

/** "deiconify_all": same arguments as "iconify_all" */
cJSON *ipc_action_deiconify_all(wm_td *wm, const cJSON *args);

/** "rearrange": arguments "desktop_id" (optional; the resolved
 *  surface's own current desktop otherwise), "surface_id" (optional) */
cJSON *ipc_action_rearrange(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_DESKTOP_H */
