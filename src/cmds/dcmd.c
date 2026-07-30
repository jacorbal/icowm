/**
 * @file cmds/dcmd.c
 *
 * @brief Implementation on executions over desktops using the XCB
 *        interface while updating EWMH and ICCCM hints
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* kill */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>  /* pid_t */
#include <signal.h>     /* kill */
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safestr.h>

/* Project includes */
#include <actdata.h>
#include <client.h>
#include <desktop.h>

/* Local includes */
#include <cmds/dcmd.h>


/* Rename the desktop */
void dcmd_desktop_rename(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->new_data.str == NULL) {
        return;
    }

    snprintf(desktop->name, WM_DESKTOP_MAX_LENGTH_NAME - 1, "%s",
            desktop_data->new_data.str);

    xcb_ewmh_set_desktop_names(desktop->ewmh, (int) desktop->screen_id,
            1, desktop->name);
}


/* Set the background color of the desktop */
void dcmd_desktop_bg_color(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL) {
        return;
    }

    desktop->background.is_image = false;
    desktop->background.bg.color = desktop_data->new_data.uvalue;
    desktop->is_outdated = true;
}


/* Clear the desktop */
void dcmd_desktop_clear(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_clear(desktop);
}


/* Add a client to the desktop */
void dcmd_desktop_client_add(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->client == NULL) {
        return;
    }

    desktop_action_client_add(desktop, desktop_data->client);
}


/* Remove a client from the desktop */
void dcmd_desktop_client_rem(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->client == NULL) {
        return;
    }

    desktop_action_client_rem(desktop, desktop_data->client);
}


/* Send a client to a specific desktop */
void dcmd_desktop_client_send(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->client == NULL ||
            desktop_data->target == NULL) {
        return;
    }

    desktop_action_client_rem(desktop, desktop_data->client);
    desktop_action_client_add(desktop_data->target,
            desktop_data->client);
    desktop_data->client->desktop_id = desktop_data->target->id;
}


/* Clone a client to a target desktop without removing from source */
void dcmd_desktop_client_clone(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->client == NULL ||
            desktop_data->target == NULL) {
        return;
    }

    /* Add to target without removing from source.  The same client
     * pointer lives in two desktops; the caller is responsible for
     * ensuring it is destroyed exactly once on cleanup. */
    desktop_action_client_add(desktop_data->target,
            desktop_data->client);
}


/* Send a client to the front of the desktop's window stack */
void dcmd_desktop_client_send_front(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->client == NULL) {
        return;
    }

    desktop_action_client_send_front(desktop, desktop_data->client);
}


/* Send a client to the back of the desktop's window stack */
void dcmd_desktop_client_send_back(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->client == NULL) {
        return;
    }

    desktop_action_client_send_back(desktop, desktop_data->client);
}


/* Rearrange the clients on the desktop */
void dcmd_desktop_clients_rearrange(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_clients_rearrange(desktop);
}


/* Iconify all clients on the desktop */
void dcmd_desktop_clients_iconify_all(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_clients_iconify_all(desktop);
}


/* Cycle through active clients on the desktop */
void dcmd_desktop_clients_cycle_active(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_cycle_clients_active(desktop);
}


/* Cycle focus to the previous active client on the desktop */
void dcmd_desktop_clients_cycle_prev(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_cycle_clients_prev(desktop);
}


/* Cycle through client icons */
void dcmd_desktop_clients_cycle_icons(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_cycle_clients_icons(desktop);
}


/* Lock the desktop */
void dcmd_desktop_lock(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_lock(desktop);
}


/* Unlock the desktop */
void dcmd_desktop_unlock(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_unlock(desktop);
}


/* Change the layout of the desktop */
void dcmd_desktop_layout(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->new_data.str == NULL) {
        return;
    }

    desktop_action_set_layout(desktop, desktop_data->new_data.str);
}


/* Launch a new process for the desktop */
pid_t dcmd_desktop_process_launch(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->new_data.str == NULL) {
        return -1;
    }

    return (pid_t) desktop_action_application_launch(desktop,
            desktop_data->new_data.str);
}


/* Kill a process associated with the desktop */
bool dcmd_desktop_process_kill(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    pid_t target_pid;

    if (desktop == NULL || desktop_data == NULL) {
        return false;
    }

    target_pid = (pid_t) desktop_data->new_data.svalue;
    if (target_pid <= 0) {
        return false;
    }

    return (kill(target_pid, SIGTERM) == 0);
}
