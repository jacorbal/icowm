/**
 * @file ipc/actions/surface.h
 *
 * @brief IPC commands mirroring @c enact.h's own
 *        @c enact_surface_desktop_switch* actions
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
/** "goto_desktop": arguments "desktop_id" (required), "surface_id"
 *  (optional) */
cJSON *ipc_action_goto_desktop(wm_td *wm, const cJSON *args);

/** "next_desktop": argument "surface_id" (optional) */
cJSON *ipc_action_next_desktop(wm_td *wm, const cJSON *args);

/** "prev_desktop": argument "surface_id" (optional) */
cJSON *ipc_action_prev_desktop(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_SURFACE_H */
