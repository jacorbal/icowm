/**
 * @file policy/placement/rect.h
 *
 * @brief Free-rectangle arithmetic the placement search walks with
 *
 * Pure geometry, with no notion of a client or a stage beyond
 * asking what is already on a desktop.  A rectangle is grown to the
 * largest it can be without touching what is there, and a position is
 * scored by how little it overlaps.  Kept apart from the rest of
 * placement because it is the one part of this policy that can be
 * reasoned about, and exercised, on its own.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_RECT_H
#define POLICY_PLACEMENT_RECT_H

/* System includes */
#include <stdint.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Local includes */
#include <defs/placement.h>


/**
 * @brief Grow a rectangle from a corner until it meets something
 *
 * @param desktop     Desktop whose clients are obstacles
 * @param skip_client Client to ignore, normally the one being placed
 * @param x0          Left edge to grow from
 * @param y0          Top edge to grow from
 * @param bound_x     Right edge the growth may not pass
 * @param bound_y     Bottom edge the same
 * @param tray_rect   Tray rectangle to treat as an obstacle, or
 *                     @c NULL when the tray does not reserve space
 * @param out_w Receives the width reached
 * @param out_h Receives the height reached
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
void placement_free_rect_grow(const desktop_td *desktop,
        const client_td *skip_client,
        int32_t x0, int32_t y0, int32_t bound_x, int32_t bound_y,
        const struct geometry_s *tray_rect,
        uint32_t *out_w, uint32_t *out_h);


/**
 * @brief Score one candidate position, lower being better
 *
 * @param desktop     Desktop whose clients are overlapped
 * @param skip_client Client to ignore, normally the one being placed
 * @param x           Left edge of the candidate
 * @param y           Top edge of the candidate
 * @param fw          Frame width of the client being placed
 * @param fh          Frame height the same
 * @param tray_rect   Tray rectangle to treat as occupied, or @c NULL
 * @param center_x    Preferred center, used to break ties
 * @param center_y    The same, on Y
 *
 * @return Overlap cost of the candidate; @c 0 means nothing is
 *         covered at all
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
uint64_t placement_score_window_pos(const desktop_td *desktop,
        const client_td *skip_client,
        int32_t x, int32_t y, uint32_t fw, uint32_t fh,
        const struct geometry_s *tray_rect,
        int32_t center_x, int32_t center_y);


#endif  /* ! POLICY_PLACEMENT_RECT_H */
