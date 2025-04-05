/**
 * @file eventq.c
 *
 * @brief Event priority queue (min-heap) handler function
 *        implementation
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* X11 includes */
#include <X11/Xlib.h>
#include <X11/Xatom.h>  // TODO: Check if this is needed when finish

/* ADT includes */
#include <adt/pqueue.h> /* Priority queue (as a heap) */

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Project includes */
#include <action.h>
#include <actdata.h>
#include <cmds/wcmd.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <window.h>
#include <wm.h>

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
        return -1;  /* pr(event1) -gt pr(event2) */
    } else if (event1->priority > event2->priority) {
        return 1;   /* pr(event1) -lt pr(event2) */
    } else {
        return 0;   /* pr(event1) -eq p(event2) */
    }
}


/* Handle window events */
static void s_event_handle_window(event_td *event)
{
    window_td *window;
    action_data_window_td *window_data;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' window event to process in " \
                " event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_WINDOW) {
        return; /* Invalid type */
    }

    if (event->action.object.window < ACTION_WINDOW_MIN ||
            event->action.object.window > ACTION_WINDOW_MAX) {
        return; /* Invalid action */
    }

    /* Point to the actual window and its data if needed */
    window = (window_td *) event->object;
    window_data = (action_data_window_td *) event->data;

    switch (event->action.object.window) {
        case ACTION_WINDOW_CREATE:
            /* This creates the window in window->xwindow, but does not
             * allocates the memory of the window object.  This action
             * is intended to be called by the desktop, therefore, it's
             * responsibility of the desktop to execute this action
             * after invoking 'window_create' */
            // XCreateWindow...
            XMapWindow(window->display, window->xwindow);

            //window_set_hidden(window);
            //window.properties.state = WINDOW_STATE_NORMAL;
            break;

        case ACTION_WINDOW_CLOSE:
            wcmd_window_close(window);
            break;

        case ACTION_WINDOW_RESTORE:
            wcmd_window_restore(window);
            break;

        case ACTION_WINDOW_FOCUS:
            wcmd_window_focus(window);
            break;

        case ACTION_WINDOW_UNFOCUS:
            wcmd_window_unfocus(window);
            break;

        case ACTION_WINDOW_MOVE:
            wcmd_window_move(window, window_data);
            break;

        case ACTION_WINDOW_RESIZE:
            wcmd_window_resize(window, window_data);
            break;

        case ACTION_WINDOW_RENAME:
            wcmd_window_rename(window, window_data);
            break;

        case ACTION_WINDOW_RECLASS:
            wcmd_window_reclass(window, window_data);
            break;

        case ACTION_WINDOW_REROLE:
            //TODO
            safe_free((void **) &(window->class_name));
            window->class_name =
                safe_strdup(window_data->new_data.class_name);
            break;

        case ACTION_WINDOW_MAXIMIZE:
            wcmd_window_maximize(window);
            break;

        case ACTION_WINDOW_MAXIMIZE_HORZ:
            wcmd_window_maximize_horz(window);
            break;

        case ACTION_WINDOW_MAXIMIZE_VERT:
            wcmd_window_maximize_vert(window);
            break;

        case ACTION_WINDOW_ICONIFY:
            wcmd_window_iconify(window);
            break;

        case ACTION_WINDOW_HIDE:
            wcmd_window_hide(window);
            break;

        case ACTION_WINDOW_UNHIDE:
            wcmd_window_unhide(window);
            break;

        case ACTION_WINDOW_SHADE:
            wcmd_window_shade(window);
            break;

        case ACTION_WINDOW_UNSHADE:
            wcmd_window_unshade(window);
            break;

        case ACTION_WINDOW_TOGGLE_SHADE:
            wcmd_window_toggle_shade(window);
            break;

        case ACTION_WINDOW_STICKY:
            wcmd_window_sticky(window);
            break;

        case ACTION_WINDOW_UNSTICKY:
            wcmd_window_unsticky(window);
            break;

        case ACTION_WINDOW_TOGGLE_STICKY:
            wcmd_window_toggle_sticky(window);
            break;

        case ACTION_WINDOW_FULLSCREEN:
            wcmd_window_fullscreen(window);
            break;

        case ACTION_WINDOW_UNFULLSCREEN:
            wcmd_window_unfullscreen(window);
            break;

        case ACTION_WINDOW_TOGGLE_FULLSCREEN:
            wcmd_window_toggle_fullscreen(window);
            break;

        case ACTION_WINDOW_RAISE:
            wcmd_window_raise(window);
            break;

        case ACTION_WINDOW_LOWER:
            wcmd_window_lower(window);
            break;

        case ACTION_WINDOW_LAYER_ABOVE:
            wcmd_window_layer_above(window);
            break;

        case ACTION_WINDOW_LAYER_NORMAL:
            wcmd_window_layer_normal(window);
            break;

        case ACTION_WINDOW_LAYER_BELOW:
            wcmd_window_layer_below(window);
            break;

        case ACTION_WINDOW_SET_URGENT:
            wcmd_window_set_urgent(window);
            break;

        case ACTION_WINDOW_CLEAR_URGENT:
            wcmd_window_clear_urgent(window);
            break;

        case ACTION_WINDOW_SET_ICON:
            wcmd_window_set_icon(window, window_data);
            break;
    }

    XFlush(window->display);
    event_destroy(event);
}


