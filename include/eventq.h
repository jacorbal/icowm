/**
 * @file eventq.h
 *
 * @brief Event priority queue (min-heap) handler function declaration
 *
 * This module provides an interface for handling an event priority
 * queue, implemented as a min-heap.  It allows for asynchronous event
 * processing through a dedicated thread that continuously monitors the
 * queue for events to handle.  The processing occurs independently of
 * the main application flow, enabling concurrent execution and ensuring
 * that the application remains responsive even when handling
 * long-running or complex events.
 *
 * @note The event processing thread operates on a separate execution
 *       context and may introduce delays when idle to minimize CPU
 *       usage
 */

#ifndef EVENTQ_H
#define EVENTQ_H


/* Project includes */
#include <event.h>


/**
 * @brief Duration (in nanoseconds) for which the event processing
 *        thread sleeps when there are no events to process
 *
 * This helps avoid busy-waiting and reduces CPU usage by introducing
 * a small delay during idle periods.
 */
#define EVENTQ_PROCESSING_SLEEP_NANOSECONDS (100000000) /* 100 ms */


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
 * @retval  1 Failed to insert the event into the event queue @p eventq
 *
 * @note Complexity: @e O(1)
 */
int eventq_add(event_td *event);

/**
 * @brief Extract an event at from the priority queue
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
 * @note Complexity: @e O(log n), where @e n is the number of events to
 *       process
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to process the event
 */
int eventq_process(void);


#endif  /* ! EVENTQ_H */
