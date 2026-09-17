/**
 * @file ipc/resolve.c
 *
 * @brief Turning a request's numeric IDs into real pointers
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
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <lookup.h>
#include <stage.h>
#include <stage/desktop.h>
#include <wm.h>

/* Local includes */
#include <ipc/args.h>
#include <ipc/response.h>
#include <ipc/resolve.h>


/* Resolve which stage a request refers to */
stage_td *ipc_resolve_stage(const wm_td *wm, const cJSON *args)
{
    uint32_t stage_id;

    if (ipc_args_get_uint(args, "stage_id", &stage_id)) {
        return wm_get_stage_by_id(stage_id);
    }

    if (wm_stages(wm) == NULL || list_is_empty(wm_stages(wm))) {
        return NULL;
    }
    return (stage_td *) list_data(list_head(wm_stages(wm)));
}


/* Resolve which desktop a request refers to, on top of
 * ipc_resolve_stage */
desktop_td *ipc_resolve_desktop(const wm_td *wm, const cJSON *args,
        bool desktop_id_required, stage_td **out_stage,
        cJSON **out_error)
{
    stage_td *stage;
    desktop_td *desktop;
    uint32_t desktop_id;

    stage = ipc_resolve_stage(wm, args);
    if (stage == NULL) {
        *out_error = ipc_response_error("no such stage");
        return NULL;
    }

    if (ipc_args_get_uint(args, "desktop_id", &desktop_id)) {
        if (desktop_id >= stage->desktop_count) {
            *out_error =
                ipc_response_error("no such desktop on that stage");
            return NULL;
        }
        desktop = stage_desktop_get(stage, desktop_id);
    } else if (desktop_id_required) {
        *out_error = ipc_response_error("missing or invalid 'desktop_id'");
        return NULL;
    } else {
        desktop = lookup_current_desktop(stage);
    }

    if (desktop == NULL) {
        *out_error = ipc_response_error("no such desktop");
        return NULL;
    }

    *out_stage = stage;
    return desktop;
}


/* Resolve which client a request refers to */
client_td *ipc_resolve_client(const wm_td *wm, const cJSON *args,
        stage_td **out_stage, desktop_td **out_desktop,
        cJSON **out_error)
{
    uint32_t client_id;
    client_td *client;

    if (!ipc_args_get_uint(args, "client_id", &client_id)) {
        *out_error = ipc_response_error("missing or invalid 'client_id'");
        return NULL;
    }

    client = lookup_find_client(wm_stages(wm), (xcb_window_t) client_id,
            out_stage, out_desktop);
    if (client == NULL) {
        *out_error = ipc_response_error("no such client");
        return NULL;
    }

    return client;
}
