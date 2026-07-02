/**
 * @file eventq.c
 *
 * @brief Event priority queue (min-heap) handler function
 *        implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdlib.h>     /* NULL, free, malloc */
#include <pthread.h>    /* pthread_t, pthread_create, pthread_join */
#include <time.h>       /* nanosleep */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/pqueue.h> /* Priority queue (as a heap) */

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Commands includes */
#include <cmds/ccmd.h>

/* Local includes */
#include <eventq.h>


/**
 * @brief Pointer to the singleton instance of the event priority queue
 *
 * Event priority queue defined over a heap data structure and organized
 * as a min-heap, using it as a tree where the value of the root node
 * must be the smallest among all its descendant nodes and the same
 * thing must be done for its left and right sub-tree also.  In other
 * words, it's a bottom-heavy heap, where in this case, it's distributed
 * by priority, where the highest priority corresponds to the smallest
 * value.
 */
static pqueue_td *eventq = NULL;                /* Event priority queue
                                                   (min-heap;
                                                   heavy-bottom) */

/**
 * @brief Holds the identifier of the thread that is responsible for
 *        processing events from the event queue
 *
 * @note Its value is assigned when the thread is created
 */
static pthread_t event_thread;                  /* Thread identifier for
                                                   the event processing
                                                   thread */

/**
 * @brief Manage the running state of the event processing thread
 *
 * @note It should be set to @c true to start processing events
 *       (@a eventq_start) and @c false to stop it (@a eventq_stop)
 */
static volatile bool eventq_is_running = false;  /* Running state of the
                                                    event processing
                                                    thread */


/**
 * @brief Thread function to process events from the event queue
 *
 * Checks continuously if the event queue is running.  If there are
 * events in the queue, it processes them using the @a eventq_process
 * function.  If the queue is empty, it will suspend the thread for
 * a specified amount of time to avoid wasting CPU cycles.
 *
 * @param arg Unused argument, can be used to pass data to the thread
 *
 * @return @c NULL, since this is intended to be run as a thread
 *
 * @note The function assumes that @p eventq_is_running is managed
 *       externally to safely start and stop the processing loop.
 * @note Complexity: @e O(n), where @e n is number of events being
 *       processed in @a eventq_process, however, it depends on the size
 *       of the event queue when events are presented for processings
 *
 * @see @a evetnq_process
 */
static void *eventq_process_thread(void *arg)
{
    while (eventq_is_running) {
        if (pqueue_size(eventq) > 0) {
            eventq_process();   /* Process events from the queue */
        } else {
            /* Sleep to prevent busy-waiting and reduce CPU usage when
             * there are no events to process */
            struct timespec req;
            req.tv_sec = 0;
            req.tv_nsec = EVENTQ_PROCESSING_SLEEP_NANOSECONDS;
            nanosleep(&req, NULL);
        }
    }

    return NULL;
}


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
        return -1;  /* pri(evt_1) > pri(evt_2) */
    } else if (event1->priority > event2->priority) {
        return 1;   /* pri(evt_2) < pri(evt_1) */
    } else {
        return 0;   /* pri(evt_1) == pri(evt_2) */
    }
}


/* Handle client events */
static void s_event_handle_client(event_td *event)
{
    client_td *client;
    action_data_client_td *client_data;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' client event to process in" \
                " event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_CLIENT) {
        return; /* Invalid type */
    }

    if (event->action.object.client < ACTION_CLIENT_MIN ||
            event->action.object.client > ACTION_CLIENT_MAX) {
        return; /* Invalid action */
    }

    /* Point to the actual client and its data if needed */
    client = (client_td *) event->object;
    client_data = (action_data_client_td *) event->data;

    switch (event->action.object.client) {
        case ACTION_CLIENT_CREATE:
            /* This creates the client in 'client->xclient', but does
             * not allocates the memory of the client object.  This
             * action is intended to be called by the desktop,
             * therefore, it's responsibility of the desktop to execute
             * this action after invoking 'client_create'. */
            // XCreateWindow...
            //XMapWindow(client->connection, client->xclient);

            //client_set_hidden(client);
            //client.properties.state = CLIENT_STATE_NORMAL;
            break;

        case ACTION_CLIENT_CLOSE:
            wcmd_client_close(client);
            break;

        case ACTION_CLIENT_RESTORE:
            wcmd_client_restore(client);
            break;

        case ACTION_CLIENT_FOCUS:
            wcmd_client_focus(client);
            break;

        case ACTION_CLIENT_UNFOCUS:
            wcmd_client_unfocus(client);
            break;

        case ACTION_CLIENT_MOVE:
            wcmd_client_move(client, client_data);
            break;

        case ACTION_CLIENT_RESIZE:
            wcmd_client_resize(client, client_data);
            break;

        case ACTION_CLIENT_RENAME:
            wcmd_client_rename(client, client_data);
            break;

        case ACTION_CLIENT_RECLASS:
            wcmd_client_reclass(client, client_data);
            break;

        case ACTION_CLIENT_REROLE:
            wcmd_client_rerole(client, client_data);
            break;

        case ACTION_CLIENT_MAXIMIZE:
            wcmd_client_maximize(client);
            break;

        case ACTION_CLIENT_MAXIMIZE_HORZ:
            wcmd_client_maximize_horz(client);
            break;

        case ACTION_CLIENT_MAXIMIZE_VERT:
            wcmd_client_maximize_vert(client);
            break;

        case ACTION_CLIENT_ICONIFY:
            wcmd_client_iconify(client);
            break;

        case ACTION_CLIENT_HIDE:
            wcmd_client_hide(client);
            break;

        case ACTION_CLIENT_UNHIDE:
            wcmd_client_unhide(client);
            break;

        case ACTION_CLIENT_SHADE:
            wcmd_client_shade(client);
            break;

        case ACTION_CLIENT_UNSHADE:
            wcmd_client_unshade(client);
            break;

        case ACTION_CLIENT_TOGGLE_SHADE:
            wcmd_client_toggle_shade(client);
            break;

        case ACTION_CLIENT_STICKY:
            wcmd_client_sticky(client);
            break;

        case ACTION_CLIENT_UNSTICKY:
            wcmd_client_unsticky(client);
            break;

        case ACTION_CLIENT_TOGGLE_STICKY:
            wcmd_client_toggle_sticky(client);
            break;

        case ACTION_CLIENT_FULLSCREEN:
            wcmd_client_fullscreen(client);
            break;

        case ACTION_CLIENT_UNFULLSCREEN:
            wcmd_client_unfullscreen(client);
            break;

        case ACTION_CLIENT_TOGGLE_FULLSCREEN:
            wcmd_client_toggle_fullscreen(client);
            break;

        case ACTION_CLIENT_RAISE:
            wcmd_client_raise(client);
            break;

        case ACTION_CLIENT_LOWER:
            wcmd_client_lower(client);
            break;

        case ACTION_CLIENT_LAYER_ABOVE:
            wcmd_client_layer_above(client);
            break;

        case ACTION_CLIENT_LAYER_NORMAL:
            wcmd_client_layer_normal(client);
            break;

        case ACTION_CLIENT_LAYER_BELOW:
            wcmd_client_layer_below(client);
            break;

        case ACTION_CLIENT_SET_URGENT:
            wcmd_client_set_urgent(client);
            break;

        case ACTION_CLIENT_CLEAR_URGENT:
            wcmd_client_clear_urgent(client);
            break;

        case ACTION_CLIENT_SET_ICON:
            wcmd_client_set_icon(client, client_data);
            break;
    }

    xcb_flush(client->connection);
    event_destroy(event);
}


