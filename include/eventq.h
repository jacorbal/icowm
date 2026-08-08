/**
 * @file eventq.h
 *
 * @brief Event priority queue handler function declaration
 *
 * Interface for handling an event queue, backed by a max-heap priority
 * queue.  Events are inserted with a priority level (lower value
 * = higher urgency) and extracted in urgency order by the main event
 * loop after each batch of X11 events is processed.
 *
 * @par What goes through the queue, and what does not:
 * Every one-shot command triggered by a menu click, keybinding, or
 * EWMH request (move, resize, shade, maximize, iconify, and so on)
 * goes through this queue: @c client_send_event_move,
 * @c client_send_event_resize, and their siblings enqueue an
 * @c ACTION_CLIENT_* event that @c eventq_process later dispatches to
 * the matching @c wcmd_client_* command.  Doing so gives every such
 * command a uniform priority, deferred execution relative to the X
 * event batch that triggered it, and, incidentally, the test-replay
 * and named-macro recording described below, since both only ever
 * observe events that actually pass through @a eventq_add.
 *
 * The one deliberate exception is interactive mouse move and resize:
 * while a drag is in progress (@c client->properties.operation is
 * @c CLIENT_OPERATION_MOVING or @c CLIENT_OPERATION_RESIZING),
 * @c client_send_event_move and @c client_send_event_resize apply the
 * new geometry immediately via a direct @c xcb_configure_window call
 * instead of enqueuing anything, and return before reaching the
 * enqueue step at all.  This is intentional, not an oversight: a drag
 * needs the window to visually track the pointer with the lowest
 * latency achievable, and every added layer of indirection (enqueue,
 * wait for the next @a eventq_process pass, dequeue, dispatch) would
 * make the window measurably lag behind the pointer, particularly for
 * a large or heavily decorated one, which is already the more
 * expensive case per motion event (see the motion-coalescing comment
 * in @c loop.c's main loop for the related fix to that same problem
 * at the X event level).  Routing every geometry change through this
 * queue unconditionally, drag or not, was considered and rejected for
 * exactly that reason.
 *
 * All public functions that insert events into the queue are
 * thread-safe: @a eventq_add acquires a mutex before modifying the
 * heap, so external threads may post events safely while the main loop
 * is idle.  All other functions (start, stop, recording, macros) must
 * be called exclusively from the main thread.
 *
 * @par Test-replay recording:
 * Call @a eventq_record_start before performing a reproducible sequence
 * of actions, then @a eventq_record_stop when done.  At any later point
 * @a eventq_replay re-enqueues deep copies of the captured events, and
 * @a eventq_record_clear discards the recording.
 *
 * @par Named macro recording:
 * Call @a eventq_macro_begin with a unique name, perform the desired
 * actions, then call @a eventq_macro_end.  The macro persists in an
 * internal registry until @a eventq_macro_clear is called.  Use
 * @a eventq_macro_play to replay a macro by name.
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
 * Allocates and initializes the singleton priority queue used to hold
 * pending events.  Events are extracted in priority order (lower
 * @c priority value implies higher urgency) by calling
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
 * Destroys all remaining events, releases the priority queue, discards
 * any active recording, and destroys the macro registry.  Must be
 * called only from the main thread after the event loop has exited.
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
 * Enqueues @p event into the priority queue.  Events with a lower
 * @c priority value (e.g., @c PRIORITY_URGENT) are extracted before
 * events with a higher value (e.g., @c PRIORITY_IDLE).
 *
 * If test-replay recording (@a eventq_record_start) or named macro
 * recording (@a eventq_macro_begin) is active, a deep copy of @p event
 * is saved to the respective buffer before the event is enqueued.
 *
 * @param event New event to enqueue (ownership transferred on success;
 *              @p event is destroyed and the pointer invalidated on
 *              failure)
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to enqueue the event
 *
 * @note This function is thread-safe: it acquires an internal mutex
 *       before inserting into the heap so that external threads may
 *       call it while the main loop is idle.
 * @note Complexity: @e O(log n), where @e n is the queue depth
 */
int eventq_add(event_td *event);

