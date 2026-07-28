/**
 * @file startup.h
 *
 * @brief WM startup helpers: root event subscription, signal handling
 *
 * Declares the three startup functions for subscribing to root window
 * events, installing signal handlers, and querying whether a stop
 * signal has been received.  All signal state is private to the
 * implementation.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef STARTUP_H
#define STARTUP_H


/* System includes */
#include <stdbool.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <wm.h>


/* Public interface */
/**
 * @brief Subscribe to root window events on all managed surfaces
 *
 * Installs the event mask required for the window manager to receive
 * @c SUBSTRUCTURE_REDIRECT, key press/release, button press/release and
 * property-change events.  Fails with @c -1 if another window manager
 * is already running on any root.
 *
 * @param wm Window manager state (connection and surfaces)
 *
 * @return 0 on success, -1 if registration fails on any surface
 *
 * @note Complexity: @e O(s), where @e s is the number of surfaces
 */
int startup_subscribe_root_events(wm_td *wm);

/**
 * @brief Install POSIX signal handlers for graceful termination
 *
 * Registers @c s_startup_handle_signal for @c SIGHUP, @c SIGINT,
 * @c SIGQUIT, and @c SIGTERM so that any of these signals trigger
 * a graceful shutdown rather than an abrupt kill.
 *
 * @return 0 on success, -1 if @c sigaction fails
 *
 * @note Complexity: @e O(1)
 */
int startup_install_signals(void);

/**
 * @brief Query whether a termination signal has been received
 *
 * @return @c true when a signal has set the internal flag
 *
 * @note Complexity: @e O(1)
 */
bool startup_stop_requested(void);


#endif  /* ! STARTUP_H */
