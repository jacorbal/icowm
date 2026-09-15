/**
 * @file wm/startup/handle.h
 *
 * @brief Signal handlers and the flags they set, queried back by the
 *        main loop
 *
 * Keeps each flag together with both the handler that writes it and the
 * query that reads and clears it, avoiding an extern between this file
 * and any other.
 *
 * @ingroup startup
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_STARTUP_HANDLE_H
#define WM_STARTUP_HANDLE_H


/* System includes */
#include <stdbool.h>


/**
 * @brief Signal handler for termination signals
 *
 * Records the signal number; the actual shutdown is handled from the
 * normal execution context in the main loop via
 * @c wm_startup_requested_stop.
 *
 * @param signum Number of the received signal
 *
 * @note Complexity: @e O(1)
 */
void wm_startup_handle_signal(int signum);

/**
 * @brief Signal handler for @c SIGHUP (configuration reload)
 *
 * Sets a flag consumed by @c wm_startup_requested_reload.  The actual
 * reload is deferred to the main loop so that it runs in a safe context
 * without async-signal-safety constraints.
 *
 * @param signum Number of the received signal (always @c SIGHUP)
 *
 * @note Complexity: @e O(1)
 */
void wm_startup_handle_reload(int signum);

/**
 * @brief Signal handler for @c SIGCONT (VT resume)
 *
 * Sets a flag consumed by @a wm_startup_requested_resume so that the
 * main loop can re-establish keyboard and mouse grabs after returning
 * from a virtual-terminal switch.
 *
 * @param signum Number of the received signal (always @c SIGCONT)
 *
 * @note Complexity: @e O(1)
 */
void wm_startup_handle_resume(int signum);

/**
 * @brief Signal handler for @c SIGCHLD
 *
 * Defers child reaping to the main loop so @c waitpid is only called in
 * normal execution context.
 *
 * @param signum Number of the received signal (always @c SIGCHLD)
 *
 * @note Complexity: @e O(1)
 */
void wm_startup_handle_child(int signum);

/**
 * @brief Async-signal-safe handler for fatal signals
 *
 * See @a wm_startup_install_crash_handlers in @c wm/startup/install.h
 * for the full reasoning: this cannot recover and keep running, only
 * make sure dying is not silent.  Every operation here is restricted to
 * what POSIX guarantees is safe from within a signal handler.
 * The @c write syscall directly to standard error, a hand-rolled
 * digit-by-digit conversion of the signal number (never @c snprintf or
 * similar, which are not on the guaranteed-safe list), @c sigaction to
 * restore the signal's default disposition, and @c raise to re-deliver
 * it so the process actually terminates through the normal mechanism
 * afterward.
 *
 * @a logger_emergency_flush is called too, which is the one thing here
 * that touches the logger at all.  It goes nowhere near that module's
 * ordinary buffered, allocating path: it writes the pending messages
 * out with @c write and returns, leaving the buffer as it found it.
 * Without it the messages leading up to a crash die with the process,
 * which are the ones worth reading afterwards.
 *
 * @param signum Number of the received fatal signal
 *
 * @note Complexity: @e O(1)
 */
void wm_startup_handle_crash(int signum);

/**
 * @brief Query whether a termination signal has been received
 *
 * @return @c true once a termination signal (SIGINT, SIGQUIT, SIGTERM)
 *         has been received
 *
 * @note Complexity: @e O(1)
 */
bool wm_startup_requested_stop(void);

/**
 * @brief Query whether a @c SIGHUP configuration-reload request was
 *        received
 *
 * @return @c true exactly once per @c SIGHUP received, clearing the
 *         flag on each call that returns @c true
 *
 * @note Complexity: @e O(1)
 */
bool wm_startup_requested_reload(void);

/**
 * @brief Query whether a @c SIGCONT (VT resume) was received
 *
 * @return @c true exactly once per @c SIGCONT received, clearing the
 *         flag on each call that returns @c true
 *
 * @note Complexity: @e O(1)
 */
bool wm_startup_requested_resume(void);

/**
 * @brief Query whether a pending child-reap request was received
 *
 * @return @c true exactly once per @c SIGCHLD received, clearing the
 *         flag on each call that returns @c true
 *
 * @note Complexity: @e O(1)
 */
bool wm_startup_requested_child_reap(void);


#endif  /* ! WM_STARTUP_HANDLE_H */
