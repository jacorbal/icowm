/**
 * @file wm/kill.c
 *
 * @brief Kill escalation implementation
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
#include <signal.h>     /* kill, SIGKILL */
#include <stdbool.h>
#include <stddef.h>     /* size_t */
#include <stdint.h>     /* uint32_t */
#include <sys/types.h>  /* pid_t */
#include <time.h>       /* clock_gettime, struct timespec */

/* Project includes */
#include <logger.h>

/* Default initial values */
#include <defs/kill.h>

/* Local includes */
#include <wm/kill.h>


/** One process still being watched for kill escalation */
struct kill_pending_s {
    bool in_use;
    pid_t pid;
    struct timespec deadline;   /* @c CLOCK_MONOTONIC */
};

/** Every process currently being watched for kill escalation */
static struct kill_pending_s s_kill_pending[WM_KILL_ESCALATE_MAX_PENDING];


/**
 * @brief Milliseconds remaining until an absolute deadline, floored
 *        at zero rather than going negative once past it
 *
 * Same computation @c wm/shutdown.c's own @a s_shutdown_ms_until
 * already performs.  Kept as its own small copy here rather than
 * shared, the same way that one and @c menu/dialog/defer.c's own
 * equivalent already are two small copies of each other rather than
 * one shared utility.
 *
 * @param due Absolute deadline (@c CLOCK_MONOTONIC) to measure
 *            against
 *
 * @return Milliseconds remaining (never negative), or @c 0 if the
 *         clock itself could not be read
 *
 * @note Complexity: @e O(1)
 */
static int s_kill_ms_until(const struct timespec *due)
{
    struct timespec now;
    long remaining_ms;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    remaining_ms =
        (long) (due->tv_sec - now.tv_sec) * 1000L +
        (due->tv_nsec - now.tv_nsec) / 1000000L;

    return (remaining_ms < 0) ? 0 : (int) remaining_ms;
}


/**
 * @brief Absolute @c CLOCK_MONOTONIC deadline @p timeout_ms from now
 *
 * @param out        Receives the computed deadline
 * @param timeout_ms Milliseconds from now the deadline should fall at
 *
 * @note Leaves @p out zeroed, same as a deadline already in the past,
 *       if the clock itself could not be read
 * @note Complexity: @e O(1)
 */
static void s_kill_deadline_from_now(struct timespec *out,
        uint32_t timeout_ms)
{
    long nsec_sum;

    if (clock_gettime(CLOCK_MONOTONIC, out) != 0) {
        out->tv_sec = 0;
        out->tv_nsec = 0;
        return;
    }

    out->tv_sec += (time_t) (timeout_ms / 1000u);
    nsec_sum = out->tv_nsec + (long) (timeout_ms % 1000u) * 1000000L;
    if (nsec_sum >= 1000000000L) {
        out->tv_sec += 1;
        nsec_sum -= 1000000000L;
    }
    out->tv_nsec = nsec_sum;
}


/* Register a process for kill escalation */
void wm_kill_register(pid_t pid)
{
    if (pid <= 0) {
        return;
    }

    for (size_t i = 0u; i < WM_KILL_ESCALATE_MAX_PENDING; ++i) {
        if (!s_kill_pending[i].in_use) {
            s_kill_pending[i].in_use = true;
            s_kill_pending[i].pid = pid;
            s_kill_deadline_from_now(&s_kill_pending[i].deadline,
                    WM_KILL_ESCALATE_TIMEOUT_MS);
            return;
        }
    }

    LOGGER_DEBUG("No free kill-escalation slot for pid %d;" \
            " leaving 'xcb_kill_client' as the only attempt made",
            (int) pid);
}


/* Milliseconds remaining before the closest pending escalation fires */
int wm_kill_ms_remaining(void)
{
    int closest_ms = -1;

    for (size_t i = 0u; i < WM_KILL_ESCALATE_MAX_PENDING; ++i) {
        int candidate_ms;

        if (!s_kill_pending[i].in_use) {
            continue;
        }

        candidate_ms = s_kill_ms_until(&s_kill_pending[i].deadline);
        if (closest_ms < 0 || candidate_ms < closest_ms) {
            closest_ms = candidate_ms;
        }
    }

    return closest_ms;
}


/* Advance every pending kill escalation */
void wm_kill_tick(void)
{
    for (size_t i = 0u; i < WM_KILL_ESCALATE_MAX_PENDING; ++i) {
        if (!s_kill_pending[i].in_use) {
            continue;
        }

        if (s_kill_ms_until(&s_kill_pending[i].deadline) > 0) {
            continue;
        }

        if (kill(s_kill_pending[i].pid, 0) == 0) {
            LOGGER_NOTICE("Process %d still alive %u ms after" \
                    " 'xcb_kill_client'; forcing it closed with" \
                    " SIGKILL", (int) s_kill_pending[i].pid,
                    (unsigned) WM_KILL_ESCALATE_TIMEOUT_MS);
            (void) kill(s_kill_pending[i].pid, SIGKILL);
        }
        s_kill_pending[i].in_use = false;
    }
}
