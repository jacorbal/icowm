/**
 * @file policy/internal.h
 *
 * @brief Private constants and helpers shared across policy modules
 *
 * Declares cost-weight constants used by both @c policy/placement.c
 * (for window scoring) and @c policy/tiling.c (for icon placement
 * scoring).  Must not be included outside of @c src/policy/.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_INTERNAL_H
#define POLICY_INTERNAL_H


/* Default initial values */
#include <defs/icon.h>     /* WM_ICON_SQUARE_SIZE */


/**
 * @brief Cost weights for the smart window placement scorer
 *
 * These constants define the relative penalty of overlapping a visible
 * window vs. overlapping an icon vs. being far from the workarea
 * center.  Overlap penalties are multiplied by the intersection area
 * (pixels), so even a 1-pixel overlap with a visible window is worth
 * thousands of distance units, ensuring non-overlapping positions are
 * always strongly preferred.
 */
#define SMART_WIN_COST_PER_WIN_PIXEL (8192u)
#define SMART_WIN_COST_PER_ICON_PIXEL (1024u)

/**
 * @brief Fallback icon dimension used by the window scorer when the
 *        exact icon size is not tracked in the client structure
 */
#define SMART_WIN_ICON_SIZE (WM_ICON_SQUARE_SIZE)

/**
 * @brief Cost weights for the smart icon placement scorer
 *
 * Overlap with any visible (non-iconified) window is penalized heavily.
 * The overflow-row penalty keeps icons compact near the preferred edge,
 * i.e., each row away from the edge adds a small, predictable cost.
 */
#define SMART_ICON_COST_PER_WIN_PIXEL (256u)
#define SMART_ICON_COST_PER_OVERFLOW_ROW (1u)


#endif  /* ! POLICY_INTERNAL_H */
