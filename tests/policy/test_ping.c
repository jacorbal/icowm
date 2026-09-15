/**
 * @file tests/policy/test_ping.c
 *
 * @brief Test battery for periodic _NET_WM_PING liveness probing
 *        (policy/ping.c)
 *
 * All probing state (s_last_probe, s_has_supported) is private and
 * static to policy/ping.c itself, with no reset entry point of its
 * own, so this file's tests run as one continuous narrative in
 * a fixed order (see main), exactly the same constraint
 * tests/policy/test_urgency.c already documents for its own sibling
 * module.  'surface_desktop_walk_all' is a link-only stand-in walking
 * a surface's real cdlist of desktops directly, the same pattern
 * test_urgency.c already uses, so both the "any client support
 * _NET_WM_PING" scan and the actual per-round probe walk run for
 * real.  'ccmd_client_ping_send' and 'client_mark_unresponsive'
 * (the latter a macro expanding to 'safeflg_set', whose real source
 * is linked directly since it has no XCB or JSON dependency of its
 * own) are exercised for real; only 'ccmd_client_ping_send' itself is
 * a call-counting stand-in, since the real one would issue an XCB
 * client message this file has no live connection to deliver.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* nanosleep */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <policy/ping.h>


/** Link-only stand-in for @a surface_desktop_walk_all (surface/
 *  desktops.c): walks a real 'cdlist_td' this file builds itself,
 *  the same pattern tests/policy/test_urgency.c already uses
 *  @note Complexity: @e O(n), where @e n is the number of desktops on
 *        @p surface
 */
void surface_desktop_walk_all(const surface_td *surface,
        surface_desktop_visitor_fn visit, void *data)
{
    cdlist_item_td *node;

    if (surface == NULL || surface->desktops == NULL || visit == NULL) {
        return;
    }

    cdlist_foreach(surface->desktops, node) {
        desktop_td *const desktop = (desktop_td *) cdlist_data(node);

        if (desktop != NULL) {
            visit(desktop, data);
        }
    }
}


/** Every client 'ccmd_client_ping_send' was called with, in call
 *  order, and how many times it ran in total */
#define MAX_RECORDED_PINGS (8)
static const client_td *s_pinged_clients[MAX_RECORDED_PINGS];
static int s_call_ping_send;

/** Call-recording stand-in for @a ccmd_client_ping_send (cmds/client/
 *  ewmh.c): the real one issues an XCB client message this file has
 *  no live connection to deliver
 *  @note Complexity: @e O(1)
 */
void ccmd_client_ping_send(client_td *client)
{
    if (s_call_ping_send < MAX_RECORDED_PINGS) {
        s_pinged_clients[s_call_ping_send] = client;
    }
    s_call_ping_send++;
}


static size_t s_id_hash1(const void *key)
{
    return (size_t) ((const client_td *) key)->id;
}


static size_t s_id_hash2(const void *key)
{
    (void) key;
    return 1u;
}


static bool s_id_match(const void *key1, const void *key2)
{
    return ((const client_td *) key1)->id ==
        ((const client_td *) key2)->id;
}


static void s_sleep_ms(long ms)
{
    struct timespec req;

    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    (void) nanosleep(&req, NULL);
}


