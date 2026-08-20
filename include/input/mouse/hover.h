/**
 * @file input/mouse/hover.h
 *
 * @brief Resize-cursor hover tracking, on motion and on periodic poll
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

#ifndef INPUT_MOUSE_HOVER_H
#define INPUT_MOUSE_HOVER_H


/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>


/* Public interface */
/**
 * @brief Clear the tracked resize-cursor poll target if it currently
 *        matches @p window
 *
 * Call from the @c LeaveNotify handler for every window a client owns
 * (its own window at minimum), so a client the pointer has actually
 * left stops being polled; a stale poll target left set after the
 * pointer leaves would keep re-querying and re-applying a cursor to
 * a window the pointer is no longer over.
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
 * the same way @a popup_ms_remaining and similar already are, so the
 * loop wakes up promptly enough for @a mouse_hover_poll_tick to feel
 * responsive without polling on every single iteration regardless of
 * whether anything is actually being tracked.
 *
 * @return Milliseconds remaining (never negative), or @c -1 if nothing
 *         is currently being tracked
 *
 * @note Complexity: @e O(1)
 */
int mouse_hover_poll_ms_remaining(void);

/**
 * @brief Re-evaluate the resize cursor for the tracked poll target, if
 *        one is set and its next scheduled poll is due
 *
 * An undecorated client has no separate frame window for
 * @a mouse_handle_motion_hover or @a mouse_handle_enter to fall back
 * on.  Moving from its border to its interior (or back) happens
 * entirely within that one same window, with no window crossing
 * whatsoever for an @c EnterNotify to catch, and its own
 * @c PointerMotion may be just as intercepted by the client's own event
 * selection as any other client's (common in GTK/Qt applications
 * tracking hover for their own UI).  Periodically polling the actual
 * pointer position via @a xcb_query_pointer, which does not depend on
 * any event ever being delivered at all, is the only mechanism left
 * that still catches that transition; see @a mouse_handle_enter for
 * where a client starts being tracked this way, and
 * @a mouse_hover_poll_clear for where it stops.
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
 * @see @a drag_is_active, @a lookup_find_client and
 *      @a mouse_resize_cursors_init
 */
void mouse_handle_motion_hover(xcb_connection_t *connection,
        list_td *surfaces, xcb_motion_notify_event_t *event);


#endif  /* ! INPUT_MOUSE_HOVER_H */
