/**
 * @file eventq.h
 *
 * @brief Event priority queue (min-heap) handler function declaration
 *
 * Interface for handling an event priority queue, implemented as
 * a min-heap.  Events are ordered by priority and drained synchronously
 * from the main event loop after each batch of X11 events is processed.
 * This model eliminates data races and potential deadlocks, and
 * achieves lower event latency than a background-thread design.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef EVENTQ_H
#define EVENTQ_H


/* Project includes */
#include <event.h>


/* Public interface */
/**
 * @brief Initialize the event priority queue
 *
 * Allocates and initializes the singleton min-heap used to hold pending
 * events.  Events are drained synchronously by calling
 * @a eventq_process from the main event loop.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Could not allocate memory
 * @retval -1 Singleton was already initialized; no action taken
 *
 * @note Complexity: @e O(1)
 */
int eventq_start(void);

/**
 * @brief Deallocate memory used by the event priority queue
 *
 * Destroys all remaining events and releases the queue.  Must be called
 * only from the main thread after the event loop has exited.
 *
 * @return Status of the operation
 * @return  0 Success
 * @return  1 Queue was not initialized; no action taken
 *
 * @note Complexity: @e O(n), where @e n is the number of events
 *       remaining in the queue
 */
int eventq_stop(void);

/**
 * @brief Add a event to the event priority queue
 *
 * Initializes a new event and inserts it on the event queue.  The
 * memory allocated for this event will be destroyed by the event queue
 * on extraction if it's still within the queue, based on its priority.
 * The event will be processed based on its priority relative to other
 * events in the queue.
 *
 * @param event New event to add to the priority queue
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to insert the event into the event queue @p eventq
 *
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue
 */
int eventq_add(event_td *event);

/**
 * @brief Extract an event at from the priority queue
 *
 * Removes and returns the highest priority event (smallest priority
 * value) from the priority queue.
 *
 * @return Pointer to extracted event, or @c NULL otherwise
 *
 * @note Upon return, @e event points to the extracted node of the queue
 * @note The allocated memory for the node must be manually freed
 * @note Complexity: @e O(log n), where @e n is the number of events in
 *       the priority queue @p eventq
 */
event_td *eventq_extract(void);

/**
 * @brief Process the event in the priority queue
 *
 * Extracts and processes all events currently in the priority queue.
 * Each event is dispatched to the appropiate handler based on its
 * action type.  This function processes events until the queue is
 * empty.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to process the event
 *
 * @note Complexity: @e O(n log n), where @e n is the number of events
 *       in the queue, due to extraction operations
 */
int eventq_process(void);


#endif  /* ! EVENTQ_H */
