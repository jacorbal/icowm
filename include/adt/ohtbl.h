/**
 * @file ohtbl.h
 *
 * @brief Open-addressed hash table (closed hasing) declaration
 *
 * The hash table dynamically resizes itself when the number of stored
 * elements exceeds a predefined load factor, ensuring that operations
 * on the table remain efficient.  The resizing process involves
 * doubling the current capacity and rehashing existing items to the new
 * storage arrangement.
 *
 * @ingroup ADT
 */

#ifndef OHTBL_H
#define OHTBL_H


/* System includes */
#include <stdbool.h>    /* bool */
#include <stddef.h>     /* NULL, size_t */


/**
 * @brief Maximum hash table load factor
 *
 * Determines the occupancy threshold that, if exceeded, will trigger a
 * resize of the hash table.  On a new insertion, the table will be
 * re-dimensioned and re-hashed when its @p ohtbl->size is equal or
 * bigger than the (100 *  @c OHTBL_MAX_LOAD_FACTOR)% of its positions
 * given by @p ohtbl->positions.
 *
 *  - If the value is 0, the condition for resizing will always be
 *    @c true, as the current size will always be greater than or equal
 *    to 0.  This means the table will attempt to resize every time an
 *    element is added, which can lead to inefficient performance.
 *  - If the value is greater than 1, the hash table will permit an
 *    excessive number of elements, leading to a significant decrease in
 *    search and insertion efficiency due to increased collisions
 *
 * @note This value must be in the domain [0 ,1]
 * @note A suggested value is 0.75f
 *
 * @see ohtbl_insert
 */
#define OHTBL_MAX_LOAD_FACTOR (0.75f)


/**
 * @brief Structure for open-addressed hash table with double hashing
 */
typedef struct {
    size_t positions; /**< Number of positions to allocate in the table */
    void *vacated;    /**< Pointer initialized to a storage location to
                           indicate that a particular position has had
                           an element removed from it */

    /**
     * @brief Pointer to a function to free the memory of each node
     *
     * @param data Pointer to the data to be deallocated
     */
    void (*destroy)(void *data);

    size_t (*h1)(const void *key);  /**< Double hashing function */
    size_t (*h2)(const void *key);  /**< Double hashing function */

    /**
     * @brief Pointer to the function to test if two keys are equal
     *
     * @param key1 First key to compare
     * @param key2 Second key to compare
     *
     * @return Status of the match inquiry operation
     * @retval true  Both keys match
     * @retval false The keys do not match
     */
    bool (*match)(const void *key1, const void *key2);

    size_t size;        /**< Size of the open-addressed hash table */
    void **table;       /**< Table to allocate items in */
} ohtbl_td;


/* Public interface */
/**
 * @brief Initialize a new open-addressed hash table with double hashing
 *
 * @param positions Number of positions to allocate in the hash table
 * @param h1        Pointer to an auxiliary hashing function
 * @param h2        Pointer to another auxiliary hashing function
 * @param match     Pointer to a function to test if two keys are equal
 * @param destroy   Pointer to a function to free the memory
 *
 * @return Pointer to new allocated open-addressed hash table, or
 *         @c NULL otherwise
 *
 * @note This operation must be called for a open-addressed hash table
 *       before the hash table can be used with any other operation
 * @note Complexity: @e O(m), where @e m is the number of positions
 */
ohtbl_td *ohtbl_init(size_t positions,
        size_t (*h1)(const void *key), size_t (*h2)(const void *key),
        bool (*match)(const void *key1, const void *key2),
        void (*destroy)(void *data));

/**
 * @brief Destroy the open-addressed hash table
 *
 * @param htbl Pointer to the hash table to deallocate
 *
 * @note No other operations are permitted after calling this function,
 *       unless @e ohtbl_init is called again
 * @note Complexity: @e O(m), where @e m is the number of positions
 */
void ohtbl_destroy(ohtbl_td *htbl);

/**
 * @brief Sets all entries to @c NULL and resets the table
 *
 * @param htbl Pointer to the open-addressed hash table to reset
 *
 * @post Size of the hash table is zero (@p htbl->size == 0)
 *
 * @note The number of positions remains, in order to change that,
 *       better to destruct this hash table and initialize another
 * @note Complexity: @e O(m), where @e m is the number of positions
 */
void ohtbl_reset(ohtbl_td *htbl);

/**
 * @brief Insert a new item in the hash table
 *
 * @param htbl Pointer to the open-addressed hash table where to insert
 * @param data Pointer to the data to be inserted
 *
 * @return Status of the insertion operation
 * @retval  0 The insertion was successful
 * @retval  1 The item already existed in the table
 * @retval -1 Exceeded positions on the table, or bad hash function
 * @retval -2 Could not resize the table
 *
 * @note Complexity: @e O(1)
 */
int ohtbl_insert(ohtbl_td *htbl, const void *data);

/**
 * @brief Update an item in the hash table
 *
 * If the item didn't exist in the table, it'll be inserted normally, if
 * exists, it'll be updated to the new value.
 *
 * @param htbl Pointer to the open-addressed hash table to update
 * @param data Pointer to the data to be updated
 *
 * @return Status of the update operation
 * @retval  0 The update was successful
 * @retval  1 The item already existed in the table
 * @retval -1 Nothing was done
 *
 * @note Complexity: @e O(1)
 */
int ohtbl_update(ohtbl_td *htbl, const void *data);

/**
 * @brief Remove an item from the hash table that matches @e data
 *
 * If @e data is a match, @e data will point to the data stored in the
 * element that was removed
 *
 * @param htbl Pointer to the open-addressed hash table
 * @param data Pointer to the data to be matched
 *
 * @return Status of the removal operation
 * @retval  0 The removal was successful
 * @retval -1 Data was not found
 *
 * @note The memory of this item has to be deallocated manually
 * @note Complexity: @e O(1)
 */
int ohtbl_remove(ohtbl_td *htbl, void **data);

/**
 * @brief Determines if an item matches the data in the hash table
 *
 * If @p data is a match, @p data will point to the matching data in the
 * open-addressed hash table
 *
 * @param htbl Pointer to the open-addressed hash table to look up in
 * @param data Pointer to the data to look for
 *
 * @return Status of the lookup operation
 * @retval  0 The item was successfully found
 * @retval -1 The item was not found
 *
 * @note Complexity: @e O(1)
 */
int ohtbl_lookup(const ohtbl_td *htbl, void **data);

/**
 * @brief Resizes the open-addressed hash table to a new capacity
 *
 * This function doubles the current positions of the hash table and
 * rehashes all existing items into the new table. If the memory
 * allocation for the new table fails, the operation is aborted and an
 * error code is returned.
 *
 * @param htbl Pointer to the open-addressed hash table to be resized
 *
 * @return Status of the resize operation
 * @retval  0 The resize operation was successful
 * @retval -1 Memory allocation for the new table failed
 *
 * @note The function assumes that the current table is using open
 *       addressing and that structures for each item are still valid
 *       after rehashing
 * @note Complexity: @e O(n), where @e n is the number of elements in
 *       the hash table
 */
int ohtbl_resize(ohtbl_td *htbl);

/**
 * @brief Macro that evaluates to the hash table size giving the number
 *        of items
 *
 * @note Complexity: @e O(1)
 */
#define ohtbl_size(self) ((self)->size)


#endif /* ! OHTBL_H */
