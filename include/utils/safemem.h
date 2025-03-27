/**
 * @file safemem.h
 *
 * @brief Provide safe memory handling functions
 *
 * Functions:
 *  - @c 'void safe_free(void **ptr)'
 *  - @c 'int safe_free_multiple(void **first, ...)'
 *
 * @ingroup mem Safe memory management utils
 */


/**
 * @brief Free a dynamically allocated memory block if the pointer is
 *        not @c NULL
 *
 * This function checks if the provided pointer is not @c NULL and, if
 * so, it frees the memory it points to. After freeing, the pointer is
 * set to @c NULL to prevent accidental access to freed memory.
 *
 * @param ptr A pointer to the pointer to be freed
 *
 * @note Complexity: @e O(1)
 */
void safe_free(void **ptr);

/**
 * @brief Free multiple dynamically allocated pointers
 *
 * Frees the memory of the provided pointers as arguments.  The list of
 * pointers is terminated with a @c NULL pointer.
 *
 * @param first A pointer to the first pointer to free
 * @param ...   Additional pointers to free, terminated with @c NULL
 *
 * @return 0 if all pointers were freed successfully, the index
 *         (0-based) of the first pointer that could not be freed, or -1
 *         if the first pointer was @c NULL or if no valid pointers were
 *         provided up to the first @c NULL
 *
 * @note This function does not take a counter argument, and uses
 *       @c NULL to determine the end of the list
 * @note Each pointer is set to @c NULL after freeing to prevent
 *       accidental access to freed memory
 * @note Each pointer is evaluated once
 * @note Complexity: @e O(n), where @e n is the number of pointers
 *       provided up to the first @c NULL
 *
 * This example demonstrates how to call the @a safe_free_multiple
 * function with a mixture of valid and @c NULL pointers:
 *
 * @code
 *  char *ptr1 = malloc(64);
 *  char *ptr2 = NULL;
 *  char *ptr3 = malloc(64);
 *
 *  int result = safe_free_multiple((void **) &ptr1,
 *                                  (void **) &ptr2,
 *                                  (void **) &ptr3,
 *                                  NULL);
 *
 *  if (result == 0) {
 *      printf("All pointers were freed successfully\n");
 *  } else if (result == -1) {
 *      printf("First pointer could not be freed or was NULL\n");
 *  } else {
 *      printf("Could not free pointer at index %d\n", result);
 *  }
 * @endcode
 *
 * In this case, @p ptr1 will be freed and set to @c NULL, but the
 * function encounters @p ptr2, which is @c NULL.  Thus, it will return
 * the index 1, indicating that the second pointer could not be
 * processed.
 */
int safe_free_multiple(void **first, ...);
