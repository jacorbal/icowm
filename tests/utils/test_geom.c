/**
 * @file tests/utils/test_geom.c
 *
 * @brief Test battery for the pure geometry utilities
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <limits.h>

/* Local includes */
#include <harness/tap.h>
#include <utils/geom.h>


/* geom_dim_clamp: a value already inside the supported range passes
 * through unchanged */
static void s_test_clamp_dim_within_range(void)
{
    TAP_EQ_INT(geom_dim_clamp(100), 100,
            "a value comfortably inside the range is unchanged");
    TAP_EQ_INT(geom_dim_clamp(4), 4,
            "the minimum itself (4) passes through unchanged");
}


/* geom_dim_clamp: values below WM_MIN_WINDOW_DIMENSION (4) clamp up
 * to it, including negative values */
static void s_test_clamp_dim_below_minimum(void)
{
    TAP_EQ_INT(geom_dim_clamp(3), 4, "just below the minimum clamps up");
    TAP_EQ_INT(geom_dim_clamp(0), 4, "zero clamps up to the minimum");
    TAP_EQ_INT(geom_dim_clamp(-100), 4,
            "a negative value clamps up to the minimum");
}


/* geom_dim_clamp: values above UINT16_MAX clamp down to it */
static void s_test_clamp_dim_above_maximum(void)
{
    TAP_EQ_INT(geom_dim_clamp((int32_t) UINT16_MAX), UINT16_MAX,
            "UINT16_MAX itself passes through unchanged");
    TAP_EQ_INT(geom_dim_clamp((int32_t) UINT16_MAX + 1), UINT16_MAX,
            "just above UINT16_MAX clamps down to it");
    TAP_EQ_INT(geom_dim_clamp(1000000), UINT16_MAX,
            "a far larger value also clamps down to UINT16_MAX");
}


/* geom_overlap_rect: rectangles with genuine interior overlap */
static void s_test_overlap_true_cases(void)
{
    TAP_OK(geom_overlap_rect(0, 0, 10, 10, 5, 5, 10, 10),
            "partially overlapping rectangles overlap");
    TAP_OK(geom_overlap_rect(0, 0, 10, 10, 2, 2, 4, 4),
            "one rectangle fully inside another overlaps");
    TAP_OK(geom_overlap_rect(0, 0, 10, 10, 0, 0, 10, 10),
            "identical rectangles overlap");
}


/* geom_overlap_rect: rectangles that do not overlap at all, or that
 * only touch at an edge or a corner, do not count as overlapping */
static void s_test_overlap_false_cases(void)
{
    TAP_OK(!geom_overlap_rect(0, 0, 10, 10, 100, 100, 10, 10),
            "far-apart rectangles do not overlap");
    TAP_OK(!geom_overlap_rect(0, 0, 10, 10, 10, 0, 10, 10),
            "rectangles sharing only a vertical edge do not overlap");
    TAP_OK(!geom_overlap_rect(0, 0, 10, 10, 0, 10, 10, 10),
            "rectangles sharing only a horizontal edge do not overlap");
    TAP_OK(!geom_overlap_rect(0, 0, 10, 10, 10, 10, 10, 10),
            "rectangles touching only at a corner do not overlap");
}


/* geom_intersection_area: matches the exact pixel count for a known,
 * hand-computed partial overlap */
static void s_test_intersection_area_partial_overlap(void)
{
    /* A: [0,10)x[0,10), B: [5,15)x[5,15) -> overlap [5,10)x[5,10),
     * a 5x5 = 25 pixel area */
    TAP_EQ_INT(geom_intersection_area(0, 0, 10, 10, 5, 5, 10, 10), 25,
            "partial overlap area matches the hand-computed 5x5 region");
}


/* geom_intersection_area: no overlap (including edge/corner-only
 * touching) always yields zero area */
static void s_test_intersection_area_zero_cases(void)
{
    TAP_EQ_INT(geom_intersection_area(0, 0, 10, 10, 100, 100, 10, 10),
            0, "far-apart rectangles have zero intersection area");
    TAP_EQ_INT(geom_intersection_area(0, 0, 10, 10, 10, 0, 10, 10),
            0, "edge-touching rectangles have zero intersection area");
    TAP_EQ_INT(geom_intersection_area(0, 0, 10, 10, 10, 10, 10, 10),
            0, "corner-touching rectangles have zero intersection area");
}


/* geom_intersection_area: one rectangle fully containing another
 * yields exactly the smaller rectangle's own area */
static void s_test_intersection_area_full_containment(void)
{
    /* Inner is fully inside outer; area is the inner's own 4x4 = 16 */
    TAP_EQ_INT(geom_intersection_area(0, 0, 10, 10, 2, 2, 4, 4), 16,
            "full containment area equals the smaller rectangle's own" \
            " area");
    TAP_EQ_INT(geom_intersection_area(0, 0, 10, 10, 0, 0, 10, 10), 100,
            "identical rectangles intersect over their full area");
}


/* geom_intersect_rect: the resulting rectangle's position and size
 * match a known, hand-computed partial overlap exactly */
static void s_test_intersect_rect_partial_overlap(void)
{
    struct geometry_s result =
        geom_intersect_rect(0, 0, 10, 10, 5, 5, 10, 10);

    TAP_EQ_INT(result.pos.x, 5, "intersection rectangle's own x");
    TAP_EQ_INT(result.pos.y, 5, "intersection rectangle's own y");
    TAP_EQ_INT((long) result.dim.w, 5, "intersection rectangle's own" \
            " width");
    TAP_EQ_INT((long) result.dim.h, 5, "intersection rectangle's own" \
            " height");
}


/* geom_intersect_rect: no overlap yields zero width and height (the
 * position is explicitly left unspecified by this function's own
 * contract, so it is deliberately not asserted on here) */
static void s_test_intersect_rect_no_overlap(void)
{
    struct geometry_s result =
        geom_intersect_rect(0, 0, 10, 10, 100, 100, 10, 10);

    TAP_EQ_INT((long) result.dim.w, 0,
            "no overlap yields zero width");
    TAP_EQ_INT((long) result.dim.h, 0,
            "no overlap yields zero height");
}


/* geom_intersect_rect: full containment yields exactly the smaller,
 * inner rectangle back unchanged */
static void s_test_intersect_rect_full_containment(void)
{
    struct geometry_s result =
        geom_intersect_rect(0, 0, 10, 10, 2, 3, 4, 4);

    TAP_EQ_INT(result.pos.x, 2, "contained rectangle's own x preserved");
    TAP_EQ_INT(result.pos.y, 3, "contained rectangle's own y preserved");
    TAP_EQ_INT((long) result.dim.w, 4,
            "contained rectangle's own width preserved");
    TAP_EQ_INT((long) result.dim.h, 4,
            "contained rectangle's own height preserved");
}


int main(void)
{
    TAP_PLAN(31);

    s_test_clamp_dim_within_range();
    s_test_clamp_dim_below_minimum();
    s_test_clamp_dim_above_maximum();
    s_test_overlap_true_cases();
    s_test_overlap_false_cases();
    s_test_intersection_area_partial_overlap();
    s_test_intersection_area_zero_cases();
    s_test_intersection_area_full_containment();
    s_test_intersect_rect_partial_overlap();
    s_test_intersect_rect_no_overlap();
    s_test_intersect_rect_full_containment();

    return TAP_DONE();
}
