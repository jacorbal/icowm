/**
 * @file input/mouse/drag/snap.h
 *
 * @brief Edge and peer-window snap math for move/resize drags
 *
 * Declares the functions @c drag/snap.c exposes for @c drag.c to call
 * directly; nothing outside the drag subsystem calls either of these.
 *
 * @note This header is private to @c input/mouse/drag/ (and
 *       @c input/mouse/drag.c, which orchestrates every drag/ file) and
 *       must not be included outside of them
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_DRAG_SNAP_H
#define INPUT_MOUSE_DRAG_SNAP_H


/* System includes */
#include <stdint.h>


/**
 * @brief Apply snapping behavior during client movement
 *
 * Adjusts the proposed position of a moving client so it "snaps" to
 * nearby window edges or screen boundaries when within a configurable
 * threshold.  It compares the moving window against other visible,
 * non-iconified clients on the same desktop and computes the smallest
 * adjustment needed to align edges.
 *
 * Snapping is applied independently along both axes and also considers
 * screen edges if available.
 *
 * @param x      Pointer to the proposed X coordinate (updated in place)
 * @param y      Pointer to the proposed Y coordinate (updated in place)
 * @param width  Width of the moving client
 * @param height Height of the moving client
 *
 * @note Requires a valid global @c s_drag context
 * @note Complexity: @e O(n), where @e n is the number of clients in the
 *       stacking list
 */
void drag_snap_move(int32_t *restrict x, int32_t *restrict y,
        uint32_t width, uint32_t height);

/**
 * @brief Apply snapping behavior during client resize
 *
 * Adjusts the proposed geometry of a resizing client so whichever edge
 * the pointer is dragging "snaps" to nearby window edges or screen
 * boundaries when within a configurable threshold, honoring which edge
 * or corner the resize is anchored on.
 *
 * @param x      Pointer to the proposed X coordinate (updated in place
 *               if the anchored edge is the left one)
 * @param y      Pointer to the proposed Y coordinate (updated in place
 *               if the anchored edge is the top one)
 * @param width  Pointer to the proposed width (updated in place)
 * @param height Pointer to the proposed height (updated in place)
 *
 * @note Requires a valid global @c s_drag context
 * @note Complexity: @e O(n), where @e n is the number of clients in the
 *       stacking list
 */
void drag_snap_resize(int32_t *restrict x, int32_t *restrict y,
        uint32_t *restrict width, uint32_t *restrict height);


#endif  /* ! INPUT_MOUSE_DRAG_SNAP_H */
