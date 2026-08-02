/**
 * @file eventq.c
 *
 * @brief Event queue infrastructure, i.e, priority queue, recording,
 *        and macros
 *
 * Manages the singleton priority queue, thread-safe add/extract, the
 * test-replay recording buffer, and the named macro registry.
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

/* ADT includes */
#include <adt/list.h>   /* list_td (macro registry) */
#include <adt/pqueue.h> /* Priority queue (heap-backed) */
#include <adt/queue.h>  /* FIFO queue (recording & macro buffers) */

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Project includes */
#include <event.h>
#include <logger.h>

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
 * Events with a lower @c priority value (more negative implies more
 * urgent) float to the top of the max-heap and are extracted first.
 */
static pqueue_td *eventq = NULL;

/**
 * @brief Mutex protecting @c eventq_add and @c eventq_extract
 *
 * Statically initialized so it is ready before @c eventq_start.  Allows
 * external threads to post events to the queue safely while the main
 * loop is processing X11 events.
 */
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Test-replay recording state */
static bool s_recording = false;            /**< Test-recording is active */
static queue_td *s_record_queue = NULL;     /**< FIFO of cloned events */

/* Named macro state */
static list_td *s_macros = NULL;            /**< Registry of named macros */
static s_macro_td *s_macro_active = NULL;   /**< Macro being recorded */


/**
 * @brief Compare two events by priority for the max-heap
 *
 * Returns +1 when @p a has higher urgency (lower priority value) than
 * @p b, so the heap root always holds the most urgent event.
 *
 * @param a First event pointer (cast to @c (const event_td*))
 * @param b Second event pointer (cast to @c (const event_td*))
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
 * @param data Pointer to the @c s_macro_td to destroy (as @c void*)
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
    if (name == NULL || s_macros == NULL) {
        return NULL;
    }

    for (list_item_td *node = list_head(s_macros); node != NULL;
            node = list_next(node)) {
        s_macro_td *m = (s_macro_td *) list_data(node);
        if (m != NULL && safe_strcmp(m->name, name) == 0) {
            return m;
        }
    }

    return NULL;
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

    for (list_item_td *node = list_head(s_record_queue); node != NULL;
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
        LOGGER_WARNING("Macro name must not be null", L_NARG);
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
    void *old;

    if (s_macro_active == NULL) {
        LOGGER_WARNING("No macro recording in progress", L_NARG);
        return 1;
    }

    /* Remove any existing macro with the same name */
    prev = NULL;
    for (list_item_td *node = list_head(s_macros); node != NULL;
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
    event_td *stored;
    event_td *clone;
    s_macro_td *saved_active;

    if (name == NULL) {
        LOGGER_WARNING("Macro name must not be null", L_NARG);
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

    for (list_item_td *node = list_head(macro->events); node != NULL;
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
    void *data;

    if (name == NULL) {
        LOGGER_WARNING("Macro name must not be null", L_NARG);
        return 1;
    }

    if (s_macros == NULL) {
        return 1;
    }

    prev = NULL;
    for (list_item_td *node = list_head(s_macros); node != NULL;
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
