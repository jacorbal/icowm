/**
 * @file wm/shutdown.c
 *
 * @brief Coordinated shutdown implementation
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
#include <stdint.h>
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <cmds/client/basic.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <wm/internal.h>
#include <wm/shutdown.h>


/** Whether a coordinated shutdown is currently in progress */
static bool s_shutdown_in_progress = false;

/** Absolute deadline (@c CLOCK_MONOTONIC) past which remaining clients
 * are force-closed regardless */
static struct timespec s_shutdown_deadline;


/**
 * @brief Adapts @c ccmd_client_close to @c wm_for_each_client's own
 *        action signature
 *
 * @param client   Client to close
 * @param userdata Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_shutdown_close_client(client_td *client, void *userdata)
{
    (void) userdata;
    ccmd_client_close(client);
}


/**
 * @brief Adapts @c ccmd_client_kill to @c wm_for_each_client's own
 *        action signature
 *
 * @param client   Client to kill
 * @param userdata Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_shutdown_kill_client(client_td *client, void *userdata)
{
    (void) userdata;
    ccmd_client_kill(client);
}


/**
 * @brief Milliseconds remaining until an absolute deadline, floored at
 *        zero rather than going negative once past it
 *
 * Same computation @c menu/dialog/confirm.c's own
 * @a s_confirm_ms_until already performs for its countdown.  Kept as
 * its own small copy here rather than shared, the same way that one and
 * @c menu/dialog/defer.c's own equivalent already are two small copies
 * of each other rather than one shared utility.
 *
 * @param due Absolute deadline (@c CLOCK_MONOTONIC) to measure against
 *
 * @return Milliseconds remaining (never negative), or @c 0 if the clock
 *         itself could not be read
 *
 * @note Complexity: @e O(1)
 */
static int s_shutdown_ms_until(const struct timespec *due)
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


/* Begin a coordinated shutdown */
void wm_shutdown_begin(const wm_td *wm)
{
    uint32_t timeout_seconds;
    config_td *config;

    if (s_shutdown_in_progress) {
        return;
    }

    if (wm_for_each_client(wm, NULL, NULL) == 0u) {
        LOGGER_DEBUG("No managed clients to wait for;" \
                " stopping right away", L_NARG);
        (void) wm_request_stop();
        return;
    }

    LOGGER_INFO("Coordinated shutdown started;" \
            " asking every managed client to close", L_NARG);
    (void) wm_for_each_client(wm, s_shutdown_close_client, NULL);

    config = wm_config(wm);
    timeout_seconds = (config != NULL)
        ? config->base.shutdown.timeout_seconds : 15u;

    s_shutdown_in_progress = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_shutdown_deadline);
    s_shutdown_deadline.tv_sec += (time_t) timeout_seconds;
}


/* Milliseconds remaining before the shutdown timeout forces closure */
int wm_shutdown_ms_remaining(void)
{
    if (!s_shutdown_in_progress) {
        return -1;
    }

    return s_shutdown_ms_until(&s_shutdown_deadline);
}


/* Advance the shutdown state machine */
void wm_shutdown_tick(const wm_td *wm)
{
    uint32_t remaining;

    if (!s_shutdown_in_progress) {
        return;
    }

    remaining = wm_for_each_client(wm, NULL, NULL);
    if (remaining == 0u) {
        LOGGER_INFO("Every managed client closed;" \
                " finishing shutdown", L_NARG);
        s_shutdown_in_progress = false;
        (void) wm_request_stop();
        return;
    }

    if (s_shutdown_ms_until(&s_shutdown_deadline) > 0) {
        return;
    }

    LOGGER_NOTICE("Shutdown timeout elapsed with %u client(s)" \
            " still open; forcing them closed", remaining);
    (void) wm_for_each_client(wm, s_shutdown_kill_client, NULL);
    s_shutdown_in_progress = false;
    (void) wm_request_stop();
}


/* Query whether a coordinated shutdown is currently in progress */
bool wm_shutdown_is_in_progress(void)
{
    return s_shutdown_in_progress;
}
