/**
 * @file tests/handler/test_colormap.c
 *
 * @brief Test battery for handler/colormap.c: the @c COLORMAP_NOTIFY
 *        event handler
 *
 * @a handler_colormap_notify itself is a thin stages walk around the
 * static helpers @a s_client_colormap_window_index and
 * @a s_colormap_update_visit, neither reachable directly from a test
 * file since both are file-static; every behavior they implement is
 * instead exercised end to end by driving the public entry point over
 * a real list_td of stages, each with a real cdlist_td of desktops
 * (src/adt/list.c, src/adt/cdlist.c, src/stage/desktops.c's
 * stage_desktop_walk_all are all linked for real, being small,
 * side-effect-free leaf data-structure code, the same rationale
 * already used for cdlist.c in tests/input/mouse/event/test_press.c),
 * and each desktop's clients kept in a real ohtbl_td (src/adt/ohtbl.c,
 * likewise a leaf ADT) populated with real, stack-built client_td
 * fixtures.
 *
 * xcb_install_colormap is the one genuinely external, X-server-bound
 * call handler_colormap_notify's collaborators make; it is a
 * link-only stand-in below recording the colormap ID it was asked to
 * install, following the same convention as
 * tests/input/mouse/test_hover.c's stand-ins for xcb_query_pointer:
 * libxcb itself is never linked, only its headers, for exactly this
 * reason.
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

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <client/state.h>
#include <defs/client.h>
#include <desktop.h>
#include <logger.h>
#include <stage.h>

/* Local includes */
#include <handler/colormap.h>
#include <harness/tap.h>


/* Recording state for the xcb_install_colormap stand-in below */
static int s_install_calls;
static xcb_colormap_t s_install_last_id;

/** Link-only stand-in for xcb_install_colormap: records the colormap
 *  ID it is asked to install instead of issuing any real X request */
xcb_void_cookie_t xcb_install_colormap(xcb_connection_t *connection,
        xcb_colormap_t cmap)
{
    xcb_void_cookie_t cookie;

    (void) connection;

    s_install_calls++;
    s_install_last_id = cmap;
    cookie.sequence = 0u;

    return cookie;
}


/** Link-only stand-in for desktop_destroy: stage_desktop_rem
 *  references it as a cdlist item destructor, but this file never
 *  calls stage_desktop_rem, so it is never actually invoked */
void desktop_destroy(desktop_td *desktop)
{
    (void) desktop;
}


/** Link-only stand-in for logger_msg: handler_colormap_notify traces
 *  every call unconditionally, and this stand-in just discards it */
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
 * one entry or two per test, so collision quality does not matter */
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


/**
 * @brief Build a client fixture with one @c WM_COLORMAP_WINDOWS entry
 *
 * @param client       Storage to initialize
 * @param id           Unique identifier for the ohtbl key
 * @param cmap_window  Window ID tracked in @p colormap_windows
 * @param is_focused   Whether @c CLIENT_FLAG_FOCUSED should be set
 */
static void s_build_client(client_td *client, xcb_window_t id,
        xcb_window_t cmap_window, bool is_focused)
{
    memset(client, 0, sizeof(*client));
    client->id = id;
    client->window = id;
    client->properties.flags = (is_focused) ?
        (uint32_t) CLIENT_FLAG_FOCUSED : (uint16_t) 0u;
    client->colormap_windows.windows[0] = cmap_window;
    client->colormap_windows.colormap_ids[0] = (xcb_colormap_t) 0x1111u;
    client->colormap_windows.count = 1u;
}


/**
 * @brief Build one stage with a single desktop whose client hash
 *        table holds exactly @p client
 */
static void s_build_single_client_stage(stage_td *stage,
        desktop_td *desktop, ohtbl_td *table, client_td *client)
{
    memset(stage, 0, sizeof(*stage));
    memset(desktop, 0, sizeof(*desktop));

    (void) ohtbl_insert(table, client);
    desktop->clients = table;
    desktop->id = 0u;

    stage->desktops = cdlist_init(NULL);
    (void) cdlist_ins_next(stage->desktops, NULL, desktop);
    stage->desktop_count = 1u;
    stage->id = 0u;
}


/**
 * @brief Exercise two stages, the match landing on the second one
 *
 * Proves handler_colormap_notify's own outer stages walk keeps
 * going past a stage whose desktops walk found nothing, not just
 * the per-desktop visitor exercised by every other case in this file
 *
 * @return How many of the three assertions it makes failed
 */
static int s_test_two_stages_second_match(void)
{
    int failed_before = tap_failed;
    list_td *stages2;
    stage_td stage_a;
    stage_td stage_b;
    desktop_td desktop_a;
    desktop_td desktop_b;
    ohtbl_td *table_a;
    ohtbl_td *table_b;
    client_td client_a;
    client_td client_b;
    xcb_colormap_notify_event_t event;

    table_a = ohtbl_init(4u, 0u, s_test_h1, s_test_h2, s_test_match,
            NULL);
    s_build_client(&client_a, 10u, 0x700u, true);
    s_build_single_client_stage(&stage_a, &desktop_a, table_a,
            &client_a);

    table_b = ohtbl_init(4u, 0u, s_test_h1, s_test_h2, s_test_match,
            NULL);
    s_build_client(&client_b, 11u, 0x800u, true);
    s_build_single_client_stage(&stage_b, &desktop_b, table_b,
            &client_b);

    stages2 = list_init(NULL);
    (void) list_ins_next(stages2, NULL, &stage_a);
    (void) list_ins_next(stages2, NULL, &stage_b);

    memset(&event, 0, sizeof(event));
    event.window = 0x800u;
    event.colormap = 0xaaaau;
    event.state = XCB_COLORMAP_STATE_INSTALLED;

    s_install_calls = 0;
    handler_colormap_notify(NULL, stages2, &event);
    TAP_EQ_INT(client_a.colormap_windows.colormap_ids[0], 0x1111,
            "the first stage's non-matching client is untouched");
    TAP_EQ_INT(client_b.colormap_windows.colormap_ids[0], 0xaaaa,
            "the second stage's matching client is updated");
    TAP_EQ_INT(s_install_calls, 1,
            "exactly one install happens once the real match is" \
            " found on the second stage");

    ohtbl_destroy(table_a);
    ohtbl_destroy(table_b);
    cdlist_destroy(stage_a.desktops);
    cdlist_destroy(stage_b.desktops);
    list_destroy(stages2);

    return tap_failed - failed_before;
}


