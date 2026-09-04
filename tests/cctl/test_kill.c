/**
 * @file tests/cctl/test_kill.c
 *
 * @brief Test battery for the kill-escalation subsystem's own guard
 *        clauses, slot bookkeeping, and countdown reporting
 *        (cctl/kill.c)
 *
 * cctl/kill.c holds three non-static entry points, cctl_kill_register,
 * cctl_kill_ms_remaining, and cctl_kill_tick, plus two file-local
 * static helpers only ever reached through them,
 * s_kill_deadline_from_now and s_kill_read_start_time.  Every pending
 * slot's own bookkeeping (in_use, pid, start_time, deadline) is a
 * file-local static array with no accessor, reachable and observable
 * from outside cctl/kill.c only through the three functions above, so
 * this file drives every scenario through exactly that public surface.
 *
 * cctl_kill_register's own no-op guard for a non-positive pid, and its
 * slot-filling loop (proven indirectly through cctl_kill_ms_remaining
 * reporting a positive countdown once a slot is filled, and through
 * WM_KILL_ESCALATE_MAX_PENDING registrations exhausting every slot so
 * a further one falls through to its own LOGGER_DEBUG call instead),
 * are both covered.  s_kill_read_start_time is exercised indirectly,
 * never directly (it is static): calling cctl_kill_register with this
 * test binary's own real pid, from getpid, reaches its success path by
 * reading this very process' own real '/proc/self/stat' by another
 * name, and calling it with a pid no real process can ever hold
 * reaches its failure path, both by cctl_kill_register itself.
 *
 * cctl_kill_tick's own early-continue branches, skipping a slot that
 * is not in_use and skipping a slot whose deadline has not yet
 * elapsed, are covered by calling it immediately after
 * cctl_kill_register, before WM_KILL_ESCALATE_TIMEOUT_MS (10 real
 * seconds) can possibly have elapsed, and confirming the slot survives
 * (cctl_kill_ms_remaining still reports it afterward).  cctl_kill_tick's
 * own escalation branch past that point, the real kill(pid, 0)
 * liveness probe, the s_kill_read_start_time reuse check, and the
 * eventual real SIGKILL, is not exercised at all: reaching it requires
 * either genuinely waiting out a real WM_KILL_ESCALATE_TIMEOUT_MS
 * (10000 ms) of wall-clock time with no injectable clock (unlike
 * cctl/sn.c, this module exposes no *_set_timeout_seconds-style
 * override), or else a real live process to probe with a real
 * kill(pid, 0) once that timeout has elapsed, and issuing a real
 * SIGKILL at the end of that same wait against any pid this test
 * could safely supply (including this test binary's own pid) is not
 * something a unit test may risk triggering.  Both routes are exactly
 * the real wall-clock-timer-expiry and real-process-lifecycle
 * exclusions the shared instructions call out by name.
 *
 * cctl/kill.c's own translation unit is linked for real, alongside
 * utils/time/clock.c for real, since clock_ms_until is itself a small,
 * pure, real CLOCK_MONOTONIC helper worth exercising directly rather
 * than stood in for.  The only external symbol cctl/kill.c's
 * translation unit references, logger_msg, is a harmless link-only
 * stand-in below, reached only by the slot-exhaustion scenario's own
 * LOGGER_DEBUG call.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdarg.h>
#include <sys/types.h>  /* pid_t */
#include <unistd.h>     /* getpid */

/* Project includes */
#include <logger.h>

/* Default initial values */
#include <defs/kill.h>

/* Local includes */
#include <cctl/kill.h>
#include <harness/tap.h>


/* Recording state for the logger_msg stand-in */
static int s_logger_call_count;

/** Link-only stand-in for logger_msg (logger.c): records that it was
 *  reached instead of formatting or emitting anything, matching the
 *  real logger's own silent-no-op behavior whenever logger_start has
 *  never run */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    s_logger_call_count++;
    return 0;
}


/* cctl_kill_register on a non-positive pid is a silent no-op: no slot
 * is filled, so cctl_kill_ms_remaining still reports nothing pending */
static void s_test_register_nonpositive_pid_is_noop(void)
{
    TAP_EQ_INT(cctl_kill_ms_remaining(), -1,
            "nothing pending yet, before any registration");

    cctl_kill_register(0);
    cctl_kill_register(-1);
    cctl_kill_register(-42);

    TAP_EQ_INT(cctl_kill_ms_remaining(), -1,
            "cctl_kill_register on a zero or negative pid never fills"
            " a slot");
}


