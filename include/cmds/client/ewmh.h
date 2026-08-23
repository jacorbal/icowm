/**
 * @file cmds/client/ewmh.h
 *
 * @brief Functions writing a client's ICCCM @c WM_STATE and EWMH
 *        @c _NET_WM_STATE properties
 *
 * @defgroup cmds Client, desktop, and surface commands
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_CCMD_EWMH_H
#define CMDS_CCMD_EWMH_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>


/* ICCCM WM_STATE property values (ICCCM section 4.1.3.1) */
#define CCMD_WM_STATE_WITHDRAWN (0u)
#define CCMD_WM_STATE_NORMAL (1u)
#define CCMD_WM_STATE_ICONIC (3u)


/* Public interface */
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
 * @note Implemented in @c cmds/client/ewmh.c
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void ccmd_set_wm_state(client_td *client,
        uint32_t state, xcb_window_t icon_window);

/**
 * @brief Remove the ICCCM @c WM_STATE property from a client
 *
 * Deletes the legacy @c WM_STATE property, typically when the client is
 * being withdrawn from window-manager control.
 *
 * @param client Pointer to the client
 *
 * @note Implemented in @c cmds/client/ewmh.c
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void ccmd_clear_wm_state(client_td *client);

/**
 * @brief Republish every @c _NET_WM_STATE atom a client currently
 *        holds, read straight off its own fields, in one single XCB
 *        write
 *
 * Openbox's own real answer to keeping @c _NET_WM_STATE in sync
 * (confirmed directly against its source, @c client_change_state in
 * @c client.c): rebuild the whole list from scratch every time, from
 * whichever of the client's own boolean fields are true right now,
 * rather than reading the property back first to add or remove one
 * specific atom.  See the full reasoning in @c cmds/client/ewmh.c,
 * right above the implementation, for exactly how each atom maps to
 * @p client's own fields.
 *
 * @param client Client whose current state to republish
 *
 * @note A null @p client or one with no @c ewmh connection is a
 *       silent no-op
 * @note Implemented in @c cmds/client/ewmh.c
 * @note Complexity: @e O(1)
 */
void ccmd_client_sync_states(client_td *client);


#endif  /* ! CMDS_CCMD_EWMH_H */
