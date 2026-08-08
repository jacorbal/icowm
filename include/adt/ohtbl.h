/**
 * @file adt/ohtbl.h
 *
 * @brief Open-addressed hash table (closed hashing) declaration
 *
 * The hash table dynamically resizes itself when the number of stored
 * elements exceeds or falls below a predefined threshold, a load
 * factor, ensuring that operations on the table remain efficient.  The
 * resizing process involves doubling or halving the current capacity
 * and rehashing existing items to the new storage arrangement.
 *
 * @ingroup ADT
 */

#ifndef OHTBL_H
#define OHTBL_H


/* System includes */
#include <stdbool.h>
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
 * @note This value must be in the domain [0.0f ,1.0f]
 * @note A suggested value is 0.75f
 *
 * @see @a ohtbl_insert, @a ohtbl_update
 */
#define OHTBL_MAX_LOAD_FACTOR (0.75f)

/**
 * @brief Minimum hash table load factor
 *
 * Determines the occupancy threshold below which the hash table will
 * attempt to resize downwards. If the size of the table falls below the
 * (100 *  @c OHTBL_MIN_LOAD_FACTOR)% of its positions given by
 * @p ohtbl->positions, the table will be resized to reduce its
 * capacity and re-hashed.
 *
 * - If the value is 1, the condition for resizing will always be
 *   @c false, as the number of elements can never exceed the total
 *   number of positions.  This means the table will not shrink even
 *   when it is sparsely populated, potentially wasting memory.
 * - If the value is less than 0, the behavior is undefined.
 *
 * @note This value must be in the domain [0.0f, 1.0f]
 * @note A suggested value is 0.25f
 *
 * @see @a ohtbl_remove
 */
#define OHTBL_MIN_LOAD_FACTOR (0.25f)


/**
 * @brief Structure for open-addressed hash table with double hashing
 */
typedef struct {
    size_t positions;   /**< Number of positions (slots) to allocate in
                             the table */

    /**
     * @brief Minimum number of positions (slots) that the hash has
     *
     * The table will not reduce its capacity below this threshold,
     * ensuring that sufficient space is always available to accommodate
     * the current and expected load.  Setting this value too low may
     * restrict the table's ability to handle the number of elements
     * efficiently, leading to increased collisions and decreased
     * performance.  If the value is zero or negative, it would allow
     * the table to become non-functional, as a hash table requires at
     * least one position to operate.
     */
    size_t min_positions;

    /**
     * @brief Pointer initialized to a storage location to indicate that
     *        a particular position has had an element removed from it
     *
     * Signifies that a position in the hash table is vacated due to an
     * item being removed.  While this slot is no longer occupied by an
     * item, it is still a valid position for probing during insertion
     * or lookup operations.  It helps in handling open addressing
     * scenarios, allowing the hash table to manage collisions more
     * effectively.
     *
     * @note Vacated positions can be reused for new insertions to
     *       maintain compactness and efficiency of the hash table
     * @note It is essential to distinguish between @c NULL (indicating
     *       an empty slot) and vacated slots, as this affects the logic
     *       of search and insertion operations.
     */
    void *vacated;

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
     * @retval  true Both keys match
     * @retval false Keys do not match
     */
    bool (*match)(const void *key1, const void *key2);

    size_t size;        /**< Size of the open-addressed hash table */
    void **table;       /**< Table to allocate items in */
} ohtbl_td;


/* Public interface */
/**
 * @brief Initialize a new open-addressed hash table with double hashing
 *
 * @param positions     Number of positions to allocate in the hash table
 * @param min_positions Minimum number of positions the table will have
 * @param h1            Pointer to an auxiliary hashing function
 * @param h2            Pointer to another auxiliary hashing function
 * @param match         Pointer to a function to test if two keys are equal
 * @param destroy       Pointer to a function to free the memory
 *
 * @return Pointer to new allocated open-addressed hash table, or
 *         @c NULL otherwise
 *
 * @note If @p min_positions is initialized to zero, or to a value
 *       greater that the initial positions, it will assume that
 *       the minimum number of positions is the same as the initial
 *       @p positions of with which the table was created
 * @note This operation must be called for a open-addressed hash table
 *       before the hash table can be used with any other operation
 * @note Complexity: @e O(m), where @e m is the number of positions
 */
