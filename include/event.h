/**
 * @file event.h
 *
 * @brief Event structure declaration
 */

#ifndef EVENT_H
#define EVENT_H


/* Project includes */
#include <action.h>
#include <desktop.h>
#include <screen.h>
#include <priority.h>
#include <window.h>


/**
 * @brief Structure containing an event data
 *
 * Contains all the necessary information about an event, including its
 * type, action, priority, and the specific object it affects (window,
 * desktop, or screen).
 */
typedef struct event_s {
    void *data;                     /**< Specific event data */
    action_td action;               /**< Event action */
    enum priority_e priority;       /**< Event priority; higher values
                                         imply less priority */

    /**< Object affected by this event (window, desktop, screen) */
    void *object;
} event_td;


/**
 * @brief Initializes a new event structure
 *
 * @param object         Object for which this event is
 * @param object_data    Data for the object with updating information
 * @param action         Action to be performed on the object
 * @param priority Initial priority for this event
 *
 * @return Pointer to new allocated event structure, or @c NULL
 *         otherwise
 *
 * @note Complexity: @e O(1)
 *
 * @see @c action_td, @c priority_e
 */
event_td *event_init(void *object, void *object_data,
        action_td action, enum priority_e priority);

/**
 * @brief Deallocate memory for an event structure
 *
 * @param event Event to deallocate
 *
 * @note Complexity: @e O(1)
 */
void event_destroy(event_td *event);


#endif  /* ! EVENT_H */
