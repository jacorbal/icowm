/**
 * @file handler/error.h
 *
 * @brief X protocol error handling: the event-shaped error handler
 *        itself, and a helper naming the underlying error code
 *
 * Split out of @c handler.h, alongside its sibling @c handler headers,
 * so a file that only needs error handling does not also pull in, and
 * rebuild against, every other unrelated one declared alongside it.
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

#ifndef HANDLER_ERROR_H
#define HANDLER_ERROR_H


/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Handle an X protocol error delivered as an event
 *
 * XCB reports a failed request whose reply was never waited for as
 * a regular event with a response type of @c 0, which is what this
 * receives, reinterpreted as the @c xcb_generic_error_t it actually is.
 * A @c BadWindow, @c BadDrawable, or @c BadMatch on one of the routine
 * per-window requests the window manager issues constantly (mapping,
 * configuring, reading properties) is logged at DEBUG level, since it
 * is the expected outcome of a client destroying its own window in the
 * gap between the request and the server processing it, and races of
 * that kind cannot be prevented from this side.
 *
 * Any other error (@c BadValue, @c BadAlloc, @c BadAccess, or even
 * @c BadWindow/@c BadMatch on a request outside that routine set) is
 * logged at WARNING level instead, so it is not lost among routine
 * traffic, since it is far more likely to be a genuine bug worth
 * noticing.
 *
 * @param event The raw event received with response type @c 0
 *
 * @note Complexity: @e O(1)
 */
void handler_error_protocol(const xcb_generic_event_t *event);


/**
 * @brief Human-readable description for an @a xcb_connection_has_error
 *        return value
 *
 * Distinguishes an ordinary per-window protocol error, which XCB
 * delivers as a regular event and never causes this, from an actual
 * connection failure: the socket to the X server itself is gone,
 * something no window manager can recover from, since the window
 * manager is just another client of that same server.
 *
 * Logging which one occurred is the most the caller can do about it; an
 * @c XCB_CONN_ERROR in particular, especially right after a client
 * (e.g., a game attempting hardware-accelerated rendering) was seen
 * doing something unusual, is worth checking the system's logs (Xorg's
 * log file, @c dmesg for a GPU driver crash) for, outside of icowm
 * entirely.
 *
 * @param error_code Value returned by @a xcb_connection_has_error
 *
 * @return A short, constant description; never @c NULL
 *
 * @note Complexity: @e O(1)
 */
const char *handler_error_connection_str(int error_code);


#endif  /* ! HANDLER_ERROR_H */
