/**
 * @file utils/xcb/reply.h
 *
 * @brief Reporting the reason an XCB request failed
 *
 * A request whose reply is waited for reports its failure through an
 * @c xcb_generic_error_t the caller has to ask for and then free.
 * Passing @c NULL instead, which is what most call sites do, reduces
 * every possible failure to the same empty reply, so that a request
 * the server rejected outright and one that simply had nothing to
 * return look identical from here.
 *
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_XCB_REPLY_H
#define UTILS_XCB_REPLY_H


/* XCB includes */
#include <xcb/xcb.h>


/* Public interface */
/**
 * @brief Log why a request failed, and release the error
 *
 * Does nothing when @p error is @c NULL, which is what a request that
 * merely had nothing to return leaves it as, so a call site can hand
 * over whatever it got without testing first.
 *
 * @param error Error the request produced, taken over by this call
 * @param what  What was being asked for, in a form that reads inside
 *              a sentence (e.g., "the screen resources")
 *
 * @note Complexity: @e O(1)
 */
void xcb_reply_log_error(xcb_generic_error_t *error, const char *what);


#endif  /* ! UTILS_XCB_REPLY_H */
