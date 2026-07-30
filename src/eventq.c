/**
 * @file eventq.c
 *
 * @brief Event queue (priority queue) handler function implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <pthread.h>    /* pthread_mutex_t, pthread_mutex_lock, _unlock */
#include <stdbool.h>
#include <stdlib.h>     /* NULL, free, malloc */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>   /* list_td (macro registry) */
#include <adt/pqueue.h> /* Priority queue (heap-backed) */
#include <adt/queue.h>  /* FIFO queue (recording & macro buffers) */

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <desktop.h>
#include <event.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Commands includes */
#include <cmds/ccmd.h>
#include <cmds/dcmd.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/meta.h>
#include <cmds/scmd.h>

/* Local includes */
#include <eventq.h>


/**
 * @brief Internal structure for a named event macro
 *
 * Stores a named sequence of cloned events that can be replayed any
 * number of times without consuming the original copies.
 */
typedef struct {
    char *name;         /**< Heap-allocated macro name */
    queue_td *events;   /**< FIFO queue of cloned @c event_td pointers */
} s_macro_td;


/**
 * @brief Singleton priority queue for events
 *
 * Events with a lower @c priority value (more negative / more urgent)
 * float to the top of the max-heap and are extracted first.
 */
static pqueue_td *eventq = NULL;

/**
 * @brief Mutex protecting @c eventq_add and @c eventq_extract
 *
 * Statically initialised so it is ready before @c eventq_start.  Allows
 * external threads to post events to the queue safely while the main
 * loop is processing X11 events.
 */
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Test-replay recording state */
static bool s_recording = false;        /**< Test-recording is active */
static queue_td *s_record_queue = NULL; /**< FIFO of cloned events */

/* Named macro state */
static list_td *s_macros = NULL;          /**< Registry of named macros */
static s_macro_td *s_macro_active = NULL; /**< Macro being recorded */


/**
 * @brief Compare two events by priority for the max-heap
 *
 * Returns +1 when @p a has higher urgency (lower priority value) than
 * @p b, so the heap root always holds the most urgent event.
 *
 * @param a First event pointer (cast to @c (const event_td *))
 * @param b Second event pointer (cast to @c (const event_td *))
 *
 * @return Comparison result
 * @retval  1 @p a is more urgent than @p b
 * @retval -1 @p b is more urgent than @p a
 * @retval  0 Equal urgency
 *
 * @note Complexity: @e O(1)
 */
static int s_event_compare(const void *a, const void *b)
{
    const event_td *ea = (const event_td *) a;
    const event_td *eb = (const event_td *) b;
    int pa = (int) ea->priority;
    int pb = (int) eb->priority;

    if (pa < pb) {
        return 1;   /* 'a' is more urgent */
    }
    if (pa > pb) {
        return -1;  /* 'b' is more urgent */
    }
    return 0;
}


/**
 * @brief Destroy an @c s_macro_td instance
 *
 * Frees the macro's name, destroys its event queue (invoking
 * @c event_destroy on every contained event), then frees the struct.
 *
 * @param data Pointer to the @c s_macro_td to destroy (as @c void *)
 *
 * @note Complexity: @e O(n), where @e n is the number of events in the
 *       macro
 */
static void s_macro_destroy(void *data)
{
    s_macro_td *macro = (s_macro_td *) data;

    if (macro == NULL) {
        return;
    }

    safe_free((void **) &macro->name);

    if (macro->events != NULL) {
        queue_destroy(macro->events);
        macro->events = NULL;
    }

    free(macro);
}


/**
 * @brief Find a named macro in the registry
 *
 * @param name Macro name to look up
 *
 * @return Pointer to the matching @c s_macro_td, or @c NULL when not
 *         found or @p name is @c NULL
 *
 * @note Complexity: @e O(m), where @e m is the number of registered
 *       macros
 */
