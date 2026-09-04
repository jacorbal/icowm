/**
 * @file tests/handler/test_sync.c
 *
 * @brief Test battery for handler/sync.c: the XSync extension
 *        @c AlarmNotify event handler
 *
 * handler_sync_event's static helper, s_find_client_by_alarm, walks a
 * real list_td of surfaces, each with a real cdlist_td of desktops
 * (src/adt/list.c, src/adt/cdlist.c linked for real, being small,
 * side-effect-free leaf data-structure code, the same rationale
 * already used for cdlist.c in
 * tests/input/mouse/event/test_press.c), and each desktop's clients
 * kept in a real ohtbl_td (src/adt/ohtbl.c, likewise a leaf ADT).
 * wm_surfaces/wm_sync_available/wm_sync_base_event reach a real,
 * stack-built 'struct wm_s' (wm/internal.h, safe to include; see the
 * same rationale used in tests/handler/test_crossing.c) through
 * src/wm/instance.c, itself nothing but narrow field accessors, also
 * linked for real.
 *
 * ccmd_client_resize_flush_pending is the one genuinely cross-module
 * collaborator, a link-only stand-in below recording which client it
 * was asked to flush.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/sync.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <handler.h>
#include <harness/tap.h>


/* Recording state for the ccmd_client_resize_flush_pending stand-in */
static int s_flush_calls;
static client_td *s_flush_last_client;


/** Link-only stand-in for ccmd_client_resize_flush_pending */
void ccmd_client_resize_flush_pending(client_td *client)
{
    s_flush_calls++;
    s_flush_last_client = client;
}


/** Link-only stand-in for logger_msg: handler_sync_event traces both
 *  its unknown-alarm and acknowledged-alarm paths unconditionally */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    va_list args;

    (void) level;
    (void) prefix;

    va_start(args, fmt);
    va_end(args);

    return 0;
}


/* Trivial identity-based hash/match pair for the test-owned ohtbl,
 * one or two entries per test, so collision quality does not matter */
static size_t s_test_h1(const void *data)
{
    const client_td *const client = (const client_td *) data;

    return (client == NULL) ? 0u : (size_t) client->id;
}


static size_t s_test_h2(const void *data)
{
    const client_td *const client = (const client_td *) data;

    return (client == NULL) ? 1u : (size_t) (client->id * 2u + 1u);
}


static bool s_test_match(const void *key1, const void *key2)
{
    const client_td *const c1 = (const client_td *) key1;
    const client_td *const c2 = (const client_td *) key2;

    return c1->id == c2->id;
}


/** Build a client fixture with a given alarm XID tracked */
static void s_build_client(client_td *client, xcb_window_t id,
        uint32_t alarm)
{
    memset(client, 0, sizeof(*client));
    client->id = id;
    client->window = id;
    client->hints_ewmh.sync.alarm = alarm;
}


/** Build one surface with a single desktop whose client hash table
 *  holds exactly @p client */
static void s_build_single_client_surface(surface_td *surface,
        desktop_td *desktop, ohtbl_td *table, client_td *client)
{
    memset(surface, 0, sizeof(*surface));
    memset(desktop, 0, sizeof(*desktop));

    (void) ohtbl_insert(table, client);
    desktop->clients = table;

    surface->desktops = cdlist_init(NULL);
    (void) cdlist_ins_next(surface->desktops, NULL, desktop);
    surface->desktop_count = 1u;
}


/** Build a minimal, real 'struct wm_s' on the stack */
static wm_td s_make_wm(list_td *surfaces, bool sync_available)
{
    wm_td local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.surfaces = surfaces;
    local_wm.is_sync_available = sync_available;
    local_wm.sync_base_event = 64u;

    return local_wm;
}


/**
 * @brief Exercise two surfaces, the match landing on the second
 *        one's desktop
 *
 * Proves handler_sync_event's own outer surfaces walk keeps going
 * past a surface whose desktop table found nothing
 */
static void s_test_two_surfaces_second_match(void)
{
    list_td *surfaces2;
    surface_td surface_a;
    surface_td surface_b;
    desktop_td desktop_a;
    desktop_td desktop_b;
    ohtbl_td *table_a;
    ohtbl_td *table_b;
    client_td client_a;
    client_td client_b;
    wm_td wm2;
    xcb_sync_alarm_notify_event_t event;

    table_a = ohtbl_init(4u, 0u, s_test_h1, s_test_h2, s_test_match,
            NULL);
    s_build_client(&client_a, 10u, 0x10u);
    s_build_single_client_surface(&surface_a, &desktop_a, table_a,
            &client_a);

    table_b = ohtbl_init(4u, 0u, s_test_h1, s_test_h2, s_test_match,
            NULL);
    s_build_client(&client_b, 11u, 0x20u);
    s_build_single_client_surface(&surface_b, &desktop_b, table_b,
            &client_b);

    surfaces2 = list_init(NULL);
    (void) list_ins_next(surfaces2, NULL, &surface_a);
    (void) list_ins_next(surfaces2, NULL, &surface_b);
    wm2 = s_make_wm(surfaces2, true);

    memset(&event, 0, sizeof(event));
    event.response_type = 64u + XCB_SYNC_ALARM_NOTIFY;
    event.alarm = 0x20u;
    s_flush_calls = 0;
    handler_sync_event(&wm2, (xcb_generic_event_t *) &event);
    TAP_EQ_INT(s_flush_calls, 1,
            "exactly one flush happens once the match is found" \
            " on the second surface");
    TAP_OK(s_flush_last_client == &client_b,
            "the second surface's client is the one flushed");

    ohtbl_destroy(table_a);
    ohtbl_destroy(table_b);
    cdlist_destroy(surface_a.desktops);
    cdlist_destroy(surface_b.desktops);
    list_destroy(surfaces2);
}


