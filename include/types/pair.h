/**
 * @file pair.h
 *
 * @brief Common shared structures for pair types (2D)
 */

#ifndef PAIR_H
#define PAIR_H


/**
 * @brief Geometry
 *
 * @note This values are always non-negative
 */
struct geometry_s {
    int x;
    int y;
};


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
 * @brief Dimensions structure
 *
 * @note Dimensions are always non-negative, referenced in 2D as width and
 *       height
 */
struct dimensions_s {
    unsigned int w;
    unsigned int h;
};


#endif  /* ! PAIR_H */
