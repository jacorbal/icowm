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
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <wm.h>


/* Public interface */
/**
 * @brief Dispatch a button-press event
 *
 * Handles popup and cycle-menu dismissal, icon-window drag start, plain
 * client focus clicks, titlebar decoration buttons, desktop cycling,
 * and drag-start for configured move/resize/lower bindings.
 *
 * @param wm         Window manager instance, needed only for the
 *                    root-window right-click's own root menu
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
 * selects @c PointerMotion for its own purposes (common in GTK/Qt
 * applications tracking hover for their own UI) intercepts motion
 * events at the X11 propagation level before
 * @a mouse_handle_motion_hover ever sees them, which otherwise leaves
 * whichever resize-border cursor was last set stuck for as long as the
 * pointer stays over that client's own content.  This enter-notify
 * still fires reliably even then, since it is selected directly on the
 * client's own window (cfr. @c client.c), giving the cursor logic
 * a second, independent chance motion alone might have missed.
 *
 * Focus itself is then applied to the client under the pointer only
 * when the configured focus policy is @c sloppy.  Normal events on
 * managed client frames raise no stacking change; an inferior
 * transition (entering this same client's own content area from its
 * frame) skips focus re-evaluation, since the client was already
 * focused to get there, but still gets the cursor re-evaluation above.
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


#endif  /* ! INPUT_MOUSE_EVENT_H */
