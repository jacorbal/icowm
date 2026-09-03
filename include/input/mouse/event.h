/**
 * @file input/mouse/event.h
 *
 * @brief Mouse button-press, button-release, and enter-notify dispatch
 *
 * @defgroup input_mouse Mouse input
 * @ingroup input
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_EVENT_H
#define INPUT_MOUSE_EVENT_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */

/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Dispatch a button-press event
 *
 * Handles popup and cycle-menu dismissal, icon-window drag start, plain
 * client focus clicks, titlebar decoration buttons, desktop cycling,
 * and drag-start for configured move/resize/lower bindings.
 *
 * A window being placed by hand takes the press ahead of all of them
 * and settles where it goes: the pointer is already held by it, so the
 * press is that question's answer and never a click on anything the
 * rest would go on to resolve.
 *
 * @param wm         Window manager instance, needed only for the
 *                   root-window right-click's root menu
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (for lookup and focus)
 * @param event      Button-press event
 * @param config     Active configuration
 *
 * @note Complexity: @e O(n) for binding lookup; @e O(1) otherwise
 */
void mouse_handle_press(wm_td *wm, xcb_connection_t *connection,
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
        list_td *surfaces, const xcb_button_release_event_t *event,
        const config_td *config);

/**
 * @brief Re-evaluate the resize cursor, then apply focus-follows-mouse,
 *        on an enter-notify event
 *
 * The cursor re-evaluation runs unconditionally as long as the event is
 * a normal one (gated only by @p event->mode below, not by whether
 * focus-follows-mouse is even enabled).  A resizable client that
 * selects @c PointerMotion for its purposes (common in GTK/Qt
 * applications tracking hover for their UI) intercepts motion events at
 * the X11 propagation level before @a mouse_handle_motion_hover ever
 * sees them, which otherwise leaves whichever resize-border cursor was
 * last set stuck for as long as the pointer stays over that client's
 * content.  This enter-notify still fires reliably even then, since it
 * is selected directly on the client's window (cfr. @c client.c),
 * giving the cursor logic a second, independent chance motion alone
 * might have missed.
 *
 * Focus itself is then applied to the client under the pointer only
 * when the configured focus policy is @c sloppy.  Normal events on
 * managed client frames raise no stacking change; an inferior
 * transition (entering this same client's content area from its frame)
 * skips focus re-evaluation, since the client was already focused to
 * get there, but still gets the cursor re-evaluation above.
 *
 * With @c windows.focus.delay-ms left at its default of @c 0, that
 * focus change happens right here, same as always; set above @c 0, it
 * is deferred instead, armed here but only actually carried out by
 * @a mouse_enter_focus_tick once the delay elapses, and dropped
 * outright by @a mouse_enter_focus_cancel if the pointer leaves first.
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
 * Returns @c true when @a mouse_handle_enter has initiated
 * a focus-follows-mouse transfer but the corresponding @c FocusIn event
 * has not yet been processed.
 *
 * @return @c true if a hover-triggered transfer is pending
 *
 * @note Complexity: @e O(1)
 */
bool mouse_enter_focus_is_active(void);

/**
 * @brief Clear the hover-triggered focus flag
 *
 * Resets the internal flag set by @a mouse_handle_enter after the
 * corresponding focus change has been processed.
 *
 * @note Complexity: @e O(1)
 */
void mouse_enter_focus_clear(void);

/**
 * @brief Cancel a pending delayed sloppy-focus if it targets @p window
 *
 * Call from the @c LeaveNotify handler for every window a client owns,
 * the same way @a mouse_hover_poll_clear already is, so a pointer that
 * leaves before @c windows.focus.delay-ms elapses never ends up
 * focusing a client it already moved past.
 *
 * @param window Window to compare against the currently pending one
 *
 * @note A no-op if @p window is not the one currently pending, or if
 *       nothing is pending at all
 * @note Complexity: @e O(1)
 */
void mouse_enter_focus_cancel(xcb_window_t window);

/**
 * @brief Milliseconds until the pending delayed sloppy-focus becomes
 *        due
 *
 * For the main loop to fold into its poll timeout computation, the same
 * way @a mouse_hover_poll_ms_remaining and similar already are.
 *
 * @return Milliseconds remaining (never negative), or @c -1 if nothing
 *         is currently pending
 *
 * @note Complexity: @e O(1)
 */
int mouse_enter_focus_ms_remaining(void);

/**
 * @brief Apply the pending delayed sloppy-focus, if one is due
 *
 * A no-op if nothing is pending, if the pending one is not yet due, or
 * if the focus policy is no longer @c sloppy by the time it comes due
 * (a config reload could have switched it to @c click in the meantime).
 *
 * @param surfaces All managed surfaces (for lookup and focus)
 * @param config   Active configuration
 *
 * @note Complexity: @e O(n)
 */
void mouse_enter_focus_tick(list_td *surfaces, const config_td *config);


#endif  /* ! INPUT_MOUSE_EVENT_H */
