/**
 * @file cmds/dcmd.c
 *
 * @brief Implementation on executions over desktops using the XCB
 *        interface while updating EWMH and ICCCM hints
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>  /* pid_t */
#include <unistd.h>     /* NULL, fork */

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
    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL ||
            desktop_data->new_data.str == NULL) {
        return;
    }

    /* Change the desktop name property */
    // TODO

    /* Update the EWMH desktop name */
/*
    xcb_ewmh_set_desktop_name(desktop->ewmh,
            (uint32_t) safe_strlen(desktop_data->new_data.str),
            desktop_data->new_data.str);
*/
}


/* Set the background color of the desktop */
void dcmd_desktop_bg_color(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return;
    }

    /* Set the background color using the provided data */
    // TODO
}


/* Clear the desktop */
void dcmd_desktop_clear(desktop_td *desktop)
{
    /* Check if the desktop is valid */
    if (desktop == NULL) {
        return;
    }

    /* Logic to clear the desktop, such as removing clients or resetting
     * properties */
    // TODO: Placeholder for client cleanup logic

    //xcb_map_window(desktop->connection, desktop->window);
}


/* Add a client to the desktop */
void dcmd_desktop_client_add(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    /* Variables for adding client */
    xcb_window_t new_client_window;

    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return;
    }

    /* Create a new client window and add it to the desktop */
    new_client_window = xcb_generate_id(desktop->connection);
    xcb_map_window(desktop->connection, new_client_window);
    // TODO: client_init...

    /* Here we can also inform EWMH if necessary */
    xcb_ewmh_set_wm_state(desktop->ewmh, new_client_window, 0, NULL);
}


/* Remove a client from the desktop */
void dcmd_desktop_client_rem(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return;
    }

    /* Logic to remove the specified client from the desktop */
    // TODO
}


/* Send a client to a specific desktop */
void dcmd_desktop_client_send(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return;
    }

    /* Logic to send the client to the specified desktop */
/*  // TODO
    xcb_unmap_window(desktop->connection,
            desktop_data->old_data.window);
    xcb_ewmh_request_change_active_window(desktop->ewmh,
            desktop->screen_id, desktop_data->old_data.window, 0,
            XCB_CURRENT_TIME, 0);
*/
}


/* Send a client to the front of the desktop's window stack */
void dcmd_desktop_client_send_front(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return;
    }

    /* Logic to raise the specified client window */
    // TODO
    //xcb_map_window(desktop->connection, desktop_data->old_data.window);
}


/* Send a client to the back of the desktop's window stack */
void dcmd_desktop_client_send_back(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return;
    }

    /* Logic to lower the specified client window */
    // TODO
    //xcb_lower_window(desktop->connection, desktop_data->old_data.window);
}


/* Rearrange the clients on the desktop */
void dcmd_desktop_clients_rearrange(desktop_td *desktop)
{
    /* Check if the desktop is valid */
    if (desktop == NULL) {
        return;
    }

    /* Logic to rearrange client windows on the desktop */
    // TODO
}


/* Iconify all clients on the desktop */
void dcmd_desktop_clients_iconify_all(desktop_td *desktop)
{
    /* Check if the desktop is valid */
    if (desktop == NULL) {
        return;
    }

    /* Logic to iconify all clients */
    // Placeholder for iterating over all clients and iconifying them
    // TODO
}


/* Cycle through active clients on the desktop */
void dcmd_desktop_clients_cycle_active(desktop_td *desktop)
{
    /* Check if the desktop is valid */
    if (desktop == NULL) {
        return;
    }

    /* Logic to cycle through active clients */
    // TODO
}


/* Cycle through client icons */
void dcmd_desktop_clients_cycle_icons(desktop_td *desktop)
{
    /* Check if the desktop is valid */
    if (desktop == NULL) {
        return;
    }

    /* Logic to cycle through client icons */
    // TODO
}


/* Lock the desktop */
void dcmd_desktop_lock(desktop_td *desktop)
{
    /* Check if the desktop is valid */
    if (desktop == NULL) {
        return;
    }

    /* Logic to lock the desktop */
    // TODO
}


/* Unlock the desktop */
void dcmd_desktop_unlock(desktop_td *desktop)
{
    /* Check if the desktop is valid */
    if (desktop == NULL) {
        return;
    }

    /* Logic to unlock the desktop */
    // TODO
}


/* Change the layout of the desktop */
void dcmd_desktop_layout(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return;
    }

    /* Logic to change the desktop layout */
}


/* Launch a new process for the desktop */
pid_t dcmd_desktop_process_launch(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    pid_t pid;

    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return -1;
    }

    /* Logic to launch a new process and return its PID */
    pid = fork();
    if (pid == 0) {
        /* TODO: Child process code here */
    }

    return pid;
}


/* Kill a process associated with the desktop */
bool dcmd_desktop_process_kill(desktop_td *desktop,
        action_data_desktop_td *desktop_data)
{
    /* Check if the desktop and data are valid */
    if (desktop == NULL || desktop_data == NULL) {
        return false;
    }

    /* Logic to kill the specified process */
    // TODO

    return true;
}
