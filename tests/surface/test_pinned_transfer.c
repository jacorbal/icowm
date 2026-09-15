/**
 * @file tests/surface/test_pinned_transfer.c
 *
 * @brief Test battery for moving every pinned client to one desktop
 *
 * Exercises 'surface_client_pinned_transfer_all' (surface/actions/
 * clients.c) linked against the real stacking order (policy/
 * stacking.c) and the real per-desktop client lookup (desktop/
 * dfind.c), so a pinned client is only ever seen by the function
 * under test the same way it would be in the real program: through
 * a real 'stacking_walk' filtered by a real 'desktop_find_client_by_id'
 * lookup against each desktop's own client table.  Everything else
 * the file under test calls (desktop moves themselves, the EWMH
 * '_NET_WM_DESKTOP' ping, and the desktop lookup by ID) is a
 * recording stand-in, so what actually gets exercised is this
 * file's own collection, allocation, and looping, not the rest of
 * the window manager.
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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Local includes */
#include <client.h>
#include <client/state.h>
#include <desktop.h>
#include <harness/tap.h>
#include <policy/stacking.h>
#include <surface.h>
#include <surface/client.h>


/**
 * @brief Link-only stand-in for @a ccmd_client_unmap_decorated
 *
 * Reached only by 'surface_client_hide_all', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unmap_decorated(client_td *client, xcb_window_t target)
{
    (void) client;
    (void) target;
}


/**
 * @brief Link-only stand-in for @a ccmd_desktop_enforce_layers
 *
 * Reached only by 'surface_client_show_all', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_desktop_enforce_layers(desktop_td *desktop)
{
    (void) desktop;
}


/**
 * @brief Link-only stand-in for @a client_focus_fallback
 *
 * Reached only by 'surface_client_show_all', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void client_focus_fallback(desktop_td *desktop, surface_td *surface,
        client_td *exclude)
{
    (void) desktop;
    (void) surface;
    (void) exclude;
}


/**
 * @brief Link-only stand-in for @a surface_monitor_for_point
 *
 * Reached only by 'surface_client_reflow_all', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
monitor_td surface_monitor_for_point(const surface_td *surface,
        struct position_s pos)
{
    monitor_td m;

    (void) surface;
    (void) pos;
    memset(&m, 0, sizeof(m));

    return m;
}


/**
 * @brief Link-only stand-in for @a systray_below_window
 *
 * Reached only by 'surface_client_show_all', which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t systray_below_window(void)
{
    return XCB_WINDOW_NONE;
}


/**
 * @brief Link-only stand-in for @a xcb_window_hide
 * @note Complexity: @e O(1)
 */
void xcb_window_hide(xcb_window_t window)
{
    (void) window;
}


/**
 * @brief Link-only stand-in for @a xcb_window_lower
 * @note Complexity: @e O(1)
 */
void xcb_window_lower(xcb_window_t window)
{
    (void) window;
}


/**
 * @brief Link-only stand-in for @a xcb_window_move
 * @note Complexity: @e O(1)
 */
void xcb_window_move(xcb_window_t window, int32_t x, int32_t y)
{
    (void) window;
    (void) x;
    (void) y;
}


/**
 * @brief Link-only stand-in for @a xcb_window_raise
 * @note Complexity: @e O(1)
 */
void xcb_window_raise(xcb_window_t window)
{
    (void) window;
}


/**
 * @brief Link-only stand-in for @a xcb_window_show
 * @note Complexity: @e O(1)
 */
void xcb_window_show(xcb_window_t window)
{
    (void) window;
}


/**
 * @brief Link-only stand-in for @a xcb_window_stack_above
 * @note Complexity: @e O(1)
 */
void xcb_window_stack_above(xcb_window_t window, xcb_window_t sibling)
{
    (void) window;
    (void) sibling;
}


/**
 * @brief Link-only stand-in for @a xcb_window_stack_below
 * @note Complexity: @e O(1)
 */
void xcb_window_stack_below(xcb_window_t window, xcb_window_t sibling)
{
    (void) window;
    (void) sibling;
}


