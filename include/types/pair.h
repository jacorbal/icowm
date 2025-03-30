/**
 * @file pair.h
 *
 * @brief Common shared structures for pair types (2D)
 *
 * @note Some of them are repeated but with different name, for clarity
 *       of the code when trying not to use 'typedef'.  I could use
 *       a common structure for those, but readability is important.
 *       Maybe in the future there will be some refactoring...
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


/*
 * @brief Position structure
 *
 * @note This values are always non-negative
 */
struct position_s {
    int x;
    int y;
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
 */
struct geometry_s {
    struct position_s pos;
    struct dimensions_s dim;
};


#endif  /* ! PAIR_H */
