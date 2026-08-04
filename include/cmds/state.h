/**
 * @file cmds/state.h
 *
 * @brief Client state-transition command declarations: shading,
 *        fullscreen, and decoration toggling
 *
 * Declares commands that change a client's visual state in ways that
 * require XCB geometry manipulation beyond a simple flag update, i.e.,
 * shade/unshade, fullscreen/unfullscreen, and decoration toggle.
 * Focus and visibility commands are declared in @c cmds/ccmd.h.
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

#ifndef CMDS_STATE_H
#define CMDS_STATE_H


/* Project includes */
#include <client.h>


/* Public interface */
/**
 * @brief Shade (roll-up) the client window
 *
 * Collapses the client window to show only its titlebar by unmapping
 * the content window and shrinking the frame to the titlebar height.
 * Sets @c CLIENT_FLAG_SHADED and synchronizes EWMH state.
 *
 * @param client Window to shade
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_shade(client_td *client);

/**
 * @brief Unshade (roll-down) the client window
 *
 * Restores the client window from shaded state by re-mapping the
 * content window and expanding the frame back to its full height.
 * Clears @c CLIENT_FLAG_SHADED and synchronizes EWMH state.
 *
 * @param client Window to unshade
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_unshade(client_td *client);

/**
 * @brief Toggle client shading state
 *
 * Calls @a wcmd_client_shade when the window is not shaded, and
 * @a wcmd_client_unshade when it is.
 *
 * @param client Window to toggle shade on
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_toggle_shade(client_td *client);

/**
 * @brief Set the client to fullscreen mode
 *
 * Saves the current geometry, removes decoration, and resizes the
 * client to cover the full screen.  Sets @c CLIENT_STATE_FULLSCREEN
 * and synchronizes EWMH @c _NET_WM_STATE.
 *
 * @param client Window to set to fullscreen
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_fullscreen(client_td *client);

/**
 * @brief Remove fullscreen mode from the client
 *
 * Restores the saved pre-fullscreen geometry and decoration state.
 * Clears @c CLIENT_STATE_FULLSCREEN and synchronizes EWMH
 * @c _NET_WM_STATE.
 *
 * @param client Window to unfullscreen
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_unfullscreen(client_td *client);

/**
 * @brief Toggle fullscreen mode for the client
 *
 * Calls @a wcmd_client_fullscreen when not in fullscreen mode, and
 * @a wcmd_client_unfullscreen when it is.
 *
 * @param client Window to toggle fullscreen state on
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_toggle_fullscreen(client_td *client);

/**
 * @brief Toggle window decoration on or off for the client
 *
 * If the client is currently decorated, removes the titlebar and
 * adjusts the frame extents so the frame covers only the client
 * content area plus border.  If undecorated, restores the titlebar
 * and the original frame extents.
 *
 * @param client Window whose decoration is to be toggled
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_toggle_decoration(client_td *client);


#endif  /* ! CMDS_STATE_H */
