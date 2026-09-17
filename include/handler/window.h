/**
 * @file handler/window.h
 *
 * @brief X event handlers for a top-level window's own lifecycle: being
 *        mapped, gravitated, circulated, unmapped, or destroyed
 *
 * Split out of @c handler.h, alongside its sibling @c handler headers,
 * so a file that only needs one of these does not also pull in, and
 * rebuild against, every other unrelated one declared alongside it.
 * Grouped together under this one anchor rather than one header per
 * event, unlike most of its siblings, because all seven are
 * fundamentally about the same thing: a top-level window coming into
 * being, changing how it is stacked or positioned, or going away, not
 * about focus, properties, or any other concern a window's own
 * lifecycle is orthogonal to.
 *
 * @see @c handler.h
 *
 * @ingroup handler
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef HANDLER_WINDOW_H
#define HANDLER_WINDOW_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle a @c MAP_REQUEST event
 *
 * Adopts newly visible windows into the window manager's desktop
 * hierarchy, applies the placement policy, and optionally focuses the
 * new client.
 *
 * Under @c windows.placement.policy of @c manual the window is instead
 * held unmapped and unfocused while the person is asked where it goes;
 * everything that follows the placement, the mapping, the focus, the
 * ICCCM @c ConfigureNotify and the IPC notification, is handed to
 * @a place_manual_enqueue to carry out once that question is settled.
 *
 * @param wm    Window manager state
 * @param event Map request event
 *
 * @note Complexity: @e O(g * n), where @e g is the number of grid
 *       positions the placement policy tests and @e n is the number of
 *       clients on the desktop
 */
void handler_window_map_request(const wm_td *wm,
        xcb_map_request_event_t *event);

/**
 * @brief Handle a @c MAP_NOTIFY event
 *
 * Triggers an EWMH resync for the affected managed client when
 * a non-override-redirect window is mapped.  Override-redirect windows
 * (tooltips, pop-up menus) are silently ignored.
 *
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Map notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       stages
 */
void handler_window_map_notify(xcb_connection_t *connection,
        list_td *stages, xcb_map_notify_event_t *event);

/**
 * @brief Handle a @c GRAVITY_NOTIFY event
 *
 * Updates the cached frame position when the X server repositions
 * a frame window following a screen resize, according to the client's
 * @c win_gravity (stored as @c client->layout.gravity).  Re-syncs
 * decoration layout and schedules a repaint.
 *
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Gravity notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       stages
 */
void handler_window_gravity_notify(xcb_connection_t *connection,
        list_td *stages, xcb_gravity_notify_event_t *event);

/**
 * @brief Handle a @c CIRCULATE_NOTIFY event
 *
 * Marks the affected stage as outdated so that @a wm_ewmh_sync
 * updates @c _NET_CLIENT_LIST_STACKING to reflect the new stacking
 * order on the next main-loop iteration.
 *
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Circulate notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       stages
 */
void handler_window_circulate_notify(xcb_connection_t *connection,
        list_td *stages, xcb_circulate_notify_event_t *event);

/**
 * @brief Handle a @c CIRCULATE_REQUEST event (ICCCM §4.1.7)
 *
 * Raises or lowers the target window as directed by the @p place field:
 * @c XCB_PLACE_ON_TOP maps to @c XCB_STACK_MODE_ABOVE and
 * @c XCB_PLACE_ON_BOTTOM maps to @c XCB_STACK_MODE_BELOW.  The WM must
 * honor this request to remain ICCCM-compliant.
 *
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Circulate request event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       stages
 */
void handler_window_circulate_request(xcb_connection_t *connection,
        list_td *stages, xcb_circulate_request_event_t *event);

/**
 * @brief Handle an @c UNMAP_NOTIFY event
 *
 * Updates desktop active-client tracking when a managed client is
 * unmapped.  Does not set the hidden flag, to avoid interfering with
 * desktop switching.
 *
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Unmap notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       stages
 */
void handler_window_unmap_notify(xcb_connection_t *connection,
        list_td *stages, xcb_unmap_notify_event_t *event);

/**
 * @brief Handle a @c DESTROY_NOTIFY event
 *
 * Drops the window from the systray when it was a docked icon, cancels
 * any drag in progress for the destroyed client, drops it from the
 * queue of windows waiting to be placed by hand, removes it from its
 * desktop, and releases its resources.
 *
 * @param wm         Window-manager singleton
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Destroy notify event
 *
 * @note Complexity: @e O(n + t), where @e n is the number of managed
 *       stages and @e t the number of docked systray icons
 */
void handler_window_destroy_notify(wm_td *wm, xcb_connection_t *connection,
        list_td *stages, xcb_destroy_notify_event_t *event);


#endif  /* ! HANDLER_WINDOW_H */
