/**
 * @file defs/input.h
 *
 * @brief Capacity limits and thresholds for keyboard/mouse bindings and
 *        pointer interaction
 *
 * @ingroup defs
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


/**
 * @brief Maximum number of supported key bindings
 */
#define WM_MAX_KEYBINDINGS (160)

/**
 * @brief Maximum number of supported mouse bindings
 */
#define WM_MAX_MOUSEBINDINGS (8)

/**
 * @brief Maximum interval in milliseconds between two presses on the
 *        same titlebar that is recognized as a double-click
 */
#define WM_DOUBLE_CLICK_MS (400u)

/**
 * @brief Minimum, in pixels, that a resize-grab margin is guaranteed to
 *        be along any one edge, regardless of how thin that edge's own
 *        visible border is
 *
 * A border already at least this wide needs no help: the border itself
 * is the margin.  A thinner border gets padded out to exactly this many
 * pixels of tolerance instead (see @a im_bounds_resize in
 * @c input/mouse/bounds.h), so a 0px, 1px, or 2px border all feel the
 * same to grab rather than each requiring hitting a progressively
 * smaller number of exact pixels; the two cases meet exactly at this
 * threshold, where the border is already wide enough on its own.
 *
 * @note Currently 0, which makes the margin always exactly equal to the
 *       real border width (no padding ever added, so the tolerance this
 *       constant otherwise describes is presently a no-op)
 */
#define WM_RESIZE_GRAB_THRESHOLD (0)

/**
 * @brief Threshold below which an icon drag is treated as a click
 */
#define WM_ICON_DRAG_THRESHOLD (16)     /* 4 (px) x 4 (px) = 16 (px^2) */

/**
 * @brief Thickness, in pixels, of each of the 4 strip windows an
 *        outline-mode drag draws as its own stand-in rectangle (see
 *        @c windows.solid-drag, config.md)
 */
#define WM_DRAG_OUTLINE_BORDER_WIDTH (4u)

/**
 * @brief Coordinate, well outside any real monitor yet still within
 *        the signed 16-bit range X11 window positions themselves are
 *        limited to, an outline-mode drag moves the real window to
 *        for the duration of the drag
 *
 * A window moved here stays fully mapped throughout (unlike unmapping
 * it, which the X server itself would answer by reverting input focus
 * away from it, per the protocol's own rules for a window no longer
 * viewable, breaking real input focus, sloppy focus tracking, and
 * active-window rendering all at once), so none of that ever happens;
 * it is simply nowhere visible for anyone to see until the drag itself
 * moves it back.
 */
#define WM_DRAG_OFFSCREEN_POS (-30000)


#endif  /* ! DEFS_INPUT_H */