static s_macro_td *s_macro_find(const char *name)
{
    list_item_td *node;

    if (name == NULL || s_macros == NULL) {
        return NULL;
    }

    for (node = list_head(s_macros); node != NULL;
            node = list_next(node)) {
        s_macro_td *m = (s_macro_td *) list_data(node);
        if (m != NULL && safe_strcmp(m->name, name) == 0) {
            return m;
        }
    }

    return NULL;
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
        LOGGER_ERROR("Received null client event to process" \
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
        LOGGER_ERROR("Received null client object in event", L_NARG);
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
            desktop = wm_get_client_desktop(client);
            if (desktop != NULL) {
                dcmd_desktop_clients_cycle_active(desktop);
            }
            break;

        case ACTION_CLIENT_CYCLE_PREV:
            desktop = wm_get_client_desktop(client);
            if (desktop != NULL) {
                dcmd_desktop_clients_cycle_prev(desktop);
            }
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
        LOGGER_ERROR("Received null desktop event to process" \
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
        LOGGER_ERROR("Received null desktop object in event", L_NARG);
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
            dcmd_desktop_client_clone(desktop, desktop_data);
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
            dcmd_desktop_clients_cycle_prev(desktop);
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
        LOGGER_ERROR("Received null screen event to process in" \
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
        LOGGER_ERROR("Received null surface object in event", L_NARG);
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
 * Process window manager-related events by dispatching them to
 * appropiate command handlers based on the action type.
 *
 * @param event Pointer to the window manager event to handle
 *
 * @note Complexity: @e O(1) for dispatch
 */
static void s_event_handle_wm(event_td *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received null window manager event to process" \
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


/* Start the priority queue to handle events */
int eventq_start(void)
{
    LOGGER_DEBUG("Initializing priority queue for events", L_NARG);

    if (eventq != NULL) {
        return -1;  /* Already initialized */
    }

    eventq = pqueue_init(s_event_compare,
            (void (*)(void *)) event_destroy);
    if (eventq == NULL) {
        LOGGER_FATAL("Failed to initialize event queue", L_NARG);
        return 1;
    }

    s_macros = list_init(s_macro_destroy);
    if (s_macros == NULL) {
        LOGGER_FATAL("Failed to initialize macro registry", L_NARG);
        pqueue_destroy(eventq);
        eventq = NULL;
        return 1;
    }

    return 0;
}


/* Deallocate memory used by the event queue */
int eventq_stop(void)
{
    LOGGER_DEBUG("Deallocating priority queue for events", L_NARG);

    if (eventq == NULL) {
        return 1;
    }

    pqueue_destroy(eventq);
    eventq = NULL;

    /* Discard any incomplete macro recording */
    if (s_macro_active != NULL) {
        s_macro_destroy(s_macro_active);
        s_macro_active = NULL;
    }

    /* Destroy the macro registry */
    if (s_macros != NULL) {
        list_destroy(s_macros);
        s_macros = NULL;
    }

    /* Discard any pending test-replay recording */
    if (s_record_queue != NULL) {
        queue_destroy(s_record_queue);
        s_record_queue = NULL;
    }
    s_recording = false;

    return 0;
}


/* Add an event to the priority event queue */
int eventq_add(event_td *event)
{
    int rc;
    event_td *rec;

    if (event == NULL) {
        LOGGER_ERROR("Attempted to add null event to queue", L_NARG);
        return 1;
    }

    /* Capture for test-replay (before enqueue; event still valid) */
    if (s_recording && s_record_queue != NULL) {
        rec = event_clone(event);
        if (rec != NULL &&
                queue_enqueue(s_record_queue, (void *) rec) != 0) {
            event_destroy(rec);
        }
    }

    /* Capture for active macro (same timing reason) */
    if (s_macro_active != NULL && s_macro_active->events != NULL) {
        rec = event_clone(event);
        if (rec != NULL &&
                queue_enqueue(s_macro_active->events, (void *) rec) != 0) {
            event_destroy(rec);
        }
    }

    LOGGER_TRACE("Enqueuing event into priority queue (priority=%d)",
            (int) event->priority);

    pthread_mutex_lock(&s_mutex);
    rc = (eventq != NULL) ? pqueue_insert(eventq, (void *) event) : -1;
    pthread_mutex_unlock(&s_mutex);

    if (rc != 0) {
        LOGGER_WARNING("Failed to enqueue event into event queue",
                L_NARG);
        event_destroy(event);
        return 1;
    }

    return 0;
}


/* Extract the highest-priority event from the queue */
event_td *eventq_extract(void)
{
    event_td *event = NULL;
    int rc;

    LOGGER_TRACE("Extracting event from event queue", L_NARG);

    pthread_mutex_lock(&s_mutex);
    if (eventq == NULL || pqueue_is_empty(eventq)) {
        pthread_mutex_unlock(&s_mutex);
        return NULL;
    }
    rc = pqueue_extract(eventq, (void **) &event);
    pthread_mutex_unlock(&s_mutex);

    return (rc == 0) ? event : NULL;
}


/* Process events in event queue */
int eventq_process(void)
{
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


/* Begin capturing events for later replay */
int eventq_record_start(void)
{
    if (s_recording) {
        LOGGER_WARNING("Test-replay recording already in progress",
                L_NARG);
        return -1;
    }

    if (s_record_queue == NULL) {
        s_record_queue = queue_init((void (*)(void *)) event_destroy);
        if (s_record_queue == NULL) {
            LOGGER_WARNING("Failed to initialize recording queue",
                    L_NARG);
            return 1;
        }
    }

    s_recording = true;
    LOGGER_DEBUG("Test-replay recording started", L_NARG);
    return 0;
}


/* Stop capturing events */
int eventq_record_stop(void)
{
    if (!s_recording) {
        LOGGER_WARNING("No test-replay recording in progress", L_NARG);
        return 1;
    }

    s_recording = false;
    LOGGER_DEBUG("Test-replay recording stopped (%zu events captured)",
            (s_record_queue != NULL)
                ? queue_size(s_record_queue)
                : (size_t) 0);
    return 0;
}


/* Re-enqueue all captured events */
int eventq_replay(void)
{
    list_item_td *node;
    event_td *stored;
    event_td *clone;
    bool saved_recording;

    if (s_record_queue == NULL) {
        LOGGER_WARNING("No recording buffer to replay", L_NARG);
        return 1;
    }

    LOGGER_DEBUG("Replaying recorded events (%zu)",
            queue_size(s_record_queue));

    /* Suspend recording so replayed events are not re-captured */
    saved_recording = s_recording;
    s_recording = false;

    for (node = list_head(s_record_queue); node != NULL;
            node = list_next(node)) {
        stored = (event_td *) list_data(node);
        clone = event_clone(stored);
        if (clone == NULL) {
            LOGGER_WARNING("Failed to clone event during replay",
                    L_NARG);
            continue;
        }
        if (eventq_add(clone) != 0) {
            /* 'eventq_add' destroys clone on failure; nothing left to do */
            LOGGER_WARNING("Failed to re-enqueue event during replay",
                    L_NARG);
        }
    }

    s_recording = saved_recording;
    return 0;
}


/* Discard the event recording buffer */
int eventq_record_clear(void)
{
    if (s_record_queue == NULL) {
        return 1;
    }

    s_recording = false;
    queue_destroy(s_record_queue);
    s_record_queue = NULL;
    LOGGER_DEBUG("Test-replay recording buffer cleared", L_NARG);
    return 0;
}


/* Begin recording a named event macro */
int eventq_macro_begin(const char *name)
{
    s_macro_td *macro;

    if (name == NULL) {
        LOGGER_WARNING("Macro name must not be NULL", L_NARG);
        return 1;
    }

    if (s_macro_active != NULL) {
        LOGGER_WARNING("Macro recording already in progress ('%s')",
                s_macro_active->name);
        return -1;
    }

    macro = malloc(sizeof(s_macro_td));
    if (macro == NULL) {
        LOGGER_WARNING("Failed to allocate macro structure", L_NARG);
        return 1;
    }

    macro->name = safe_strdup(name);
    if (macro->name == NULL) {
        free(macro);
        return 1;
    }

    macro->events = queue_init((void (*)(void *)) event_destroy);
    if (macro->events == NULL) {
        safe_free((void **) &macro->name);
        free(macro);
        return 1;
    }

    s_macro_active = macro;
    LOGGER_DEBUG("Macro recording started: '%s'", name);
    return 0;
}


/* Finish recording the active macro */
int eventq_macro_end(void)
{
    list_item_td *prev;
    list_item_td *node;
    void *old;

    if (s_macro_active == NULL) {
        LOGGER_WARNING("No macro recording in progress", L_NARG);
        return 1;
    }

    /* Remove any existing macro with the same name */
    prev = NULL;
    for (node = list_head(s_macros); node != NULL;
            node = list_next(node)) {
        s_macro_td *m = (s_macro_td *) list_data(node);
        if (m != NULL && safe_strcmp(m->name, s_macro_active->name) == 0) {
            if (list_rem_next(s_macros, prev, &old) == 0) {
                s_macro_destroy(old);
            }
            break;
        }
        prev = node;
    }

    /* Append the completed macro to the registry */
    if (list_ins_next(s_macros, list_tail(s_macros),
                s_macro_active) != 0) {
        LOGGER_WARNING("Failed to register macro '%s'",
                s_macro_active->name);
        s_macro_destroy(s_macro_active);
        s_macro_active = NULL;
        return 1;
    }

    LOGGER_DEBUG("Macro '%s' registered (%zu events)",
            s_macro_active->name, queue_size(s_macro_active->events));

    s_macro_active = NULL;
    return 0;
}


/* Replay a named macro */
int eventq_macro_play(const char *name)
{
    s_macro_td *macro;
    list_item_td *node;
    event_td *stored;
    event_td *clone;
    s_macro_td *saved_active;

    if (name == NULL) {
        LOGGER_WARNING("Macro name must be non-null", L_NARG);
        return 1;
    }

    macro = s_macro_find(name);
    if (macro == NULL) {
        LOGGER_WARNING("Macro '%s' not found", name);
        return 1;
    }

    if (macro->events == NULL || queue_size(macro->events) == 0) {
        return 0;  /* Empty macro is a no-op success */
    }

    LOGGER_DEBUG("Playing macro '%s' (%zu events)",
            name, queue_size(macro->events));

    /* Suspend macro recording during playback to prevent recursion */
    saved_active = s_macro_active;
    s_macro_active = NULL;

    for (node = list_head(macro->events); node != NULL;
            node = list_next(node)) {
        stored = (event_td *) list_data(node);
        clone = event_clone(stored);
        if (clone == NULL) {
            LOGGER_WARNING("Failed to clone event during macro play",
                    L_NARG);
            continue;
        }
        if (eventq_add(clone) != 0) {
            LOGGER_WARNING("Failed to enqueue event during macro play",
                    L_NARG);
        }
    }

    s_macro_active = saved_active;
    return 0;
}


/* Remove a named macro from the registry */
int eventq_macro_clear(const char *name)
{
    list_item_td *prev;
    list_item_td *node;
    void *data;

    if (name == NULL) {
        LOGGER_WARNING("Macro name must be non-null", L_NARG);
        return 1;
    }

    if (s_macros == NULL) {
        return 1;
    }

    prev = NULL;
    for (node = list_head(s_macros); node != NULL;
            node = list_next(node)) {
        s_macro_td *m = (s_macro_td *) list_data(node);
        if (m != NULL && safe_strcmp(m->name, name) == 0) {
            if (list_rem_next(s_macros, prev, &data) == 0) {
                s_macro_destroy(data);
                LOGGER_DEBUG("Macro '%s' cleared", name);
                return 0;
            }
            return 1;
        }
        prev = node;
    }

    LOGGER_WARNING("Macro '%s' not found for clearing", name);
    return 1;
}
