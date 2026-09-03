/**
 * @file cctl/kill.c
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
#include <signal.h>     /* kill, SIGKILL, size_t */
#include <stdbool.h>
#include <stdint.h>     /* uint32_t */
#include <stdio.h>      /* fopen, fclose, fscanf */
#include <string.h>     /* strrchr */
#include <sys/types.h>  /* pid_t */
#include <time.h>       /* clock_gettime, timespec */

/* Project includes */
#include <logger.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Default initial values */
#include <defs/kill.h>

/* Local includes */
#include <cctl/kill.h>


/** One process still being watched for kill escalation */
struct kill_pending_s {
    struct timespec deadline;   /* 'CLOCK_MONOTONIC' */
    unsigned long long start_time;   /* '/proc/<pid>/stat' field 22 */
    pid_t pid;
    bool in_use;
};

/** Every process currently being watched for kill escalation */
static struct kill_pending_s s_kill_pending[WM_KILL_ESCALATE_MAX_PENDING];




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


/**
 * @brief Read a process' start time (in kernel jiffies since boot)
 *        from @c /proc/<pid>/stat
 *
 * Used to tell a still-running process apart from a different one
 * that happens to reuse the same @c pid_t after the original one
 * has already exited, since a @c pid_t is only unique while its
 * process is alive.
 *
 * @param pid Process to read the start time of
 * @param out Receives the parsed start time
 *
 * @return @c true if @c /proc/<pid>/stat was read and the start time
 *         field was found
 *
 * @note Complexity: @e O(1)
 */
static bool s_kill_read_start_time(pid_t pid, unsigned long long *out)
{
    char path[32];
    char line[512];
    FILE *f;
    char *paren;
    int n;

    n = snprintf(path, sizeof(path), "/proc/%d/stat", (int) pid);
    if (n < 0 || (size_t) n >= sizeof(path)) {
        return false;
    }

    f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }

    if (fgets(line, (int) sizeof(line), f) == NULL) {
        fclose(f);
        return false;
    }
    fclose(f);

    /* Skip past 'pid (comm)': the command name is parenthesized and
     * may itself contain spaces or parentheses, so the fields that
     * matter are only ever found safely by searching for the last
     * ')' on the line, never by counting whitespace-separated tokens
     * from the start */
    paren = strrchr(line, ')');
    if (paren == NULL) {
        return false;
    }

    /* 'starttime' is the 20th whitespace-separated field after
     * the comm's closing ')', per 'proc(5)' */
    if (sscanf(paren + 1,
                " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u" \
                " %*u %*u %*d %*d %*d %*d %*d %*d %llu",
                out) != 1) {
        return false;
    }

    return true;
}


/* Register a process for kill escalation */
void cctl_kill_register(pid_t pid)
{
    if (pid <= 0) {
        return;
    }

    for (size_t i = 0u; i < WM_KILL_ESCALATE_MAX_PENDING; ++i) {
        if (!s_kill_pending[i].in_use) {
            s_kill_pending[i].in_use = true;
            s_kill_pending[i].pid = pid;
            if (!s_kill_read_start_time(pid,
                        &s_kill_pending[i].start_time)) {
                s_kill_pending[i].start_time = 0ull;
            }
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
int cctl_kill_ms_remaining(void)
{
    int closest_ms = -1;

    for (size_t i = 0u; i < WM_KILL_ESCALATE_MAX_PENDING; ++i) {
        int candidate_ms;

        if (!s_kill_pending[i].in_use) {
            continue;
        }

        candidate_ms = (int) clock_ms_until(&s_kill_pending[i].deadline);
        if (closest_ms < 0 || candidate_ms < closest_ms) {
            closest_ms = candidate_ms;
        }
    }

    return closest_ms;
}


/* Advance every pending kill escalation */
void cctl_kill_tick(void)
{
    for (size_t i = 0u; i < WM_KILL_ESCALATE_MAX_PENDING; ++i) {
        if (!s_kill_pending[i].in_use) {
            continue;
        }

        if (clock_ms_until(&s_kill_pending[i].deadline) > 0) {
            continue;
        }

        if (kill(s_kill_pending[i].pid, 0) == 0) {
            unsigned long long current_start_time;
            bool same_process = (s_kill_pending[i].start_time == 0ull) ||
                (s_kill_read_start_time(s_kill_pending[i].pid,
                        &current_start_time) &&
                    current_start_time == s_kill_pending[i].start_time);

            if (same_process) {
                LOGGER_NOTICE("Process %d still alive %u ms after" \
                        " 'xcb_kill_client'; forcing it closed with" \
                        " SIGKILL", (int) s_kill_pending[i].pid,
                        (unsigned) WM_KILL_ESCALATE_TIMEOUT_MS);
                (void) kill(s_kill_pending[i].pid, SIGKILL);
            } else {
                LOGGER_DEBUG("pid %d was reused by a different" \
                        " process before its kill escalation fired;" \
                        " skipping SIGKILL", (int) s_kill_pending[i].pid);
            }
        }
        s_kill_pending[i].in_use = false;
    }
}
