/**
 * @file cmds/client/move.h
 *
 * @brief Client positioning command declarations
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

#ifndef CMDS_CCMD_MOVE_H
#define CMDS_CCMD_MOVE_H

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>



/**
 * @brief Apply a client's geometry to its target window in a single
 *        XCB call
 *
 * One shared function every geometry-changing operation in this
 * project funnels through (move, resize, maximize in any of its
 * three variants, shade/unshade, fullscreen/unfullscreen), matching
 * Openbox's own @c client_configure (@c client.c): builds the
 * correctly ordered values array @c xcb_configure_window itself
 * requires from whichever @c XCB_CONFIG_WINDOW_* bits @p mask sets,
 * rather than each caller building that same array by hand.  See the
 * full reasoning in @c cmds/client/geom.c, right above the
 * implementation.
 *
 * @param client       Client whose target window to configure
 * @param target       Window to configure; @a ccmd_target_win's own
 *                      result
 * @param mask         Bitwise OR of whichever @c XCB_CONFIG_WINDOW_X/
 *                      @c _Y/@c _WIDTH/@c _HEIGHT/@c _BORDER_WIDTH
 *                      bits are actually changing
 * @param x            New X position, only applied if @c XCB_CONFIG_
 *                      WINDOW_X is set in @p mask
 * @param y            New Y position, only applied if @c XCB_CONFIG_
 *                      WINDOW_Y is set in @p mask
 * @param w            New width, only applied if @c XCB_CONFIG_
 *                      WINDOW_WIDTH is set in @p mask
 * @param h            New height, only applied if @c XCB_CONFIG_
 *                      WINDOW_HEIGHT is set in @p mask
 * @param border_width New native border width, only applied if
 *                      @c XCB_CONFIG_WINDOW_BORDER_WIDTH is set in
 *                      @p mask
 *
 * @note A null @p client, one with no connection, or a @c XCB_WINDOW_
 *       NONE @p target is a silent no-op
 * @note Implemented in @c cmds/client/geom.c
 * @note Complexity: @e O(1)
 */
void ccmd_client_apply_geometry(client_td *client, xcb_window_t target,
        uint16_t mask, int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width);


/**
 * @brief Move the client to a new position
 *
 * @param client Window to move
 * @param pos    New position
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_move(client_td *client, struct position_s pos);


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
 * @brief Move the client to the monitor north of the current one on
 *        its own surface
 *
 * Resolves @p client's current monitor, then calls @a surface_
 * monitor_direction (surface.h) to find whichever one of the
 * surface's own monitors genuinely lies to the north of it, by real
 * physical position rather than RandR's own arbitrary detection
 * order the way the array-index-based @c next/@c prev pair this
 * replaced did.  Deliberately never wraps around to the southmost
 * monitor once already at the northmost, the same as @c windows.
 * desktop's own move-to-desktop pair (@a enact_client_send_to_
 * desktop_north and its three siblings, enact.h) already does not,
 * by default, without @c desktops.wrap-at-bounds enabled: unlike
 * wrapping a definite, ordered list (a desktop's own circular list,
 * or the array order this pair used to walk before it had any real
 * spatial meaning), wrapping a genuinely 2-D physical arrangement
 * has no one obviously correct meaning to begin with (does "east,
 * wrapped" mean the westmost monitor overall, or the westmost one
 * still on the same row?), so no attempt is made to invent one; a
 * monitor list, unlike a desktop one, also has no equivalent of
 * @c wrap-at-bounds to make that choice configurable in the first
 * place.
 *
 * @param client Window to move
 *
 * @note A no-op on a surface with one monitor or none, or when no
 *       monitor lies to the north of @p client's own current one
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void ccmd_client_move_to_monitor_north(client_td *client);


/**
 * @brief Move the client to the monitor south of the current one on
 *        its own surface
 *
 * See @a ccmd_client_move_to_monitor_north's comment for the
 * fuller reasoning, including why this never wraps around either.
 *
 * @param client Window to move
 *
 * @note A no-op on a surface with one monitor or none, or when no
 *       monitor lies to the south of @p client's own current one
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void ccmd_client_move_to_monitor_south(client_td *client);


/**
 * @brief Move the client to the monitor east of the current one on
 *        its own surface
 *
 * See @a ccmd_client_move_to_monitor_north's comment for the
 * fuller reasoning, including why this never wraps around either.
 *
 * @param client Window to move
 *
 * @note A no-op on a surface with one monitor or none, or when no
 *       monitor lies to the east of @p client's own current one
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void ccmd_client_move_to_monitor_east(client_td *client);


/**
 * @brief Move the client to the monitor west of the current one on
 *        its own surface
 *
 * See @a ccmd_client_move_to_monitor_north's comment for the
 * fuller reasoning, including why this never wraps around either.
 *
 * @param client Window to move
 *
 * @note A no-op on a surface with one monitor or none, or when no
 *       monitor lies to the west of @p client's own current one
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void ccmd_client_move_to_monitor_west(client_td *client);


#endif  /* ! CMDS_CCMD_MOVE_H */
