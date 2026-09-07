/**
 * @file wm/shutdown.h
 *
 * @brief Coordinated shutdown for the normal quit action
 *
 * When the user confirms quit, every managed client is asked to close
 * itself first (ICCCM @c WM_DELETE_WINDOW where supported), the same
 * way closing one window individually already works, rather than the
 * window manager exiting out from under every running application at
 * once.  A bounded wait lets each application react, e.g., an editor
 * with unsaved changes warning the user before actually closing, before
 * whichever clients are still open past that point get forced closed
 * and the window manager exits anyway.
 *
 * Deliberately never involved in the emergency exit shortcut, which
 * bypasses this (and even the exit session hooks) entirely by design.
 *
 * @see @p config_base_s.shutdown.enable_emergency_shortcut and
 *      @p config_base_s.shutdown.timeout_seconds, both in @c config.h
 *
 * @defgroup wm_shutdown Coordinated shutdown
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_SHUTDOWN_H
#define WM_SHUTDOWN_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <types/handles.h>


/**
 * @brief Begin a coordinated shutdown
 *
 * Asks every currently managed client, across every surface and
 * desktop, to close itself, then lets @a wm_shutdown_tick finish the
 * job once every client has actually closed or the configured timeout
 * elapses, whichever comes first.
 *
 * Requests the window manager's stop directly instead, with no wait at
 * all, when there is nothing to wait for, e.g., no managed clients.
 * Calling this while a shutdown is already in progress has no further
 * effect.
 *
 * @param wm Window manager instance
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across every surface and desktop
 *
 * @see see @a ccmd_client_close in @c cmds/client/focus.h
 */
void wm_shutdown_begin(const wm_td *wm);

/**
 * @brief Milliseconds remaining before the shutdown timeout forces the
 *        remaining clients closed
 *
 * Meant to be folded into the main loop's poll timeout the same way
 * every other timed subsystem's @p *_ms_remaining already is.
 *
 * @return Milliseconds remaining, or a negative value when no shutdown
 *         is currently in progress
 *
 * @note Complexity: @e O(1)
 *
 * @see @a loop_run, obviously located in @c loop.c
 */
int wm_shutdown_ms_remaining(void);

/**
 * @brief Bring one client to the desktop and viewport page the user
 *        is looking at
 *
 * Each axis on its own: being on every desktop, as a pinned client
 * is, says nothing about which page it sits on, and being on every
 * page, as a sticky one is, says nothing about its desktop.  An
 * iconified client is restored first, since a window showing nothing
 * cannot show a dialog either.
 *
 * Placement is untouched.  A dialog is still centered over its
 * parent, ICCCM §4.1.2.6 as before; bringing the parent over is what
 * puts the dialog in view.
 *
 * @param client Client to bring over
 *
 * @note Complexity: @e O(n), where @e n is the size of the client's
 *       transient family
 */
void wm_shutdown_gather_client(client_td *client);

/**
 * @brief Whether a coordinated shutdown is under way
 *
 * Read by @a handler_map_notify so that a client mapping while
 * everything is closing, a "save your work?" dialog above all, is
 * brought to the desktop and viewport page being looked at instead of
 * appearing wherever its parent used to be.
 *
 * @retval  true between @a wm_shutdown_begin and the moment the last
 *               client closes or the timeout elapses
 * @retval false at any other time
 *
 * @note Complexity: @e O(1)
 */
bool wm_shutdown_is_in_progress(void);

/**
 * @brief Advance the shutdown state machine
 *
 * Counts how many managed clients remain open and, once none are left,
 * requests the window manager's stop directly.  Once the configured
 * timeout elapses with clients still open, force-closes every one of
 * them and requests the stop regardless.  Meant to be called once per
 * main loop iteration the same way every other timed subsystem's
 * @c *_tick already is.
 *
 * @param wm Window manager instance
 *
 * @note A no-op unless a shutdown is currently in progress
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across every surface and desktop
 *
 * @see @a ccmd_client_kill in @c cmds/client/focus.h
 */
void wm_shutdown_tick(const wm_td *wm);


#endif  /* ! WM_SHUTDOWN_H */
