/**
 */

#ifndef PRIORITY_H
#define PRIORITY_H


/**
 * @brief Initial values for priority
 *
 * The default priority is 0.  Positive priorities are used for less
 * critical events that run when no other higher priority task is ready
 * to run.  Negative priorities cause an event to be handled more
 * frequently.  This is often confusing because the higher the numerical
 * value, the lower the execution precedence.
 *
 * The actual event priority can exceed @c EVENT_PRIORITY_HIGH or
 * @c EVENT_PRIORITY_LOW.  They can be adjusted as needed, but are
 * primarily intended for initializing event priorities in a standard
 * way.
 */
enum priority_e {
    PRIORITY_URGENT = -10,    /**< Urgent priority: immediate */
    PRIORITY_HIGH= -5,        /**< High priority: critical events */
    PRIORITY_NORMAL = 0,      /**< Normal priority: standard */
    PRIORITY_LOW = 5,         /**< Low priority: unimportant */
    PRIORITY_LOWEST = 10,     /**< Lowest priority: can wait */
};


#endif  /* ! PRIORITY_H */
