/**
 * @file defs/icon.h
 *
 * @brief Dimensions for a client's iconified representation on the
 *        desktop
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

#ifndef DEFS_ICON_H
#define DEFS_ICON_H


/** Width/height of icon square in pixels */
#define WM_ICON_SQUARE_SIZE (48u)

/**
 * @brief Percentage of @c WM_ICON_SQUARE_SIZE a client's own
 *        @c _NET_WM_ICON image is scaled to fill
 *
 * Applies uniformly regardless of whichever size the client actually
 * published (see @c wmicon_draw in render/wmicon.h), so every icon
 * ends up the same visual size on screen instead of each one being
 * however large or small its own source image happened to be.  Kept
 * below @c 100 so the image sits with a small margin inside the
 * square rather than touching its edges.  An integer percentage
 * rather than a @c 0.0-1.0 scale factor: every consumer only ever
 * needs @c (value @c * @c PERCENT) @c / @c 100, plain integer
 * arithmetic with no float/double promotion or truncation involved.
 */
#define WM_ICON_PIXMAP_SCALE_PERCENT (75u)

/** Caption area below icon in pixels */
#define WM_ICON_CAPTION_HEIGHT (14u)

/** Extra border pixels added to the selected icon in the cycle menu */
#define WM_ICON_CYCLE_SEL_BORDER_EXTRA (1u)

/**
 * @brief Single-letter state-hint characters drawn in an iconified
 *        client's own top-right corner (see @c ri_draw_icon_hints in
 *        render/icon.c), one per @c pre_iconify_state value it
 *        restores to on de-iconify (@c CLIENT_STATE_NORMAL draws
 *        none)
 */
#define WM_ICON_HINT_FULLSCREEN      ('f')
#define WM_ICON_HINT_MAXIMIZED       ('m')
#define WM_ICON_HINT_MAXIMIZED_HORZ  ('h')
#define WM_ICON_HINT_MAXIMIZED_VERT  ('v')


#endif  /* ! DEFS_ICON_H */
