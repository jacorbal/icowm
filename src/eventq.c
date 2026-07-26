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

#define _POSIX_C_SOURCE 200112L /* nanosleep (199309L would suffice) */


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
#include <cmds/dcmd.h>
#include <cmds/scmd.h>
//#include <cmds/wmcmd.h>

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
 * @brief Mutex used to synchronize access to the event queue
 *
 * Protects operations on @a eventq to prevent race conditions when
 * multiple threads access or modify the queue concurrently.
 *
 * @note It must be locked before accessing the queue and unlocked
 *       immediately after the operation
 */
static pthread_mutex_t eventq_mutex = PTHREAD_MUTEX_INITIALIZER;


/**
 * @brief Retrieve current size of the event queue in a thread-safe way
 *
 * Accesses @a eventq while holding @a eventq_mutex to ensure
 * consistency when other threads may be modifying the queue.
 *
 * @return Number of events currently in the queue, or 0 if the queue is
 *         not initialized, (@c NULL)
 *
 * @note This function is thread-safe due to the use of a mutex
 * @note Complexity depends on @a pqueue_size implementation, typically
 *       @e O(1)
 */
static size_t s_eventq_size(void)
{
    size_t size = 0;

    pthread_mutex_lock(&eventq_mutex);
    if (eventq != NULL) {
        size = pqueue_size(eventq);
    }
    pthread_mutex_unlock(&eventq_mutex);

    return size;
}


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
 * @see @a eventq_process
 */
static void *eventq_process_thread(void *arg)
{
    (void) arg;

    while (eventq_is_running) {
        if (s_eventq_size() > 0) {
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


/**
 * @brief Handle client events
 *
 * Process client-related events by dispatching them to appropiate
 * command handlers based on the action type.
 *
 * @param event Pointer to the client event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_client(event_td *event)
{
    desktop_td *desktop = NULL;
    client_td *client;
    action_data_client_td *client_data;


    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' client event to process" \
                " in event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_CLIENT) {
        event_destroy(event);
        return; /* Invalid type */
    }

    if (event->action.object.client < ACTION_CLIENT_MIN ||
            event->action.object.client > ACTION_CLIENT_MAX) {
        LOGGER_WARNING("Invalid client action type: %d",
                event->action.object.client);
        event_destroy(event);
        return; /* Invalid action */
    }

    /* Point to the actual client and its data if needed */
    client = (client_td *) event->object;
    client_data = (action_data_client_td *) event->data;

    if (client == NULL) {
        LOGGER_ERROR("Received 'NULL' client object in event", L_NARG);
        event_destroy(event);
        return;
    }

    LOGGER_TRACE("Processing client event: action=%d, client=%p",
            event->action.object.client, (void *) client);

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

        case ACTION_CLIENT_KILL:
            wcmd_client_kill(client);
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

        case ACTION_CLIENT_CENTER:
            wcmd_client_center(client);
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

        case ACTION_CLIENT_CYCLE_NEXT:
            /* Find the desktop owning this client */

            /* NOTE: We traverse surfaces/desktops to find the owner */
            /* For now dispatch via desktop action */
            (void) desktop;
            /* Cycle is dispatched as a desktop event from 'wm.c' */
            break;

        case ACTION_CLIENT_CYCLE_PREV:
            break;

        case ACTION_CLIENT_TOGGLE_DECORATION:
            wcmd_client_toggle_decoration(client);
            break;
    }

    xcb_flush(client->connection);
    event_destroy(event);
}


