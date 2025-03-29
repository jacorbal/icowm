/**
 * @file pqueue.h
 *
 * @brief Priority queue declaration as a heap
 *
 * @ingroup ADT
 */

#ifndef PQUEUE_H
#define PQUEUE_H


/* System includes */
#include <stddef.h>     /* NULL */

/* ADT includes */
#include <adt/heap.h>   /* Heap (as a rustic binary tree) */


/**
 * @brief Priority queue implementation as a heap
 *
 * @see @c heap_td
 */
typedef heap_td pqueue_td;


/* Public interface */
/**
 * @brief Macro that evaluates to the priority queue initialization
 *
 * @see @a heap_init
 */
#define pqueue_init heap_init

/**
 * @brief Macro that evaluates to the priority queue deallocation
 *
 * @see @a heap_destroy
 */
#define pqueue_destroy heap_destroy

/**
 * @brief Insert a new item in the priority queue
 *
 * @see @a heap_insert
 */
#define pqueue_insert heap_insert

/**
 * @brief Extract a item from the priority queue
 *
 * @see @a heap_extract
 */
#define pqueue_extract heap_extract

/**
 * @brief Highest priority element in the priority queue, or @c NULL
 */
#define pqueue_peek(self) \
    (((self)->tree == NULL) ? NULL : (self)->tree[0])

/**
 * @brief Macro that evaluates to the size of the priority queue
 *
 * @see @a heap_size
 */
#define pqueue_size heap_size

/**
 * @brief Macro that evaluates to the emptiness state of the priority
 *        queue
 *
 * @see @a heap_is_empty
 */
#define pqueue_is_empty heap_is_empty


#endif  /* ! PQUEUE_H */