int main(void)
{
    list_td *surfaces;
    surface_td surface;
    desktop_td desktop;
    ohtbl_td *table;
    client_td client;
    wm_td wm;
    xcb_sync_alarm_notify_event_t event;

    TAP_PLAN(10);

    /* Guard clauses: a null wm, null event, null surfaces, or the
     * XSync extension being unavailable are all silent no-ops, proven
     * only by the absence of a crash under ASan/UBSan and by
     * ccmd_client_resize_flush_pending never firing */
    s_flush_calls = 0;
    handler_sync_event(NULL, NULL);
    TAP_EQ_INT(s_flush_calls, 0, "a null wm never flushes anything");

    memset(&event, 0, sizeof(event));
    event.response_type = 64u + XCB_SYNC_ALARM_NOTIFY;
    event.alarm = 0x77u;

    surfaces = list_init(NULL);
    wm = s_make_wm(surfaces, false);
    s_flush_calls = 0;
    handler_sync_event(&wm, (xcb_generic_event_t *) &event);
    TAP_EQ_INT(s_flush_calls, 0,
            "XSync unavailable means the event is ignored entirely");
    list_destroy(surfaces);

    wm = s_make_wm(NULL, true);
    s_flush_calls = 0;
    handler_sync_event(&wm, (xcb_generic_event_t *) &event);
    TAP_EQ_INT(s_flush_calls, 0,
            "a null surfaces list is treated as a no-op");

    /* XSync available, real surfaces, but the response_type does not
     * match this connection's AlarmNotify code: ignored */
    surfaces = list_init(NULL);
    wm = s_make_wm(surfaces, true);
    memset(&event, 0, sizeof(event));
    event.response_type = 99u;
    event.alarm = 0x77u;
    s_flush_calls = 0;
    handler_sync_event(&wm, (xcb_generic_event_t *) &event);
    TAP_EQ_INT(s_flush_calls, 0,
            "a non-matching response_type is ignored");
    list_destroy(surfaces);

    /* A matching AlarmNotify, but no managed client tracks this exact
     * alarm XID: looked up, not found, ignored */
    table = ohtbl_init(4u, 0u, s_test_h1, s_test_h2, s_test_match,
            NULL);
    s_build_client(&client, 1u, 0x55u);
    s_build_single_client_surface(&surface, &desktop, table, &client);

    surfaces = list_init(NULL);
    (void) list_ins_next(surfaces, NULL, &surface);
    wm = s_make_wm(surfaces, true);

    memset(&event, 0, sizeof(event));
    event.response_type = 64u + XCB_SYNC_ALARM_NOTIFY;
    event.alarm = 0x99u;   /* nobody tracks this one */
    s_flush_calls = 0;
    handler_sync_event(&wm, (xcb_generic_event_t *) &event);
    TAP_EQ_INT(s_flush_calls, 0,
            "an unknown alarm XID is looked up and ignored");

    /* The matching alarm: the owning client is found and its pending
     * resize is flushed exactly once */
    memset(&event, 0, sizeof(event));
    event.response_type = 64u + XCB_SYNC_ALARM_NOTIFY;
    event.alarm = 0x55u;
    s_flush_calls = 0;
    handler_sync_event(&wm, (xcb_generic_event_t *) &event);
    TAP_EQ_INT(s_flush_calls, 1,
            "the client owning the matching alarm is flushed once");
    TAP_OK(s_flush_last_client == &client,
            "the exact client instance found is the one flushed");

    /* A client with alarm == 0 (never subscribed) never matches an
     * event carrying alarm == 0 either, since 0 is treated as
     * "no alarm at all", not a valid XID to search for */
    client.hints_ewmh.sync.alarm = 0u;
    memset(&event, 0, sizeof(event));
    event.response_type = 64u + XCB_SYNC_ALARM_NOTIFY;
    event.alarm = 0u;
    s_flush_calls = 0;
    handler_sync_event(&wm, (xcb_generic_event_t *) &event);
    TAP_EQ_INT(s_flush_calls, 0,
            "an alarm value of zero never matches, even against a" \
            " client whose own alarm field is also zero");

    ohtbl_destroy(table);
    cdlist_destroy(surface.desktops);
    list_destroy(surfaces);

    /* Two surfaces, the match landing on the second one's desktop:
     * proves the outer surfaces walk, not just one desktop's table */
    s_test_two_surfaces_second_match();

    return TAP_DONE();
}
