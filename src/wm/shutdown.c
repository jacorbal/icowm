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

/** Absolute deadline (@c CLOCK_MONOTONIC) past which remaining
 *  clients are force-closed regardless */
static struct timespec s_shutdown_deadline;


/**
 * @brief Visit every currently managed client across every surface
 *        and desktop, optionally applying an action to each
 *
 * Same enumeration @c wm/ewmhinit.c's own '_NET_CLIENT_LIST' builder
 * already walks, reused here rather than duplicated with its own
 * separate traversal logic.
 *
 * @param action Function called once per client found, or @c NULL to
 *               only count them without acting on any
 *
 * @return Number of managed clients found
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients across every surface and desktop
 */
static uint32_t s_shutdown_for_each_client(void (*action)(client_td *))
{
    uint32_t count = 0u;

    if (wm == NULL || wm->surfaces == NULL) {
        return 0u;
    }

    for (list_item_td *snode = list_head(wm->surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }

        for (uint32_t did = 0u; did < surface->desktop_count; ++did) {
            desktop_td *desktop = surface_desktop_get(surface, did);
            void *elem;

            if (desktop == NULL || desktop->clients == NULL) {
                continue;
            }

            ohtbl_foreach(desktop->clients, elem) {
                client_td *client = (client_td *) elem;

                if (client != NULL) {
                    ++count;
                    if (action != NULL) {
                        action(client);
                    }
                }
            }
        }
    }

    return count;
}


/**
 * @brief Milliseconds remaining until an absolute deadline, floored
 *        at zero rather than going negative once past it
 *
 * Same computation 'menu/dialog/confirm.c''s own
 * 's_confirm_ms_until' already performs for its countdown; kept as
 * its own small copy here rather than shared, the same way that one
 * and 'menu/dialog/defer.c''s own equivalent already are two small
 * copies of each other rather than one shared utility.
 *
 * @param due Absolute deadline (@c CLOCK_MONOTONIC) to measure against
 *
 * @return Milliseconds remaining (never negative), or @c 0 if the
 *         clock itself could not be read
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
void wm_shutdown_begin(void)
{
    uint32_t timeout_seconds;

    if (s_shutdown_in_progress) {
        return;
    }

    if (s_shutdown_for_each_client(NULL) == 0u) {
        LOGGER_DEBUG("No managed clients to wait for;" \
                " stopping right away", L_NARG);
        (void) wm_request_stop();
        return;
    }

    LOGGER_INFO("Coordinated shutdown started;" \
            " asking every managed client to close", L_NARG);
    (void) s_shutdown_for_each_client(ccmd_client_close);

    timeout_seconds = (wm != NULL && wm->config != NULL)
        ? wm->config->base.shutdown.timeout_seconds : 15u;

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
void wm_shutdown_tick(void)
{
    uint32_t remaining;

    if (!s_shutdown_in_progress) {
        return;
    }

    remaining = s_shutdown_for_each_client(NULL);
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
    (void) s_shutdown_for_each_client(ccmd_client_kill);
    s_shutdown_in_progress = false;
    (void) wm_request_stop();
}


/* Query whether a coordinated shutdown is currently in progress */
bool wm_shutdown_is_in_progress(void)
{
    return s_shutdown_in_progress;
}
