/**
 * @file input/mouse/drag/warp.h
 *
 * @brief Edge-triggered desktop warp during a drag
 *
 * @ingroup input_mouse
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_DRAG_WARP_H
#define INPUT_MOUSE_DRAG_WARP_H


/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Track whether the pointer is currently held against a
 *        warp-eligible screen edge, and schedule (or keep, or cancel)
 *        the pending desktop-warp countdown accordingly
 *
 * A no-op, clearing any pending warp, unless the surface the drag is
 * on actually has @c desktops.warp_on_edge_drag enabled and more than
 * one desktop to warp between.  Restarting the countdown on every
 * single motion notify while the same edge stays held is deliberately
 * avoided, since that would leave the countdown permanently reset and
 * never actually elapse.  When the pointer sits against two edges
 * at once (a screen corner), the horizontal edge wins, matching
 * whichever edge this same check already preferred before a second,
 * vertical one existed at all.
 *
 * @param root_x Pointer X position in root-window coordinates
 * @param root_y Pointer Y position in root-window coordinates
 *
 * @note Complexity: @e O(1)
 */
void drag_warp_edge_check(int16_t root_x, int16_t root_y);

/**
 * @brief Milliseconds until a pointer held against a warp-eligible
 *        screen edge is due to switch desktops
 *
 * Tracked by @c drag_update (@c input/mouse/drag.h) as the pointer
 * moves (see @p desktops.warp_on_edge_drag in @c config.json,
 * @c config_desktop_s); serviced by @a drag_warp_tick.
 *
 * @return Milliseconds remaining (never negative), or @c -1 if the
 *         pointer is not currently held against an eligible edge
 *
 * @note Complexity: @e O(1)
 */
int drag_warp_ms_remaining(void);

/**
 * @brief Perform the pending edge warp, if its countdown has elapsed
 *
 * Meant to be called on every main-loop iteration, the same way
 * @a menu_confirm_dialog_tick is (see @c loop.c), so a pointer left
 * resting against a screen edge during a window or icon move still
 * switches desktops even with no further @c MotionNotify arriving to
 * drive it.  A no-op when no warp is currently pending, its countdown
 * has not yet elapsed, the drag it belonged to is no longer a plain
 * window or icon move, warping is disabled, there is only one desktop,
 * or (with @p desktops.wrap_at_bounds off) the edge held is already the
 * first or last desktop.
 *
 * Moves the dragged client to the adjacent desktop without unmapping it
 * at any point (it must stay visible throughout), switches the
 * surface's current desktop to match, and repositions the pointer
 * to the opposite edge.  Adjusting the drag's internal state so
 * that jump does not make the dragged window visually snap on the next
 * @c MotionNotify.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       either desktop involved (from @a surface_clients_hide /
 *       @a surface_clients_show)
 */
void drag_warp_tick(xcb_connection_t *connection);


#endif  /* ! INPUT_MOUSE_DRAG_WARP_H */