/** Desktops this file's own 'surface_desktop_get' stand-in answers
 *  from, registered by @a s_register_desktop */
#define MAX_TEST_DESKTOPS (8)
static desktop_td *s_desktops_by_id[MAX_TEST_DESKTOPS];
static int s_desktops_registered = 0;


/**
 * @brief Test-controlled stand-in for @a surface_desktop_get
 *
 * The real one walks 'surface->desktops' by ID; this one answers
 * from a small table this file fills directly, so a test never has
 * to build a well-formed cdlist just to satisfy a lookup it is not
 * itself the one exercising.
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 *       registered
 */
desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;

    for (int i = 0; i < s_desktops_registered; ++i) {
        if (s_desktops_by_id[i] != NULL &&
                s_desktops_by_id[i]->id == (xcb_window_t) desktop_id) {
            return s_desktops_by_id[i];
        }
    }

    return NULL;
}


/** Recording stand-ins' logs: every 'desktop_action_client_move',
 *  'desktop_action_client_send_back', and 'ccmd_publish_wm_desktop'
 *  call the function under test makes, in the order it makes them */
#define LOG_CAP (256)
static client_td *s_move_log[LOG_CAP];
static int s_move_log_used;
static client_td *s_send_back_log[LOG_CAP];
static int s_send_back_log_used;
static client_td *s_publish_log[LOG_CAP];
static uint32_t s_publish_desktop_log[LOG_CAP];
static int s_publish_log_used;


/**
 * @brief Recording stand-in for @a desktop_action_client_move
 *
 * Records the client moved rather than actually moving it: what is
 * under test here is which clients the caller decides to move, and
 * how many times, not the hash-table mechanics of the move itself
 * (already covered on its own elsewhere).
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_move(desktop_td *from, desktop_td *to,
        client_td *client)
{
    (void) from;
    (void) to;

    if (client != NULL && s_move_log_used < LOG_CAP) {
        s_move_log[s_move_log_used] = client;
        s_move_log_used++;
    }

    return 0;
}


/**
 * @brief Recording stand-in for @a desktop_action_client_send_back
 * @note Complexity: @e O(1)
 */
int desktop_action_client_send_back(desktop_td *desktop, client_td *client)
{
    (void) desktop;

    if (client != NULL && s_send_back_log_used < LOG_CAP) {
        s_send_back_log[s_send_back_log_used] = client;
        s_send_back_log_used++;
    }

    return 0;
}


/**
 * @brief Recording stand-in for @a ccmd_publish_wm_desktop
 * @note Complexity: @e O(1)
 */
void ccmd_publish_wm_desktop(client_td *client, uint32_t desktop_id)
{
    if (client != NULL && s_publish_log_used < LOG_CAP) {
        s_publish_log[s_publish_log_used] = client;
        s_publish_desktop_log[s_publish_log_used] = desktop_id;
        s_publish_log_used++;
    }
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
    return ((const client_td *) key1)->id == ((const client_td *) key2)->id;
}


/** Every client and desktop this file calloc's, freed in one place by
 *  @a s_teardown rather than at each test's own end */
#define MAX_TEST_CLIENTS (64)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;
static desktop_td *s_owned_desktops[MAX_TEST_DESKTOPS];
static int s_owned_desktops_used;


static desktop_td *s_make_desktop(uint32_t id)
{
    desktop_td *desktop = calloc(1, sizeof(*desktop));

    desktop->id = (xcb_window_t) id;
    desktop->clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    (void) stacking_create(desktop);
    s_owned_desktops[s_owned_desktops_used] = desktop;
    s_owned_desktops_used++;
    s_desktops_by_id[s_desktops_registered] = desktop;
    s_desktops_registered++;

    return desktop;
}


static client_td *s_make_client(uint32_t id, bool pinned)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    if (pinned) {
        client->properties.flags |= CLIENT_FLAG_PIN;
    }
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


