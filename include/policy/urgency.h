/**
 * @file policy/urgency.h
 *
 * @brief Urgent-client attention blink interface
 *
 * Periodically swaps an urgent client's titlebar (and, if iconified,
 * its icon) between its normal colors and the opposite active/ inactive
 * color set, without ever touching the font, so it draws
 * the user's attention the same way a taskbar's flashing entry does on
 * other desktops.  All blink state is private to the implementation.
 *
 * @ingroup policy
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_URGENCY_H
#define POLICY_URGENCY_H


/* System includes */
#include <stdbool.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>


/**
 * @brief Query the current blink phase
 *
 * @return State of the query
 * @retval  true during the "swapped colors" half of the blink cycle
 * @retval false during the "normal colors" half
 *
 * @note Complexity: @e O(1)
 */
bool urgency_blink_is_on(void);

/**
 * @brief Advance the blink cycle and repaint whatever it changed
 *
 * Meant to be called once per main-loop iteration, unconditionally, the
 * same way @p systray_clock_tick is.  Scans every client across every
 * surface and desktop for the urgent flag; a no-op when none is found.
 *
 * When at least one is found and
 * @p config->a11y.urgency.blink_interval_ms (@c config.h) has elapsed
 * since the last phase change, flips the phase and repaints every
 * urgent client's titlebar (and icon, if iconified) to match.  Also
 * sounds an audible bell right on the transition into urgency when
 * @p config->a11y.urgency.audible_bell is set.
 *
 * @param surfaces All managed surfaces
 * @param config   Active configuration; a @c NULL falls back to the
 *                 built-in and never sounds a bell
 *
 * @note The bell depends on the configuration of XBell
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients
 *
 * @see @c WM_URGENCY_BLINK_INTERVAL_MS in @c defs/urgency.h
 */
void urgency_blink_tick(list_td *surfaces, const config_td *config);

/**
 * @brief How many milliseconds until the blink cycle next needs a
 *        tick
 *
 * @param config Active configuration; a @c NULL falls back to the
 *               built-in @c WM_URGENCY_BLINK_INTERVAL_MS
 *               (defs/urgency.h)
 *
 * @return Milliseconds until the next phase change, or @c -1 when no
 *         client is currently urgent (as of the most recent
 *         @a urgency_blink_tick call) and nothing needs to blink
 *
 * @note Complexity: @e O(1)
 */
int urgency_blink_ms_remaining(const config_td *config);


#endif  /* ! POLICY_URGENCY_H */
