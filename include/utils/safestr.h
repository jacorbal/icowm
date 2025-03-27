/**
 * @file safestr.h
 *
 * @brief Provide safe string handling functions declaration
 *
 * Contains various safe string manipulation functions that ensure no
 * buffer overflows occur and that strings are properly null-terminated.
 *
 * Functions:
 *  - @c 'size_t safe_strnlen(const char *str, size_t maxlen)'
 *  - @c 'size_t safe_strlen(const char *str)'
 *  - @c 'char *safe_strncpy(char *dst, const char *src, size_t size)'
 *  - @c 'char *safe_strcpy(char *restrict dst, const char *restrict src)'
 *  - @c 'char *safe_strndup(const char *s, size_t n)'
 *  - @c 'char *safe_strdup(const char *s)'
 *  - @c 'int safe_strncmp(const char *s1, const char *s2)'
 *  - @p 'int safe_strcmp(const char *s1, const char *s2)'
 *
 * @ingroup str Safe string utils
 */

#ifndef SAFESTR_H
#define SAFESTR_H

#include <stddef.h>     /* size_t */


/* Public interface */
/**
 * @brief Calculate the length of a string, up to a maximum length
 *
 * Calculates the length of the string pointed to by @p str, up to
 * a maximum of @p maxlen bytes, not including the terminating null
 * byte.
 *
 * @param str    Pointer to the string to be measured
 * @param maxlen The maximum number of bytes to consider when measuring
 *               the string
 *
 * @return The length of the string, up to @p maxlen, excluding the
 *         terminating null byte, or 0 if @p str is @c NULL
 *
 * @note If @p str is @c NULL, the function returns 0
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       being measured, up to @p size - 1
 */
size_t safe_strnlen(const char *str, size_t maxlen);

/**
 * @brief Calculate the length of a string
 *
 * Calculates the length of the string pointed to by @p str, not
 * including the terminating null byte.
 *
 * @param str Pointer to the string to be measured
 *
 * @return The length of the string, excluding the terminating null
 *         byte, or 0 if @p str is @c NULL
 *
 * @note If @p str is NULL, the function returns 0
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       being measured
 */
size_t safe_strlen(const char *str);

/**
 * @brief Safely copies a string from @p src to @p dst
 *
 * Copies up to @p size - 1 characters from @p src to @p dst and
 * null-terminates the destination string.
 *
 * @param dst  Pointer to the destination buffer where the string is
 *             copied
 * @param src  Pointer to the source string to be copied
 * @param size The size of the destination buffer
 *
 * @return A pointer to @p dst
 *
 * @note If @p size is 0, the function will not perform any copying and
 *       will return @p dst
 * @note If the length of the source string exceeds @p size, the
 *       destination will be truncated
 * @note Destination string will always be null-terminated
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       being copied, up to @p size - 1
 */
char *safe_strncpy(char *restrict dst, const char *restrict src,
        size_t size);

/**
 * @brief Safely copies a string from @p src to @p dst without
 *        exceeding buffer size
 *
 * Copies up to @p size - 1 characters from @p src to @p dst and
 * null-terminates the destination string.
 *
 * @param dst  Pointer to the destination buffer where string is copied
 * @param src  Pointer to the source string to be copied
 *
 * @return A pointer to @p dst
 *
 * @note If @p size is 0, the function will not perform any copying and
 *       will return @p dst
 * @note Destination string will always be null-terminated
 * @note This could have been donde referencing @a safe_strncpty with
 *       @c "return safe_strncpy(dst, src, safe_strlen(src) + 1);", but
 *       it was done otherwise to reduce coupling
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       being copied
 */
char *safe_strcpy(char *restrict dst, const char *restrict src);

/**
 * @brief Creates a duplicate of a string with a specified maximum length
 *
 * Allocates memory for a copy of the string @p s, copies up to
 * @p n characters, and returns a pointer to the newly allocated string.
 *
 * @param s Pointer to the source string to duplicate
 * @param n The maximum number of characters to copy from @p s
 *
 * @return A pointer to the newly allocated string containing a copy of
 *         @p s, or @c NULL if @p s is @c NULL
 *
 * @note The caller is responsible for freeing the allocated memory
 * @note Complexity: @e O(n), where @e n is the minimum of the length of
 *       the string and @e n
 */
char *safe_strndup(const char *s, size_t n);

/**
 * @brief Duplicates a string
 *
 * Allocates memory for a copy of the string @p s, copies it, and
 * returns a pointer to the newly allocated string.
 *
 * @param s Pointer to the source string to duplicate
 *
 * @return A pointer to the newly allocated string containing a copy of
 *         @p s, or @c NULL if @p s is @c NULL
 *
 * @note The caller is responsible for freeing the allocated memory
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       being duplicated
 */
char *safe_strdup(const char *s);

/**
 * @brief Safely compares two strings up to a specific length
 *
 * Compares the first @e n characters of two strings lexicographically.
 *
 * @param s1 First string to compare
 * @param s2 Second string to compare
 * @param n  Maximum number of characters to compare
 *
 * @return Negative value if @p s1 < @p s2, zero if @p s1 == @p s2,
 *         positive @p value if s1 > @p s2, or the value that would be
 *         returned by @a strncmp when one of the strings is @c NULL
 *
 * @note If both strings are @c NULL, they are considered equal
 * @note If one of the strings is @c NULL, it handles it appropriately
 *       to avoid undefined behavior
 * @note Complexity: @e O(n), where @e n is the number of characters to
 *       compare
 */
int safe_strncmp(const char *s1, const char *s2, size_t n);

/**
 * @brief Safely compares two strings
 *
 * Compares two strings lexicographically.
 *
 * @param s1 First string to compare
 * @param s2 Second string to compare
 *
 * @return Negative value if @p s1 < @p s2, zero if @p s1 == @p s2,
 *         positive value if @p s1 > @p s2, or the value of str_cmp
 *         returns when one of the strings is @c NULL
 * @retval  0 Both strings are equal
 *
 * @note If both strings are @c NULL, they are considered equal
 * @note If either string is @c NULL, the function returns a value based
 *       on the @c NULL string
 * @note It prevents undefined behavior from dereferencing @c NULL
 *       pointers
 * @note Complexity: @e O(n), where @e n is the length of the longest
 *       string between @p s1 and @p s2
 */
int safe_strcmp(const char *s1, const char *s2);


#endif  /* ! SAFESTR_H */
