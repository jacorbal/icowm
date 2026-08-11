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
 * @brief Pixels kept between an automatically placed icon and the
 *        nearest screen/monitor edge, and between one such icon and
 *        the next
 *
 * See @c place_icon (policy/placement.h): added to @c WM_ICON_SQUARE_
 * SIZE (and an icon's own caption height, when captioned) to get the
 * grid's own step between one candidate slot and the next, and used
 * on its own as the offset from the screen edge the very first slot
 * starts at.  A single value serves both roles, rather than two
 * separate ones, since nothing in this project distinguishes "space
 * from the screen edge" from "space between icons" as a design
 * choice.
 */
#define WM_ICON_GRID_MARGIN (8u)

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

/**
 * @brief Vertical gap, in pixels, kept between an icon and the
 *        systray's own rectangle when the two would otherwise overlap
 *
 * See @c icon_avoid_systray_overlap (policy/placement.h): applied on
 * whichever side the icon actually gets pushed toward, so it never
 * ends up sitting flush against the tray's own edge.  Kept equal to
 * @c WM_ICON_GRID_MARGIN on purpose, the same spacing an icon already
 * keeps from a screen edge or another icon, rather than some other
 * value picked independently.
 */
#define WM_ICON_SYSTRAY_GAP (8u)

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
