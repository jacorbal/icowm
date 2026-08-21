/**
 * @file types/pair.h
 *
 * @brief Common shared structures for pair types (2D)
 *
 * @note Some of them are repeated but with different name, for clarity
 *       of the code when trying not to use @c typedef.
 *
 * @defgroup types Generic reusable types
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef TYPE_PAIR_H
#define TYPE_PAIR_H


#include <stdint.h>


/**
 * @brief Generic size structure
 *
 * @note Size is always positive
 */
struct size_s {
    uint32_t x;
    uint32_t y;
};


/**
 * @brief Generic position structure
 */
struct position_s {
    int32_t x;
    int32_t y;
};


/**
 * @brief Sides structure
 */
struct sides_s {
    int32_t left;
    int32_t right;
    int32_t top;
    int32_t bottom;
};


/**
 * @brief EWMH-style partial strut: reserved space on each of the four
 *        screen edges, plus the along-edge range each reservation spans
 *
 * @note A traditional, non-partial strut (@c _NET_WM_STRUT) is
 *       represented the same way, with @c start and @c end both left
 *       at zero on every side, whatever reads this treats 0..0 as
 *       unbounded for exactly that reason (e.g., @c s_ranges_overlap
 *       in @c desktop.c).
 *
 * @see @c _NET_WM_STRUT_PARTIAL
 */
struct strut_partial_s {
    struct sides_s sides;       /* [left, right, top, bottom] */
    struct sides_s start;       /* [left_start_y, right_start_y,
                                    top_start_x, bottom_start_x] */
    struct sides_s end;         /* [left_end_y, right_end_y,
                                    top_end_x, bottom_end_x] */
};


/**
 * @brief DPI structure
 *
 * @note DPI is always positive
 */
struct dpi_s {
    uint32_t x;
    uint32_t y;
};


/**
 * @brief Dimensions structure
 *
 * @note Dimensions are always non-negative
 */
struct dimensions_s {
    uint32_t w;
    uint32_t h;
};


/**
 * @brief Geometry structure
 *
 * @see @c position_s, @c dimensions_s
 */
struct geometry_s {
    struct position_s pos;
    struct dimensions_s dim;
};


/**
 * @brief A ratio expressed as a numerator and denominator
 *
 * @note May be negative per the ICCCM @c WM_NORMAL_HINTS wire format
 *       (@c xcb_size_hints_t's own aspect fields are signed), even
 *       though no well-behaved client ever sends one; kept signed so
 *       a defensive @c > @c 0 check against a malformed value still
 *       rejects it correctly instead of it wrapping to a huge unsigned
 *       number and silently passing
 */
struct ratio_s {
    int32_t num;
    int32_t den;
};


/**
 * @brief The minimum and maximum allowed window aspect ratios
 */
struct aspect_range_s {
    struct ratio_s min;
    struct ratio_s max;
};


#endif  /* ! TYPE_PAIR_H */
