/**
 * @file eventq.h
 *
 * @brief Event queue (FIFO) handler function declaration
 *
 * Interface for handling an event queue, implemented as a FIFO singly
 * linked list.  Events are inserted at the tail and drained from the
 * head synchronously from the main event loop after each batch of X11
 * events is processed.  This model eliminates data races and potential
 * deadlocks, and achieves lower event latency than a background-thread
 * design.
 *
 * @note Priority levels are accepted by @a eventq_add for API
 *       compatibility but are no longer used to reorder events; all
 *       events are processed in arrival order (FIFO).
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
 * @brief Initialize the event queue
 *
 * Allocates and initializes the singleton FIFO queue used to hold
 * pending events.  Events are drained synchronously by calling
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
 * @brief Deallocate memory used by the event queue
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
 * @brief Add a event to the event queue
 *
 * Enqueues @p event at the tail of the FIFO queue.  The priority field
 * is accepted but ignored; events are always processed in arrival
 * order.
 *
 * @param event New event to enqueue
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to enqueue the event
 *
 * @note Complexity: @e O(1)
 */
int eventq_add(event_td *event);

/**
 * @brief Extract an event at from the queue
 *
 * Removes and returns the oldest event (FIFO head) from the queue.
 *
 * @return Pointer to the extracted event, or @c NULL when empty
 *
 * @note The caller is responsible for freeing the returned event
 * @note Complexity: @e O(1)
 */
event_td *eventq_extract(void);

/**
 * @brief Process the event in the queue
 *
 * Extracts and dispatches every event in the queue until it is empty.
 * Each event is routed to the appropriate handler based on its action
 * type.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to process an event
 *
 * @note Complexity: @e O(n), where @e n is the number of events
 */
int eventq_process(void);


#endif  /* ! EVENTQ_H */
