/**
 * @file eventq.h
 *
 * @brief Event priority queue (min-heap) handler function declaration
 */

#ifndef EVENTQ_H
#define EVENTQ_H


/* Project includes */
#include <event.h>


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
