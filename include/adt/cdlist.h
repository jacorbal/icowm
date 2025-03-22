/**
 * @file cdlist.h
 *
 * @brief Doubly linked circular list data structure declaration
 *
 * @ingroup ADT
 */

#ifndef CDLIST_H
#define CDLIST_H


/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stddef.h>     /* NULL, size_t */


/**
 * @brief Doubly linked list items structure
 */
typedef struct cdlist_item_td_s {
    void *data;                     /**< Pointer to the data of this item */
    struct cdlist_item_td_s *next;  /**< Pointer to the next element */
    struct cdlist_item_td_s *prev;  /**< Pointer to the previous element */
} cdlist_item_td;

/**
 * @brief Doubly linked circular list structure
 */
typedef struct {
    /**
     * @brief Pointer to a function to free the memory of each node
     *
     * @param data Pointer to deallocate
     */
    void (*destroy)(void *data);

    size_t size;            /**< Number of elements in the list */
    cdlist_item_td *head;   /**< Pointer to the head of the list */
    cdlist_item_td *tail;   /**< Pointer to the tail of the list */
} cdlist_td;


/* Public interface */
/**
 * @brief Initialize a doubly linked circular list structure
 *
 * @param destroy Pointer to a function to free the memory
 *
 * @return New allocated doubly linked circular list, or @c NULL
 *         otherwise
 *
 * @note This operation must be called for a list before it
 *       can be used with any other operation
 * @note Complexity: @e O(1)
 */
cdlist_td *cdlist_init(void (*destroy)(void *data));

/**
 * @brief Destroy a doubly linked circular list freeing its allocated
 *        memory
 *
 * @param cdlist List to destroy
 *
 * @note No other operations are permitted after calling this function,
 *       unless @e cdlist_init is called again
 * @note Complexity: @e O(n), where @e n is the number of items
 */
void cdlist_destroy(cdlist_td *cdlist);

/**
 * @brief Clear the list without destroying it, freeing each item
 *
 * @param cdlist List to clear
 *
 * @note Complexity: @e O(n), where @e n is the number of items
 */
void cdlist_clear(cdlist_td *cdlist);

/**
 * @brief Insert an item before a given item
 *
 * @param cdlist List to insert the data into
 * @param item   Item in the position after new item will be inserted
 * @param data   Pointer to the data to be inserted
 *
 * @return Status of the insertion operation
 * @retval  0 Successfully inserted the item
 *
 * @note If @e item is @c NULL, the new item is inserted at the tail
 * @note Complexity: @e O(1)
 */
int cdlist_ins_prev(cdlist_td *cdlist, cdlist_item_td *item,
        const void *data);

/**
 * @brief Insert an item after a given item
 *
 * @param cdlist List to insert the data into
 * @param item   Item in the position before new item will be inserted
 * @param data   Pointer to the data to be inserted
 *
 * @return Status of the insertion operation
 * @retval  0 Successfully inserted the item
 *
 * @note If @e item is @c NULL, the new item is inserted at the head
 * @note Complexity: @e O(1)
 */
int cdlist_ins_next(cdlist_td *cdlist, cdlist_item_td *item,
        const void *data);

/**
 * @brief Remove an item before a given item
 *
 * @param cdlist List to remove the data from
 * @param item   Item in the position after the item to be removed
 * @param data   Pointer to the data to be removed
 *
 * @return Status of the removal operation
 * @retval  0 Successfully removed the item
 *
 * @note If @e item is @c NULL, the tail of the list will be removed
 * @note The allocated memory of the item must be manually freed
 * @note Complexity: @e O(1)
 */
int cdlist_rem_prev(cdlist_td *cdlist, cdlist_item_td *item,
        void **data);

/**
 * @brief Remove an item after a given item
 *
 * @param cdlist List to remove the data from
 * @param item   Item in the position before of the item to be removed
 * @param data   Pointer to the data to be removed
 *
 * @return Status of the removal operation
 * @retval  0 Successfully removed the item
 *
 * @note If @e item is @c NULL, the head of the list will be removed
 * @note The allocated memory of the item must be manually freed
 * @note Complexity: @e O(1)
 */
int cdlist_rem_next(cdlist_td *cdlist, cdlist_item_td *item,
        void **data);

/**
 * @brief Macro that evaluates to the size of the doubly linked circular
 *        list
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_size(self) ((self)->size)

/**
 * @brief Macro that evaluates to the head of the doubly linked circular
 *        list
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_head(self) ((self)->head)

/**
 * @brief Macro that evaluates to the tail of the doubly linked circular
 *        list
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_tail(self) ((self)->tail)

/**
 * @brief Macro that inquires about an item being the head of the list
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_is_head(self, item) \
    (((item) == (self)->head) ? true : false)

/**
 * @brief Macro that inquires about an item being the tail of the list
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_is_tail(self, item) \
    (((item) == (self)->tail) ? true : false)

/**
 * @brief Macro that evaluates to the data of an item
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_data(item) ((item)->data)

/**
 * @brief Macro that evaluates to the next item of the doubly linked
 *        circular list
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_next(item) ((item)->next)

/**
 * @brief Macro that evaluates to the previous item of the doubly linked
 *        circular list
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_prev(item) ((item)->prev)

/**
 * @brief Macro that inquires if the list is empty
 *
 * @note Complexity: @e O(1)
 */
#define cdlist_is_empty(self) (((self)->size == 0) ? true : false)


#endif  /* ! CDLIST_H */