ohtbl_td *ohtbl_init(size_t positions, const size_t min_positions,
        size_t (*h1)(const void *key), size_t (*h2)(const void *key),
        bool (*match)(const void *key1, const void *key2),
        void (*destroy)(void *data));

/**
 * @brief Destroy the open-addressed hash table
 *
 * @param htbl Pointer to the hash table to deallocate
 *
 * @note No other operations are permitted after calling this function,
 *       unless @a ohtbl_init is called again
 * @note Complexity: @e O(m), where @e m is the number of positions
 */
void ohtbl_destroy(ohtbl_td *htbl);

/**
 * @brief Set all entries to @c NULL and resets the table
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
 * If the size of the table exceeds the @c OHTBL_MAX_LOAD_FACTOR
 * threshold, the table will be resized by doubling its positions.
 *
 * @param htbl Pointer to the open-addressed hash table where to insert
 * @param data Pointer to the data to be inserted
 *
 * @return Status of the insertion operation
 * @retval  0 Insertion was successful
 * @retval  1 Item already existed in the table
 * @retval -2 Could not resize the table, or bad hash functions
 *
 * @note Complexity: @e O(1)
 */
int ohtbl_insert(ohtbl_td *htbl, const void *data);

/**
 * @brief Update an existing element in the has table, or insert it as
 *        new if didn't exist
 *
 * If the item didn't exist in the table, it'll be inserted normally; if
 * exists, it'll be updated to the new value.  If the size of the table
 * is below the @c OHTBL_MAX_LOAD_FACTOR threshold, the table will be
 * resized doubling its positions.  This means that if the item doesn't
 * previously exist in the hash table, it will be inserted only after
 * resizing the table it, only if the size of the table requires it.
 *
 * @param htbl Pointer to the open-addressed hash table to update
 * @param data Pointer to the data to be updated
 *
 * @return Status of the update operation
 * @retval  0 Update was successful
 * @retval  1 Item already existed in the table
 * @retval -1 Nothing was done, possible bad hash functions
 * @retval -2 Could not resize the table, and the element was not
 *            inserted
 *
 * @note Complexity: @e O(1)
 */
int ohtbl_update(ohtbl_td *htbl, const void *data);

/**
 * @brief Remove an item from the hash table that matches @p data
 *
 * If @p data is a match, @p data will point to the data stored in the
 * element that was removed.  If the size of the table falls below the
 * @c OHTBL_MIN_LOAD_FACTOR threshold, the table will be resized by
 * halving its positions.
 *
 * @param htbl Pointer to the open-addressed hash table
 * @param data Pointer to the data to be matched
 *
 * @return Status of the removal operation
 * @retval  0 Removal was successful
 * @retval -1 Data was not found
 * @retval -2 Removal successful, but could not resize the table
 *
 * @note Memory for this item has to be deallocated manually
 * @note Complexity: @e O(1)
 */
int ohtbl_remove(ohtbl_td *htbl, void **data);

/**
 * @brief Determine if an item matches the data in the hash table
 *
 * If @p data is a match, @p data will point to the matching data in the
 * open-addressed hash table.
 *
 * @param htbl Pointer to the open-addressed hash table to look up in
 * @param data Pointer to the data to look for
 *
 * @return Status of the lookup operation
 * @retval  0 Item was successfully found
 * @retval -1 Item was not found
 *
 * @note Complexity: @e O(1)
 */
int ohtbl_lookup(const ohtbl_td *htbl, void **data);

