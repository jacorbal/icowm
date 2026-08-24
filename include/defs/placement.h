/**
 * @file defs/placement.h
 *
 * @brief Cost-weight constants for the smart window and icon placement
 *        scorers
 *
 * @c policy/placement/score.c (the shared overlap-scoring core both
 * draw from) and @c policy/placement/window.c and
 * @c policy/placement/icon.c themselves each pass their own weights to
 * @a place_overlap_score rather than reading them directly.
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

#ifndef DEFS_PLACEMENT_H
#define DEFS_PLACEMENT_H


/* Default initial values */
#include <defs/icon.h>      /* WM_ICON_SQUARE_SIZE */


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
#define PLACE_SMART_WIN_COST_PER_WIN_PIXEL (8192u)
#define PLACE_SMART_WIN_COST_PER_ICON_PIXEL (1024u)

/**
 * @brief Extra multiplier on top of
 *        @c PLACE_SMART_WIN_COST_PER_WIN_PIXEL for a candidate
 *        overlapping the systray, when @c systray.avoid-overlap applies
 *
 * The systray's footprint is typically a small fraction of an ordinary
 * window's (e.g., roughly 1/35th for an 80x24 xterm against a 200x24
 * systray), so the same per-pixel cost as an ordinary window overlap
 * makes only a tiny contribution to a candidate's total cost once the
 * desktop is full enough that every candidate overlaps something:
 * a position overlapping the systray entirely can still end up cheaper
 * overall than one overlapping more of another window only partially,
 * defeating the whole point of avoiding it.  This multiplier scales
 * that penalty back up so overlapping the systray stays comparably
 * costly to overlapping a real window, regardless of how small the
 * systray's footprint happens to be.
 *
 * @see @a place_window_smart, in @c policy/placement/window.c
 */
#define PLACE_SMART_WIN_SYSTRAY_COST_MULTIPLIER (16u)

/**
 * @brief Fallback icon dimension used by the window scorer when the
 *        exact icon size is not tracked in the client structure
 */
#define PLACE_SMART_WIN_ICON_SIZE (WM_ICON_SQUARE_SIZE)

/**
 * @brief Cost weights for the smart icon placement scorer
 *
 * Overlap with any visible (non-iconified) window is penalized heavily.
 * The overflow-row penalty keeps icons compact near the preferred edge,
 * i.e., each row away from the edge adds a small, predictable cost.
 */
#define PLACE_SMART_ICON_COST_PER_WIN_PIXEL (256u)
#define PLACE_SMART_ICON_COST_PER_OVERFLOW_ROW (1u)


#endif  /* ! DEFS_PLACEMENT_H */
