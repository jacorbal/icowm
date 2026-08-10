/**
 * @file adt/queue.h
 *
 * @brief Queue (FIFO) data structure declaration as linked list
 *
 * @ingroup adt
 */

#ifndef QUEUE_H
#define QUEUE_H


/* System includes */
#include <stddef.h>     /* NULL */

/* ADT includes */
#include <adt/list.h>   /* Singly linked list */


/**
 * @brief Queue data type implementation as a linked list
 *
 * @see @c list_td
 */
typedef list_td queue_td;


/* Public interface */
/**
 * @brief Macro that evaluates to the queue initialization
 *
 * @see @a list_init
 */
#define queue_init list_init

/**
 * @brief Macro that evaluates to the queue deallocation
 *
 * @see @a list_destroy
 */
#define queue_destroy list_destroy

/**
 * @brief Macro that evaluates to the queue clear action
 *
 * @see @a list_clear
 */
#define queue_clear list_clear

/**
 * @brief Enqueue a new item to the queue
 *
 * @see @a list_ins_next
 */
#define queue_enqueue(self, data) \
    list_ins_next(self, list_tail(self), data)

/**
 * @brief Dequeue an item from the queue
 *
 * @see @a list_rem_next
 */
#define queue_dequeue(self, data) list_rem_next(self, NULL, data)

/**
 * @brief Macro that evaluates to the data in front of the queue
 */
#define queue_peek(self) \
    (((self)->head == NULL) ? NULL : (self)->head->data)

/**
 * @brief Macro that evaluates to the size of the queue
 *
 * @see @a list_size
 */
#define queue_size list_size

/**
 * @brief Macro that inquires if the queue is empty
 *
 * @see @a list_is_empty
 */
#define queue_is_empty list_is_empty


#endif  /* ! QUEUE_H */