/**
 * @brief Handle desktop events
 *
 * Process desktop-related events by dispatching them to appropiate
 * command handlers based on the action type.
 *
 * @param event Pointer to the desktop event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_desktop(event_td *event)
{
    desktop_td *desktop;
    action_data_desktop_td *desktop_data;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' desktop event to process" \
                " in event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_DESKTOP) {
        event_destroy(event);
        return; /* Invalid type */
    }

    if (event->action.object.desktop < ACTION_DESKTOP_MIN ||
        event->action.object.desktop > ACTION_DESKTOP_MAX) {
        LOGGER_WARNING("Invalid desktop action type: %d",
                event->action.object.desktop);
        event_destroy(event);
        return; /* Invalid action */
    }

    /* Point to the actial desktop and its data if needed */
    desktop = (desktop_td *) event->object;
    desktop_data = (action_data_desktop_td *) event->data;

    if (desktop == NULL) {
        LOGGER_ERROR("Received 'NULL' desktop object in event", L_NARG);
        event_destroy(event);
        return;
    }

    LOGGER_TRACE("Processing desktop event: action=%d, desktop=%p",
            event->action.object.desktop, (void *) desktop);

    switch (event->action.object.desktop) {
        case ACTION_DESKTOP_RENAME:
            dcmd_desktop_rename(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_SET_BACKGROUND:
            dcmd_desktop_bg_color(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLEAR:
            dcmd_desktop_clear(desktop);
            break;

        case ACTION_DESKTOP_CLIENT_ADD:
            dcmd_desktop_client_add(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_REMOVE:
            dcmd_desktop_client_rem(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_SEND:
            dcmd_desktop_client_send(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_CLONE:
            dcmd_desktop_client_send(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_SEND_FRONT:
            dcmd_desktop_client_send_front(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENT_SEND_BACK:
            dcmd_desktop_client_send_back(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_CLIENTS_REARRANGE:
            dcmd_desktop_clients_rearrange(desktop);
            break;

        case ACTION_DESKTOP_CLIENTS_ICONIFY_ALL:
            dcmd_desktop_clients_iconify_all(desktop);
            break;

        case ACTION_DESKTOP_CYCLE_CLIENTS_PREV:
            break;

        case ACTION_DESKTOP_CYCLE_CLIENTS_ACTIVE:
            dcmd_desktop_clients_cycle_active(desktop);
            break;

        case ACTION_DESKTOP_CYCLE_CLIENTS_ICONS:
            dcmd_desktop_clients_cycle_icons(desktop);
            break;

        case ACTION_DESKTOP_LOCK:
            dcmd_desktop_lock(desktop);
            break;

        case ACTION_DESKTOP_UNLOCK:
            dcmd_desktop_unlock(desktop);
            break;

        case ACTION_DESKTOP_SET_LAYOUT:
            dcmd_desktop_layout(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_COMMAND_LAUNCH:
            dcmd_desktop_process_launch(desktop, desktop_data);
            break;

        case ACTION_DESKTOP_PROCESS_KILL:
            dcmd_desktop_process_kill(desktop, desktop_data);
            break;
    }

    event_destroy(event);
}


/**
 * @brief Handle surface events
 *
 * Process surface-related events by dispatching them to appropiate
 * command handlers based on the action type.
 *
 * @param event Pointer to the surface event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_surface(event_td *event)
{
    surface_td *surface;
    action_data_surface_td *surface_data;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' screen event to process in" \
                " event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_SURFACE) {
        event_destroy(event);
        return; /* Invalid type */
    }

    if (event->action.object.surface < ACTION_SURFACE_MIN ||
        event->action.object.surface > ACTION_SURFACE_MAX) {
        LOGGER_WARNING("Invalid surface action type: %d",
                event->action.object.surface);
        event_destroy(event);
        return; /* Invalid action */
    }

    /* Point to the actual surface and its data if needed */
    surface = (surface_td *) event->object;
    surface_data = (action_data_surface_td *) event->data;

    if (surface == NULL) {
        LOGGER_ERROR("Received 'NULL' surface object in event", L_NARG);
        event_destroy(event);
        return;
    }


    LOGGER_TRACE("Processing surface event: action=%d, surface=%p",
            event->action.object.surface, (void *) surface);

    switch (event->action.object.surface) {
        case ACTION_SURFACE_DESKTOP_ADD:
            scmd_surface_desktop_add(surface, surface_data);
            break;

        case ACTION_SURFACE_DESKTOP_REMOVE:
            scmd_surface_desktop_rem(surface, surface_data);
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH:
            scmd_surface_desktop_switch(surface, surface_data);
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH_NEXT:
            scmd_surface_desktop_switch_next(surface);
            break;

        case ACTION_SURFACE_DESKTOP_SWITCH_PREV:
            scmd_surface_desktop_switch_prev(surface);
            break;

        case ACTION_SURFACE_TOGGLE_FULLSCREEN:
            scmd_surface_toggle_fullscreen(surface);
            break;

        case ACTION_SURFACE_SET_RESOLUTION:
            scmd_surface_set_resolution(surface, surface_data);
            break;

        case ACTION_SURFACE_SET_ORIENTATION:
            scmd_surface_set_orientation(surface, surface_data);
            break;

        case ACTION_SURFACE_SET_BRIGHTNESS:
            scmd_surface_set_brightness(surface, surface_data);
            break;

        case ACTION_SURFACE_SET_CONTRAST:
            scmd_surface_set_contrast(surface, surface_data);
            break;

        case ACTION_SURFACE_CONFIGURE_SETTINGS:
            scmd_surface_configure_settings(surface, surface_data);
            break;
    }

    event_destroy(event);
}


/**
 * @brief Handle window manager events
 *
 * Process window manager -related events by dispatching them to
 * appropiate command handlers based on the action type.
 *
 * @param event Pointer to the window manager event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_wm(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' window manager event to process" \
                " in event queue", L_NARG);
        return;
    }

    if (event->action.type != ACTION_TYPE_WM) {
        event_destroy(event);
        return; /* Invalid type */
    }

    if (event->action.object.wm < ACTION_WM_MIN ||
            event->action.object.wm > ACTION_WM_MAX) {
        LOGGER_WARNING("Invalid window manager action type: %d",
                event->action.object.wm);
        event_destroy(event);
        return; /* Invalid action */
    }

    LOGGER_TRACE("Processing window manager event: action=%d",
            event->action.object.wm);

    switch (event->action.object.wm) {
        case ACTION_WM_CONFIGURATION_RELOAD:
            LOGGER_TRACE("Window manager configuration reload requested",
                    L_NARG);
            break;

        case ACTION_WM_CONFIGURATION_SAVE:
            LOGGER_TRACE("Window manager configuration save requested",
                    L_NARG);
            break;

        case ACTION_SURFACE_ADD:
            LOGGER_TRACE("Surface addition requested", L_NARG);
            break;

        case ACTION_SURFACE_REMOVE:
            LOGGER_TRACE("Surface removal requested", L_NARG);
            break;

        case ACTION_WM_EXIT:
            LOGGER_TRACE("Window manager exit action requested", L_NARG);
            wm_request_stop();
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
            pqueue_destroy(eventq);
            eventq = NULL;
            eventq_is_running = false;
            return 1;
        }
        return 0;
    }

    return -1;
}


/* Deallocate memory used by the event priority queue */
int eventq_stop(void)
{
    LOGGER_DEBUG("Deallocating priority queue for events", L_NARG);
    if (eventq == NULL) {
        return 1;
    }

    eventq_is_running = false;
    if (pthread_equal(pthread_self(), event_thread)) {
        LOGGER_WARNING("Skipping self-join in event thread; defer stop"
                " to main thread", L_NARG);
        return 0;
    }

    LOGGER_TRACE("Waiting for the event thread to finish", L_NARG);
    pthread_join(event_thread, NULL);

    pthread_mutex_lock(&eventq_mutex);
    pqueue_destroy(eventq);
    eventq = NULL;  /* Reset the singleton instance pointer to 'NULL' */
    pthread_mutex_unlock(&eventq_mutex);

    return 0;
}


/* Add a event to the event priority queue */
int eventq_add(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Attempted to add 'NULL' event to queue", L_NARG);
        return 1;
    }

    pthread_mutex_lock(&eventq_mutex);
    LOGGER_TRACE("Inserting event into event queue", L_NARG);
    if (eventq == NULL || pqueue_insert(eventq, (void *) event) != 0) {
        pthread_mutex_unlock(&eventq_mutex);
        LOGGER_WARNING("Failed to insert event into event queue",
                L_NARG);

        /* Release the event to prevent a memory leak: ownership of
         * 'event' was transferred to this function, so it must be
         * freed on failure since it will never be dequeued */
        event_destroy(event);
        return 1;
    }
    pthread_mutex_unlock(&eventq_mutex);

    return 0;
}


/* Dequeue event from priority queue */
event_td *eventq_extract(void)
{
    event_td *event;

    pthread_mutex_lock(&eventq_mutex);
    LOGGER_TRACE("Extracting event from event queue", L_NARG);
    if (eventq == NULL || pqueue_size(eventq) == 0 ||
            pqueue_extract(eventq, (void **) &event) != 0) {
        pthread_mutex_unlock(&eventq_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&eventq_mutex);

    return event;
}


/* Process events in event queue */
int eventq_process(void)
{
    /* Process 'eventq' events */
    event_td *processed_event;

    /* Loop until the queue is empty; 'eventq_extract' handles sync. */
    while (true) {
        processed_event = eventq_extract();
        if (processed_event == NULL) {
            break;
        }

        /* Handle each type of event by dispatching to the appropiate
         * handler function */
        switch (processed_event->action.type) {
            case ACTION_TYPE_CLIENT:
                s_event_handle_client(processed_event);
                break;
            case ACTION_TYPE_DESKTOP:
                s_event_handle_desktop(processed_event);
                break;
            case ACTION_TYPE_SURFACE:
                s_event_handle_surface(processed_event);
                break;
            case ACTION_TYPE_WM:
                s_event_handle_wm(processed_event);
                break;
        }
    }

    return 0;
}
