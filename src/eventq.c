/**
 * @file eventq.c
 *
 * @brief Event priority queue (min-heap) handler function
 *        implementation
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* X11 includes */
#include <X11/Xlib.h>   /* XEvent */
#include <X11/Xatom.h>
//#include <X11/keysym.h> /* XK_* */

/* ADT includes */
#include <adt/pqueue.h> /* Priority queue (as a heap) */

/* Project includes */
#include <action.h>
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


/* Handle window events */
static void s_event_handle_window(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' event to process in event queue",
                L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_WINDOW) {
        return; /* Invalid type */
    }

    if (event->action.object.window < ACTION_WINDOW_MIN ||
        event->action.object.window > ACTION_WINDOW_MAX) {
        return; /* Invalid action */
    }

    switch (event->action.object.window) {
        case ACTION_WINDOW_CREATE:
            break;

        case ACTION_WINDOW_CLOSE:
            break;

        case ACTION_WINDOW_RESTORE:
            break;

        case ACTION_WINDOW_FOCUS:
            break;

        case ACTION_WINDOW_UNFOCUS:
            break;

        case ACTION_WINDOW_RESIZE:
            break;

        case ACTION_WINDOW_MOVE:
            break;

        case ACTION_WINDOW_RENAME:
            break;

        case ACTION_WINDOW_RECLASS:
            /* TODO */
            break;

        case ACTION_WINDOW_MAXIMIZE:
            break;

        case ACTION_WINDOW_MAXIMIZE_HORZ:
            break;

        case ACTION_WINDOW_MAXIMIZE_VERT:
            break;

        case ACTION_WINDOW_ICONIFY:
            break;

        case ACTION_WINDOW_STICKY:
            /* TODO */
            break;

        case ACTION_WINDOW_UNSTICKY:
            /* TODO */
            break;

        case ACTION_WINDOW_TOGGLE_STICKY:
            /* TODO */
            break;

        case ACTION_WINDOW_FULLSCREEN:
            /* TODO */
            break;

        case ACTION_WINDOW_UNFULLSCREEN:
            /* TODO */
            break;

        case ACTION_WINDOW_TOGGLE_FULLSCREEN:
            /* TODO */
            break;

        case ACTION_WINDOW_RAISE:
            /* TODO */
            XRaiseWindow(((window_td *) event->object)->display,
                    ((window_td *) event->object)->window);
            break;

        case ACTION_WINDOW_LOWER:
            /* TODO */
            XLowerWindow(((window_td *) event->object)->display,
                    ((window_td *) event->object)->window);
            break;

        case ACTION_WINDOW_LAYER_ON_TOP:
            /* TODO */
            break;

        case ACTION_WINDOW_LAYER_NORMAL:
            /* TODO */
            break;

        case ACTION_WINDOW_LAYER_ON_BOTTOM:
            /* TODO */
            break;

        case ACTION_WINDOW_SET_URGENT:
            /* TODO */
            break;

        case ACTION_WINDOW_SET_ICON:
            /* TODO */
            break;
    }

    event_destroy(event);
}


/* Handle desktop events */
static void s_event_handle_desktop(event_td *event)
{
}


/* Handle screen events */
static void s_event_handle_screen(event_td *event)
{
}


/* Handle window manager events */
static void s_event_handle_wm(event_td *event)
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


/* Dequeue event from priority queue */
event_td *eventq_extract(void)
{
    event_td *event;

    LOGGER_TRACE("Extracting event from event queue", L_NARG);
    if (pqueue_extract(eventq, (void **) &event) != 0) {
        LOGGER_WARNING("Failed to extract event from event queue",
                L_NARG);
        return NULL;
    }

    return event;
}


/* Process events in event queue */
int eventq_process(void)
{
    /* Process 'eventq' events */
    event_td *processed_event;
    while (pqueue_size(eventq) > 0) {
        /* pqueue_extract(eventq, (void **) &processed_event); */
        processed_event = eventq_extract();
        if (processed_event != 0) {
            LOGGER_WARNING("Failed to process event", L_NARG);
            return 1;
        }

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
                s_event_handle_wm(processed_event);
                break;
        }

        /* Deallocate processed event */
        event_destroy(processed_event);
    }

    return 0;
}
