/**
 * @file utils/time/clock.c
 *
 * @brief Shared @c CLOCK_MONOTONIC helper implementation
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
#include <time.h>       /* clock_gettime, CLOCK_MONOTONIC, struct timespec */

/* Local includes */
#include <utils/time/clock.h>


/* Milliseconds remaining from now until a future point in time */
long clock_ms_until(const struct timespec *due)
{
    struct timespec now;
    long remaining_ms;

    if (due == NULL || clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0L;
    }

    remaining_ms = (due->tv_sec - now.tv_sec) * 1000L +
        (due->tv_nsec - now.tv_nsec) / 1000000L;

    return (remaining_ms < 0L) ? 0L : remaining_ms;
}


/* Milliseconds elapsed since a past point in time */
long clock_ms_since(const struct timespec *start)
{
    struct timespec now;
    long elapsed_ms;

    if (start == NULL || clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0L;
    }

    elapsed_ms = (now.tv_sec - start->tv_sec) * 1000L +
        (now.tv_nsec - start->tv_nsec) / 1000000L;

    return (elapsed_ms < 0L) ? 0L : elapsed_ms;
}


/* Advance a moment in time by a number of milliseconds */
void clock_add_ms(struct timespec *ts, unsigned int ms)
{
    unsigned long nsec;

    if (ts == NULL) {
        return;
    }

    ts->tv_sec += (time_t) (ms / 1000u);

    nsec = (unsigned long) ts->tv_nsec +
        (unsigned long) (ms % 1000u) * 1000000UL;
    if (nsec >= 1000000000UL) {
        ts->tv_sec += 1;
        nsec -= 1000000000UL;
    }

    ts->tv_nsec = (long) nsec;
}
