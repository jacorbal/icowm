/**
 * @file ipc/actions/scratchpad.h
 *
 * @brief IPC command mirroring scratchpad.h's own toggle action
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_SCRATCHPAD_H
#define IPC_ACTIONS_SCRATCHPAD_H


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/** @c toggle_scratchpad: arguments @c desktop_id (optional; the resolved
 *  surface's own current desktop otherwise), @c surface_id (optional) */
cJSON *ipc_action_toggle_scratchpad(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_SCRATCHPAD_H */
