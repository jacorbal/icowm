/**
 * @file startup.h
 *
 * @brief Window manager startup helpers declaration
 *
 * Declares helpers for subscribing to root-window events, installing
 * signal handlers, and querying deferred signal work that the main loop
 * must process in normal execution context.
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
 * @brief Initialize optional XRandR support
 *
 * Probes the XRandR extension, stores extension metadata in @p wm, and
 * negotiates a compatible protocol version when available.
 *
 * @param wm Window manager state
 *
 * @return 0 on success or when XRandR is unavailable, -1 on fatal input
 */
int startup_randr_init(wm_td *wm);

/**
 * @brief Probe XSync extension support and cache metadata in @p wm
 *
 * Used for @c _NET_WM_SYNC_REQUEST: caches the base event code so
 * @c AlarmNotify events can be recognized in the main loop.  No
 * per-root event subscription is required for the XSync extension
 * (unlike XRandR); alarms deliver their notifications directly to the
 * connection that created them.
 *
 * @param wm Window manager state
 *
 * @return 0 on success or when XSync is unavailable, -1 on fatal input
 */
int startup_sync_init(wm_td *wm);

/**
 * @brief Subscribe XRandR change notifications on every managed root
 *
 * Registers interest in monitor/output/screen-change notifications for
 * each managed surface root when XRandR is available.
 *
 * @param wm Window manager state
 *
 * @return 0 on success, -1 on invalid input
 */
int startup_subscribe_randr_events(wm_td *wm);

/**
 * @brief Install POSIX signal handlers for graceful termination
 *
 * Registers deferred handlers for graceful shutdown, configuration
 * reload, VT resume, and child reaping.  @c SIGHUP requests a config
 * reload, @c SIGINT/@c SIGQUIT/@c SIGTERM request shutdown, @c SIGCONT
 * requests input-grab restoration, and @c SIGCHLD schedules zombie
 * reaping in the main loop.
 *
 * @return 0 on success, -1 if @c sigaction fails
 *
 * @note Complexity: @e O(1)
 */
int startup_install_signals(void);

/**
 * @brief Install handlers for fatal signals (@c SIGSEGV, @c SIGABRT,
 *        @c SIGBUS, @c SIGFPE) that log a diagnostic before dying
 *
 * These cannot recover and keep running: by the time one of these
 * signals arrives, the process's own memory state may already be
 * corrupted, and continuing to issue X requests from that state risks
 * doing more damage (to other clients, or to the X server itself)
 * than simply dying would.  What they do instead is make sure dying
 * is not silent: each writes a short diagnostic naming the signal
 * directly to standard error using nothing but the @c write syscall
 * (the only output primitive POSIX guarantees safe to call from a
 * signal handler; the logger's own buffered, allocating machinery is
 * not), then restores that signal's default disposition and re-raises
 * it, so the process actually terminates through the normal mechanism
 * afterward (a core dump, if the system is configured to produce one,
 * and the correct exit status reported to whatever started icowm).
 *
 * A client application crashing on its own (for example, failing its
 * own graphics initialization) does not, by itself, send icowm any
 * signal at all; installing this does not change whether such a
 * crash brings icowm down with it; see @c xcb_connection_has_error's
 * own handling in loop.c for the separate, and far more likely,
 * mechanism behind that (the X server itself becoming unreachable,
 * which no window manager can recover from either, being just
 * another client of that same server) instead.
 *
 * @return 0 on success, -1 if @c sigaction fails
 *
 * @note Complexity: @e O(1)
 */
int startup_install_crash_handlers(void);

/**
 * @brief Query whether a termination signal has been received
 *
 * @return @c true when a signal has set the internal flag
 *
 * @note Complexity: @e O(1)
 */
bool startup_stop_requested(void);

/**
 * @brief Query whether a @c SIGHUP configuration-reload was requested
 *
 * Returns @c true and clears the internal flag on the first call after
 * a @c SIGHUP is received; subsequent calls return @c false until the
 * next signal.
 *
 * @return @c true if a reload was requested since the last call
 *
 * @note Complexity: @e O(1)
 */
bool startup_reload_requested(void);

/**
 * @brief Query whether a @c SIGCONT (VT resume) was received
 *
 * Returns @c true and clears the internal flag on the first call after
 * @c SIGCONT is received; subsequent calls return @c false until the
 * next signal.  The main loop uses this to re-establish keyboard and
 * mouse grabs after a virtual-terminal switch.
 *
 * @return @c true if a VT resume was requested since the last call
 *
 * @note Complexity: @e O(1)
 */
bool startup_resume_requested(void);

/**
 * @brief Query whether terminated children should be reaped
 *
 * Returns @c true and clears the internal flag on the first call after
 * a @c SIGCHLD is received; subsequent calls return @c false until the
 * next child-termination signal.
 *
 * @return @c true if child reaping is pending
 *
 * @note Complexity: @e O(1)
 */
bool startup_child_reap_requested(void);


#endif  /* ! STARTUP_H */
