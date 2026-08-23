/**
 * @file policy/placement/score.h
 *
 * @brief Shared overlap-scoring core for SMART placement declarations
 *
 * The overlap-penalty loop @c place_window_apply and
 * @c place_icon_apply's own @c CONFIG_ICON_PLACEMENT_SMART search each
 * run against @p desktop's own stacking list is identical in shape
 * (skip self/hidden/locked, weigh intersection area against every other
 * visible client's window and, optionally, its icon).  Only the
 * tie-breaker each applies afterward genuinely differs (real distance
 * to the workarea center for a window, grid overflow-row compactness
 * for an icon), so only the shared loop lives here; each caller still
 * computes and adds its own tie-breaker on top of what this returns.
 *
 * @ingroup policy
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_SCORE_H
#define POLICY_PLACEMENT_SCORE_H


/* System includes */
#include <stdint.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <desktop.h>


/* Public interface */
/**
 * @brief Accumulate overlap-penalty cost for a candidate rectangle
 *        against every visible, unlocked client on a desktop
 *
 * Shared core of the @c CONFIG_PLACEMENT_POLICY_SMART window search
 * (@c place_window_apply) and the @c CONFIG_ICON_PLACEMENT_SMART icon
 * search (@c place_icon_apply): iterates @p desktop's own stacking list
 * once, weighing @p candidate's own overlap against every other
 * client's current window rectangle (by @p win_pixel_cost) and,
 * whenever that other client is currently a mapped, visible icon
 * itself, its own icon rectangle too (by @p icon_pixel_cost).
 *
 * A locked client (@a client_is_locked, @c client.h; the scratchpad is
 * the only one today) never counts as an obstacle here, exposed or not:
 * it already sits above every other client (@c CLIENT_LAYER_ABOVE) and
 * is positioned by its own dedicated policy, so avoiding its current
 * rectangle would only ever avoid a window it can never actually be
 * occluded by, while still fragmenting the layout around a window that
 * may not even be there the next time this same desktop is scored.
 *
 * @param desktop         Desktop whose clients are inspected
 * @param skip_client     Client to ignore (the one being placed)
 * @param candidate       Candidate rectangle to score
 * @param win_pixel_cost  Cost weight per pixel of overlap with another
 *                        client's own current window rectangle
 * @param icon_pixel_cost Cost weight per pixel of overlap with
 *                        another client's own currently visible icon
 *                        rectangle (fixed @c PLACE_SMART_WIN_ICON_SIZE
 *                        square); pass @c 0 to skip icon-overlap
 *                        scoring entirely, e.g., for an icon-placement
 *                        caller that already rejects icon-vs-icon
 *                        collision outright elsewhere, rather than
 *                        scoring it here
 *
 * @return Accumulated overlap-penalty cost; @c 0 means no overlap
 *         with anything this call weighs
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
uint64_t place_overlap_score(const desktop_td *desktop,
        const client_td *skip_client, struct geometry_s candidate,
        uint64_t win_pixel_cost, uint64_t icon_pixel_cost);


#endif  /* ! POLICY_PLACEMENT_SCORE_H */
