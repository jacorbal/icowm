/**
 * @file loop/signals.h
 *
 * @brief Deferred signal handling for the main event loop
 *
 * The installed signal handlers do nothing but raise a flag (see
 * @c wm/startup/handle.h), since almost nothing a window manager does
 * is safe to do from a handler.  Acting on those flags happens here,
 * once per loop iteration, with the whole process back in a state
 * where reloading a configuration or reaping a child is safe.
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

#ifndef LOOP_SIGNALS_H
#define LOOP_SIGNALS_H


/* System includes */
#include <stdbool.h>

/* Local includes */
#include <loop/context.h>


/* Public interface */
/**
 * @brief Act on whichever signal flags were raised since last checked
 *
 * Reloads the configuration for @c SIGHUP, re-establishes the input
 * grabs for @c SIGCONT, reaps exited children for @c SIGCHLD, and
 * reports a termination request back to the caller.  Every flag is
 * cleared by the query that reads it, so a signal raised while this
 * runs is picked up on the next iteration rather than lost.
 *
 * @param ctx Main loop context
 *
 * @return @c false when a termination signal asked the window manager
 *         to stop, @c true when the loop should keep running
 *
 * @note Complexity: @e O(n * b), where @e n is the number of managed
 *       surfaces and @e b the number of configured bindings, paid
 *       only on a @c SIGCONT that re-grabs input
 */
bool loop_signals_process(const loop_ctx_td *ctx);


#endif  /* ! LOOP_SIGNALS_H */
