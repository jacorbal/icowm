/**
 * @file ipc/actions/desktop.h
 *
 * @brief IPC commands mirroring @c enact.h's @c enact_desktop_* and
 *        @a enact_client_*-(on-a-desktop) actions
 *
 * @a enact_desktop_client_add and @a enact_desktop_client_remove are
 * deliberately not exposed here since they are internal bookkeeping
 * (adding or removing a client from a desktop's tracking) used
 * while mapping or unmapping a window, not a user-facing action on
 * their; calling either directly over IPC, detached from the window
 * (re)parenting it is normally paired with, could leave IcoWM's
 * internal state inconsistent with what is actually on screen.
 * @a enact_desktop_cycle_clients_active/_prev/_icons_next/_icons_prev
 * are not exposed either, for each opens the interactive cycle menu,
 * which then expects further keyboard or mouse input to actually pick
 * something, not something a fire-and-forget socket command can
 * usefully drive.
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


/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/** @c set_background_desktop: arguments @c desktop_id (required),
 *  @c stage_id" (optional), @c color (required, a packed @c 0xRRGGBB
 *  value) */
cJSON *ipc_action_set_background_desktop(const wm_td *wm, const cJSON *args);

/** @c show_desktop: arguments @c desktop_id (required), @c stage_id
 *  (optional), @c show (required boolean) */
cJSON *ipc_action_show_desktop(const wm_td *wm, const cJSON *args);

/** @c send_client_to_desktop: arguments @c client_id,
 *  @c target_desktop_id (on the client's current stage) */
cJSON *ipc_action_send_client_to_desktop(const wm_td *wm, const cJSON *args);

/** @c send_client_to_front: raise the client to the front of its own
 *  desktop's window stack; argument @c client_id */
cJSON *ipc_action_send_client_to_front(const wm_td *wm, const cJSON *args);

/** @c send_client_to_back: send the client to the back of its own
 *  desktop's window stack; argument @c client_id */
cJSON *ipc_action_send_client_to_back(const wm_td *wm, const cJSON *args);

/** @c iconify_all: arguments @c desktop_id, optional and defaulting
 *  to the resolved stage's current desktop, and
 *  @c stage_id, optional */
cJSON *ipc_action_iconify_all(const wm_td *wm, const cJSON *args);

/** @c deiconify_all: same arguments as @c iconify_all */
cJSON *ipc_action_deiconify_all(const wm_td *wm, const cJSON *args);

/** @c rearrange_desktop: arguments @c desktop_id (optional; the
 * resolved stage's current desktop otherwise), @c stage_id
 * (optional) */
cJSON *ipc_action_rearrange(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_DESKTOP_H */
