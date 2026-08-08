/**
 * @file input/mouse.h
 *
 * @brief Mouse binding types, parsing, grab and event handling
 *
 * Declares the mouse binding enum, the resolved binding record, and the
 * three public functions that load mouse bindings from configuration,
 * dispatch button-press events, and apply focus-follows-mouse on
 * enter-notify events.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_H
#define INPUT_MOUSE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/**
 * @brief Actions that a mouse binding can trigger
 */
enum wm_mousebind_type_e {
    MOUSEBIND_NONE,
    MOUSEBIND_MOVE,             /**< Move the clicked client */
    MOUSEBIND_RESIZE,           /**< Resize the clicked client */
    MOUSEBIND_LOWER,            /**< Lower the clicked client */
    MOUSEBIND_DESKTOP_NEXT,     /**< Switch to next desktop (wheel 5) */
    MOUSEBIND_DESKTOP_PREV,     /**< Switch to previous desktop (wheel 4) */
};


/**
 * @brief Resolved mouse binding record
 *
 * Associates a button index, a modifier mask, and an action type loaded
 * from the configuration file.
 */
typedef struct {
    xcb_button_index_t button;
    uint16_t modmask;
    enum wm_mousebind_type_e type;
} wm_mousebinding_td;


/* Public interface */
/**
 * @brief Parse mouse bindings from configuration and grab buttons
 *
 * Reads mouse binding strings from @p config, parses each one,
 * registers it in the internal binding table, and grabs the
 * corresponding button on all roots in @p surfaces (with lock-modifier
 * variants so that @c Caps_Lock and @c Num_Lock do not interfere).
 *
 * @param surfaces All managed surfaces
 * @param config   Active configuration
 *
 * @note Replaces any * previously loaded bindings.
 * @note Complexity: @e O(s * b * L), where @e s is the number of
 *       surfaces, @e b the number of configured bindings, and @e L is
 *       the number of lock-modifier variants (4)
 */
void mouse_load(list_td *surfaces, const config_td *config);

/**
 * @brief Return the number of loaded mouse bindings
 *
 * @return Number of active bindings in the binding table
 *
 * @note Complexity: @e O(1)
 */
int mousebind_count(void);

/**
 * @brief Access a binding entry by index
 *
 * Retrieves the button index, modifier mask, and action type of the
 * binding at position @p idx in the binding table.
 *
 * @param idx         Zero-based index into the binding table
 * @param button_out  Receives the binding's button index (may be
 *                    @c NULL)
 * @param modmask_out Receives the binding's modifier mask (may be
 *                    @c NULL)
 *
 * @return Action type for that entry, or @c MOUSEBIND_NONE if out
 *         of range
 *
 * @note Complexity: @e O(1)
 */
enum wm_mousebind_type_e mousebind_at(int idx,
        xcb_button_index_t *button_out, uint16_t *modmask_out);

/**
 * @brief Dispatch a button-press event
 *
 * Handles popup and cycle-menu dismissal, icon-window drag start, plain
 * client focus clicks, titlebar decoration buttons, desktop cycling,
 * and drag-start for configured move/resize/lower bindings.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (for lookup and focus)
 * @param event      Button-press event
 * @param config     Active configuration
 *
 * @note Complexity: @e O(n) for binding lookup; @e O(1) otherwise
 */
void mouse_handle_press(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config);

/**
 * @brief Handle a button-release event to end a drag
 *
 * Finalizes a move or resize drag, decides whether an icon drag was
 * a click or a real drag, ungrab the pointer, and resets drag state.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (for lookup and focus on
 *                   icon-click restore)
 * @param event      Button-release event
 * @param config     Active configuration
 *
 * @note Complexity: @e O(1)
 */
void mouse_handle_release(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_release_event_t *event,
        const config_td *config);

/**
 * @brief Re-evaluate the resize cursor, then apply focus-follows-mouse,
 *        on an enter-notify event
 *
 * The cursor re-evaluation runs unconditionally as long as the event
 * is a normal one (gated only by @c event->mode below, not by whether
 * focus-follows-mouse is even enabled): a resizable client that
 * selects @c PointerMotion for its own purposes (common in GTK/Qt
 * applications tracking hover for their own UI) intercepts motion
 * events at the X11 propagation level before @c mouse_handle_motion_hover
 * ever sees them, which otherwise leaves whichever resize-border
 * cursor was last set stuck for as long as the pointer stays over that
 * client's own content.  This enter-notify still fires reliably even
 * then, since it is selected directly on the client's own window (see
 * client.c), giving the cursor logic a second, independent chance
 * motion alone might have missed.
 *
 * Focus itself is then applied to the client under the pointer only
 * when the configured focus policy is @c follow-mouse.  Normal events
 * on managed client frames raise no stacking change; an inferior
 * transition (entering this same client's own content area from its
 * frame) skips focus re-evaluation, since the client was already
 * focused to get there, but still gets the cursor re-evaluation
 * above.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (for lookup and focus)
 * @param event      Enter-notify event
 * @param config     Active configuration
 *
 * @note Complexity: @e O(n)
 */
void mouse_handle_enter(xcb_connection_t *connection,
        list_td *surfaces, xcb_enter_notify_event_t *event,
        const config_td *config);

/**
 * @brief Test whether a hover-triggered focus transfer is in progress
 *
 * Returns @c true when @c mouse_handle_enter has initiated
 * a focus-follows-mouse transfer but the corresponding @c FocusIn event
 * has not yet been processed.
 *
 * @return @c true if a hover-triggered transfer is pending, @c false
 *         otherwise
 *
 * @note Complexity: @e O(1)
 */
bool mouse_enter_focus_is_active(void);

