/**
 * @file ipc/actions/client/basic.c
 *
 * @brief IPC commands mirroring cmds/client/basic.h implementation
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
#include <ipc/actions/client/basic.h>


static void s_close(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_close(client);
}

static void s_kill(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_kill(client);
}

static void s_restore(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_restore(client);
}

static void s_focus(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_focus(client);
}

static void s_unfocus(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_unfocus(client);
}

static void s_iconify(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_iconify(client);
}

static void s_hide(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_hide(client);
}

static void s_unhide(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_unhide(client);
}

static void s_sticky(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_pin(client);
}

static void s_unsticky(wm_td *wm, client_td *client, surface_td *surface,
        desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_unpin(client);
}

static void s_toggle_sticky(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_toggle_pin(client);
}

static void s_set_urgent(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_urge(client);
}

static void s_clear_urgent(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm; (void) surface; (void) desktop;
    enact_client_unurge(client);
}


cJSON *ipc_action_close_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_close);
}

cJSON *ipc_action_kill_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_kill);
}

cJSON *ipc_action_deiconify_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_restore);
}

cJSON *ipc_action_focus_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_focus);
}

cJSON *ipc_action_unfocus_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_unfocus);
}

cJSON *ipc_action_iconify_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_iconify);
}

cJSON *ipc_action_hide_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_hide);
}

cJSON *ipc_action_unhide_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_unhide);
}

cJSON *ipc_action_pin_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_sticky);
}

cJSON *ipc_action_unpin_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_unsticky);
}

cJSON *ipc_action_toggle_pin_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_toggle_sticky);
}

cJSON *ipc_action_urge_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_set_urgent);
}

cJSON *ipc_action_unurge_client(wm_td *wm, const cJSON *args)
{
    return ipc_dispatch_client_action(wm, args, s_clear_urgent);
}
