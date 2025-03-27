/**
 * @file eventq.c
 *
 * @brief Event priority queue (min-heap) handler function
 *        implementation
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* External libraries */
#include <X11/Xlib.h>   /* XEvent */
//#include <X11/keysym.h> /* XK_* */

/* ADT includes */
#include <adt/pqueue.h> /* Priority queue (as a heap) */

/* Project includes */
#include <actions.h>
#include <desktop.h>
#include <logger.h>
#include <screen.h>
#include <window.h>

/* Local includes */
#include <eventq.h>


/* This is the event priority queue defined over a heap data structure
 * and organized as a min-heap, using it as a tree where the value of
 * the root node must be the smallest among all its descendant nodes and
 * the same thing must be done for its left and right sub-tree also.  In
 * other words, it's a bottom-heavy heap, where in this case, it's
 * distributed by priority, where the highest priority corresponds to
 * the smallest value. */
static pqueue_td *eventq = NULL;   /**< Pointer to the singleton
                                        instance of the event priority
                                        queue (min-heap; heavy-bottom) */


/**
 * @brief Compare data of two events based on their priority
 *
 * Determines the order of events in a priority queue, allowing events
 * with lower priority values (more negative) to be considered of higher
 * priority.
 *
 * @param e1 Pointer to the first event
 * @param e2 Pointer to the second event
 *
 * @retval -1 @p event1 has greater priority (lower value) than @p event2
 * @retval  1 @p event1 has lower priority (higher value) than @p event2
 * @retval  0 Both events have equal priority
 *
 * @note A negative priority value indicates a higher importance
 * @note Complexity: @e O(1), as it performs a constant number of
 *       comparisons between the two priority values
 */
static int s_event_compare(const void *e1, const void *e2)
{
    const event_td *event1 = (const event_td *) e1;
    const event_td *event2 = (const event_td *) e2;

    if (event1->priority < event2->priority) {
        return -1;  /* PRIO(event1) > PRIO(event2) */
    } else if (event1->priority > event2->priority) {
        return 1;   /* PRIO(event1) < PRIO(event2) */
    } else {
        return 0;   /* PRIO(event1) == PRIO(event2) */
    }
}


/* */
static void s_event_handle_window(event_td *event)
{
}


/* */
static void s_event_handle_desktop(event_td *event)
{
}


/* */
static void s_event_handle_screen(event_td *event)
{
}


/* Start the priority queue (as min-heap) to handle events */
int eventq_start(void)
{
    LOGGER_DEBUG("Initializing priority queue for events", L_NARG);
    if (eventq == NULL) {
        eventq = pqueue_init(s_event_compare,
                (void (*)(void *)) event_destroy);
        if (eventq == NULL) {
            LOGGER_FATAL("Failed to initialize event priority queue",
                    L_NARG);
            return 1;
        }
        return 0;
    }

    return -1;
}


/* Deallocate memory for the event priority queue */
int eventq_stop(void)
{
    LOGGER_DEBUG("Deallocating priority queue for events", L_NARG);
    if (eventq == NULL) {
        return 1;
    }

    pqueue_destroy(eventq);
    eventq = NULL;  /* Reset the singleton instance pointer to 'NULL' */

    return 0;
}


/* Initializes a new event structure */
event_td *event_init(void *object,
        action_td action,
        enum event_priority_e priority)
{
    event_td *event;

    LOGGER_TRACE("Initializing event data structure", L_NARG);
    event = malloc(sizeof(event_td));
    if (event == NULL) {
        LOGGER_WARNING("Failed to allocate memory for event data" \
                "structure", L_NARG);
        return NULL;
    }

    event->action = action;
    event->priority = priority;

    switch (event->action.type) {
        case ACTION_TYPE_WINDOW:
            event->object.window = (window_td *) object;
            break;

        case ACTION_TYPE_DESKTOP:
            event->object.desktop = (desktop_td *) object;
            break;

        case ACTION_TYPE_SCREEN:
            event->object.screen = (screen_td *) object;
            break;

        case ACTION_TYPE_WM:
            break;
    }

    return event;
}


/* Deallocate memory for an event structure */
void event_destroy(event_td *event)
{
    LOGGER_TRACE("Destroying event data structure", L_NARG);
    if (event != NULL) {
        free(event);
    }
}


/* Add a event to the event priority queue */
int eventq_add(event_td *event)
{
    LOGGER_TRACE("Inserting event into event queue", L_NARG);
    if (pqueue_insert(eventq, (void *) event) != 0){
        LOGGER_WARNING("Failed to insert event into event queue",
                L_NARG);
        return 1;
    }

    return 0;
}


/* Process events in event queue */
void eventq_process(void)
{
    /* Process 'eventq' events */
    event_td *processed_event;
    while (pqueue_size(eventq) > 0) {
        pqueue_extract(eventq, (void **) &processed_event);

        /* Handle each type of event */
        switch (processed_event->action.type) {
            case ACTION_TYPE_WINDOW:
                s_event_handle_window(processed_event);
                break;
            case ACTION_TYPE_DESKTOP:
                s_event_handle_desktop(processed_event);
                break;
            case ACTION_TYPE_SCREEN:
                s_event_handle_screen(processed_event);
                break;
            case ACTION_TYPE_WM:
                break;
        }

        /* Deallocate processed event */
        event_destroy(processed_event);
    }
}
