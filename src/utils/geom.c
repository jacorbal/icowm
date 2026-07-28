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
