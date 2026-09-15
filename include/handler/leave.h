/**
 * @file handler/leave.h
 *
 * @brief X @c LeaveNotify event handler
 *
 * Split out of @c handler.h, alongside its sibling @c handler headers,
 * so a file that only needs this one event's handler does not also pull
 * in, and rebuild against, every other unrelated one declared alongside
 * it.
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

#ifndef HANDLER_LEAVE_H
#define HANDLER_LEAVE_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle a @c LEAVE_NOTIFY event
 *
 * Stops polling the resize-cursor target the pointer just left and,
 * under focus-follows-mouse, releases the X11 input focus so that no
 * client stays visually focused while the pointer rests on the root
 * background.
 *
 * @param wm    Window-manager singleton
 * @param event Leave-notify event to process
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected by @a lookup_find_client
 */
void handler_leave_notify(const wm_td *wm,
        xcb_leave_notify_event_t *event);


#endif  /* ! HANDLER_LEAVE_H */