/* Handle desktop events */
static void s_event_handle_desktop(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' desktop event to process in" \
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

        case ACTION_DESKTOP_CLIENT_ADD:
            break;

        case ACTION_DESKTOP_CLIENT_REMOVE:
            break;

        case ACTION_DESKTOP_CLIENT_SEND:
            break;

        case ACTION_DESKTOP_CLIENT_CLONE:
            break;

        case ACTION_DESKTOP_CLIENT_SEND_FRONT:
            break;

        case ACTION_DESKTOP_CLIENT_SEND_BACK:
            break;

        case ACTION_DESKTOP_CLIENTS_REARRANGE:
            break;

        case ACTION_DESKTOP_CLIENTS_ICONIFY_ALL:
            break;

        case ACTION_DESKTOP_CYCLE_CLIENTS_ACTIVE:
            break;

        case ACTION_DESKTOP_CYCLE_CLIENTS_ICONS:
            break;

        case ACTION_DESKTOP_LOCK:
            break;

        case ACTION_DESKTOP_UNLOCK:
            break;

        case ACTION_DESKTOP_SET_LAYOUT:
            break;

        case ACTION_DESKTOP_COMMAND_LAUNCH:
            break;

        case ACTION_DESKTOP_PROCESS_KILL:
            break;
    }

    event_destroy(event);
}


/* Handle screen events */
static void s_event_handle_screen(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' screen event to process in" \
                " event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_SURFACE) {
        return; /* Invalid type */
    }

    if (event->action.object.surface < ACTION_SURFACE_MIN ||
        event->action.object.surface > ACTION_SURFACE_MAX) {
        return; /* Invalid action */
    }

    switch (event->action.object.surface) {
        case ACTION_SURFACE_DESKTOP_ADD:
            break;

        case ACTION_SURFACE_DESKTOP_REMOVE:
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH:
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH_NEXT:
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH_PREV:
            break;

        case ACTION_SURFACE_TOGGLE_FULLSCREEN:
            break;

        case ACTION_SURFACE_SET_RESOLUTION:
            break;

        case ACTION_SURFACE_SET_ORIENTATION:
            break;

        case ACTION_SURFACE_SET_BRIGHTNESS:
            break;

        case ACTION_SURFACE_SET_CONTRAST:
            break;

        case ACTION_SURFACE_CONFIGURE_SETTINGS:
            break;
    }

    event_destroy(event);
}


/* Handle client manager events */
static void s_event_handle_wm(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' manager event to process in" \
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

        case ACTION_SURFACE_ADD:
            break;

        case ACTION_SURFACE_REMOVE:
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

        LOGGER_TRACE("Starting event thread", L_NARG);
        eventq_is_running = true;
        if (pthread_create(&event_thread, NULL,
                    eventq_process_thread, NULL) != 0) {
            LOGGER_FATAL("Failed to create event thread", L_NARG);
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

    eventq_is_running = false;
    LOGGER_TRACE("Waiting for the event thread to finish", L_NARG);
    pthread_join(event_thread, NULL);

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
        processed_event = eventq_extract();
        if (processed_event != 0) {
            LOGGER_WARNING("Failed to process event", L_NARG);
            return 1;
        }

        /* Handle each type of event */
        switch (processed_event->action.type) {
            case ACTION_TYPE_CLIENT:
                s_event_handle_client(processed_event);
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
    }

    return 0;
}
