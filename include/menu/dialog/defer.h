/**
 * @file menu/dialog/defer.h
 *
 * @brief Deferred click-triggered close/accept, shared by every modal
 *        dialog that closes itself in response to a button click
 *
 * A mouse click on a dialog's own button changes which one is selected,
 * and that needs to actually be visible on screen for a moment before
 * the dialog goes away, or the click reads as though it did not
 * register at the right spot even though it did.  Closing on the very
 * same repaint that shows the new selection would not give a user any
 * real chance to perceive it, since screen updates too fast and human
 * perception has none of a Vulcan's freaking reflexes.  Blocking with
 * a sleep to wait one out would freeze the whole window manager's event
 * loop for that long instead.
 *
 * This module holds exactly one pending deferred action at a time,
 * appropriate since only one modal dialog is ever open at once in this
 * window manager.  A caller repaints its own dialog with the freshly
 * changed selection, then schedules the actual close/accept here
 * (@a menu_dialog_defer_schedule).  The main loop calls
 * @a menu_dialog_defer_tick every iteration (folding
 * @a menu_dialog_defer_ms_remaining into its own poll timeout first,
 * the same way every other timer-driven subsystem in @c loop.c already
 * does), which runs the callback once its own deadline arrives.
 *
 * @ingroup menu_dialog
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_DIALOG_DEFER_H
#define MENU_DIALOG_DEFER_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>


/* Public interface */
/**
 * @brief Signature a deferred action's own callback must match
 *
 * @param connection XCB connection, passed through from whichever call
 *                   to @c menu_dialog_defer_tick actually runs the
 *                   callback
 */
typedef void (*menu_dialog_defer_callback_td)(xcb_connection_t *connection);

/**
 * @brief Schedule @p callback to run once @p delay_ms milliseconds have
 *        elapsed
 *
 * Replaces any previously scheduled action outright, without running
 * it, since only one dialog is ever open at a time: a caller that
 * schedules a new one already knows the old one no longer applies.
 *
 * @param connection XCB connection; only used if the system clock
 *                   cannot be read to schedule the delay, in which
 *                   case @p callback runs immediately instead of being
 *                   left scheduled with no way to ever become due
 * @param delay_ms   Milliseconds from now until @p callback should run
 * @param callback   Function to call once due; a @c NULL cancels
 *                   whatever was previously scheduled without replacing
 *                   it with anything new
 *
 * @note Complexity: @e O(1)
 */
void menu_dialog_defer_schedule(xcb_connection_t *connection,
        int delay_ms, menu_dialog_defer_callback_td callback);

/**
 * @brief Cancel whatever deferred action is currently pending, without
 *        running it
 *
 * Meant for a dialog closing through some other path (e.g., @c Escape,
 * or its own countdown elapsing) before a click-triggered close it had
 * already scheduled became due on its own.
 *
 * @note A no-op when nothing is pending
 * @note Complexity: @e O(1)
 */
void menu_dialog_defer_cancel(void);

/**
 * @brief Milliseconds remaining until the pending deferred action
 *        becomes due
 *
 * Meant to be folded into the main loop's own poll timeout (see
 * @c loop.c's own @c s_loop_tighten_poll_timeout), the same way every
 * other timer-driven subsystem already reports its own next wake-up
 * time.
 *
 * @return Milliseconds remaining (never negative), or @c -1 if nothing
 *         is currently pending
 *
 * @note Complexity: @e O(1)
 */
int menu_dialog_defer_ms_remaining(void);

/**
 * @brief Run the pending deferred action once its own deadline has
 *        arrived
 *
 * Meant to be called every main-loop iteration, unconditionally, the
 * same way every other timer-driven subsystem's own tick function
 * already is.
 *
 * @param connection XCB connection, passed through to whichever
 *                   callback actually runs
 *
 * @note A no-op when nothing is pending, or when the pending action is
 *       not yet due
 * @note Complexity: @e O(1)
 */
void menu_dialog_defer_tick(xcb_connection_t *connection);


#endif  /* ! MENU_DIALOG_DEFER_H */
