/**
 * @file cmds/client/basic.h
 *
 * @brief Functions on executions over clients using the XCB interface
 *        with needed EWMH and ICCCM updates
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

#ifndef CMDS_CCMD_BASIC_H
#define CMDS_CCMD_BASIC_H


/* Command includes */
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>


/* Public interface */
/**
 * @brief Perform the action to close the client
 *
 * @param client Window to close
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_close(client_td *client);

/**
 * @brief Forcibly terminate the client's connection to the X server
 *
 * Unlike @a ccmd_client_close (which only destroys the client's window
 * resource), this severs the client's entire X connection at the
 * protocol level via @a xcb_kill_client, matching the conventional
 * "force kill an unresponsive window" behavior (e.g., @c xkill).
 * Intended as a last resort for clients that do not react to a normal
 * close request.
 *
 * @param client Window whose owning client connection should be
 *               forcibly terminated
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_kill(client_td *client);

/**
 * @brief Restore the client to its original state
 *
 * @param client Window to restore
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_restore(client_td *client);

/**
 * @brief Focus on the given client
 *
 * @param client Window to focus
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_focus(client_td *client);

/**
 * @brief Remove focus from the given client
 *
 * @param client Window to unfocus
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unfocus(client_td *client);

/**
 * @brief Perform the action to iconify (and minimize it)
 *
 * @param client Window to iconify
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_iconify(client_td *client);

/**
 * @brief Hide the client by minimizing it without iconifying
 *
 * @param client Window to hide
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_hide(client_td *client);

/**
 * @brief Show (unhide) the client
 *
 * @param client Window to unhide
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unhide(client_td *client);

/**
 * @brief Pin the client (visible on all desktops)
 *
 * @param client Window to pin
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_pin(client_td *client);

/**
 * @brief Unpin the client
 *
 * @param client Window to unpin
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unpin(client_td *client);

/**
 * @brief Toggle pin mode for the client
 *
 * @param client Window to toggle pin state
 *
 * @note No-op on a surface with only one desktop: stickiness has
 *       nothing to actually toggle when there is only the one
 * @note Complexity: @e O(1)
 */
void ccmd_client_toggle_pin(client_td *client);

/**
 * @brief Mark the client as urgent (requesting attention)
 *
 * @param client Window to mark as urgent
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_urge(client_td *client);

/**
 * @brief Clear urgency marking from the client
 *
 * @param client Window to clear urgency
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unurge(client_td *client);

/**
 * @brief Publish @c _NET_WM_ALLOWED_ACTIONS for a client
 *
 * Computes the set of EWMH actions currently permitted for @p client
 * based on its resizable, focusable, and decoration properties, and
 * writes the result to the @c _NET_WM_ALLOWED_ACTIONS window property.
 * Must be called whenever the client's capabilities change (e.g., after
 * toggling resizability or decoration).
 *
 * @param client Client whose allowed-actions property should be updated
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_update_allowed_actions(client_td *client);


#endif  /* ! CMDS_CCMD_BASIC_H */
