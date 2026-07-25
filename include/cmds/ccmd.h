/**
 * @file cmds/ccmd.h
 *
 * @brief Functions on executions over clients using the XCB interface
 *        with needed EWMH and ICCCM updates
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_CCMD_H
#define CMDS_CCMD_H


/* Project includes */
#include <actdata.h>
#include <client.h>


/* Public interface */
/**
 * @brief Perform the action to close the client
 *
 * @param client Window to close
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_close(client_td *client);

/**
 * @brief Forcibly terminate the client's connection to the X server
 *
 * Unlike @a wcmd_client_close (which only destroys the client's window
 * resource), this severs the client's entire X connection at the
 * protocol level via @c xcb_kill_client, matching the conventional
 * "force kill an unresponsive window" behavior (e.g.,
 * @c xkill). Intended as a last resort for clients that do not react to
 * a normal close request.
 *
 * @param client Window whose owning client connection should be
 *               forcibly terminated
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_kill(client_td *client);

/**
 * @brief Restore the client to its original state
 *
 * @param client Window to restore
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_restore(client_td *client);

/**
 @brief Focus on the given client
 *
 * @param client Window to focus
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_focus(client_td *client);

/**
 * @brief Remove focus from the given client
 *
 * @param client Window to unfocus
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_unfocus(client_td *client);

/**
* @brief Move the client to a new position
 *
 * @param client      Window to move
 * @param client_data Data containing new position
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_move(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Center the client on its current surface
 *
 * @param client Window to center
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_center(client_td *client);

/**
 * @brief Resize the client to new dimensions
 *
 * @param client      Window to resize
 * @param client_data Data containing new size
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data);

/**
* @brief Rename the client
 *
 * @param client      Window to rename
 * @param client_data Data containing new name
 *
 * @note Complexity: @e O(n), where @e n is the length of the new name
 */
void wcmd_client_rename(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Change the class of the client
 *
 * @param client      Window to reclassify
 * @param client_data Data containing new class
 *
 * @note Complexity: @e O(1), where @e n is the length of the class name
 */
void wcmd_client_reclass(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Change the role of the client
 *
 * @param client      Window to change its role
 * @param client_data Data containing new class
 *
 * @note Complexity: @e O(1), where @e n is the length of the role name
 */
void wcmd_client_rerole(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Perform the action to maximize a client horizontally
 *
 * @param client Window to maximize horizontally
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_maximize_horz(client_td *client);

/**
 * @brief Perform the action to maximize a client vertically
 *
 * @param client Window to maximize vertically
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_maximize_vert(client_td *client);

/**
 * @brief Perform the action to maximize a client entirely
 *
 * @param client Window to maximize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_maximize(client_td *client);

/**
 * @brief Perform the action to iconify (and minimize it)
 *
 * @param client Window to iconify
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_iconify(client_td *client);

/**
 * @brief Hide the client by minimizing it without iconifying
 *
 * @param client Window to hide
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_hide(client_td *client);

/**
 * @brief Show (unhide) the client
 *
 * @param client Window to hide
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_unhide(client_td *client);

/**
 * @brief Shade (roll-up) the client
 *
 * @param client Window to shade
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_shade(client_td *client);

/**
 * @brief Unshade (roll-down) the client
 *
 * @param client Window to unshade
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_unshade(client_td *client);

/**
 * @brief Toggle client shading
 *
 * @param client Window to toggle shade in
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_toggle_shade(client_td *client);

/**
 * @brief Set the client to sticky mode
 *
 * @param client Window to make sticky
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_sticky(client_td *client);

/**
 * @brief Remove sticky mode from the client
 *
 * @param client Window to unstick
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_unsticky(client_td *client);

/**
 * @brief Toggle sticky mode for the client
 *
 * @param client Window to toggle sticky state
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_toggle_sticky(client_td *client);

/**
 * @brief Set the client to full screen mode
 *
 * @param client Window to maximize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_fullscreen(client_td *client);

/**
 * @brief Remove full screen mode from the client
 *
 * @param client Window to unmaximize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_unfullscreen(client_td *client);

/**
 * @brief Toggle full screen mode for the client
 *
 * @param client Window to toggle full screen state
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_toggle_fullscreen(client_td *client);

/**
 * @brief Center the client on screen
 *
 * @param client Window to center on screen
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_center(client_td *client);

/**
 * @brief Raise the client to the top of stack
 *
 * @param client Window to raise
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_raise(client_td *client);

/**
 * @brief Lower the client to the bottom of stack
 *
 * @param client Window to lower
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_lower(client_td *client);

/**
 * @brief Layer the client above others
 *
 * @param client Window to layer above
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_layer_above(client_td *client);

/**
 * @brief Layer the client in normal position
 *
 * @param client Window to normalize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_layer_normal(client_td *client);

/**
 * @brief Layer the client below others
 *
 * @param client Window to layer below
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_layer_below(client_td *client);

/**
 * @brief Mark the client as urgent
 *
 * @param client Window to mark as urgent
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_set_urgent(client_td *client);

/**
 * @brief Clear urgency marking from the client
 *
 * @param client Window to clear urgency
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_clear_urgent(client_td *client);

/**
 * @brief Set the icon for the client
 *
 * @param client      Window to set icon
 * @param client_data Data containing icon information
 *
 * @note Complexity: @e O(1), where @e n is the length of the icon name
 */
void wcmd_client_set_icon(client_td *client,
        action_data_client_td *client_data);


/**
 * @brief Toggle window decoration on or off for the client
 *
 * If the client is currently decorated, removes the titlebar (unmaps
 * it) and adjusts the frame extents so the frame covers only the client
 * content area plus border.  If not decorated, restores the titlebar
 * and the original frame extents.
 *
 * @param client Window whose decoration is to be toggled
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_toggle_decoration(client_td *client);


#endif  /* ! CMDS_CCMD_H */
