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
 *  - @c 'long clock_ms_until(const struct timespec *due)'
 *  - @c 'long clock_ms_since(const struct timespec *start)'
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


#endif  /* ! UTILS_TIME_CLOCK_H */