/* Handle desktop events */
static void s_event_handle_desktop(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' desktop event to process in " \
                     " event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_DESKTOP) {
        return; /* Invalid type */
    }

    if (event->action.object.desktop < ACTION_DESKTOP_MIN ||
        event->action.object.desktop > ACTION_DESKTOP_MAX) {
        return; /* Invalid action */
    }

    switch (event->action.object.desktop) {
        case ACTION_DESKTOP_RENAME:
            break;

        case ACTION_DESKTOP_SET_BACKGROUND:
            break;

        case ACTION_DESKTOP_CLEAR:
            break;

        case ACTION_DESKTOP_WINDOW_ADD:
            break;

        case ACTION_DESKTOP_WINDOW_REMOVE:
            break;

        case ACTION_DESKTOP_WINDOW_SEND:
            break;

        case ACTION_DESKTOP_WINDOW_CLONE:
            break;

        case ACTION_DESKTOP_WINDOW_SEND_FRONT:
            break;

        case ACTION_DESKTOP_WINDOW_SEND_BACK:
            break;

        case ACTION_DESKTOP_WINDOWS_REARRANGE:
            break;

        case ACTION_DESKTOP_WINDOWS_ICONIFY_ALL:
            break;

        case ACTION_DESKTOP_CYCLE_WINDOWS_ACTIVE:
            break;

        case ACTION_DESKTOP_CYCLE_WINDOWS_ICONS:
            break;

        case ACTION_DESKTOP_LOCK:
            break;

        case ACTION_DESKTOP_UNLOCK:
            break;

        case ACTION_DESKTOP_SET_LAYOUT:
            break;

        case ACTION_DESKTOP_APPLICATION_LAUNCH:
            break;

        case ACTION_DESKTOP_APPLICATION_KILL:
            break;
    }

    event_destroy(event);
}


/* Handle screen events */
static void s_event_handle_screen(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' screen event to process in " \
                     " event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_SURFACE) {
        return; /* Invalid type */
    }

    if (event->action.object.surface < ACTION_SCREEN_MIN ||
        event->action.object.surface > ACTION_SCREEN_MAX) {
        return; /* Invalid action */
    }

    switch (event->action.object.surface) {
        case ACTION_SCREEN_DESKTOP_ADD:
            break;

        case ACTION_SCREEN_DESKTOP_REMOVE:
            break;

        case ACTION_SCREEN_DESKTOP_SWITCH:
            break;

        case ACTION_SCREEN_DESKTOP_SWITCH_NEXT:
            break;

        case ACTION_SCREEN_DESKTOP_SWITCH_PREV:
            break;

        case ACTION_SCREEN_TOGGLE_FULLSCREEN:
            break;

        case ACTION_SCREEN_SET_RESOLUTION:
            break;

        case ACTION_SCREEN_SET_ORIENTATION:
            break;

        case ACTION_SCREEN_SET_BRIGHTNESS:
            break;

        case ACTION_SCREEN_SET_CONTRAST:
            break;

        case ACTION_SCREEN_CONFIGURE_SETTINGS:
            break;
    }

    event_destroy(event);
}


/* Handle window manager events */
static void s_event_handle_wm(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' manager event to process in " \
                " event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_WM) {
        return; /* Invalid type */
    }

    if (event->action.object.wm < ACTION_WM_MIN ||
            event->action.object.wm > ACTION_WM_MAX) {
        return; /* Invalid action */
    }

    switch (event->action.object.wm) {
        case ACTION_WM_CONFIGURATION_RELOAD:
            break;

        case ACTION_WM_CONFIGURATION_SAVE:
            break;

        case ACTION_SCREEN_ADD:
            break;

        case ACTION_SCREEN_REMOVE:
            break;

        case ACTION_WM_EXIT:
            wm_stop();
            break;
    }

    event_destroy(event);
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
            case ACTION_TYPE_SURFACE:
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