int main(void)
{
    list_td *stages;
    stage_td stage;
    desktop_td desktop;
    ohtbl_td *table;
    client_td client;
    xcb_colormap_notify_event_t event;

    TAP_PLAN(15);

    /* Guard clauses: a NULL event or a NULL stages list is a no-op,
     * proven here only by the absence of any crash under ASan/UBSan,
     * since neither has an observable side effect to assert on */
    handler_colormap_notify(NULL, NULL, NULL);
    handler_colormap_notify(NULL, NULL, &event);
    TAP_OK(true, "a null stages list does not crash");

    stages = list_init(NULL);
    TAP_NOT_NULL(stages, "list_init builds an empty stages list");

    handler_colormap_notify(NULL, stages, NULL);
    TAP_OK(true, "a null event does not crash with a real stages list");

    /* An empty stages list (no stages at all): the walk loop body
     * never runs, another silent no-op */
    memset(&event, 0, sizeof(event));
    event.window = 0x100u;
    event.colormap = 0x200u;
    event.state = XCB_COLORMAP_STATE_INSTALLED;
    handler_colormap_notify(NULL, stages, &event);
    TAP_OK(true, "an empty stages list does not crash");
    list_destroy(stages);

    /* A client tracking colormap window 0x100, not focused: the
     * cached colormap_ids[0] slot is updated to the notified
     * colormap, but xcb_install_colormap is never called, since only
     * a focused client's colormap changes are actually installed */
    table = ohtbl_init(4u, 0u, s_test_h1, s_test_h2, s_test_match, NULL);
    s_build_client(&client, 1u, 0x100u, false);
    s_build_single_client_stage(&stage, &desktop, table, &client);

    stages = list_init(NULL);
    (void) list_ins_next(stages, NULL, &stage);

    memset(&event, 0, sizeof(event));
    event.window = 0x100u;
    event.colormap = 0x999u;
    event.state = XCB_COLORMAP_STATE_INSTALLED;

    s_install_calls = 0;
    handler_colormap_notify(NULL, stages, &event);
    TAP_EQ_INT(client.colormap_windows.colormap_ids[0], 0x999,
            "matching window's cached colormap id is updated");
    TAP_EQ_INT(s_install_calls, 0,
            "an unfocused client's colormap is never installed");

    /* Same client, now focused: the update happens and
     * xcb_install_colormap is called with the new colormap id */
    client.properties.flags = (uint32_t) CLIENT_FLAG_FOCUSED;
    s_install_calls = 0;
    handler_colormap_notify(NULL, stages, &event);
    TAP_EQ_INT(s_install_calls, 1,
            "a focused client's updated colormap is installed once");
    TAP_EQ_INT(s_install_last_id, 0x999,
            "the installed colormap id matches the notification");

    /* Uninstalling (state != INSTALLED): the cached slot is cleared to
     * XCB_NONE, and no install call happens even for a focused
     * client, since XCB_NONE is explicitly excluded from installation */
    memset(&event, 0, sizeof(event));
    event.window = 0x100u;
    event.colormap = 0x999u;
    event.state = XCB_COLORMAP_STATE_UNINSTALLED;

    s_install_calls = 0;
    handler_colormap_notify(NULL, stages, &event);
    TAP_EQ_INT(client.colormap_windows.colormap_ids[0],
            (int) XCB_NONE,
            "uninstalling clears the cached colormap id to XCB_NONE");
    TAP_EQ_INT(s_install_calls, 0,
            "an uninstall notification never calls install_colormap");

    ohtbl_destroy(table);
    cdlist_destroy(stage.desktops);
    list_destroy(stages);

    /* A notification for a window nobody's colormap_windows list
     * mentions: the walk still runs, nothing matches, no crash, and
     * no client is mutated */
    table = ohtbl_init(4u, 0u, s_test_h1, s_test_h2, s_test_match, NULL);
    s_build_client(&client, 2u, 0x555u, true);
    s_build_single_client_stage(&stage, &desktop, table, &client);

    stages = list_init(NULL);
    (void) list_ins_next(stages, NULL, &stage);

    memset(&event, 0, sizeof(event));
    event.window = 0xdeadu;
    event.colormap = 0xbeefu;
    event.state = XCB_COLORMAP_STATE_INSTALLED;

    s_install_calls = 0;
    handler_colormap_notify(NULL, stages, &event);
    TAP_EQ_INT(client.colormap_windows.colormap_ids[0], 0x1111,
            "a non-matching window leaves the client's cache untouched");
    TAP_EQ_INT(s_install_calls, 0,
            "a non-matching window never calls install_colormap");

    /* Two stages, the match on the second one: the outer stages
     * walk must keep going past a stage whose desktops walk found
     * nothing, proving handler_colormap_notify's own loop, not just
     * the per-desktop visitor */
    (void) s_test_two_stages_second_match();

    ohtbl_destroy(table);
    cdlist_destroy(stage.desktops);
    list_destroy(stages);

    return TAP_DONE();
}
