/**
 * @file utils/path.h
 *
 * @brief Declarations of path handling functions
 *
 * Functions:
 *   - @c 'void path_simplify(char *restricted path)'
 *
 * @ingroup path Path management utils
 */

#ifndef UTILS_PATH
#define UTILS_PATH


/**
 * @brief Normalize a given file path by removing unnecessary components
 *
 * Processes the input path and simplifies it by:
 *   - Removing consecutive slashes
 *   - Ignoring the current directory indicators ('./')
 *   - Resolving the parent directory indicators ('../')
 *
 * @param path Pointer to the input path string to be normalized
 * 
 * @note The function modifies the path in place
 * @note Complexity: @e O(n), where @e n is the length of the input path
 */
void path_simplify(char *restrict path);


#endif  /* UTILS_PATH */
