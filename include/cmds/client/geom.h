/**
 * @file cmds/client/geom.h
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

#ifndef CMDS_CCMD_GEOM_H
#define CMDS_CCMD_GEOM_H


/* Project includes */
#include <client.h>


/* Public interface */
/**
 * @brief Move the client to a new position
 *
 * @param client Window to move
 * @param x      New X position
 * @param y      New Y position
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_move(client_td *client, int32_t x, int32_t y);

/**
 * @brief Center the client on its current screen
 *
 * @param client Window to center
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void ccmd_client_center(client_td *client);

/**
 * @brief Move the client to a specific monitor on its own surface
 *
 * Keeps the client's offset from its current monitor's own top-left
 * corner (not a resize, not a re-centering), translated onto the target
 * monitor's own top-left corner instead, then clamped so the window
 * stays fully on that monitor even if it is smaller than the one the
 * client came from.
 *
 * @param client        Window to move
 * @param monitor_index Zero-based index into the client's own surface's
 *                      monitor list; out of range falls back to the
 *                      monitor with index 0, logging a warning
 *
 * @note A no-op if @p client is already on the target monitor
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void ccmd_client_move_to_monitor(client_td *client,
        uint32_t monitor_index);

/**
 * @brief Move the client to the next monitor on its own surface
 *
 * Resolves @p client's current monitor, then calls
 * @a ccmd_client_move_to_monitor with the next index in the surface's
 * monitor list, wrapping back to @c 0 after the last one.
 *
 * @param client Window to move
 *
 * @note A no-op on a surface with one monitor or none
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void ccmd_client_move_to_next_monitor(client_td *client);

/**
 * @brief Resize the client to new dimensions
 *
 * @param client Window to resize
 * @param x      New frame X position
 * @param y      New frame Y position
 * @param w      New frame width
 * @param h      New frame height
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_resize(client_td *client, int32_t x, int32_t y,
        uint32_t w, uint32_t h);

/**
 * @brief Apply a client's pending @c (_NET_WM_SYNC_REQUEST)-throttled
 *        resize
 *
 * Called from @a handler_sync_event when an @c AlarmNotify confirms the
 * client has redrawn to match the last size it was sent.  Clears the
 * client's wait state and, if a newer geometry arrived from
 * @a ccmd_client_resize while it was waiting, applies that geometry now
 * and sends the next sync request so the throttling pipeline keeps up
 * with an ongoing interactive resize.
 *
 * @param client Client whose alarm just fired
 *
 * @note A no-op for clients that are not currently waiting on an
 *       acknowledgement
 * @note Complexity: @e O(1)
 */
void ccmd_client_resize_flush_pending(client_td *client);

/**
 * @brief Maximize the client horizontally
 *
 * @param client Window to maximize horizontally
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void ccmd_client_maximize_horz(client_td *client);

/**
 * @brief Maximize the client vertically
 *
 * @param client Window to maximize vertically
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void ccmd_client_maximize_vert(client_td *client);

/**
 * @brief Maximize the client entirely
 *
 * @param client Window to maximize
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void ccmd_client_maximize(client_td *client);

/**
 * @brief Resolve the workarea a client's own maximize/fullscreen target
 *        should fill, on whichever monitor it currently sits on
 *
 * The same resolution @a ccmd_client_maximize/_horz/_vert already use
 * internally, exposed here for any other caller needing the exact same
 * target rect a maximized client already fills:
 * @a ccmd_client_toggle_decorate (@c cmds/client/state.c),
 * recalculating a maximized client's own geometry to still fill it
 * after decoration changes size how much of it its own frame extents
 * eat into, rather than just growing or shrinking the frame in place
 * around whatever position/size it already had.
 *
 * @param client Client to resolve the workarea for
 * @param out_x  Receives the workarea's own left edge (may be null to
 *               skip)
 * @param out_y  Receives the workarea's own top edge (may be null to
 *               skip)
 * @param out_w  Receives the workarea's own width; required
 * @param out_h  Receives the workarea's own height; required
 *
 * @return Status of the operation
 * @retval  true on success
 * @retval false if @p client is @c NULL, has no monitor or desktop
 *               resolvable, or that desktop's own workarea has not been
 *               computed yet
 *
 * @note Complexity: @e O(1)
 */
bool ccmd_client_monitor_workarea(client_td *client,
        int32_t *out_x, int32_t *out_y,
        uint16_t *out_w, uint16_t *out_h);

/**
 * @brief Re-fill an already-maximized client's own geometry against
 *        its current workarea
 *
 * Resolved against @a ccmd_client_monitor_workarea (the same
 * resolution @a ccmd_client_maximize itself already uses), so the
 * client ends up exactly refilling the workarea as it now stands,
 * the same as if it had only just been maximized; a no-op unless
 * @p client is currently maximized on at least one axis.  Only the
 * axis (or axes) its own @c properties.state actually names gets
 * touched.
 *
 * @param client Client to re-fill
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_refill_maximized(client_td *client);


#endif  /* ! CMDS_CCMD_GEOM_H */