/**
 * @brief Resize the open-addressed hash table to a new capacity
 *
 * Resizes the current positions of the hash table to the specific new
 * capacity, and rehashes all existing items into the new table. If the
 * memory allocation for the new table fails, the operation is aborted
 * and an error code is returned.
 *
 * @param htbl          Pointer to the hash table to be resized
 * @param new_positions New positions to resize to
 *
 * @return Status of the resize operation
 * @retval  0 Resize operation was successful
 * @retval -1 Failed to allocate memory for the new table
 *
 * @note It is assumed that the current table is using open addressing
 *       and that structures for each item are still valid after
 *       rehashing
 * @note Complexity: @e O(n), where @e n is the number of elements in
 *       the hash table
 */
int ohtbl_resize(ohtbl_td *htbl, size_t new_positions);


/**
 * @brief Double the current size of the open-addressed hash table
 *
 * Invokes the resize operation to increase the capacity of the hash
 * table by doubling the current number of positions.
 *
 * This doubling is triggered on insertions when the load factor exceeds
 * the defined maximum load factor (@c OHTBL_MAX_LOAD_FACTOR).
 *
 * @param htbl Pointer to the hash table to be doubled in size
 *
 * @return Status of the resize operation
 * @retval  0 Resize operation was successful
 * @retval -1 Failed to allocate memory for the new table
 *
 * @note It is assumed that the current table is using open addressing
 *       and that structures for each item are still valid after
 *       rehashing
 * @note Complexity: @e O(n), where @e n is the number of elements in
 *       the hash table
 *
 * @see @a obtbl_resize
 */
int ohtbl_resize_double(ohtbl_td *htbl);

/**
 * @brief Halve the current size of the open-addressed hash table
 *
 * Invokes the resize operation to decrease the capacity of the hash
 * table by halving the current number of positions.
 *
 * This halving is triggered on removals when the load factor falls
 * below the minimum load factor (@c OHTBL_MIN_LOAD_FACTOR), as long as
 * the table size does not go below the specified minimum positions.
 *
 * @param htbl Pointer to the hash table to be halved in size
 *
 * @return Status of the resize operation
 * @retval  0 Resize operation was successful
 * @retval  1 Below threshold of minimum size
 * @retval -1 Memory allocation for the new table failed
 *
 * @note It is assumed that the current table is using open addressing
 *       and that structures for each item are still valid after
 *       rehashing
 * @note Complexity: @e O(n), where @e n is the number of elements in
 *       the hash table
 *
 * @see @a obtbl_resize
 */
int ohtbl_resize_halve(ohtbl_td *htbl);

/**
 * @brief Macro that evaluates to the current size of the hash table,
 *        returning the number of stored items
 *
 * @note Complexity: @e O(1)
 */
#define ohtbl_size(self) ((self)->size)

/**
 * @brief Macro that initializes the hash table with default postions
 *        value
 *
 * @see @a ohtbl_init
 */
#define ohtbl_init_quick(p, h1, h2, m, d) \
    ohtbl_init(p, p, h1, h2, m, d)

/**
 * @brief Iterate over every valid (non-NULL, non-vacated) entry in an
 *        open-addressed hash table
 *
 * Expands to a @c for statement; @c break and @c continue work as
 * expected inside the loop body.  Variable @p item must be declared as
 * a @c void* before invoking the macro and receives a pointer to each
 * stored element in turn.
 *
 * @param self Pointer to the hash table to iterate over
 * @param item A @c void* variable that receives each valid element
 *
 * @note Iteration order reflects the internal slot layout and is not
 *       defined in terms of insertion order
 * @note Complexity: @e O(m), where @e m is the number of positions
 */
#define ohtbl_foreach(self, item) \
    for (size_t _i_ = 0; _i_ < (self)->positions; ++_i_) \
        if ((self)->table[_i_] != NULL && \
            (self)->table[_i_] != (self)->vacated && \
            ((item) = (self)->table[_i_], 1))


#endif  /* ! OHTBL_H */
