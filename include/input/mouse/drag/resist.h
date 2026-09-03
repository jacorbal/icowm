/**
 * @file input/mouse/drag/resist.h
 *
 * @brief Resistance-threshold math for a maximized-axis mouse resize
 *
 * Declares the functions @c drag/resist.c exposes for @c drag.c to call
 * directly; nothing outside the drag subsystem calls either of these.
 *
 * @note This header is private to @c input/mouse/drag/ (and
 *       @c input/mouse/drag.c, which orchestrates every @c drag/ file)
 *       and must not be included outside of them
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_DRAG_RESIST_H
#define INPUT_MOUSE_DRAG_RESIST_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Recompute a maximized-on-one-axis client's resistance state
 *        for the current motion event, live
 *
 * For each such axis, compares @p drag_dist_w/@p drag_dist_h against
 * @p resistance and updates @c s_drag.resize_w/@c s_drag.resize_h to
 * match, reversibly, on every call along the drag.  Dragging past the
 * threshold lets that axis start resizing, dragging back under it
 * before release re-freezes it at the maximized size again.
 *
 * Under @c s_drag.solid_drag, a transition either way also updates
 * @c s_drag.client's @c properties.state (and its EWMH atoms) to match,
 * this same instant, via @c ccmd_client_demote_axis_state /
 * @c ccmd_client_promote_axis_state; under @c !solid_drag the real
 * window sits off-screen for the whole drag regardless, so state stays
 * untouched here and is instead settled once, in
 * @c drag_resist_axis_finalize, once the real window reappears with its
 * own real geometry.
 *
 * @param drag_dist_w Absolute drag distance so far on the width axis
 * @param drag_dist_h Absolute drag distance so far on the height axis
 * @param resistance  Configured resistance threshold in pixels
 *
 * @note A no-op unless @c s_drag.resist_axis_w and/or
 *       @c s_drag.resist_axis_h is set (only @c true when the drag
 *       started with that axis maximize-locked)
 * @note Requires a valid global @c s_drag context
 * @note Complexity: @e O(1)
 *
 * @see @c drag_start_resize_axis_locked in @c drag.c
 */
void drag_resist_axis_update(uint32_t drag_dist_w, uint32_t drag_dist_h,
        uint32_t resistance);

/**
 * @brief Settle a maximize-locked axis's final state once a resize drag
 *        ends, for whichever case the live sync of
 *        @a drag_resist_axis_update could not already handle
 *
 * Under @c solid_drag, every threshold crossing already updated
 * @c s_drag.client's state live, on the very motion event it happened
 * on, so nothing further is needed here for that case.
 *
 * Under @c !solid_drag, state was never touched mid-drag at all (see
 * @c drag_resist_axis_update's comment), so it still reflects whichever
 * axis this client started this drag maximized on; @c s_drag.resize_w /
 * @c s_drag_resize_h, read here fresh at the very end, say whether the
 * resistance threshold on that same axis ended up crossed by release
 * time, demoting it to match via @c ccmd_client_demote_axis_state if
 * so.  Geometry itself must already be correctly settled by the time
 * this is called, from this same drag's finalize step; this only ever
 * updates @c properties.  state (and its EWMH atoms) to match, never
 * geometry.
 *
 * @param finalize_resize Whether this drag's geometry just got
 *                        finalized as a genuine resize (not a move, and
 *                        not one this drag ultimately canceled)
 *
 * @note A no-op unless @p finalize_resize is set and
 *       @c s_drag.solid_drag is @c false
 * @note Requires a valid global @c s_drag context
 * @note Complexity: @e O(1)
 */
void drag_resist_axis_finalize(bool finalize_resize);


#endif  /* ! INPUT_MOUSE_DRAG_RESIST_H */
