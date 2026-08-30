/**
 * @file cctl/kill.h
 *
 * @brief Escalating an unresponsive client's kill from the X11 protocol
 *        level to its owning process
 *
 * @a ccmd_client_kill (@c cmds/client/focus.h) already terminates
 * a client's connection to the X server via @a xcb_kill_client,
 * which is enough for the common case.  Losing that connection is
 * normally fatal to whatever toolkit the client is built on, so the
 * process exits on its own shortly after.  A genuinely unresponsive
 * client, stuck in some loop that never processes its X connection
 * at all, never notices that loss and keeps running regardless, even
 * though its window has already vanished from every list the window
 * manager itself keeps.
 *
 * This module tracks a client's real OS process (its @c pid_t, read
 * from @c _NET_WM_PID where the client publishes one) for a bounded
 * window after @a ccmd_client_kill already ran, and sends it a real
 * @c SIGKILL, a signal no process can ignore or catch, if it is still
 * alive once that window elapses.  A client that already exited on its
 * own in the meantime is left alone.
 *
 * @see @a cctl_kill_register, called from @a ccmd_client_kill itself
 * @see @c defs/kill.h for the timeout and pending-slot count this
 *      module itself is bound by
 *
 * @defgroup wm_kill Kill escalation
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CCTL_KILL_H
#define CCTL_KILL_H


/* System includes */
#include <sys/types.h>  /* pid_t */


/**
 * @brief Register a process for kill escalation, unless a PID is
 *        unavailable or every pending slot is already in use
 *
 * Meant to be called once, right after @a ccmd_client_kill's
 *       @a xcb_kill_client, with that same client's
 *       @p client_td.process.pid.
 *
 * @param pid Process to watch; a no-op if not strictly positive
 *            (@p client_td.process.pid defaults to @c -1 for a
 *            client that never published @c _NET_WM_PID)
 *
 * @note A no-op, silently, once @c WM_KILL_ESCALATE_MAX_PENDING
 *       registrations are already pending at once
 * @note Complexity: @e O(n), where @e n is
 *       @c WM_KILL_ESCALATE_MAX_PENDING
 */
void cctl_kill_register(pid_t pid);

/**
 * @brief Milliseconds remaining before the closest pending escalation
 *        fires
 *
 * Meant to be folded into the main loop's poll timeout the same way
 * every other timed subsystem's @p *_ms_remaining already is.
 *
 * @return Milliseconds remaining before the soonest pending escalation,
 *         or a negative value when nothing is pending
 *
 * @note Complexity: @e O(n), where @e n is
 *       @c WM_KILL_ESCALATE_MAX_PENDING
 *
 * @see @a loop_run, obviously located in @c loop.c
 */
int cctl_kill_ms_remaining(void);

/**
 * @brief Advance every pending kill escalation
 *
 * For each pending registration whose timeout has elapsed, checks
 * whether its process is still alive and sends it @c SIGKILL if so,
 * then frees its slot either way.  Meant to be called once per main
 * loop iteration the same way every other timed subsystem's
 * @p *_tick already is.
 *
 * @note Complexity: @e O(n), where @e n is
 *       @c WM_KILL_ESCALATE_MAX_PENDING
 */
void cctl_kill_tick(void);


#endif  /* ! CCTL_KILL_H */
