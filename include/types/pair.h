/**
 * @file types/pair.h
 *
 * @brief Common shared structures for pair types (2D)
 *
 * @note Some of them are repeated but with different name, for clarity
 *       of the code when trying not to use 'typedef'.  I could use
 *       a common structure for those, but readability is important.
 *       Maybe in the future there will be some refactoring...
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


/*
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
 * @brief DPI structure
 *
 * @note DPI is always positive
 */
struct dpi_s {
    uint32_t x;
    uint32_t y;
};


/**
 * @brief Resolution structure
 *
 * @note Resolution is always positive
 */
struct resolution_s {
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


#endif  /* ! TYPE_PAIR_H */
