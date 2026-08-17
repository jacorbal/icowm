/**
 * @file ipc/actions/surface.c
 *
 * @brief IPC commands mirroring enact.h's own
 *        enact_surface_desktop_switch* actions implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <desktop.h>
#include <enact.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/resolve.h>
#include <ipc/response.h>
#include <ipc/actions/surface.h>


cJSON *ipc_action_goto_desktop(wm_td *wm, const cJSON *args)
{
    surface_td *surface = NULL;
    const desktop_td *desktop;
    cJSON *error = NULL;

    desktop = ipc_resolve_desktop(wm, args, true, &surface, &error);
    if (desktop == NULL) {
        return error;
    }

    enact_surface_desktop_switch(surface, desktop->id);
    return ipc_response_ok();
}


cJSON *ipc_action_goto_next_desktop(wm_td *wm, const cJSON *args)
{
    surface_td *surface = ipc_resolve_surface(wm, args);

    if (surface == NULL) {
        return ipc_response_error("no such surface");
    }

    enact_surface_desktop_switch_next(surface);
    return ipc_response_ok();
}


cJSON *ipc_action_goto_prev_desktop(wm_td *wm, const cJSON *args)
{
    surface_td *surface = ipc_resolve_surface(wm, args);

    if (surface == NULL) {
        return ipc_response_error("no such surface");
    }

    enact_surface_desktop_switch_prev(surface);
    return ipc_response_ok();
}
