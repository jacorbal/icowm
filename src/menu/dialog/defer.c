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
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Local includes */
#include <menu/dialog/defer.h>


/** Absolute time the pending deferred action becomes due, valid only
 *  while @c s_defer_pending is true */
static struct timespec s_defer_due;

/** Whether a deferred action is currently pending */
static bool s_defer_pending = false;

/** Callback to run once @c s_defer_due arrives, valid only while
 *  @c s_defer_pending is true */
static menu_dialog_defer_callback_td s_defer_callback = NULL;


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

    /* A negative delay would mean "already due"; treated as zero
     * rather than wrapping into a very distant deadline */
    clock_add_ms(&s_defer_due,
            (delay_ms > 0) ? (unsigned int) delay_ms : 0u);

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


/* Milliseconds remaining until the pending deferred action becomes
 * due */
int menu_dialog_defer_ms_remaining(void)
{
    if (!s_defer_pending) {
        return -1;
    }
    return (int) clock_ms_until(&s_defer_due);
}


/* Run the pending deferred action once its deadline has arrived */
void menu_dialog_defer_tick(xcb_connection_t *connection)
{
    menu_dialog_defer_callback_td callback;

    if (!s_defer_pending || clock_ms_until(&s_defer_due) != 0) {
        return;
    }

    callback = s_defer_callback;
    s_defer_pending = false;
    s_defer_callback = NULL;

    if (callback != NULL) {
        callback(connection);
    }
}
