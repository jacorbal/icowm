/**
 * @file ipc/dispatch.c
 *
 * @brief Shared "resolve a client, act on it, report success" wrapper
 *        implementation
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
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/resolve.h>
#include <ipc/response.h>
#include <ipc/dispatch.h>


/* Resolve a request's own client_id, run one action on it, and
 * report the outcome */
cJSON *ipc_dispatch_client_action(const wm_td *wm, const cJSON *args,
        ipc_client_action_fn action)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    cJSON *error = NULL;

    client = ipc_resolve_client(wm, args, &surface, &desktop, &error);
    if (client == NULL) {
        return error;
    }

    action(wm, client, surface, desktop);

    return ipc_response_ok();
}
