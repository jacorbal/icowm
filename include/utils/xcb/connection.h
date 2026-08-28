/**
 * @file utils/xcb/connection.h
 *
 * @brief The session's own connections to the X server
 *
 * One connection is opened at start-up and used until the window
 * manager exits, alongside the EWMH connection built over it.  Both
 * are held here for the same reason and are set together.
 *
 * Kept here rather than carried from one place to the next: each was a
 * member of the client, the desktop and the surface alike, all holding
 * the same pointer, and a parameter of every function that had to
 * reach one of them.
 *
 * That is the sort of thing that looks harmless and is not.  Nothing
 * outside this file can tell whether those copies are the same
 * connection, so nothing can rely on it; a function needing to talk to
 * the server has to be handed something that happens to carry one,
 * which decides its signature for reasons that have nothing to do with
 * what it does.
 *
 * The connection is set once, by @a wm_init, and read from anywhere
 * after that.
 *
 * @defgroup xcbconn X server connection
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_XCB_CONNECTION_H
#define UTILS_XCB_CONNECTION_H


/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>


/**
 * @brief Record the connection this session talks to the server over
 *
 * @param connection Connection just opened; may be @c NULL to forget
 *                   the one held, which the shutdown path does
 *
 * @note Called once, from @a wm_init, before anything else needs it
 * @note Complexity: @e O(1)
 */
void xcb_connection_set(xcb_connection_t *connection);

/**
 * @brief The connection this session talks to the server over
 *
 * @return That connection, or @c NULL before one has been opened
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void);

/**
 * @brief Record the EWMH connection built over the session's own
 *
 * @param ewmh EWMH connection just set up; may be @c NULL to forget
 *             the one held, which the shutdown path does
 *
 * @note Called once, from @a wm_init, once its atoms have been
 *       interned
 * @note Complexity: @e O(1)
 */
void xcb_ewmh_connection_set(xcb_ewmh_connection_t *ewmh);

/**
 * @brief The EWMH connection built over the session's own
 *
 * @return That connection, or @c NULL before one has been set up
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void);


#endif /* !UTILS_XCB_CONNECTION_H */
