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


/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>


/* Public interface */
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
 * @param border_width New native border width, only applied if @c
 *                      XCB_CONFIG_WINDOW_BORDER_WIDTH is set in
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
 * See @a ccmd_client_move_to_monitor_north's own doc comment for the
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
 * See @a ccmd_client_move_to_monitor_north's own doc comment for the
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
 * See @a ccmd_client_move_to_monitor_north's own doc comment for the
 * fuller reasoning, including why this never wraps around either.
 *
 * @param client Window to move
 *
 * @note A no-op on a surface with one monitor or none, or when no
 *       monitor lies to the west of @p client's own current one
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void ccmd_client_move_to_monitor_west(client_td *client);

/**
 * @brief Resize the client to new dimensions
 *
 * @param client Window to resize
 * @param geom   New frame position and dimensions
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_resize(client_td *client, struct geometry_s geom);

/**
 * @brief Resize the client to new dimensions immediately, bypassing
 *        any in-flight @c _NET_WM_SYNC_REQUEST throttling
 *
 * @c ccmd_client_resize's own queue-behind-the-outstanding-
 * acknowledgment behavior exists to avoid piling up unacknowledged
 * configures during a live sequence of rapid resize calls (an
 * ordinary interactive drag).  It is the wrong behavior for a single,
 * already-final geometry with no further calls to follow, since a
 * client that happens to still be mid-exchange from an earlier,
 * unrelated resize would otherwise have this one silently queued
 * behind that exchange's own @c AlarmNotify, with nothing left to
 * ever flush it once no further resize call arrives to retry it.
 * Callers with exactly that shape (one call, known to be the last)
 * should call this instead of @a ccmd_client_resize.
 *
 * @param client Window to resize
 * @param geom   New frame position and dimensions
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_resize_force(client_td *client, struct geometry_s geom);

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
 * @brief Demote a single axis's maximize state alone, without
 *        touching geometry at all
 *
 * For a caller that has already applied the correct un-maximized
 * geometry itself (a mouse-drag resize crossing the resistance
 * threshold on a maximized axis, live, on the very same motion
 * event; see @c drag_update, input/mouse/drag.c), unlike @c
 * ccmd_client_maximize_horz/@c _vert's own demote branch, which
 * always restores geometry from @c layout.geometry.old itself as
 * part of the same call.
 *
 * @param client Client whose axis just stopped being maximized
 * @param dir    @c 1 for horizontal, @c 2 for vertical; matches
 *               @c ccmd_client_maximize_horz/@c _vert's own axis
 *               numbering
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_demote_axis_state(client_td *client, int dir);

/**
 * @brief Promote a single axis's maximize state back, the exact
 *        inverse of @c ccmd_client_demote_axis_state, without
 *        touching geometry at all
 *
 * For a caller whose own drag has already re-frozen that axis back
 * at its maximized geometry itself (a mouse-drag resize dragged back
 * under the resistance threshold before release, live, on the very
 * same motion event; see @c drag_update, input/mouse/drag.c): the
 * live, reversible half of the same mechanism @c ccmd_client_demote_
 * axis_state's own doc comment describes, restoring @c MAXIMIZED
 * itself rather than @c NORMAL when the other axis is already
 * maximized on its own, @c MAXIMIZED_HORZ/@c _VERT otherwise.
 *
 * @param client Client whose axis just became maximized again
 * @param dir    @c 1 for horizontal, @c 2 for vertical; matches
 *               @c ccmd_client_maximize_horz/@c _vert's own axis
 *               numbering
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_promote_axis_state(client_td *client, int dir);

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
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h);

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
