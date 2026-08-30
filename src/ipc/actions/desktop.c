/**
 * @file ipc/actions/desktop.c
 *
 * @brief IPC commands mirroring enact.h's desktop-scoped actions
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

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/args.h>
#include <ipc/resolve.h>
#include <ipc/response.h>
#include <ipc/actions/desktop.h>


cJSON *ipc_action_set_desktop_background(const wm_td *wm, const cJSON *args)
{
    uint32_t color;
    surface_td *surface = NULL;
    desktop_td *desktop;
    cJSON *error = NULL;

    if (!ipc_args_get_uint(args, "color", &color)) {
        return ipc_response_error("missing or invalid 'color'");
    }

    desktop = ipc_resolve_desktop(wm, args, true, &surface, &error);
    if (desktop == NULL) {
        return error;
    }

    enact_desktop_set_background(desktop, color);
    return ipc_response_ok();
}


cJSON *ipc_action_show_desktop(const wm_td *wm, const cJSON *args)
{
    bool show;
    surface_td *surface = NULL;
    desktop_td *desktop;
    cJSON *error = NULL;

    if (!ipc_args_get_bool(args, "show", &show)) {
        return ipc_response_error("missing or invalid 'show'");
    }

    desktop = ipc_resolve_desktop(wm, args, true, &surface, &error);
    if (desktop == NULL) {
        return error;
    }

    enact_desktop_show(desktop, show);
    return ipc_response_ok();
}


cJSON *ipc_action_send_client_to_desktop(const wm_td *wm, const cJSON *args)
{
    uint32_t target_desktop_id;
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    desktop_td *target;
    cJSON *error = NULL;

    if (!ipc_args_get_uint(args, "target_desktop_id", &target_desktop_id)) {
        return ipc_response_error("missing or invalid 'target_desktop_id'");
    }

    client = ipc_resolve_client(wm, args, &surface, &desktop, &error);
    if (client == NULL) {
        return error;
    }

    if (target_desktop_id >= surface->desktop_count) {
        return ipc_response_error(
                "no such desktop on that client's own surface");
    }
    target = surface_desktop_get(surface, target_desktop_id);
    if (target == NULL) {
        return ipc_response_error("no such desktop");
    }

    enact_desktop_client_send(desktop, client, target);
    return ipc_response_ok();
}


cJSON *ipc_action_send_client_to_front(const wm_td *wm, const cJSON *args)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    cJSON *error = NULL;

    client = ipc_resolve_client(wm, args, &surface, &desktop, &error);
    if (client == NULL) {
        return error;
    }

    enact_desktop_client_send_front(desktop, client);
    return ipc_response_ok();
}


cJSON *ipc_action_send_client_to_back(const wm_td *wm, const cJSON *args)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    cJSON *error = NULL;

    client = ipc_resolve_client(wm, args, &surface, &desktop, &error);
    if (client == NULL) {
        return error;
    }

    enact_desktop_client_send_back(desktop, client);
    return ipc_response_ok();
}


cJSON *ipc_action_iconify_all(const wm_td *wm, const cJSON *args)
{
    surface_td *surface = NULL;
    desktop_td *desktop;
    cJSON *error = NULL;

    desktop = ipc_resolve_desktop(wm, args, false, &surface, &error);
    if (desktop == NULL) {
        return error;
    }

    enact_desktop_clients_iconify_all(desktop);
    return ipc_response_ok();
}


cJSON *ipc_action_deiconify_all(const wm_td *wm, const cJSON *args)
{
    surface_td *surface = NULL;
    desktop_td *desktop;
    cJSON *error = NULL;

    desktop = ipc_resolve_desktop(wm, args, false, &surface, &error);
    if (desktop == NULL) {
        return error;
    }

    enact_desktop_clients_deiconify_all(desktop);
    return ipc_response_ok();
}


cJSON *ipc_action_rearrange(const wm_td *wm, const cJSON *args)
{
    surface_td *surface = NULL;
    desktop_td *desktop;
    cJSON *error = NULL;

    desktop = ipc_resolve_desktop(wm, args, false, &surface, &error);
    if (desktop == NULL) {
        return error;
    }

    enact_desktop_clients_rearrange(wm, surface, desktop);
    return ipc_response_ok();
}
