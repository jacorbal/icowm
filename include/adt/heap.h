/**
 * @file adt/heap.h
 *
 * @brief Heap data structure declaration
 *
 * @ingroup adt
 */

#ifndef HEAP_H
#define HEAP_H


/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stddef.h>     /* size_t */


/**
 * @brief Heap occupancy threshold for shrinking the backing array
 *
 * When the number of stored nodes falls below this load factor, the
 * heap backing array may be resized down to reduce memory usage.
 *
 * @note This value must be in the domain [0.0f, 1.0f]
 * @note A suggested value is 0.25f
 */
#define HEAP_SHRINK_LOAD_FACTOR (0.25f)

/**
 * @brief Minimum backing-array capacity kept by the heap allocator
 *
 * The heap starts with this capacity and never shrinks below it while
 * non-empty.
 */
#define HEAP_MIN_CAPACITY ((size_t) 4u)


#ifndef HEAP_TD_DECLARED
#define HEAP_TD_DECLARED
/** Handle to a @c heap_s; the definition follows below */
typedef struct heap_s heap_td;
#endif

/**
 * @brief Heap structure implemented as a rustic binary tree
 */
struct heap_s {
    /**
     * @brief Use various heap operations to compare nodes when fixing
     *        the heap
     *
     * @param key1 First key to compare
     * @param key2 Second key to compare
     *
     * @return Status of the operation
     * @retval -1 if @p key1 < @p key2 for a top-heavy heap
     * @retval -1 if @p key1 > @p key2 for a bottom-heavy heap
     * @retval  0 if @p key1 == @p key2
     * @retval  1 if @p key1 > @p key2 for a top-heavy heap
     * @retval  1 if @p key1 < @p key2 for a bottom-heavy heap
     */
    int (*compare)(const void *key1, const void *key2);

    /**
     * @brief Pointer to a function to free the memory of allocated data
     *
     * @param data Pointer to the data to be deallocated
     */
    void (*destroy)(void *data);

    size_t size;        /**< Size of the heap */
    size_t capacity;    /**< Allocated array capacity (slots) */
    void **tree;        /**< Array of nodes in the heap */
};


/* Public interface */
/**
 * @brief Initialize a new heap structure
 *
 * @param compare Pointer to the function to compare nodes
 * @param destroy Pointer to a function to free the memory
 *
 * @return New allocated heap, or @c NULL otherwise
 *
 * @note This operation must be called for a heap before the heap can be
 *       used with any other operation
 * @note Complexity: @e O(1)
 */
heap_td *heap_init(int (*compare)(const void *key1, const void *key2),
        void (*destroy)(void *data));

/**
 * @brief Destroy a heap freeing its allocated memory
 *
 * @param heap Heap to destroy
 *
 * @note No other operations are permitted after calling this function,
 *       unless @a heap_init is called again
 * @note Complexity: @e O(n), where @e n is the number of nodes
 */
void heap_destroy(heap_td *heap);

/**
 * @brief Insert a node into the heap
 *
 * @param heap Heap to insert the node into
 * @param data Data of the node to be inserted
 *
 * @return Status of the node insertion operation
 * @retval  0 Successfully inserted the node
 * @retval -1 Failed to allocate memory for the new node
 *
 * @note Complexity: @e O(log n), where @e n is the number of nodes
 */
int heap_insert(heap_td *heap, const void *data);

/**
 * @brief Extract a node at the top of the heap
 *
 * @param heap Heap to extract a node from
 * @param data Pointer to the data to be extracted
 *
 * @return Status of the extraction operation
 * @retval  0 Successfully extracted the node
 * @retval -1 Heap is empty, or failed to reallocate storage
 *
 * @note Upon return, @e data points to the extracted node of the heap
 * @note The allocated memory for the node must be manually freed
 * @note Complexity: @e O(log n), where @e n is the number of nodes
 */
int heap_extract(heap_td *heap, void **data);

/**
 * @brief Macro that evaluates to the number of nodes in the heap
 *
 * @note Complexity: @e O(1)
 */
#define heap_size(self) ((self)->size)

/**
 * @brief Macro that evaluates to the emptiness state of the heap
 *
 * @note Complexity: @e O(1)
 */
#define heap_is_empty(p) ((heap_size(p) == 0) ? true : false)


#endif  /* ! HEAP_H */
