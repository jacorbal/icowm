/**
 * @file utils/time/clock.h
 *
 * @brief Declarations for shared @c CLOCK_MONOTONIC helpers
 *
 * Every one of these takes or returns a millisecond count derived from
 * @c CLOCK_MONOTONIC, replacing what was, before this file existed,
 * the exact same handful of lines re-derived independently in each of
 * a dozen or so different translation units across the project (a
 * countdown until some future deadline, or an elapsed time since some
 * past mark).
 *
 * Functions:
 *
 * @code{.c}
 * long clock_ms_until(const struct timespec *due)
 * long clock_ms_since(const struct timespec *start)
 * @endcode
 *
 * @ingroup utils_time
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L /* struct timespec */
#endif

#ifndef UTILS_TIME_CLOCK_H
#define UTILS_TIME_CLOCK_H


/* System includes */
#include <time.h>       /* struct timespec */


/**
 * @brief Milliseconds remaining from now, on @c CLOCK_MONOTONIC, until
 *        @p due
 *
 * @param due Future point in time, from an earlier @c clock_gettime
 *            call on @c CLOCK_MONOTONIC
 *
 * @return Milliseconds remaining until @p due
 * @retval 0 @p due has already passed, @p due is @c NULL, or the
 *           underlying @c clock_gettime call itself failed
 *
 * @note Never negative: a @p due already in the past returns 0,
 *       never a negative count of milliseconds
 * @note Complexity: @e O(1)
 */
long clock_ms_until(const struct timespec *due);

/**
 * @brief Milliseconds elapsed on @c CLOCK_MONOTONIC since @p start
 *
 * @param start Past point in time, from an earlier @c clock_gettime
 *              call on @c CLOCK_MONOTONIC
 *
 * @return Milliseconds elapsed since @p start
 * @retval 0 @p start is @c NULL, the underlying @c clock_gettime call
 *           itself failed, or @p start is somehow still in the future
 *           (@c CLOCK_MONOTONIC never legitimately goes backward, so
 *           this should never actually happen)
 *
 * @note Never negative
 * @note Complexity: @e O(1)
 */
long clock_ms_since(const struct timespec *start);


/**
 * @brief Advance a moment in time by a number of milliseconds
 *
 * @param ts Moment to advance, in place
 * @param ms Milliseconds to add
 *
 * @note The nanosecond carry is computed in unsigned arithmetic on
 *       purpose: comparing a signed sum against one whole second is
 *       what lets the optimizer assume that sum never overflows, and
 *       @c -Wstrict-overflow reports on exactly that
 * @note Complexity: @e O(1)
 */
void clock_add_ms(struct timespec *ts, unsigned int ms);


#endif  /* ! UTILS_TIME_CLOCK_H */
