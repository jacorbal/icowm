/**
 * @file defs/input.h
 *
 * @brief Capacity limits and thresholds for keyboard/mouse bindings
 *        and pointer interaction
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_INPUT_H
#define DEFS_INPUT_H


/** Maximum number of supported key bindings */
#define WM_MAX_KEYBINDINGS (160)

/** Maximum number of supported mouse bindings */
#define WM_MAX_MOUSEBINDINGS (8)

/** Maximum interval in milliseconds between two presses on the same
 *  titlebar that is recognized as a double-click */
#define WM_DOUBLE_CLICK_MS (400u)

/**
 * @brief Minimum, in pixels, that a resize-grab margin is guaranteed
 *        to be along any one edge, regardless of how thin that edge's
 *        own visible border is
 *
 * A border already at least this wide needs no help: the border
 * itself is the margin.  A thinner border gets padded out to exactly
 * this many pixels of tolerance instead (see @c im_resize_bounds in
 * input/mouse/bounds.h), so a 0px, 1px, or 2px border all feel the
 * same to grab rather than each requiring hitting a progressively
 * smaller number of exact pixels; the two cases meet exactly at this
 * threshold, where the border is already wide enough on its own.
 *
 * @note Currently 0, which makes the margin always exactly equal to
 *       the real border width (no padding ever added, so the
 *       tolerance this constant otherwise describes is presently a
 *       no-op): testing found a value above the true border width
 *       off by a pixel or so at some positions, while 0 consistently
 *       was not.  The underlying cause has not been tracked down yet;
 *       raising this back above 0 once it is would restore the
 *       intended tolerance.
 */
#define WM_RESIZE_GRAB_THRESHOLD (0)

/** Threshold below which an icon drag is treated as a click */
#define WM_ICON_DRAG_THRESHOLD (16)     /* 4 (px) x 4 (px) = 16 (px^2) */


#endif  /* ! DEFS_INPUT_H */
