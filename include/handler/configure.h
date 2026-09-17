/**
 * @file handler/configure.h
 *
 * @brief X @c ConfigureRequest and @c ConfigureNotify event handlers
 *
 * Split out of @c handler.h, alongside its sibling @c handler headers,
 * so a file that only needs one event's handler does not also pull
 * in, and rebuild against, every other unrelated one declared
 * alongside it.
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

#ifndef HANDLER_CONFIGURE_H
#define HANDLER_CONFIGURE_H


/* XCB includes */
#include <xcb/xcb.h>


/* Public interface */
/**
 * @brief Handle a @c CONFIGURE_REQUEST event
 *
 * Applies geometry and stacking requests directly through XCB, keeping
 * the managed client's cached geometry synchronized when applicable.
 * A request is ignored outright while the window manager itself is
 * actively moving, resizing, or has this same client in fullscreen, and
 * also, for a short grace period afterward, right after the window
 * manager itself shaded, unshaded, or entered or left fullscreen on
 * this same client, as a stale echo of that transition rather than
 * a genuine independent request (see @c WM_SHADE_CONFIGURE_COOLDOWN_MS
 * and @c WM_FULLSCREEN_CONFIGURE_COOLDOWN_MS).
 *
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Configure request event
 *
 * @note Complexity: @e O(1)
 */
void handler_configure_request(xcb_connection_t *connection,
        list_td *stages,
        xcb_configure_request_event_t *event);

/**
 * @brief Handle a @c CONFIGURE_NOTIFY event
 *
 * Updates the client's cached geometry from the event.  Only frame
 * (outermost) window events are trusted to avoid corrupting cached
 * screen-relative positions with frame-relative values from inner
 * windows.
 *
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Configure notify event
 *
 * @note Complexity: @e O(1)
 */
void handler_configure_notify(xcb_connection_t *connection,
        list_td *stages,
        xcb_configure_notify_event_t *event);


#endif  /* ! HANDLER_CONFIGURE_H */
