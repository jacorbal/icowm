/**
 * @file utils/config/path.h
 *
 * @brief Declarations of path handling functions
 *
 * @ingroup utils_config
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_CONFIG_PATH
#define UTILS_CONFIG_PATH


/**
 * @brief Normalize a given file path by removing unnecessary components
 *
 * Processes the input path and simplifies it by:
 *   - removing consecutive slashes;
 *   - ignoring the current directory indicators ('./');
 *   - resolving the parent directory indicators ('../').
 *
 * @param path Pointer to the input path string to be normalized
 *
 * @note The function modifies the path in place
 * @note Complexity: @e O(n), where @e n is the length of the input path
 */
void path_simplify(char *restrict path);


#endif  /* UTILS_CONFIG_PATH */
