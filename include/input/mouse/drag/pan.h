/**
 * @file input/mouse/drag/pan.h
 *
 * @brief Edge-triggered viewport pan during a drag
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

#ifndef INPUT_MOUSE_DRAG_PAN_H
#define INPUT_MOUSE_DRAG_PAN_H


/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Track whether the pointer is currently held against
 *        a pan-eligible screen edge, and schedule (or keep, or cancel)
 *        the pending viewport-pan countdown accordingly
 *
 * Restarting the countdown on every single motion notify while the same
 * edge stays held is deliberately avoided, since that would leave the
 * countdown permanently reset and never actually elapse.  When the
 * pointer sits against two edges at once (a screen corner), the
 * horizontal edge wins, matching whichever edge @a drag_warp_edge_check
 * (@c input/mouse/drag/warp.h) already preferred before this file
 * existed.
 *
 * @param root_x Pointer X position in root-window coordinates
 * @param root_y Pointer Y position in root-window coordinates
 *
 * @note A no-op, clearing any pending pan, unless the stage the drag
 *       is on has @c viewport.pan_on_edge_drag enabled and its current
 *       desktop's viewport still has room to pan toward the held edge
 *       (see @a scmd_stage_viewport_pan_available, @c cmds/stage.h)
 * @note Complexity: @e O(1)
 */
void drag_pan_edge_check(int16_t root_x, int16_t root_y);

/**
 * @brief Milliseconds until a pointer held against a pan-eligible
 *        screen edge is due to pan the viewport
 *
 * Tracked by @c drag_update (@c input/mouse/drag.h) as the pointer
 * moves (see @p viewport.pan_on_edge_drag in @c config.json,
 * @c config_desktop_s); serviced by @a drag_pan_tick.
 *
 * @return Milliseconds remaining (never negative), or @c -1 if the
 *         pointer is not currently held against an eligible edge
 *
 * @note Complexity: @e O(1)
 */
int drag_pan_ms_remaining(void);

/**
 * @brief Perform the pending edge pan, if its countdown has elapsed
 *
 * Meant to be called on every main-loop iteration, the same way
 * @a drag_warp_tick is (see @c loop.c), so a pointer left resting
 * against a screen edge during a window or icon move still keeps
 * panning the viewport even with no further @c MotionNotify arriving to
 * drive it: unlike a desktop warp, panning never moves the pointer
 * itself, so nothing else would otherwise wake this back up.  A no-op
 * when no pan is currently pending, its countdown has not yet elapsed,
 * the drag it belonged to is no longer a plain window or icon move,
 * panning is disabled, or the viewport no longer has room to pan that
 * way.
 *
 * Pans the current desktop's viewport by one screen toward the held
 * edge, translating every non-sticky client the same way
 * @a scmd_stage_viewport_pan_north and its three siblings already do,
 * then keeps the dragged window or icon (unless it is itself sticky,
 * in which case the pan already left it untouched) under the pointer
 * across the pan, adjusting the drag's internal state so the shift
 * does not make it visually snap on the next @c MotionNotify.  Once
 * fired, immediately re-arms its own countdown at the shorter
 * @c WM_VIEWPORT_PAN_REPEAT_MS interval (@c defs/desktop.h) for as long
 * as the edge stays held, so a single edge hold pans repeatedly rather
 * than only once, exactly like @c input/mouse/viewport/edge.h's
 * hover-triggered counterpart.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void drag_pan_tick(xcb_connection_t *connection);


#endif  /* ! INPUT_MOUSE_DRAG_PAN_H */
