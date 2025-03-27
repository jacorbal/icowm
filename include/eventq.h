/**
 * @file eventq.h
 *
 * @brief Event priority queue (min-heap) handler function declaration
 */

#ifndef EVENTQ_H
#define EVENTQ_H


/* Project includes */
#include <actions.h>
#include <desktop.h>
#include <screen.h>
#include <window.h>


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
enum event_priority_e {
    EVENT_PRIORITY_URGENT = -10,    /**< Urgent priority: immediate */
    EVENT_PRIORITY_HIGH= -5,        /**< High priority: critical events */
    EVENT_PRIORITY_NORMAL = 0,      /**< Normal priority: standard */
    EVENT_PRIORITY_LOW = 5,         /**< Low priority: unimportant */
    EVENT_PRIORITY_LOWEST = 10,     /**< Lowest priority: can wait */
};


/**
 * @brief Structure containing an event data
 *
 * Contains all the necessary information about an event, including its
 * type, action, priority, and the specific object it affects (window,
 * desktop, or screen).
 */
typedef struct {
    void *data;                     /**< Specific event data */
    action_td action;               /**< Event action */
    enum event_priority_e priority; /**< Event priority; higher values
                                         imply less priority */
    union {
        window_td *window;
        desktop_td *desktop;
        screen_td *screen;
    } object;                       /**< Object affected by this event */
} event_td;


/* Public interface */
/**
 * @brief Start priority queue (min-heap) for events
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Could not allocate memory
 * @retval -1 Singleton was already initialized; no action taken
 */
int eventq_start(void);

/**
 * @brief Deallocates memory used by this event priority queue
 *
 * @return Status of the operation
 * @return  0 Success
 * @return  1 No operation has been performed
 */
int eventq_stop(void);

/**
 * @brief Initializes a new event structure
 *
 * @param object         Object for which this event is
 * @param action   Action to be performed on the object
 * @param event_priority Initial priority for this event
 *
 * @return Pointer to new allocated event structure, or @c NULL
 *         otherwise
 *
 * @note Complexity: @e O(1)
 *
 * @see @c action_td, @c event_priority_e
 */
event_td *event_init(void *object, action_td action,
        enum event_priority_e event_priority);

/**
 * @brief Deallocate memory for an event structure
 *
 * @param event Event to deallocate
 *
 * @note Complexity: @e O(1)
 */
void event_destroy(event_td *event);

/**
 * @brief Add a event to the event priority queue
 *
 * Initializes a new event and inserts it on the event queue.  The
 * memory allocated for this event will be destroyed by the event queue
 * on extraction if it's still within the queue.
 *
 * @param event New event to add to the priority queue
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to insert the event into the event queue
 *
 * @note Complexity: @e O(1)
 */
int eventq_add(event_td *event);

/**
 * @brief Process the event in the priority queue
 *
 * @note Complexity: @e O(log n) where @e n is the number of events to
 *       process
 */
void eventq_process(void);


#endif  /* ! EVENTQ_H */
