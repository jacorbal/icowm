/**
 * @file policy/ping.c
 *
 * @brief Periodic _NET_WM_PING liveness probing implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <time.h>       /* clock_gettime, struct timespec */

/* ADT includes */
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Default initial values */
#include <defs/ewmh.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Project includes */
#include <client.h>
#include <cmds/client/ewmh.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <policy/ping.h>


/** When the most recent probing round happened, @c CLOCK_MONOTONIC */
static struct timespec s_last_probe;

/**
 * @brief Whether at least one client advertised @c _NET_WM_PING
 *        support as of the most recent @a ping_tick call
 *
 * Cached here rather than recomputed by @a ping_ms_remaining itself,
 * so the one full scan over every client each iteration needs happens
 * exactly once, in @c ping_tick, the same way @a urgency_blink_tick
 * and @a urgency_blink_ms_remaining split the same two
 * responsibilities (@c src/policy/urgency.c).
 */
static bool s_has_supported = false;


/**
 * @brief Note whether any client on a desktop advertises
 *        @c _NET_WM_PING support
 *
 * @param desktop Desktop reached by the walk
 * @param data    Pointer to the @c bool being set
 *
 * @note The whole walk runs even once one is found, a visitor having
 *       no way to end it; the caller stops on the flag instead
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_ping_supported_visit(desktop_td *desktop, void *data)
{
    bool *const has_supported = data;
    const void *elem;

    if (has_supported == NULL || desktop->clients == NULL) {
        return;
    }

    ohtbl_foreach(desktop->clients, elem) {
        const client_td *const client = (const client_td *) elem;

        if (client != NULL && client->hints_ewmh.ping.is_supported) {
            *has_supported = true;
        }
    }
}


/**
 * @brief Whether at least one managed client, anywhere, currently
 *        advertises @c _NET_WM_PING support
 *
 * A pure scan with no side effects at all, safe to call on every
 * single @a ping_tick (i.e., every main-loop iteration, not just at
 * the actual @c WM_EWMH_PING_INTERVAL_SECONDS cadence).
 *
 * @param surfaces All managed surfaces
 *
 * @return @c true when at least one such client was found
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients
 */
static bool s_ping_any_supported(list_td *surfaces)
{
    if (surfaces == NULL) {
        return false;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        const surface_td *const surface = (surface_td *) list_data(snode);
        bool has_supported = false;

        if (surface == NULL) {
            continue;
        }

        surface_desktops_walk(surface, s_ping_supported_visit,
                &has_supported);
        if (has_supported) {
            return true;
        }
    } /* ! for (snode) */

    return false;
}


/**
 * @brief Probe or age out one desktop's ping-capable clients
 *
 * A client not yet waiting on a reply is simply sent a fresh probe.
 * One already waiting has its pending-round count advanced first,
 * and is marked unresponsive once that count covers
 * @c WM_EWMH_PING_TIMEOUT_SECONDS worth of rounds, before being sent
 * this round's probe regardless, so a client that recovers later is
 * still given the chance to answer and clear the mark again (see
 * @a handler_message_event, handler/message.c, for that side).
 *
 * @param desktop Desktop reached by the walk
 * @param data    Unused
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_ping_probe_visit(desktop_td *desktop, void *data)
{
    void *elem;

    (void) data;

    if (desktop->clients == NULL) {
        return;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *const client = (client_td *) elem;

        if (client == NULL || !client->hints_ewmh.ping.is_supported) {
            continue;
        }

        if (client->hints_ewmh.ping.is_waiting) {
            if (client->hints_ewmh.ping.pending_ticks < UINT8_MAX) {
                client->hints_ewmh.ping.pending_ticks += 1u;
            }

            if (client->hints_ewmh.ping.pending_ticks >=
                    (uint8_t) (WM_EWMH_PING_TIMEOUT_SECONDS /
                        WM_EWMH_PING_INTERVAL_SECONDS)) {
                client_mark_unresponsive(client);
            }
        }

        ccmd_client_ping_send(client);
    }
}


/**
 * @brief Send this round's probe to every ping-capable client, across
 *        every surface and desktop
 *
 * @param surfaces All managed surfaces
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients
 */
static void s_ping_probe_round(list_td *surfaces)
{
    if (surfaces == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        const surface_td *const surface =
            (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }

        surface_desktops_walk(surface, s_ping_probe_visit, NULL);
    } /* ! for (snode) */
}


/* Probe every ping-capable client and age out unanswered ones */
void ping_tick(list_td *surfaces)
{
    s_has_supported = s_ping_any_supported(surfaces);

    if (!s_has_supported) {
        return;
    }

    if (s_last_probe.tv_sec == 0 && s_last_probe.tv_nsec == 0) {
        (void) clock_gettime(CLOCK_MONOTONIC, &s_last_probe);
        return;
    }

    if (clock_ms_since(&s_last_probe) >=
            (long) WM_EWMH_PING_INTERVAL_SECONDS * 1000L) {
        (void) clock_gettime(CLOCK_MONOTONIC, &s_last_probe);
        s_ping_probe_round(surfaces);
    }
}


/* How many milliseconds until the ping cycle next needs a tick */
int ping_ms_remaining(void)
{
    long elapsed_ms;
    const long interval_ms = (long) WM_EWMH_PING_INTERVAL_SECONDS * 1000L;

    if (!s_has_supported) {
        return -1;
    }

    if (s_last_probe.tv_sec == 0 && s_last_probe.tv_nsec == 0) {
        return 0;
    }

    elapsed_ms = clock_ms_since(&s_last_probe);
    if (elapsed_ms >= interval_ms) {
        return 0;
    }

    return (int) (interval_ms - elapsed_ms);
}
