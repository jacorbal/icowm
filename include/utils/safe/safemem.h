/**
 * @file utils/safe/safemem.h
 *
 * @brief Provide safe memory handling functions
 *
 * Functions:
 *  - @c 'void safe_free(void **ptr)'
 *  - @c 'int safe_free_var(void **first, ...)'
 *
 * @ingroup mem Safe memory management utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SAFEMEM_H
#define SAFEMEM_H


/** Terminator for @c safe_free_var argument lists */
#define SAFE_FREE_VAR_END ((void **) NULL)


/**
 * @brief Free a dynamically allocated memory block if the pointer is
 *        non-null
 *
 * This function checks if the provided pointer is non-null and, if so,
 * it frees the memory it points to. After freeing, the pointer is set
 * to @c NULL to prevent accidental access to freed memory.
 *
 * @param ptr A pointer to the pointer to be freed
 *
 * @note Complexity: @e O(1)
 */
void safe_free(void **ptr);

/**
 * @brief Free multiple dynamically allocated pointers
 *
 * Iterates through a null-terminated list of @c void** arguments,
 * freeing each pointed-to block that is non-null and setting the
 * pointer to @c NULL afterwards.  Pointers whose target is already null
 * are silently skipped, so the function is safe to call on partially
 * initialized pointer sets.
 *
 * @param first The first @c void** in the list
 * @param ...   Additional @c void** pointers to free, terminated with
 *              a @c NULL sentinel
 *
 * @return  0 on success
 * @return -1 if @p first is @c NULL
 *
 * @note The list terminator must be @c SAFE_FREE_VAR_END:
 *       @c safe_free_var((void**)&p1, (void**)&p2, SAFE_FREE_VAR_END)
 * @note Complexity: @e O(n), where @e n is the number of pointers
 *       provided up to the @c NULL terminator
 */
int safe_free_var(void **first, ...);


#endif  /* ! SAFEMEM_H */
