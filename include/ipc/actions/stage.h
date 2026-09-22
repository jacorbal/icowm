/**
 * @file ipc/actions/stage.h
 *
 * @brief IPC commands mirroring @c enact.h's
 *        @a enact_stage_desktop_switch family,
 *        @a enact_stage_desktop_add and
 *        @a enact_stage_desktop_remove actions
 *
 * @defgroup ipc_actions_stage IPC desktop actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_STAGE_H
#define IPC_ACTIONS_STAGE_H


/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/** @c goto_desktop: arguments @c desktop_id (required), @c stage_id
 *  (optional) */
cJSON *ipc_action_goto_desktop(const wm_td *wm, const cJSON *args);

/** @c goto_desktop_north: argument @c stage_id (optional) */
cJSON *ipc_action_goto_desktop_north(const wm_td *wm, const cJSON *args);

/** @c goto_desktop_south: argument @c stage_id (optional) */
cJSON *ipc_action_goto_desktop_south(const wm_td *wm, const cJSON *args);

/** @c goto_desktop_east: argument @c stage_id (optional) */
cJSON *ipc_action_goto_desktop_east(const wm_td *wm, const cJSON *args);

/** @c goto_desktop_west: argument @c stage_id (optional) */
cJSON *ipc_action_goto_desktop_west(const wm_td *wm, const cJSON *args);

/** @c add_desktop: argument @c stage_id (optional) */
cJSON *ipc_action_add_desktop(const wm_td *wm, const cJSON *args);

/** @c remove_desktop: argument @c stage_id (optional); refused with
 *  an error when only one desktop remains on the target stage */
cJSON *ipc_action_remove_desktop(const wm_td *wm, const cJSON *args);

/** @c toggle_strutless_maximize: argument @c stage_id (optional) */
cJSON *ipc_action_toggle_strutless_maximize(const wm_td *wm,
        const cJSON *args);


#endif  /* ! IPC_ACTIONS_STAGE_H */
