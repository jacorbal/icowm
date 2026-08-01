/**
 * @file cmds/util.h
 *
 * @brief Internal utility declarations shared across the @c cmds
          subsystem
 *
 * Helpers that provide low-level XCB and EWMH plumbing used by the
 * client-command modules (@c ccmd, @c geom, @c layer, @c meta).
 *
 * @ingroup cmds Client commands subsystem
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_UTIL_H
#define CMDS_UTIL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Project includes */
#include <client.h>


#define WCMD_WM_STATE_WITHDRAWN (0u)
#define WCMD_WM_STATE_NORMAL    (1u)
#define WCMD_WM_STATE_ICONIC    (3u)


/* Internal interface */
/**
 * @brief Retrieve the ID of the currently active window for a screen
 *
 * @param ewmh      Pointer to the EWMH connection
 * @param screen_id Screen identifier
 *
 * @return ID of the active window, or @c XCB_WINDOW_NONE on error
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t wcmd_active_win(xcb_ewmh_connection_t *ewmh,
        uint32_t screen_id);

/**
 * @brief Intern an atom name in the X11 system
 *
 * @param connection Pointer to the X11 connection
 * @param name       Name of the atom
 *
 * @return Interned atom ID, or @c XCB_ATOM_NONE on failure
 *
 * @note Complexity: @e O(n), where @e n is the length of @p name
 */
xcb_atom_t wcmd_intern_atom(xcb_connection_t *connection,
        const char *name);

/**
 * @brief Return the frame window when decorated, otherwise the client
 *        window
 *
 * @param client Pointer to the client to inspect
 *
 * @return Frame window when available, client window otherwise;
 *         @c XCB_WINDOW_NONE if @p client is @c NULL
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t wcmd_target_win(client_td *client);

/**
 * @brief Retrieve the pixel dimensions of the client's current screen
 *
 * Either @p out_w or @p out_h may be null but not both.
 *
 * @param client Pointer to the client whose screen is queried
 * @param out_w  Destination for the screen width in pixels, or null
 * @param out_h  Destination for the screen height in pixels, or null
 *
 * @return @c true on success, @c false on failure
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
bool wcmd_screen_dim(client_td *client,
        uint16_t *out_w, uint16_t *out_h);

/**
 * @brief Passively grab all mouse buttons on an undecorated client
 *
 * Installs a synchronous passive grab on the client window so the
 * window manager can focus the client on click before replaying or
 * consuming the button event.
 *
 * @param client Pointer to the client
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_grab_buttons(client_td *client);

/**
 * @brief Remove passive button grabs from an undecorated client
 *
 * Releases the passive grab installed by @c wcmd_client_grab_buttons so
 * mouse input flows directly to the client again.
 *
 * @param client Pointer to the client
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_ungrab_buttons(client_td *client);

/**
 * @brief Write the ICCCM @c WM_STATE property for a client
 *
 * Stores the client state and optional icon window in the legacy
 * @c WM_STATE property expected by pagers, taskbars, and older X11
 * clients.
 *
 * @param client      Pointer to the client
 * @param state       ICCCM window-manager state value
 * @param icon_window Icon window associated with @p state, or
 *                    @c XCB_NONE
 *
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void wcmd_set_wm_state(client_td *client,
        uint32_t state, xcb_window_t icon_window);

/**
 * @brief Remove the ICCCM @c WM_STATE property from a client
 *
 * Deletes the legacy @c WM_STATE property, typically when the client is
 * being withdrawn from window-manager control.
 *
 * @param client Pointer to the client
 *
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void wcmd_clear_wm_state(client_td *client);

/**
 * @brief Add multiple EWMH window states to a client
 *
 * @param client     Pointer to the client
 * @param num_states Number of state name strings that follow
 * @param ...        @c (const char*) state name arguments
 *
 * @note Complexity: @e O(n), where @e n is @p num_states
 */
void wcmd_add_states(client_td *client, uint32_t num_states, ...);

/**
 * @brief Remove multiple EWMH window states from a client
 *
 * @param client     Pointer to the client
 * @param num_states Number of state name strings that follow
 * @param ...        @c (const char*) state name arguments
 *
 * @note Complexity: @e O(n * m), where @e n is @p num_states and @e m
 *       is the current number of window states
 */
void wcmd_rem_states(client_td *client, uint32_t num_states, ...);


#endif  /* ! CMDS_UTIL_H */
