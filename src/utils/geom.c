/**
 * @file utils/geom.c
 *
 * @brief Pure geometry utility implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <limits.h>     /* UINT16_MAX */
#include <stdbool.h>
#include <stdint.h>

/* Default initial values */
#include <defs/wm.h>    /* WM_MIN_WINDOW_DIMENSION */

/* Local includes */
#include <utils/geom.h>


/* Clamp a signed dimension value to the supported client bounds */
uint16_t geom_clamp_dim(int32_t value)
{
    if (value < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        return WM_MIN_WINDOW_DIMENSION;
    }

    if (value > (int32_t) UINT16_MAX) {
        return UINT16_MAX;
    }

    return (uint16_t) value;
}


/* Test whether two axis-aligned rectangles overlap */
bool geom_rect_overlap(int32_t ax, int32_t ay,
        uint32_t aw, uint32_t ah,
        int32_t bx, int32_t by,
        uint32_t bw, uint32_t bh)
{
    return ax < bx + (int32_t) bw &&
        bx < ax + (int32_t) aw &&
        ay < by + (int32_t) bh &&
        by < ay + (int32_t) ah;
}


/* Compute the intersection area of two axis-aligned rectangles */
uint32_t geom_intersection_area(int32_t ax, int32_t ay,
        uint32_t aw, uint32_t ah,
        int32_t bx, int32_t by,
        uint32_t bw, uint32_t bh)
{
    int32_t ix1;
    int32_t iy1;
    int32_t ix2;
    int32_t iy2;
    int32_t ax_end;
    int32_t ay_end;
    int32_t bx_end;
    int32_t by_end;

    ax_end = ax + (int32_t) aw;
    ay_end = ay + (int32_t) ah;
    bx_end = bx + (int32_t) bw;
    by_end = by + (int32_t) bh;

    ix1 = (ax > bx) ? ax : bx;
    iy1 = (ay > by) ? ay : by;
    ix2 = (ax_end < bx_end) ? ax_end : bx_end;
    iy2 = (ay_end < by_end) ? ay_end : by_end;

    if (ix2 <= ix1 || iy2 <= iy1) {
        return 0u;
    }

    return (uint32_t) (ix2 - ix1) * (uint32_t) (iy2 - iy1);
}
