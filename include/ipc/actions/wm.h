/**
 * @file ipc/actions/wm.h
 *
 * @brief IPC commands mirroring @c enact.h's own whole-window-manager
 *        actions
 *
 * @defgroup ipc_actions_wm IPC whole-window-manager actions
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ACTIONS_WM_H
#define IPC_ACTIONS_WM_H


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/** @c exit_wm: request that IcoWM stop and exit; no arguments */
cJSON *ipc_action_exit_wm(wm_td *wm, const cJSON *args);

/** @c reload_config: reload every configuration file, the same as
 *  sending IcoWM @c SIGHUP; no arguments */
cJSON *ipc_action_reload_config(wm_td *wm, const cJSON *args);


#endif  /* ! IPC_ACTIONS_WM_H */
