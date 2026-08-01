/**
 * @file event.h
 *
 * @brief Event structure declaration
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef EVENT_H
#define EVENT_H


/* Project includes */
#include <action.h>
#include <priority.h>


/**
 * @brief Structure containing an event data
 *
 * Contains all the necessary information about an event, including its
 * type, action, priority, and the specific object it affects (window,
 * desktop, or screen).
 */
typedef struct event_s {
    void *data;                 /**< Specific event data */
    action_td action;           /**< Event action */
    enum priority_e priority;   /**< Event priority; higher values imply
                                     less priority */

    /**
     * @brief Object affected by this event (window, desktop, screen)
     */
    void *object;
} event_td;


/**
 * @brief Initializes a new event structure
 *
 * @param object      Object for which this event is
 * @param object_data Data for the object with updating information
 * @param action      Action to be performed on the object
 * @param priority    Initial priority for this event
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

/**
 * @brief Deep-copy an event structure
 *
 * Allocates a new @c event_td and duplicates all heap-allocated data
 * inside it.  The @c object pointer (@c client*, @c desktop*, ...) is
 * shallow-copied; the caller is responsible for ensuring the referenced
 * object outlives both the original and the clone.
 *
 * For @c ACTION_TYPE_CLIENT events whose action is one of
 * @c ACTION_CLIENT_RENAME, @c ACTION_CLIENT_RECLASS,
 * @c ACTION_CLIENT_REROLE, or @c ACTION_CLIENT_SET_ICON the
 * @c new_data.str strings are duplicated with @a safe_strdup.  All
 * other @c data payloads are shallow-copied by value.
 *
 * @param src Source event to clone
 *
 * @return Pointer to the new allocated clone, or @c NULL if @p src is
 *         a null pointer or memory allocation fails.  On failure no
 *         partial clone is left alive.
 *
 * @note Complexity: @e O(1) for most actions; @e O(k) when duplicating
 *       strings of total length @e k
 *
 * @see @c event_destroy
 */
event_td *event_clone(const event_td *src);


#endif  /* ! EVENT_H */
