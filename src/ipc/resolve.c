/**
 * @file ipc/resolve.c
 *
 * @brief Turning a request's own numeric IDs into real pointers
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
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/args.h>
#include <ipc/response.h>
#include <ipc/resolve.h>


/* Resolve which surface a request refers to */
surface_td *ipc_resolve_surface(const wm_td *wm, const cJSON *args)
{
    uint32_t surface_id;

    if (ipc_args_get_uint(args, "surface_id", &surface_id)) {
        return wm_get_surface_by_id(surface_id);
    }

    if (wm_surfaces(wm) == NULL || list_is_empty(wm_surfaces(wm))) {
        return NULL;
    }
    return (surface_td *) list_data(list_head(wm_surfaces(wm)));
}


/* Resolve which desktop a request refers to, on top of
 * ipc_resolve_surface */
desktop_td *ipc_resolve_desktop(const wm_td *wm, const cJSON *args,
        bool desktop_id_required, surface_td **out_surface,
        cJSON **out_error)
{
    surface_td *surface;
    desktop_td *desktop;
    uint32_t desktop_id;

    surface = ipc_resolve_surface(wm, args);
    if (surface == NULL) {
        *out_error = ipc_response_error("no such surface");
        return NULL;
    }

    if (ipc_args_get_uint(args, "desktop_id", &desktop_id)) {
        if (desktop_id >= surface->desktop_count) {
            *out_error =
                ipc_response_error("no such desktop on that surface");
            return NULL;
        }
        desktop = surface_desktop_get(surface, desktop_id);
    } else if (desktop_id_required) {
        *out_error = ipc_response_error("missing or invalid 'desktop_id'");
        return NULL;
    } else {
        desktop = lookup_current_desktop(surface);
    }

    if (desktop == NULL) {
        *out_error = ipc_response_error("no such desktop");
        return NULL;
    }

    *out_surface = surface;
    return desktop;
}


/* Resolve which client a request refers to */
client_td *ipc_resolve_client(const wm_td *wm, const cJSON *args,
        surface_td **out_surface, desktop_td **out_desktop,
        cJSON **out_error)
{
    uint32_t client_id;
    client_td *client;

    if (!ipc_args_get_uint(args, "client_id", &client_id)) {
        *out_error = ipc_response_error("missing or invalid 'client_id'");
        return NULL;
    }

    client = lookup_find_client(wm_surfaces(wm), (xcb_window_t) client_id,
            out_surface, out_desktop);
    if (client == NULL) {
        *out_error = ipc_response_error("no such client");
        return NULL;
    }

    return client;
}
