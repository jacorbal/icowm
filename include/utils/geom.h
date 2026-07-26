/**
 * @file utils/geom.h
 *
 * @brief Pure geometry utility declarations
 *
 * Functions:
 *  - @c 'uint16_t geom_clamp_dim(int32_t value)'
 *  - @c 'bool geom_rect_overlap(int32_t ax, int32_t ay,
 *                                uint32_t aw, uint32_t ah,
 *                                int32_t bx, int32_t by,
 *                                uint32_t bw, uint32_t bh)'
 *
 * @ingroup geom Geometry utils
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


/* Public interface */
/**
 * @brief Clamp a signed dimension value to the supported client bounds
 *
 * Ensures that @p value stays within the minimum client window size
 * (@c WM_MIN_WINDOW_DIMENSION) and the maximum representable by
 * @c uint16_t.
 *
 * @param value Dimension value to clamp
 *
 * @return Clamped dimension as @c uint16_t
 *
 * @note Complexity: @e O(1)
 */
uint16_t geom_clamp_dim(int32_t value);

/**
 * @brief Test whether two axis-aligned rectangles overlap
 *
 * Rectangles that only touch at an edge or corner are not considered to
 * overlap.
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
 * @return @c true if the interiors overlap, @c false otherwise
 *
 * @note Complexity: @e O(1)
 */
bool geom_rect_overlap(int32_t ax, int32_t ay,
        uint32_t aw, uint32_t ah,
        int32_t bx, int32_t by,
        uint32_t bw, uint32_t bh);


#endif  /* ! UTILS_GEOM_H */
