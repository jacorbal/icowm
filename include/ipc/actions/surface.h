/**
 * @file ipc/actions/surface.h
 *
 * @brief IPC commands mirroring @c enact.h's own
 *        @a enact_surface_desktop_switch*, @a enact_surface_desktop_add,
 *        and @a enact_surface_desktop_remove actions
 *
 * @defgroup ipc_actions_surface IPC desktop actions
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
#include <types/handles.h>
#include <cjson/cJSON.h>

/* Project includes */


/* Public interface */
/** @c goto_desktop: arguments @c desktop_id (required), @c surface_id
 *  (optional) */
cJSON *ipc_action_goto_desktop(const wm_td *wm, const cJSON *args);

/** @c goto_north_desktop: argument @c surface_id (optional) */
cJSON *ipc_action_goto_north_desktop(const wm_td *wm, const cJSON *args);

/** @c goto_south_desktop: argument @c surface_id (optional) */
cJSON *ipc_action_goto_south_desktop(const wm_td *wm, const cJSON *args);

/** @c goto_east_desktop: argument @c surface_id (optional) */
cJSON *ipc_action_goto_east_desktop(const wm_td *wm, const cJSON *args);

/** @c goto_west_desktop: argument @c surface_id (optional) */
cJSON *ipc_action_goto_west_desktop(const wm_td *wm, const cJSON *args);

/** @c add_desktop: argument @c surface_id (optional) */
cJSON *ipc_action_add_desktop(const wm_td *wm, const cJSON *args);

/** @c remove_desktop: argument @c surface_id (optional); refused with
 *  an error when only one desktop remains on the target surface */
cJSON *ipc_action_remove_desktop(const wm_td *wm, const cJSON *args);

/** @c toggle_strutless_maximize: argument @c surface_id (optional) */
cJSON *ipc_action_toggle_strutless_maximize(const wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_SURFACE_H */