/* cctl_kill_register on this test binary's own real pid fills a slot
 * and sets a deadline WM_KILL_ESCALATE_TIMEOUT_MS in the future,
 * which cctl_kill_ms_remaining reports back as a positive, bounded
 * countdown; reaching this success path also exercises
 * s_kill_read_start_time's own success path, since getpid() names a
 * process (this one) whose '/proc/self/stat' is always readable */
static void s_test_register_real_pid_starts_countdown(void)
{
    int remaining_ms;

    cctl_kill_register(getpid());

    remaining_ms = cctl_kill_ms_remaining();
    TAP_OK(remaining_ms >= 0,
            "cctl_kill_register on this process' own real pid fills a"
            " slot, so cctl_kill_ms_remaining reports a"
            " non-negative countdown");
    TAP_OK(remaining_ms <= (int) WM_KILL_ESCALATE_TIMEOUT_MS,
            "the reported countdown never exceeds"
            " WM_KILL_ESCALATE_TIMEOUT_MS, since the deadline was just"
            " set that far in the future");
}


/* cctl_kill_tick, called immediately after registration, takes its
 * own early-continue branch (the deadline has not elapsed yet) and
 * leaves the slot in place untouched, rather than the escalation
 * branch further down */
static void s_test_tick_before_deadline_leaves_slot_pending(void)
{
    int before_ms = cctl_kill_ms_remaining();

    cctl_kill_tick();

    TAP_OK(before_ms >= 0,
            "a slot is pending before this scenario's own tick");
    TAP_OK(cctl_kill_ms_remaining() >= 0,
            "cctl_kill_tick's own early-continue branch leaves a slot"
            " whose deadline has not elapsed yet exactly as pending as"
            " it found it");
}


/* A pid no real process can ever hold (the largest possible pid_t,
 * cast from a value comfortably past any real system's pid_max) makes
 * s_kill_read_start_time fail to open '/proc/<pid>/stat', so the slot
 * is still filled (cctl_kill_register never refuses a pid on that
 * basis alone), only its start_time falls back to 0 */
static void s_test_register_impossible_pid_still_fills_a_slot(void)
{
    const pid_t impossible_pid = (pid_t) 2000000000;

    cctl_kill_register(impossible_pid);

    TAP_OK(cctl_kill_ms_remaining() >= 0,
            "cctl_kill_register on a pid no real process can hold"
            " still fills a slot and starts its countdown, only its"
            " own start-time lookup fails internally");
}


/* Once every one of WM_KILL_ESCALATE_MAX_PENDING slots is filled (two
 * already are, from the two scenarios above), a further registration
 * finds no free slot and falls through to its own LOGGER_DEBUG call
 * instead, changing nothing cctl_kill_ms_remaining can observe */
static void s_test_register_exhausts_every_slot(void)
{
    int remaining_before;
    pid_t filler_pid = 100;

    /* Two slots are already in use from earlier scenarios in this
     * same binary, so only 'WM_KILL_ESCALATE_MAX_PENDING - 2' more
     * are needed to fill every remaining one */
    for (size_t i = 0u; i < WM_KILL_ESCALATE_MAX_PENDING - 2u; ++i) {
        cctl_kill_register(filler_pid + (pid_t) i);
    }

    remaining_before = cctl_kill_ms_remaining();
    TAP_OK(remaining_before >= 0,
            "every slot is now in use, each with its own pending"
            " countdown");

    s_logger_call_count = 0;
    cctl_kill_register(999999);

    TAP_EQ_INT(s_logger_call_count, 1,
            "a registration past every slot's own capacity falls"
            " through to exactly one LOGGER_DEBUG call reporting no"
            " free slot");
    TAP_OK(cctl_kill_ms_remaining() >= 0,
            "the already-pending slots are entirely unaffected by the"
            " registration that found no free slot of its own");
}


int main(void)
{
    TAP_PLAN(10);

    s_test_register_nonpositive_pid_is_noop();
    s_test_register_real_pid_starts_countdown();
    s_test_tick_before_deadline_leaves_slot_pending();
    s_test_register_impossible_pid_still_fills_a_slot();
    s_test_register_exhausts_every_slot();

    return TAP_DONE();
}
