/**
 * @file utils/geom.h
 *
 * @brief Pure geometry utility declarations and implementations
 *
 * Thin @c static @c inline geometry helpers with no hidden or shared
 * state, so every one of them is safe to inline directly at each call
 * site without duplicating any state across translation units.
 *
 * @defgroup utils Generic utilities
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_GEOM_H
#define UTILS_GEOM_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Type includes */
#include <types/pair.h> /* geometry_s */

/* Default initial values */
#include <defs/client.h>


/* Public interface */
/**
 * @brief Clamp a signed dimension value to the supported client bounds
 *
 * Ensures that @p value stays within the minimum client window size
 * @c (WM_MIN_WINDOW_DIMENSION) and the maximum representable by
 * @c uint16_t.
 *
 * @param value Dimension value to clamp
 *
 * @return Clamped dimension as @c uint16_t
 *
 * @note Complexity: @e O(1)
 */
static inline uint16_t geom_dim_clamp(int32_t value)
{
    if (value < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        return WM_MIN_WINDOW_DIMENSION;
    }

    if (value > (int32_t) UINT16_MAX) {
        return UINT16_MAX;
    }

    return (uint16_t) value;
}

/**
 * @brief Test whether two axis-aligned rectangles overlap
 *
 * Rectangles that only touch at an edge or corner are not considered to
 * overlap.  Cheaper than @a geom_intersection_area or
 * @a geom_intersect_rect for a caller that only needs the yes/no
 * answer.  No intersection bounds are computed at all, only relational
 * comparisons.
 *
 * @param ax Left coordinate of the first rectangle
 * @param ay Top coordinate of the first rectangle
 * @param aw Width of the first rectangle
 * @param ah Height of the first rectangle
 * @param bx Left coordinate of the second rectangle
 * @param by Top coordinate of the second rectangle
 * @param bw Width of the second rectangle
 * @param bh Height of the second rectangle
 *
 * @return @c true if the interiors overlap
 *
 * @note Complexity: @e O(1)
 */
static inline bool geom_overlap_rect(
        int32_t ax, int32_t ay, uint32_t aw, uint32_t ah,
        int32_t bx, int32_t by, uint32_t bw, uint32_t bh)
{
    return ax < bx + (int32_t) bw &&
        bx < ax + (int32_t) aw &&
        ay < by + (int32_t) bh &&
        by < ay + (int32_t) ah;
}

/**
 * @brief Compute the intersection rectangle of two axis-aligned
 *        rectangles
 *
 * @param ax Left coordinate of the first rectangle
 * @param ay Top coordinate of the first rectangle
 * @param aw Width of the first rectangle
 * @param ah Height of the first rectangle
 * @param bx Left coordinate of the second rectangle
 * @param by Top coordinate of the second rectangle
 * @param bw Width of the second rectangle
 * @param bh Height of the second rectangle
 *
 * @return Overlapping rectangle, or a rectangle with zero width and
 *         height (position otherwise unspecified) if the two do not
 *         overlap
 *
 * @note Complexity: @e O(1)
 */
static inline struct geometry_s geom_intersect_rect(
        int32_t ax, int32_t ay, uint32_t aw, uint32_t ah,
        int32_t bx, int32_t by, uint32_t bw, uint32_t bh)
{
    struct geometry_s result =
        {.pos = {.x = 0, .y = 0}, .dim = {.w = 0u, .h = 0u}};
    int64_t ix1;
    int64_t iy1;
    int64_t ix2;
    int64_t iy2;
    int64_t ax_end;
    int64_t ay_end;
    int64_t bx_end;
    int64_t by_end;

    /* Widened before adding: a rectangle whose right edge lands past
     * 'INT32_MAX' would otherwise overflow, and comparing the sums in
     * the same width they were computed in is what lets the optimizer
     * assume that never happens */
    ax_end = (int64_t) ax + (int64_t) aw;
    ay_end = (int64_t) ay + (int64_t) ah;
    bx_end = (int64_t) bx + (int64_t) bw;
    by_end = (int64_t) by + (int64_t) bh;

    ix1 = (ax > bx) ? (int64_t) ax : (int64_t) bx;
    iy1 = (ay > by) ? (int64_t) ay : (int64_t) by;
    ix2 = (ax_end < bx_end) ? ax_end : bx_end;
    iy2 = (ay_end < by_end) ? ay_end : by_end;

    if (ix2 <= ix1 || iy2 <= iy1) {
        return result;
    }

    result.pos.x = (int32_t) ix1;
    result.pos.y = (int32_t) iy1;
    result.dim.w = (uint32_t) (ix2 - ix1);
    result.dim.h = (uint32_t) (iy2 - iy1);

    return result;
}

/**
 * @brief Compute the intersection area of two axis-aligned rectangles
 *
 * Returns the number of pixels in the intersection of two rectangles.
 * Rectangles that only touch at an edge or corner have an intersection
 * area of zero.  A thin wrapper over @a geom_intersect_rect, which
 * already computes the same intersection bounds this needs; both stay
 * in agreement by construction, since neither repeats the other's
 * arithmetic independently.
 *
 * @param ax Left coordinate of the first rectangle
 * @param ay Top coordinate of the first rectangle
 * @param aw Width of the first rectangle
 * @param ah Height of the first rectangle
 * @param bx Left coordinate of the second rectangle
 * @param by Top coordinate of the second rectangle
 * @param bw Width of the second rectangle
 * @param bh Height of the second rectangle
 *
 * @return Area of the intersection in pixels, or @c 0 if the rectangles
 *         do not overlap
 *
 * @note Complexity: @e O(1)
 */
static inline uint32_t geom_intersection_area(
        int32_t ax, int32_t ay, uint32_t aw, uint32_t ah,
        int32_t bx, int32_t by, uint32_t bw, uint32_t bh)
{
    struct geometry_s r = geom_intersect_rect(ax, ay, aw, ah,
            bx, by, bw, bh);

    return r.dim.w * r.dim.h;
}


#endif  /* ! UTILS_GEOM_H */
