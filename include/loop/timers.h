/**
 * @file loop/timers.h
 *
 * @brief Countdown aggregation for the main event loop
 *
 * Every feature that has to act at a moment of its own rather than on
 * an incoming X event (an auto-closing popup, a blinking urgency hint,
 * a startup-notification cursor, a pending kill escalation) exposes the
 * same pair.  How long is left before it needs attention, and what to
 * do when the loop next comes around.  These two functions gather both
 * halves, so that the loop itself neither knows nor has to be told
 * which features happen to keep a countdown.
 *
 * @ingroup loop
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef LOOP_TIMERS_H
#define LOOP_TIMERS_H


/* Local includes */
#include <loop/context.h>


/* Public interface */
/**
 * @brief Compute how long the loop may block waiting for input
 *
 * Starts from @c WM_EVENT_POLL_TIMEOUT_MS and shortens it to the
 * soonest deadline any active countdown reports, so that a countdown
 * elapses on time instead of waiting for the next unrelated event to
 * wake the loop up.  A countdown that is not running reports a
 * negative value and is skipped.
 *
 * @param ctx Main loop context
 *
 * @return Timeout in milliseconds, never negative
 *
 * @note Complexity: @e O(t), where @e t is the number of countdowns,
 *       a compile-time constant
 */
int loop_timers_timeout(const loop_ctx_td *ctx);

/**
 * @brief Give every countdown its chance to act
 *
 * Called once per loop iteration, right after @c poll returns and
 * before the pending X events are drained, so that whatever a
 * countdown changes on screen is picked up by the same iteration's
 * own render pass.
 *
 * @param ctx Main loop context
 *
 * @note Complexity: @e O(t + n), where @e t is the number of
 *       countdowns and @e n the number of managed clients the ones
 *       that walk them inspect
 */
void loop_timers_tick(const loop_ctx_td *ctx);


#endif  /* ! LOOP_TIMERS_H */
