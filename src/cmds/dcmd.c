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
#include <signal.h>     /* kill */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* calloc, free */
#include <string.h>     /* memcpy, strlen */
#include <sys/types.h>  /* pid_t */

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


/**
 * @brief Publish full @c _NET_DESKTOP_NAMES for the desktop screen
 *
 * Rebuilds the NUL-separated list from the base configuration and
 * updates the root EWMH property for the corresponding screen.
 *
 * @note Complexity: @e O(n), where @e n is screen desktop count
 */
static void s_dcmd_sync_desktop_names(const desktop_td *desktop)
{
    uint32_t desktop_count;
    size_t names_len;
    size_t offset;
    char *names;

    if (desktop == NULL || desktop->ewmh == NULL ||
            desktop->config_base == NULL ||
            desktop->screen_id >= desktop->config_base->screen_count) {
        return;
    }

    desktop_count = desktop->config_base->screens[desktop->screen_id]
        .desktop_count;
    if (desktop_count == 0u) {
        return;
    }

    names_len = 0u;
    for (uint32_t i = 0u; i < desktop_count; ++i) {
        const char *name = desktop->config_base
            ->screens[desktop->screen_id].desktops[i].name;
        names_len += strlen(name) + 1u;
    }

    if (names_len == 0u || names_len > UINT32_MAX) {
        return;
    }

    names = calloc(names_len, sizeof(char));
    if (names == NULL) {
        return;
    }

    offset = 0u;
    for (uint32_t i = 0u; i < desktop_count; ++i) {
        const char *name = desktop->config_base
            ->screens[desktop->screen_id].desktops[i].name;
        size_t name_len = strlen(name);
        if (offset + name_len + 1u > names_len) {
            break;
        }
        memcpy(names + offset, name, name_len + 1u);
        offset += name_len + 1u;
    }

    if (offset > 0u) {
        xcb_ewmh_set_desktop_names(desktop->ewmh,
                (int) desktop->screen_id, (uint32_t) offset, names);
    }

    free(names);
}


/* Rename the desktop */
void dcmd_desktop_rename(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    uint32_t screen_id;
    uint32_t desktop_id;

    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->new_data.str == NULL) {
        return;
    }

    if (desktop_action_rename(desktop,
                desktop_data->new_data.str) != 0) {
        return;
    }

    if (desktop->config_base == NULL) {
        return;
    }

    screen_id = desktop->screen_id;
    desktop_id = desktop->id;
    if (screen_id >= desktop->config_base->screen_count ||
            desktop_id >= desktop->config_base
            ->screens[screen_id].desktop_count) {
        return;
    }

    safe_strncpy(desktop->config_base->screens[screen_id]
            .desktops[desktop_id].name, desktop->name,
            CONFIG_MAX_LENGTH_NAME);
    s_dcmd_sync_desktop_names(desktop);
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

    return (pid_t) desktop_action_process_launch(desktop,
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
