/**
 * @file systray/clock.h
 *
 * @brief Systray clock widget: redraw timing and the redraw itself
 *
 * Split out of @c systray.h, alongside @c systray/handle.h and
 * @c systray/icon.h, so a file that only needs one of these does not
 * also pull in, and rebuild against, every other unrelated concern
 * declared alongside it.
 *
 * @see @c systray.h
 *
 * @ingroup systray
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SYSTRAY_CLOCK_H
#define SYSTRAY_CLOCK_H


/**
 * @brief How many milliseconds until the systray clock needs its next
 *        redraw
 *
 * Meant for the main event loop's @p poll timeout.  Call this once per
 * iteration and use the result to shorten the timeout when it is
 * smaller, the same way the info popup and desktop-switch notification
 * already do, so the clock's displayed text advances promptly at each
 * wall-clock second instead of only when some unrelated X event happens
 * to wake the loop up.
 *
 * @return Milliseconds until next redraw
 * @retval  0 when a redraw is due right now
 * @retval -1 when the clock is disabled or the tray does not currently
 *            own the systray selection (nothing to redraw)
 * @retval  n with @c (n > 0), a small positive number of milliseconds
 *            until next redraw
 *
 * @note Complexity: @e O(1)
 */
int systray_clock_ms_remaining(void);

/**
 * @brief Redraw the systray clock if the wall-clock second has changed
 *        since it was last drawn
 *
 * Call this once per main-loop iteration, after @p poll returns,
 * regardless of whether it returned due to an X event or a timeout.
 *
 * @note A no-op when the clock is disabled, the tray does not own the
 *       systray selection, or less than a second has passed since the
 *       last redraw
 * @note Complexity: @e O(1) plus whatever the tray's reflow costs
 *       when a redraw actually happens (seek its complexity note,
 *       if thou wouldst know)
 */
void systray_clock_tick(void);


#endif  /* ! SYSTRAY_CLOCK_H */
