/**
 * @file ipc/actions/scratchpad.h
 *
 * @brief IPC command mirroring scratchpad.h's toggle action
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


/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/** @c toggle_scratchpad: arguments @c desktop_id, optional and
 *  defaulting to the resolved surface's current desktop, and
 *  @c surface_id, optional */
cJSON *ipc_action_toggle_scratchpad(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_SCRATCHPAD_H */
