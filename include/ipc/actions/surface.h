/**
 * @file ipc/actions/surface.h
 *
 * @brief IPC commands mirroring @c enact.h's own
 *        @a enact_surface_desktop_switch* actions
 *
 * @defgroup ipc_actions_surface IPC desktop-switching actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_SURFACE_H
#define IPC_ACTIONS_SURFACE_H


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/** @c goto_desktop: arguments @c desktop_id (required), @c surface_id
 *  (optional) */
cJSON *ipc_action_goto_desktop(wm_td *wm, const cJSON *args);

/** @c goto_next_desktop: argument @c surface_id (optional) */
cJSON *ipc_action_goto_next_desktop(wm_td *wm, const cJSON *args);

/** @c goto_prev_desktop: argument @c surface_id (optional) */
cJSON *ipc_action_goto_prev_desktop(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_SURFACE_H */
