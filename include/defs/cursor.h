/**
 * @file defs/cursor.h
 *
 * @brief Glyph indices into the X server's built-in "cursor" font
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_CURSOR_H
#define DEFS_CURSOR_H


/**
 * @brief Glyph index of @c XC_left_ptr (source) in the X cursor font
 *
 * The X cursor font stores source and mask glyphs in pairs.  The left
 * pointer cursor uses glyph 68 as the source shape and glyph 69 as its
 * transparency mask.
 */
#define WM_CURSOR_LEFT_PTR_GLYPH (68u)

/** Glyph index of the mask for @c XC_left_ptr in the X cursor font */
#define WM_CURSOR_LEFT_PTR_MASK_GLYPH (69u)

/**
 * @brief Glyph indices of the eight border-resize cursors in the X
 *        cursor font
 *
 * Each source glyph's mask is always the very next glyph index (the
 * font stores every shape as a source/mask pair), same convention as
 * @c WM_CURSOR_LEFT_PTR_GLYPH / @c WM_CURSOR_LEFT_PTR_MASK_GLYPH above,
 * so only the source glyph needs naming here; the mask is always
 * "this value plus one".  Used to show the matching resize direction
 * when the pointer hovers over a window's border or corner, before any
 * button is pressed.
 */
#define WM_CURSOR_TOP_SIDE_GLYPH            (138u)
#define WM_CURSOR_BOTTOM_SIDE_GLYPH         (16u)
#define WM_CURSOR_LEFT_SIDE_GLYPH           (70u)
#define WM_CURSOR_RIGHT_SIDE_GLYPH          (96u)
#define WM_CURSOR_TOP_LEFT_CORNER_GLYPH     (134u)
#define WM_CURSOR_TOP_RIGHT_CORNER_GLYPH    (136u)
#define WM_CURSOR_BOTTOM_LEFT_CORNER_GLYPH  (12u)
#define WM_CURSOR_BOTTOM_RIGHT_CORNER_GLYPH (14u)

/**
 * @brief Glyph index of @c XC_watch (source) in the X cursor font
 *
 * The "busy" cursor shown while a startup-notification sequence is
 * pending; its mask is, as with every other glyph here, the very next
 * index.
 */
#define WM_CURSOR_WATCH_GLYPH (150u)

/** Glyph index of the mask for @c XC_watch in the X cursor font */
#define WM_CURSOR_WATCH_MASK_GLYPH (151u)


#endif  /* ! DEFS_CURSOR_H */
