/**
 * @file priority.h
 *
 * @brief Enumeration for priority levels to be used in the event
 *        priority queue
 *
 * Events are created with an initial priority level, in this case, zero
 * or @c PRIORITY_NORMAL.  This enumeration just shows possible initial
 * values to standard events that always will use the same priorirty.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
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
 * The actual event priority can exceed @c PRIORITY_HIGH or
 * @c PRIORITY_LOW.  They can be adjusted as needed, but are primarily
 * intended for initializing event priorities in a standard way.
 */
enum priority_e {
    PRIORITY_URGENT = -20,      /**< Urgent: immediate action */
    PRIORITY_HIGHEST= -15,      /**< Highest: requires immediate attention */
    PRIORITY_HIGHER = -10,      /**< Higher: immediate response is necessary */
    PRIORITY_HIGH = -5,         /**< High: important but not critical */
    PRIORITY_NORMAL = 0,        /**< Normal: standard behavior */
    PRIORITY_LOW = 5,           /**< Low: events of little importance */
    PRIORITY_LOWER = 10,        /**< Lower: events that can wait */
    PRIORITY_LOWEST = 15,       /**< Lowest: unimportant events */
    PRIORITY_IDLE = 20          /**< Idle: events that can be deferred */
};


#endif  /* ! PRIORITY_H */
