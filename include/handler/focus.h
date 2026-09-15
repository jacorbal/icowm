/**
 * @file handler/focus.h
 *
 * @brief X @c FocusIn and @c FocusOut event handlers
 *
 * Split out of @c handler.h, alongside its sibling @c handler headers,
 * so a file that only needs one event's handler does not also pull in,
 * and rebuild against, every other unrelated one declared alongside it.
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

#ifndef HANDLER_FOCUS_H
#define HANDLER_FOCUS_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle a @c FOCUS_IN event
 *
 * Synchronizes the desktop active-client identifier with the real X11
 * input focus when a managed client receives focus.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Focus-in event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_focus_in(xcb_connection_t *connection,
        list_td *surfaces, const xcb_focus_in_event_t *event);


/**
 * @brief Handle a @c FOCUS_OUT event
 *
 * Marks the surface owning the client that lost the real X11 input
 * focus as outdated, so its decoration colors are repainted on the next
 * render pass.
 *
 * @param wm    Window-manager singleton
 * @param event Focus-out event to process
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected by @a lookup_find_client
 */
void handler_focus_out(const wm_td *wm, xcb_focus_out_event_t *event);


#endif  /* ! HANDLER_FOCUS_H */
