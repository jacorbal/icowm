/**
 * @file loop/pollset.h
 *
 * @brief Descriptor set the main event loop waits on
 *
 * The loop blocks on two kinds of descriptor at once.  The X
 * connection, whose readiness is then drained through XCB rather than
 * read directly, and however many IPC descriptors are currently open,
 * the listening socket included.  Building that set, waiting on it,
 * and handing the ready IPC descriptors to their handler all
 * happen here.
 *
 * @ingroup loop
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef LOOP_POLLSET_H
#define LOOP_POLLSET_H


/* System includes */
#include <stdbool.h>

/* Local includes */
#include <loop/context.h>


/* Public interface */
/**
 * @brief Wait for the X connection or any IPC descriptor to be ready
 *
 * Checks the connection for a fatal error first, since a window
 * manager whose socket to the X server is gone has nothing left to
 * wait for, then blocks for at most @p timeout_ms and dispatches
 * whichever IPC descriptors came back readable.  The X connection's
 * own readiness needs no dispatch here.  The caller drains it with
 * @c xcb_poll_for_event right afterward.
 *
 * @param ctx        Main loop context
 * @param timeout_ms Longest time to block, in milliseconds
 *
 * @return @c false when the connection failed or @c poll itself did,
 *         @c true when the loop should keep running
 *
 * @note An interrupted @c poll is not an error
 * @note The signal that interrupted it is acted upon at the top of the
 *       next iteration, by @a loop_signals_process
 * @note Complexity: @e O(c), where @e c is the number of connected
 *       IPC clients
 */
bool loop_pollset_wait(const loop_ctx_td *ctx, int timeout_ms);


#endif  /* ! LOOP_POLLSET_H */
