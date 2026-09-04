/**
 * @file enact.c
 *
 * @brief Window-manager-level actions that do not belong to the client,
 *        desktop, or surface domain
 *
 * Every client-, desktop-, and surface-level action lives in
 * @c enact/client.c, @c enact/desktop.c, and @c enact/surface.c
 * instead; this file only declares the domain they share.
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

/* Project includes */
#include <wm.h>

/* IPC includes */
#include <ipc.h>

/* Local includes */
#include <enact.h>


/* 'action_wm_e' */

/* Request that the window manager stop and exit */
int enact_wm_exit(void)
{
    return wm_request_stop();
}


/* Request that the window manager stop and restart itself in place */
int enact_wm_restart(void)
{
    return wm_request_restart();
}


/* Reload the window manager's configuration */
int enact_wm_configuration_reload(const wm_td *wm)
{
    const int status = wm_action_config_reload(wm);

    if (status == 0) {
        ipc_broadcast_event(IPC_EVENT_CONFIG_RELOADED, NULL);
    }
    return status;
}
