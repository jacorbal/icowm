/**
 * @file ipc/actions/wm.c
 *
 * @brief IPC commands mirroring enact.h's own whole-window-manager
 *        actions implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <enact.h>
#include <wm.h>

/* Local includes */
#include <ipc/response.h>
#include <ipc/actions/wm.h>


cJSON *ipc_action_exit_wm(wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;

    if (enact_wm_exit() != 0) {
        return ipc_response_error("failed to request shutdown");
    }
    return ipc_response_ok();
}


cJSON *ipc_action_reload_config(wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;

    if (enact_wm_configuration_reload() != 0) {
        return ipc_response_error("failed to reload the configuration");
    }
    return ipc_response_ok();
}