/**
 * @brief Extract the highest-priority event from the queue
 *
 * Removes and returns the most urgent event (lowest @c priority value)
 * from the priority queue.
 *
 * @return Pointer to the extracted event, or @c NULL when empty
 *
 *
 * @note This function is thread-safe: it acquires the same mutex used
 *       by @a eventq_add.
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
 * @note Complexity: @e O(n log n), where @e n is the number of events
 */
int eventq_process(void);

/* Test-replay recording */
/**
 * @brief Begin capturing events for later replay
 *
 * All subsequent events passed to @a eventq_add are deep-copied into an
 * internal recording buffer until @a eventq_record_stop is called.  The
 * events are still enqueued normally; recording only observes them.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval -1 A recording was already in progress; no action taken
 *
 * @note Must be called from the main thread
 * @note Complexity: @e O(1)
 */
int eventq_record_start(void);

/**
 * @brief Stop capturing events
 *
 * Ends an active recording.  The captured buffer is retained and
 * accessible via @a eventq_replay until @a eventq_record_clear is
 * called.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 No recording was in progress; no action taken
 *
 * @note Must be called from the main thread
 * @note Complexity: @e O(1)
 */
int eventq_record_stop(void);

/**
 * @brief Re-enqueue all captured events
 *
 * Deep-copies every event in the recording buffer and inserts the
 * copies into the priority queue in their original order.  The
 * recording buffer is left intact so that multiple replays are
 * possible.  Call @a eventq_record_clear when no further replays are
 * needed.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 No recording buffer exists; no action taken
 *
 * @note The recording must have been stopped (@a eventq_record_stop)
 *       before calling this function; replayed events are not
 *       re-captured.
 * @note Must be called from the main thread
 * @note Complexity: @e O(n log n), where @e n is the number of recorded
 *       events
 */
int eventq_replay(void);

/**
 * @brief Discard the event recording buffer
 *
 * Destroys all captured events and releases the recording buffer.  If
 * a recording is still in progress it is stopped first.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 No recording buffer existed; no action taken
 *
 * @note Must be called from the main thread
 * @note Complexity: @e O(n), where @e n is the number of recorded
 *       events
 */
int eventq_record_clear(void);

/* Named macro recording */
/**
 * @brief Begin recording a named event macro
 *
 * All subsequent events passed to @a eventq_add are deep-copied into an
 * internal macro buffer named @p name until @a eventq_macro_end is
 * called.  If a macro with the same name already exists it is replaced
 * when @a eventq_macro_end finalizes the recording.
 *
 * @param name Null-terminated name for the macro (copied internally)
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Memory allocation failure or @p name is @c NULL
 * @retval -1 A macro recording was already in progress; no action taken
 *
 * @note Must be called from the main thread
 * @note Complexity: @e O(1)
 */
int eventq_macro_begin(const char *name);

/**
 * @brief Finish recording the active macro
 *
 * Finalizes the active macro and registers it in the internal macro
 * registry.  If a macro with the same name already exists, it is
 * replaced.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 No macro recording was in progress or failed to register
 *
 * @note Must be called from the main thread
 * @note Complexity: @e O(m), where @e m is the number of registered
 *       macros (for the duplicate search)
 */
int eventq_macro_end(void);

/**
 * @brief Replay a named macro
 *
 * Deep-copies every event in the named macro and inserts the copies
 * into the priority queue in their recorded order.  The macro is
 * preserved and may be played again.
 *
 * @param name Name of the macro to replay
 *
 * @return Status of the operation
 * @retval  0 Success (including an empty macro)
 * @retval  1 Macro not found or @p name is @c NULL
 *
 * @note Must be called from the main thread
 * @note Complexity: @e O(n log n), where @e n is the number of events
 *       in the macro
 */
int eventq_macro_play(const char *name);

/**
 * @brief Remove a named macro from the registry
 *
 * Destroys the named macro and all events it contains.
 *
 * @param name Name of the macro to remove
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Macro not found or @p name is @c NULL
 *
 * @note Must be called from the main thread
 * @note Complexity: @e O(m), where @e m is the number of registered
 *       macros
 */
int eventq_macro_clear(const char *name);


#endif  /* ! EVENTQ_H */
