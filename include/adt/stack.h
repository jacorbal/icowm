/**
 * @file adt/stack.h
 *
 * @brief Stack (LIFO) data structure declaration as linked list
 *
 * @ingroup ADT
 */

#ifndef STACK_H
#define STACK_H


/* System includes */
#include <stddef.h>     /* NULL */

/* ADT includes */
#include <adt/list.h>   /* Singly linked list */


/**
 * @brief Stack data type implementation as a linked list
 *
 * @see @c list_td
 */
typedef list_td stack_td;


/* Public interface */
/**
 * @brief Macro that evaluates to the stack initialization
 *
 * @see @a list_init
 */
#define stack_init list_init

/**
 * @brief Macro that evaluates to the stack deallocation
 *
 * @see @a list_destroy
 */
#define stack_destroy list_destroy

/**
 * @brief Push a new item to the stack
 *
 * @see @a list_ins_next
 */
#define stack_push(self, data) list_ins_next(self, NULL, data)

/**
 * @brief Pop an item from the stack
 *
 * @see @a list_rem_next
 */
#define stack_pop(self, data) list_rem_next(self, NULL, data)

/**
 * @brief Macro that evaluates to the top of the stack
 */
#define stack_peek(self) \
    (((self)->head == NULL) ? NULL : (self)->head->data)

/**
 * @brief Macro that evaluates to the size of the stack
 *
 * @see @a list_size
 */
#define stack_size list_size

/**
 * @brief Macro that inquires if the stack is empty
 *
 * @see @a list_is_empty
 */
#define stack_is_empty list_is_empty


#endif  /* ! STACK_H */
