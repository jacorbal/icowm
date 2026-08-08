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
 * @brief Size in pixels of the corner zone used for edge/corner resize
 *
 * Clicks on the frame border within this many pixels of a frame corner
 * are treated as corner-resize initiation events rather than plain
 * border clicks.
 */
#define WM_RESIZE_CORNER_SIZE (12)

/** Threshold below which an icon drag is treated as a click */
#define WM_ICON_DRAG_THRESHOLD (16)     /* 4 (px) x 4 (px) = 16 (px^2) */


#endif  /* ! DEFS_INPUT_H */