int main(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td supported_client;
    client_td plain_client;
    list_td *surfaces;

    TAP_PLAN(19);

    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&supported_client, 0, sizeof(supported_client));
    memset(&plain_client, 0, sizeof(plain_client));

    supported_client.id = 1u;
    supported_client.hints_ewmh.ping.is_supported = true;
    plain_client.id = 2u;
    plain_client.hints_ewmh.ping.is_supported = false;

    desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    surface.desktops = cdlist_init(NULL);
    (void) cdlist_ins_next(surface.desktops, NULL, &desktop);

    surfaces = list_init(NULL);
    (void) list_ins_next(surfaces, NULL, &surface);

    /* 1-2: no clients registered at all yet: nothing to probe, ever */
    ping_tick(surfaces);
    TAP_EQ_INT(ping_ms_remaining(), -1,
            "no client at all: nothing to probe, so nothing to wait"
            " for");
    TAP_EQ_INT(s_call_ping_send, 0,
            "no client at all: no probe is ever sent");

    /* 3-4: a client that does not support the protocol is still
     * ignored entirely */
    (void) ohtbl_insert(desktop.clients, &plain_client);
    ping_tick(surfaces);
    TAP_EQ_INT(ping_ms_remaining(), -1,
            "a client without _NET_WM_PING support changes nothing");
    TAP_EQ_INT(s_call_ping_send, 0,
            "a non-supporting client is never sent a probe");

    /* 5-6: a supporting client's first tick only arms the timer,
     * since 's_last_probe' starts at zero and 'ping_tick' treats that
     * as "just armed, wait for the next tick" rather than probing
     * immediately */
    (void) ohtbl_insert(desktop.clients, &supported_client);
    s_call_ping_send = 0;
    ping_tick(surfaces);
    TAP_EQ_INT(s_call_ping_send, 0,
            "the very first tick after a supporting client appears"
            " only arms the timer, sending no probe yet");
    TAP_OK(ping_ms_remaining() >= 0,
            "now something to wait for, since a client supports the"
            " protocol");

    /* 7-9: ticking again immediately (interval not yet elapsed) still
     * sends nothing */
    s_call_ping_send = 0;
    ping_tick(surfaces);
    TAP_EQ_INT(s_call_ping_send, 0,
            "ticking again before the interval elapses sends nothing"
            " yet");
    TAP_OK(!supported_client.hints_ewmh.ping.is_waiting,
            "the supporting client is not yet marked waiting on a"
            " probe");
    TAP_EQ_INT((int) supported_client.properties.flags &
                CLIENT_FLAG_UNRESPONSIVE, 0,
            "the supporting client is not yet marked unresponsive");

    /* 10-12: WM_EWMH_PING_INTERVAL_SECONDS is 5s; there is no test
     * hook to shrink it, so this sleeps past it for real, the same
     * trade-off test_urgency.c already accepts for its own timing
     * assertions */
    s_sleep_ms(5200);
    s_call_ping_send = 0;
    ping_tick(surfaces);
    TAP_EQ_INT(s_call_ping_send, 1,
            "once the interval elapses, exactly the one supporting"
            " client is probed");
    TAP_OK(s_pinged_clients[0] == &supported_client,
            "the client probed is the one that supports the protocol");
    TAP_OK(ping_ms_remaining() > 0,
            "immediately after probing, the full interval is armed"
            " again for the next round");

    /* 13-14: manually mark the client as waiting on a reply, the same
     * way the real send path (ccmd_client_ping_send, stubbed away
     * here) would; a further round before it answers advances its
     * pending-round count without yet marking it unresponsive */
    supported_client.hints_ewmh.ping.is_waiting = true;
    supported_client.hints_ewmh.ping.pending_ticks = 0u;
    s_sleep_ms(5200);
    s_call_ping_send = 0;
    ping_tick(surfaces);
    TAP_EQ_INT((int) supported_client.hints_ewmh.ping.pending_ticks, 1,
            "a client still waiting on a reply has its pending-round"
            " count advanced by one");
    TAP_EQ_INT((int) supported_client.properties.flags &
                CLIENT_FLAG_UNRESPONSIVE, 0,
            "one pending round alone does not yet mark it"
            " unresponsive");

    /* 15-16: enough consecutive unanswered rounds (three, at this
     * 5s/15s ratio) marks the client unresponsive, and it is still
     * sent this round's probe regardless */
    s_sleep_ms(5200);
    ping_tick(surfaces);
    s_sleep_ms(5200);
    s_call_ping_send = 0;
    ping_tick(surfaces);
    TAP_OK((supported_client.properties.flags &
                CLIENT_FLAG_UNRESPONSIVE) != 0,
            "enough consecutive unanswered rounds marks the client"
            " unresponsive");
    TAP_EQ_INT(s_call_ping_send, 1,
            "an unresponsive client is still sent this round's probe");

    /* 17: pending_ticks saturates at UINT8_MAX rather than wrapping
     * back to zero, staying marked unresponsive regardless */
    supported_client.hints_ewmh.ping.pending_ticks = 255u;
    s_sleep_ms(5200);
    ping_tick(surfaces);
    TAP_EQ_INT((int) supported_client.hints_ewmh.ping.pending_ticks,
            255,
            "pending_ticks saturates at its maximum rather than"
            " wrapping around");

    /* 18-19: once the client stops supporting the protocol
     * altogether, the next tick reports nothing left to probe again */
    supported_client.hints_ewmh.ping.is_supported = false;
    ping_tick(surfaces);
    TAP_EQ_INT(ping_ms_remaining(), -1,
            "once no client supports the protocol anymore, there is"
            " nothing left to wait for");
    s_call_ping_send = 0;
    ping_tick(surfaces);
    TAP_EQ_INT(s_call_ping_send, 0,
            "and no further probe is ever sent");

    ohtbl_destroy(desktop.clients);
    cdlist_destroy(surface.desktops);
    list_destroy(surfaces);

    return TAP_DONE();
}
