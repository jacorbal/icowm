/**
 * @file defs/icon.h
 *
 * @brief Dimensions for a client's iconified representation on the
 *        desktop
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
 * @brief Fraction of @c WM_ICON_SQUARE_SIZE a client's own
 *        @c _NET_WM_ICON image is scaled to fill
 *
 * Applies uniformly regardless of whichever size the client actually
 * published (see @c wmicon_draw in render/wmicon.h), so every icon
 * ends up the same visual size on screen instead of each one being
 * however large or small its own source image happened to be.  Kept
 * below @c 1.0 so the image sits with a small margin inside the
 * square rather than touching its edges.
 */
#define WM_ICON_PIXMAP_SCALE (0.75)

/** Caption area below icon in pixels */
#define WM_ICON_CAPTION_HEIGHT (14u)

/** Extra border pixels added to the selected icon in the cycle menu */
#define WM_ICON_CYCLE_SEL_BORDER_EXTRA (1u)


#endif  /* ! DEFS_ICON_H */
