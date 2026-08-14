/**
 * @file menu/dialog/defer.c
 *
 * @brief Deferred click-triggered close/accept implementation
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
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <menu/dialog/defer.h>


/** Absolute time the pending deferred action becomes due, valid only
 *  while 's_defer_pending' is true */
static struct timespec s_defer_due;

/** Whether a deferred action is currently pending */
static bool s_defer_pending = false;

/** Callback to run once 's_defer_due' arrives, valid only while
 *  's_defer_pending' is true */
static menu_dialog_defer_callback_td s_defer_callback = NULL;


/**
 * @brief Milliseconds remaining until an absolute deadline, floored
 *        at zero rather than going negative once past it
 *
 * @param due Absolute deadline (@c CLOCK_MONOTONIC) to measure against
 *
 * @return Milliseconds remaining (never negative), or @c 0 if the
 *         clock itself could not be read
 *
 * @note Complexity: @e O(1)
 */
static int s_defer_ms_until(const struct timespec *due)
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


/* Schedule 'callback' to run once 'delay_ms' milliseconds have
 * elapsed */
void menu_dialog_defer_schedule(xcb_connection_t *connection, int delay_ms,
        menu_dialog_defer_callback_td callback)
{
    if (callback == NULL) {
        menu_dialog_defer_cancel();
        return;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &s_defer_due) != 0) {
        /* Could not read the clock to schedule the delay; still
         * better to run the callback immediately than to leave it
         * scheduled with no way to ever become due. */
        s_defer_pending = false;
        s_defer_callback = NULL;
        callback(connection);
        return;
    }

    s_defer_due.tv_nsec += (long) delay_ms * 1000000L;
    if (s_defer_due.tv_nsec >= 1000000000L) {
        s_defer_due.tv_sec += 1;
        s_defer_due.tv_nsec -= 1000000000L;
    }

    s_defer_callback = callback;
    s_defer_pending = true;
}


/* Cancel whatever deferred action is currently pending, without
 * running it */
void menu_dialog_defer_cancel(void)
{
    s_defer_pending = false;
    s_defer_callback = NULL;
}


/* Whether a deferred action is currently pending */
bool menu_dialog_defer_is_pending(void)
{
    return s_defer_pending;
}


/* Milliseconds remaining until the pending deferred action becomes
 * due */
int menu_dialog_defer_ms_remaining(void)
{
    if (!s_defer_pending) {
        return -1;
    }
    return s_defer_ms_until(&s_defer_due);
}


/* Run the pending deferred action once its own deadline has arrived */
void menu_dialog_defer_tick(xcb_connection_t *connection)
{
    menu_dialog_defer_callback_td callback;

    if (!s_defer_pending || s_defer_ms_until(&s_defer_due) != 0) {
        return;
    }

    callback = s_defer_callback;
    s_defer_pending = false;
    s_defer_callback = NULL;

    if (callback != NULL) {
        callback(connection);
    }
}
