/**
 * @file cmds/dcmd.h
 *
 * @brief Functions on executions over desktops using the XCB interface
 *        with needed EWMH and ICCCM updates
 *
 * @ingroup cmds
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_DCMD_H
#define CMDS_DCMD_H


/* System includes */
#include <stdbool.h>
#include <sys/types.h>  /* pid_t */

/* Project includes */
#include <actdata.h>
#include <desktop.h>


/* Public interface */
/**
 * @brief Rename the desktop to a new name
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing new name information
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_rename(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Change the background color of the specified desktop
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing new background color
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_bg_color(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Clear the desktop
 *
 * @param desktop Pointer to the desktop to be cleared
 *
 * @note Complexity: @e O(n) where @e n is the number of clients
 */
void dcmd_desktop_clear(desktop_td *desktop);

/**
 * @brief Add a client to the desktop
 *
 * Adds a specified client to the current desktop
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing information for the client to add
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_client_add(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Remove a specific client from the desktop
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing information for the client to
 *                     remove
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_client_rem(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Send a client to a specific desktop
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing information for the client
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_client_send(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Clone a client to a target desktop without removing it from
 *        the source desktop
 *
 * Adds the client to the target desktop's stacking order and client
 * table while leaving the original in place.
 *
 * @param desktop      Pointer to the source desktop
 * @param desktop_data Data containing the client pointer and target
 *                     desktop pointer
 *
 * @note The client pointer is shared between two desktops; lifecycle
 *       management (destruction) is the responsibility of the caller
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_client_clone(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Send a client to the front of the desktop's window stack (top)
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing information for the client to
 *                     send front
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_client_send_front(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Send client to the back of the desktop's window stack (bottom)
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing information for the client to
 *                     send back
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_client_send_back(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Rearrange the clients on the desktop
 *
 * Rearranges the displayed clients according to a layout algorithm
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(n) where @e n is the number of clients
 */
void dcmd_desktop_clients_rearrange(desktop_td *desktop);

/**
 * @brief Iconify all clients on the desktop
 *
 * Minimizes and iconifies all clients on the specified desktop.
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(n) where @e n is the number of clients
 */
void dcmd_desktop_clients_iconify_all(desktop_td *desktop);

/**
 * @brief Cycles focus between active clients on the desktop
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_clients_cycle_active(desktop_td *desktop);

/**
 * @brief Cycles focus to the previous active client on the desktop
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(n), where @e n is the number of active clients
 */
void dcmd_desktop_clients_cycle_prev(desktop_td *desktop);

/**
 * @brief Cycle focus between client icons on the desktop
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_clients_cycle_icons(desktop_td *desktop);

/**
 * @brief Lock the specified desktop to prevent user interaction
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_lock(desktop_td *desktop);

/**
 * @brief Unlock the specified desktop to allow user interaction
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_unlock(desktop_td *desktop);

/**
 * @brief Change the layout of the current desktop
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing layout configuration
 *
 * @note Complexity: @e O(1)
 */
void dcmd_desktop_layout(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Launch a new process for the desktop
 *
 * Launches a new process associated with the current desktop and
 * returns its PID.
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing information of the process to
 *                     launch
 * @return PID of the launched process
 *
 * @note Complexity: @e O(1)
 */
pid_t dcmd_desktop_process_launch(desktop_td *desktop,
        action_data_desktop_td *desktop_data);

/**
 * @brief Kill a process associated with the desktop
 *
 * Kills the specified process associated with the current desktop.
 *
 * @param desktop      Pointer to the desktop
 * @param desktop_data Data containing PID of the process to kill
 *
 * @return Return @c true if the operation was successful, or otherwise
 *
 * @note Complexity: @e O(1)
 */
bool dcmd_desktop_process_kill(desktop_td *desktop,
        action_data_desktop_td *desktop_data);


#endif  /* ! CMDS_DCMD_H */
