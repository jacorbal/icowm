/**
 * @file pair.h
 *
 * @brief Common shared structures for pair types (2D)
 *
 * @note Some of them are repeated but with different name, for clarity
 *       of the code, although I will implement this in a better way in
 *       the future
 */

#ifndef PAIR_H
#define PAIR_H


/**
 * @brief Generic size
 *
 * @note Size is always positive or zero
 */
struct size_s {
    unsigned int x;
    unsigned int y;
};


/**
 * @brief DPI structure
 */
struct dpi_s {
    unsigned int x;
    unsigned int y;
};


/**
 * @brief Resolution structure
 *
 */
struct resolution_s {
    unsigned int x;
    unsigned int y;
};


/**
 * @brief Dimensions structure
 */
struct dimensions_s {
    unsigned int w;
    unsigned int h;
};


/**
 * @brief Geometry structure
 *
 * @note This values are always non-negative
 */
struct geometry_s {
    int x;
    int y;
};


#endif  /* ! PAIR_H */
