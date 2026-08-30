/**
 * @file utils/xcb/wait.h
 *
 * @brief Bounded wait for an XCB connection's file descriptor to
 *        become readable
 *
 * A blocking XCB reply call (any @c xcb_..._reply function) waits for
 * as long as it takes the X server to answer, with no way to give up if
 * it never does; the server (or the specific client or window that
 * reply concerns) going away mid-request leaves that call blocked
 * forever, and this window manager's whole event loop along with it.
 * @a xcb_wait_readable checks, with an explicit timeout, whether the
 * connection has anything to read at all before the caller ever makes
 * that blocking call, using the same @c poll on the connection's
 * file descriptor this project's main event loop ('loop.c') already
 * relies on, so a caller can choose to skip the blocking call entirely
 * instead of risking it.
 *
 * @defgroup utils_xcb XCB protocol helpers
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_XCB_WAIT_H
#define UTILS_XCB_WAIT_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Wait, with a bounded timeout, for @p connection to have
 *        something to read
 *
 * Never reads or consumes anything itself: only observes whether the
 * connection's file descriptor is ready, exactly like @c poll
 * already does for this project's main event loop.  A caller that
 * receives @c true back is free to make its blocking XCB call right
 * afterward in the ordinary way, since data is already known to be
 * waiting for it; on @c false the caller decides for itself what to do
 * instead of blocking indefinitely, e.g., treating this the same way it
 * already treats a reply that turned out to be @c NULL.
 *
 * @param connection  XCB connection
 * @param timeout_ms  How long to wait in milliseconds before giving up
 *
 * @return Status of the wait
 * @retval  true  The connection has data ready to read
 * @retval false  @p connection is @c NULL, the timeout elapsed with
 *                nothing ready, or @c poll itself failed
 *
 * @note Complexity: @e O(1) plus however long the wait itself takes,
 *       bounded by @p timeout_ms
 */
bool xcb_wait_readable(xcb_connection_t *connection, int timeout_ms);


#endif  /* ! UTILS_XCB_WAIT_H */
