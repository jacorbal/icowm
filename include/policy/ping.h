/**
 * @file policy/ping.h
 *
 * @brief Periodic @c _NET_WM_PING liveness probing interface
 *
 * Sends every client that advertises @c _NET_WM_PING support a fresh
 * probe every @c WM_EWMH_PING_INTERVAL_SECONDS, and marks a client
 * unresponsive once @c WM_EWMH_PING_TIMEOUT_SECONDS worth of those have
 * gone unanswered.  @a handler_message_event (in @c handler/message.c)
 * clears that mark again the moment any reply, even a late one, finally
 * arrives.  All scan state is private to the implementation.
 *
 * @ingroup policy
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PING_H
#define POLICY_PING_H


/* Project includes */
#include <types/handles.h>


/**
 * @brief Probe every ping-capable client and age out unanswered ones
 *
 * Meant to be called once per main-loop iteration, unconditionally, the
 * same way @a urgency_blink_tick is.  Walks every client across every
 * surface and desktop; a client that does not advertise @c _NET_WM_PING
 * support is skipped entirely.
 *
 * At the actual @c WM_EWMH_PING_INTERVAL_SECONDS cadence, each
 * remaining client is sent a fresh probe (@a ccmd_client_ping_send,
 * in @c cmds/client/ewmh.h), and one still waiting on a previous
 * probe's reply has its pending-round count advanced; once that count
 * covers @c WM_EWMH_PING_TIMEOUT_SECONDS worth of rounds, the client is
 * marked unresponsive.
 *
 * @param surfaces All managed surfaces
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients
 *
 * @see @c WM_EWMH_PING_INTERVAL_SECONDS and
 *      @c WM_EWMH_PING_TIMEOUT_SECONDS in @c defs/ewmh.h
 */
void ping_tick(list_td *surfaces);

/**
 * @brief How many milliseconds until the ping cycle next needs a tick
 *
 * @return Milliseconds until the next probing round
 * @retval -1 no client currently advertises @c _NET_WM_PING support (as
 *            of the most recent @a ping_tick call), so there is nothing
 *            to probe
 *
 * @note Complexity: @e O(1)
 */
int ping_ms_remaining(void);


#endif  /* ! POLICY_PING_H */
