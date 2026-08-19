/**
 * @file ipc/actions/client/state.c
 *
 * @brief IPC commands mirroring cmds/client/state.h implementation
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
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <ipc/dispatch.h>
#include <ipc/actions/client/state.h>


static void s_shade(const wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_shade(client);
}

static void s_unshade(const wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_unshade(client);
}

static void s_toggle_shade(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_toggle_shade(client);
}

static void s_fullscreen(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_fullscreen(client);
}

static void s_unfullscreen(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_unfullscreen(client);
}

static void s_toggle_fullscreen(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_toggle_fullscreen(client);
}

static void s_toggle_decoration(const wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_toggle_decorate(client);
}


cJSON *ipc_action_shade_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_shade);
}

cJSON *ipc_action_unshade_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_unshade);
}

cJSON *ipc_action_toggle_shade_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_toggle_shade);
}

cJSON *ipc_action_fullscreen_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_fullscreen);
}

cJSON *ipc_action_unfullscreen_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_unfullscreen);
}

cJSON *ipc_action_toggle_fullscreen_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_toggle_fullscreen);
}

cJSON *ipc_action_toggle_decorate_client(const wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_toggle_decoration);
}