/* Put a client on a desktop's own table and the shared stacking
 * order, bottom of that desktop's stack first */
static void s_desktop_add_client(desktop_td *desktop, client_td *client)
{
    ohtbl_insert(desktop->clients, client);
    (void) stacking_add(desktop, client);
}


static void s_reset(void)
{
    s_move_log_used = 0;
    s_send_back_log_used = 0;
    s_publish_log_used = 0;
    s_desktops_registered = 0;
    memset(s_desktops_by_id, 0, sizeof(s_desktops_by_id));
}


static void s_teardown(void)
{
    /* Each desktop must give up its own clients from the shared
     * stacking order, and it needs its still-intact client table to
     * find them, so this comes before either table or client is
     * freed below */
    for (int i = 0; i < s_owned_desktops_used; ++i) {
        stacking_destroy(s_owned_desktops[i]);
    }

    for (int i = 0; i < s_owned_desktops_used; ++i) {
        ohtbl_destroy(s_owned_desktops[i]->clients);
        free(s_owned_desktops[i]);
    }
    s_owned_desktops_used = 0;

    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* A desktop with more pinned clients than the old fixed 32-slot
 * buffer used to hold is transferred in full: none of them are left
 * behind, the regression this whole file exists to guard against */
static void s_test_no_drop_past_old_fixed_cap(void)
{
    surface_td surface;
    desktop_td *from_desktop;
    desktop_td *to_desktop;
    client_td *pinned[40];
    bool seen[40];
    bool all_seen;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    from_desktop = s_make_desktop(0u);
    to_desktop = s_make_desktop(1u);

    for (int i = 0; i < 40; ++i) {
        pinned[i] = s_make_client(100u + (uint32_t) i, true);
        s_desktop_add_client(from_desktop, pinned[i]);
        seen[i] = false;
    }

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, from_desktop);
    cdlist_ins_next(surface.desktops, NULL, to_desktop);
    surface.desktop_count = 2u;

    surface_client_pinned_transfer_all(&surface, 1u);

    TAP_EQ_INT(s_move_log_used, 40,
            "all 40 pinned clients are moved, none dropped past 32");

    all_seen = true;
    for (int i = 0; i < s_move_log_used; ++i) {
        for (int j = 0; j < 40; ++j) {
            if (s_move_log[i] == pinned[j]) {
                seen[j] = true;
            }
        }
    }
    for (int j = 0; j < 40; ++j) {
        if (!seen[j]) {
            all_seen = false;
        }
    }
    TAP_OK(all_seen, "every one of the 40 pinned clients is among"
            " the moved ones, not just the first 32");

    TAP_EQ_INT(s_publish_log_used, 40,
            "the desktop-changed ping is sent for every one of them");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* The exact case the counting pass's own guard used to mishandle: a
 * single pinned client, counted with capacity zero */
static void s_test_single_pinned_client(void)
{
    surface_td surface;
    desktop_td *from_desktop;
    desktop_td *to_desktop;
    client_td *client;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    from_desktop = s_make_desktop(0u);
    to_desktop = s_make_desktop(1u);
    client = s_make_client(200u, true);
    s_desktop_add_client(from_desktop, client);

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, from_desktop);
    cdlist_ins_next(surface.desktops, NULL, to_desktop);
    surface.desktop_count = 2u;

    surface_client_pinned_transfer_all(&surface, 1u);

    TAP_EQ_INT(s_move_log_used, 1,
            "a single pinned client is still moved (counting pass with"
            " zero capacity does not silently count zero)");
    TAP_OK(s_move_log_used == 1 && s_move_log[0] == client,
            "and it is the right one");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* A desktop with no pinned clients at all is skipped outright: no
 * allocation, no move, no send-back */
static void s_test_no_pinned_clients_is_a_no_op(void)
{
    surface_td surface;
    desktop_td *from_desktop;
    desktop_td *to_desktop;
    client_td *unpinned;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    from_desktop = s_make_desktop(0u);
    to_desktop = s_make_desktop(1u);
    unpinned = s_make_client(300u, false);
    s_desktop_add_client(from_desktop, unpinned);

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, from_desktop);
    cdlist_ins_next(surface.desktops, NULL, to_desktop);
    surface.desktop_count = 2u;

    surface_client_pinned_transfer_all(&surface, 1u);

    TAP_EQ_INT(s_move_log_used, 0,
            "a desktop with nothing pinned on it triggers no move");
    TAP_EQ_INT(s_publish_log_used, 0, "and no desktop-changed ping");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* Only the pinned clients on a source desktop are ever touched; an
 * ordinary, unpinned one stays exactly where it is */
static void s_test_unpinned_clients_never_move(void)
{
    surface_td surface;
    desktop_td *from_desktop;
    desktop_td *to_desktop;
    client_td *pinned;
    client_td *unpinned;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    from_desktop = s_make_desktop(0u);
    to_desktop = s_make_desktop(1u);
    pinned = s_make_client(400u, true);
    unpinned = s_make_client(401u, false);
    s_desktop_add_client(from_desktop, pinned);
    s_desktop_add_client(from_desktop, unpinned);

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, from_desktop);
    cdlist_ins_next(surface.desktops, NULL, to_desktop);
    surface.desktop_count = 2u;

    surface_client_pinned_transfer_all(&surface, 1u);

    TAP_EQ_INT(s_move_log_used, 1, "only the pinned client is moved");
    TAP_OK(s_move_log_used == 1 && s_move_log[0] == pinned,
            "and the unpinned one is never touched");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* The pinned client that was active on the source desktop arrives
 * active on the target too, and is the only one skipped for a
 * send-to-back */
static void s_test_active_client_focus_follows(void)
{
    surface_td surface;
    desktop_td *from_desktop;
    desktop_td *to_desktop;
    client_td *active;
    client_td *idle;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    from_desktop = s_make_desktop(0u);
    to_desktop = s_make_desktop(1u);
    idle = s_make_client(500u, true);
    active = s_make_client(501u, true);
    s_desktop_add_client(from_desktop, idle);
    s_desktop_add_client(from_desktop, active);
    from_desktop->client_active_id = active->id;

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, from_desktop);
    cdlist_ins_next(surface.desktops, NULL, to_desktop);
    surface.desktop_count = 2u;

    surface_client_pinned_transfer_all(&surface, 1u);

    TAP_EQ_INT((long) to_desktop->client_active_id, (long) active->id,
            "the previously active pinned client is active on the"
            " target desktop too");
    TAP_OK(to_desktop->is_focus_dirty,
            "the target desktop is marked to refresh focus decoration");
    TAP_EQ_INT((long) from_desktop->client_active_id, 0,
            "the source desktop no longer claims it as active");

    TAP_EQ_INT(s_send_back_log_used, 1,
            "only the client that was not active is sent to the back");
    TAP_OK(s_send_back_log_used == 1 && s_send_back_log[0] == idle,
            "and it is the idle one, not the one that was active");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* Several pinned clients, bottom of the source stack to top, arrive
 * sent to the back in the reverse order: each lands under the one
 * before it, so the whole run is not silently reshuffled on the very
 * next desktop change */
static void s_test_relative_order_is_preserved(void)
{
    surface_td surface;
    desktop_td *from_desktop;
    desktop_td *to_desktop;
    client_td *bottom;
    client_td *middle;
    client_td *top;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    from_desktop = s_make_desktop(0u);
    to_desktop = s_make_desktop(1u);
    bottom = s_make_client(600u, true);
    middle = s_make_client(601u, true);
    top = s_make_client(602u, true);
    /* Added bottom, then middle, then top: 'stacking_add' places
     * each new arrival at the top, so this is also their final
     * bottom-to-top order on 'from_desktop' */
    s_desktop_add_client(from_desktop, bottom);
    s_desktop_add_client(from_desktop, middle);
    s_desktop_add_client(from_desktop, top);

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, from_desktop);
    cdlist_ins_next(surface.desktops, NULL, to_desktop);
    surface.desktop_count = 2u;

    surface_client_pinned_transfer_all(&surface, 1u);

    TAP_EQ_INT(s_send_back_log_used, 3, "all three are sent to back");
    TAP_OK(s_send_back_log_used == 3 && s_send_back_log[0] == top &&
            s_send_back_log[1] == middle && s_send_back_log[2] == bottom,
            "processed top-of-source-stack first, bottom-of-source-"
            "stack last, so the last one sent to the back (the true"
            " bottom, on a target with nothing further sent after it)"
            " is the one that was already at the bottom");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* A pinned client already on the target desktop itself is left
 * alone: the target is never treated as one of its own sources */
static void s_test_target_desktop_is_never_its_own_source(void)
{
    surface_td surface;
    desktop_td *from_desktop;
    desktop_td *to_desktop;
    client_td *already_there;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    from_desktop = s_make_desktop(0u);
    to_desktop = s_make_desktop(1u);
    already_there = s_make_client(700u, true);
    s_desktop_add_client(to_desktop, already_there);

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, from_desktop);
    cdlist_ins_next(surface.desktops, NULL, to_desktop);
    surface.desktop_count = 2u;

    surface_client_pinned_transfer_all(&surface, 1u);

    TAP_EQ_INT(s_move_log_used, 0,
            "a pinned client already on the target desktop is never"
            " moved to itself");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* An unknown target desktop ID leaves everything untouched */
static void s_test_unknown_target_is_a_no_op(void)
{
    surface_td surface;
    desktop_td *from_desktop;
    client_td *pinned;

    s_reset();
    memset(&surface, 0, sizeof(surface));

    from_desktop = s_make_desktop(0u);
    pinned = s_make_client(800u, true);
    s_desktop_add_client(from_desktop, pinned);

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, from_desktop);
    surface.desktop_count = 1u;

    surface_client_pinned_transfer_all(&surface, 99u);

    TAP_EQ_INT(s_move_log_used, 0,
            "an unresolvable target desktop ID moves nothing, and does"
            " not crash");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


/* Pinned clients scattered across more than one source desktop are
 * all gathered onto the one target, none of either source's own
 * dropped */
static void s_test_multiple_source_desktops(void)
{
    surface_td surface;
    desktop_td *desktop_a;
    desktop_td *desktop_b;
    desktop_td *target;
    client_td *from_a[35];
    client_td *from_b[6];

    s_reset();
    memset(&surface, 0, sizeof(surface));

    desktop_a = s_make_desktop(0u);
    desktop_b = s_make_desktop(1u);
    target = s_make_desktop(2u);

    for (int i = 0; i < 35; ++i) {
        from_a[i] = s_make_client(900u + (uint32_t) i, true);
        s_desktop_add_client(desktop_a, from_a[i]);
    }
    for (int i = 0; i < 6; ++i) {
        from_b[i] = s_make_client(1000u + (uint32_t) i, true);
        s_desktop_add_client(desktop_b, from_b[i]);
    }

    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, desktop_a);
    cdlist_ins_next(surface.desktops, NULL, desktop_b);
    cdlist_ins_next(surface.desktops, NULL, target);
    surface.desktop_count = 3u;

    surface_client_pinned_transfer_all(&surface, 2u);

    TAP_EQ_INT(s_move_log_used, 41,
            "every pinned client from both source desktops (35 plus"
            " 6) is moved to the target, across the old 32-slot cap");

    cdlist_destroy(surface.desktops);
    s_teardown();
}


int main(void)
{
    TAP_PLAN(19);

    s_test_no_drop_past_old_fixed_cap();
    s_test_single_pinned_client();
    s_test_no_pinned_clients_is_a_no_op();
    s_test_unpinned_clients_never_move();
    s_test_active_client_focus_follows();
    s_test_relative_order_is_preserved();
    s_test_target_desktop_is_never_its_own_source();
    s_test_unknown_target_is_a_no_op();
    s_test_multiple_source_desktops();

    return TAP_DONE();
}
