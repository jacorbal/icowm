/**
 * @file defs/kill.h
 *
 * @brief Timing and capacity for escalating an unresponsive client's
 *        kill from the X11 protocol level to its owning process
 *
 * @ingroup defs
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_KILL_H
#define DEFS_KILL_H


/**
 * @brief Milliseconds to wait, after @a ccmd_client_kill's own
 *        @a xcb_kill_client, before checking whether the owning process
 *        is still alive and escalating to @c SIGKILL if so
 *
 * Long enough that a client merely slow to unwind after losing its
 * X connection, rather than genuinely unresponsive, is never mistaken
 * for the latter.  Short enough that a user who really does need the
 * escalation is not left waiting long for it.  A starting value,
 * expected to be tuned once real use suggests a better one.
 */
#define WM_KILL_ESCALATE_TIMEOUT_MS (10000u)    /* 10 seconds */

/**
 * @brief Maximum number of process kills that can be pending escalation
 *        at once
 *
 * A person forcing several unresponsive clients closed in quick
 * succession, before the first one's own timeout has even elapsed, is
 * unusual but not implausible.  Further kills past this many pending at
 * once simply skip escalation, leaving their own @a xcb_kill_client as
 * the only attempt made for them.
 */
#define WM_KILL_ESCALATE_MAX_PENDING (8u)


#endif  /* ! DEFS_KILL_H */
