/**
 * @file ipc/actions/scratchpad.c
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

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <desktop.h>
#include <scratchpad.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/resolve.h>
#include <ipc/response.h>
#include <ipc/actions/scratchpad.h>


cJSON *ipc_action_toggle_scratchpad(const wm_td *wm, const cJSON *args)
{
    surface_td *surface = NULL;
    desktop_td *desktop;
    cJSON *error = NULL;

    desktop = ipc_resolve_desktop(wm, args, false, &surface, &error);
    if (desktop == NULL) {
        return error;
    }

    scratchpad_toggle(wm, desktop);
    return ipc_response_ok();
}
