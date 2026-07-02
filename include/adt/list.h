/**
 * @file list.h
 *
 * @brief Linked list data structure declaration
 *
 * @ingroup ADT
 */

#ifndef LIST_H
#define LIST_H


/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stddef.h>     /* NULL, size_t */


/**
 * @brief Linked list items structure
 */
typedef struct list_item_td_s {
    void *data;                     /**< Pointer to the data of this item */
    struct list_item_td_s *next;    /**< Pointer to the next element */
} list_item_td;


/**
 * @brief Linked list structure
 */
typedef struct {
    /**
     * @brief Pointer to the function to test if two keys are equal
     *
     * @param key1 First key to compare
     * @param key2 Second key to compare
     *
     * @return Status of the operation
     * @retval  true Both keys match
     * @retval false Keys do not match
     *
     * @note This linked list does not use this function, but it's
     *       required for other modules that use a linked list as
     *       a base, such as a set or a graph
     */
    bool (*match)(const void *key1, const void *key2);

    /**
     * @brief Pointer to a function to free the memory of each node
     *
     * @param data Pointer to deallocate
     */
    void (*destroy)(void *data);

    size_t size;        /**< Number of elements in the list */
    list_item_td *head; /**< Pointer to the head of the list */
    list_item_td *tail; /**< Pointer to the tail of the list */
} list_td;


/* Public interface */
/**
 * @brief Initialize a linked list structure
 *
 * @param destroy Pointer to a function to free the memory
 *
 * @return New allocated linked list, or @c NULL otherwise
 *
 * @note This operation must be called for a linked list before the list
 *       can be used with any other operation
 * @note Complexity: @e O(1)
 */
list_td *list_init(void (*destroy)(void *data));

/**
 * @brief Destroy a linked list freeing its allocated memory
 *
 * @param list List to destroy
 *
 * @note No other operations are permitted after calling this function,
 *       unless @a list_init is called again
 * @note Complexity: @e O(n), where @e n is the number of items
 */
void list_destroy(list_td *list);

/**
 * @brief Clear the list without destroying it, freeing each item
 *
 * @param list List to clear
 *
 * @note Complexity: @e O(n), where @e n is the number of items
 */
void list_clear(list_td *list);

/**
 * @brief Insert an item after a given item
 *
 * @param list List to insert the data into
 * @param item Item in the position before the new item will be inserted
 * @param data Pointer to the data to be inserted
 *
 * @return Status of the insertion operation
 * @retval  0 Successfully inserted the item
 *
 * @note If @p item is @c NULL, the new item is inserted at the head
 * @note Complexity: @e O(1)
 */
int list_ins_next(list_td *list, list_item_td *item,
        const void *data);

/**
 * @brief Remove an item after a given item
 *
 * @param list List to remove the data from
 * @param item Item in the position before of the item to be removed
 * @param data Pointer to the data to be removed
 *
 * @return Status of the removal operation
 * @retval  0 Successfully removed the item
 *
 * @note If @p item is @c NULL, the head of the list will be removed
 * @note Allocated memory for the item must be manually freed
 * @note Complexity: @e O(1)
 */
int list_rem_next(list_td *list, list_item_td *item, void **data);

/**
 * @brief Macro that evaluates to the size of the linked list
 *
 * @note Complexity: @e O(1)
 */
#define list_size(self) ((self)->size)

/**
 * @brief Macro that evaluates to the head of the linked list
 *
 * @note Complexity: @e O(1)
 */
#define list_head(self) ((self)->head)

/**
 * @brief Macro that evaluates to the tail of the linked list
 *
 * @note Complexity: @e O(1)
 */
#define list_tail(self) ((self)->tail)

/**
 * @brief Macro that inquires about an item being the head of the list
 *
 * @note Complexity: @e O(1)
 */
#define list_is_head(self, item) (((item) == (self)->head) ? true : false)

/**
 * @brief Macro that inquires about an item being the tail of the list
 *
 * @note Complexity: @e O(1)
 */
#define list_is_tail(item) (((item)->next == NULL) ? true : false)

/**
 * @brief Macro that evaluates to the data of an item
 *
 * @note Complexity: @e O(1)
 */
#define list_data(item) ((item)->data)

/**
 * @brief Macro that evaluates to the next item of the linked list
 *
 * @note Complexity: @e O(1)
 */
#define list_next(item) ((item)->next)

/**
 * @brief Macro that inquires if the list is empty
 *
 * @note Complexity: @e O(1)
 */
#define list_is_empty(self) (((self)->size == 0) ? true : false)


#endif  /* ! LIST_H */