/**
 * @brief Clear the hover-triggered focus flag
 *
 * Resets the internal flag set by @c mouse_handle_enter after the
 * corresponding focus change has been processed.
 *
 * @note Complexity: @e O(1)
 */
void mouse_enter_focus_clear(void);

/**
 * @brief Clear the tracked resize-cursor poll target if it currently
 *        matches @p window
 *
 * Call from the @c LeaveNotify handler for every window a client owns
 * (its own window at minimum), so a client the pointer has actually
 * left stops being polled; a stale poll target left set after the
 * pointer leaves would keep re-querying and re-applying a cursor to a
 * window the pointer is no longer over.
 *
 * @param window Window to compare against the currently tracked one
 *
 * @note A no-op if @p window is not the one currently tracked
 * @note Complexity: @e O(1)
 */
void mouse_hover_poll_clear(xcb_window_t window);

/**
 * @brief Milliseconds until the tracked resize-cursor poll target
 *        should next be re-evaluated
 *
 * For the main loop to fold into its own @c poll timeout computation,
 * the same way @c popup_ms_remaining and similar already are, so the
 * loop wakes up promptly enough for @c mouse_hover_poll_tick to feel
 * responsive without polling on every single iteration regardless of
 * whether anything is actually being tracked.
 *
 * @return Milliseconds remaining (never negative), or -1 if nothing
 *         is currently being tracked
 *
 * @note Complexity: @e O(1)
 */
int mouse_hover_poll_ms_remaining(void);

/**
 * @brief Re-evaluate the resize cursor for the tracked poll target,
 *        if one is set and its next scheduled poll is due
 *
 * An undecorated client has no separate frame window for
 * @c mouse_handle_motion_hover or @c mouse_handle_enter to fall back
 * on: moving from its border to its interior (or back) happens
 * entirely within that one same window, with no window crossing
 * whatsoever for an @c EnterNotify to catch, and its own
 * @c PointerMotion may be just as intercepted by the client's own
 * event selection as any other client's (common in GTK/Qt
 * applications tracking hover for their own UI).  Periodically
 * polling the actual pointer position via @c xcb_query_pointer, which
 * does not depend on any event ever being delivered at all, is the
 * only mechanism left that still catches that transition; see
 * @c mouse_handle_enter for where a client starts being tracked this
 * way, and @c mouse_hover_poll_clear for where it stops.
 *
 * @param connection XCB connection
 * @param surfaces   Every managed surface, to look up the tracked
 *                   window's client
 *
 * @note No-op if nothing is currently tracked, or if tracked but not
 *       yet due for its next poll
 * @note Complexity: @e O(1)
 */
void mouse_hover_poll_tick(xcb_connection_t *connection, list_td *surfaces);

/**
 * @brief Create the eight border-resize cursors used for hover feedback
 *
 * Allocates the cursors once for the whole session (matching the
 * left-pointer cursor already set up in @c startup_subscribe_root_events)
 * so that @c mouse_handle_motion_hover only ever has to look one up,
 * never create one. Safe to call more than once; only the first call
 * actually allocates anything. Call @c mouse_destroy_resize_cursors at
 * shutdown to free them.
 *
 * @param connection XCB connection used to create the cursors
 *
 * @note Complexity: @e O(1)
 */
void mouse_create_resize_cursors(xcb_connection_t *connection);

/**
 * @brief Free the cursors created by @c mouse_create_resize_cursors
 *
 * Safe to call even if they were never created.
 *
 * @param connection XCB connection used to free the cursors
 *
 * @note Complexity: @e O(1)
 */
void mouse_destroy_resize_cursors(xcb_connection_t *connection);

/**
 * @brief The plain-pointer cursor, the same one shown for
 *        @c S_RESIZE_ZONE_NONE
 *
 * Meant for a client's own window to be given this cursor explicitly,
 * once, at decoration time (see @c ci_create_decorations in
 * client/geom.c), rather than left to inherit whatever the frame's
 * own cursor happens to currently be set to.  Explicit beats
 * inherited: once the client's own window has its own cursor, the X
 * server shows it the instant the pointer crosses into that window,
 * with no window-manager-side event handling required at all, unlike
 * relying on catching every possible crossing or motion event (which
 * a client that intercepts pointer motion for its own purposes, e.g.
 * to track hover for its own UI, can prevent from ever reaching this
 * window manager in the first place).
 *
 * @return The plain-pointer cursor, or 0 if
 *         @c mouse_create_resize_cursors has not run yet
 *
 * @note Complexity: @e O(1)
 */
xcb_cursor_t mouse_plain_cursor(void);

/**
 * @brief Update the pointer cursor to match a window's resize border
 *
 * Meant to be called for every @c MotionNotify while no drag is active.
 * Finds the client that owns @p event's window (its frame or, for an
 * undecorated client, the window itself) and, if the pointer is within
 * the resize border on one of its edges or corners, sets that window's
 * cursor to the matching directional shape; otherwise restores the
 * plain left-pointer cursor.
 *
 * A no-op if the event's window is not a managed client, or the client
 * cannot be resized.  Skips the X request entirely when the target
 * window and resize zone are unchanged since the last call, since this
 * runs on every pointer motion and a plain cursor-attribute change
 * carries no risk of visible flicker on its own but is still needless
 * traffic to repeat every single motion step while sitting still in the
 * same zone.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces, for the client lookup
 * @param event      Motion-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (for the lookup)
 *
 * @see @c drag_is_active, @c lookup_find_client and
 *      @c mouse_create_resize_cursors
 */
void mouse_handle_motion_hover(xcb_connection_t *connection,
        list_td *surfaces, xcb_motion_notify_event_t *event);


#endif  /* ! INPUT_MOUSE_H */
