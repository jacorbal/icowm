/**
 * @file cmds/geom.h
 *
 * @brief Client geometry command declarations
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

#ifndef CMDS_GEOM_H
#define CMDS_GEOM_H


/* Project includes */
#include <actdata.h>
#include <client.h>


/* Public interface */
/**
 * @brief Move the client to a new position
 *
 * @param client      Window to move
 * @param client_data Data containing the new position
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_move(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Center the client on its current screen
 *
 * @param client Window to center
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void wcmd_client_center(client_td *client);

/**
 * @brief Move the client to a specific monitor on its own surface
 *
 * Keeps the client's offset from its current monitor's own top-left
 * corner (not a resize, not a re-centering), translated onto the
 * target monitor's own top-left corner instead, then clamped so the
 * window stays fully on that monitor even if it is smaller than the
 * one the client came from.  A no-op if @p client is already on the
 * target monitor.
 *
 * @param client        Window to move
 * @param monitor_index Zero-based index into the client's own
 *                       surface's monitor list; out of range falls
 *                       back to monitor 0, logging a warning
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void wcmd_client_move_to_monitor(client_td *client, uint32_t monitor_index);

/**
 * @brief Move the client to the next monitor on its own surface
 *
 * Resolves @p client's current monitor, then calls @c
 * wcmd_client_move_to_monitor with the next index in the surface's
 * monitor list, wrapping back to @c 0 after the last one.  A no-op on
 * a surface with one monitor or none.
 *
 * @param client Window to move
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void wcmd_client_move_to_next_monitor(client_td *client);

/**
 * @brief Resize the client to new dimensions
 *
 * @param client      Window to resize
 * @param client_data Data containing the new size
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Apply a client's pending @c (_NET_WM_SYNC_REQUEST)-throttled
 *        resize
 *
 * Called from @c handler_sync_event when an @c AlarmNotify confirms the
 * client has redrawn to match the last size it was sent.  Clears the
 * client's wait state and, if a newer geometry arrived from
 * @c wcmd_client_resize while it was waiting, applies that geometry now
 * and sends the next sync request so the throttling pipeline keeps up
 * with an ongoing interactive resize.  A no-op for clients that are not
 * currently waiting on an acknowledgement.
 *
 * @param client Client whose alarm just fired
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_resize_flush_pending(client_td *client);

/**
 * @brief Maximize the client horizontally
 *
 * @param client Window to maximize horizontally
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void wcmd_client_maximize_horz(client_td *client);

/**
 * @brief Maximize the client vertically
 *
 * @param client Window to maximize vertically
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void wcmd_client_maximize_vert(client_td *client);

/**
 * @brief Maximize the client entirely
 *
 * @param client Window to maximize
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void wcmd_client_maximize(client_td *client);


#endif  /* ! CMDS_GEOM_H */
