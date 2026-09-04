/**
 * @file tests/cmds/client/test_layer.c
 *
 * @brief Test battery for client stacking-order commands
 *
 * Exercises 'ccmd_client_raise', 'ccmd_client_lower',
 * 'ccmd_client_layer_above', 'ccmd_client_layer_normal',
 * 'ccmd_client_layer_below', 'ccmd_client_cycle_layer', and
 * 'ccmd_desktop_enforce_layers' (cmds/client/layer.c) linked against
 * the real 'policy/stacking.c', 'desktop/dfind.c', 'adt/cdlist.c',
 * and 'adt/ohtbl.c': every one of those is genuinely reachable
 * without a live X connection, since none of them touch XCB
 * themselves, so this file's own stand-ins are only for the raw
 * window-stacking calls ('utils/xcb/window.h'), 'systray_below_
 * window', 'client_group_transient_anchor', and the desktop-action
 * and focus-tracking helpers a live window manager would otherwise
 * supply.  Building real 'desktop_td'/'client_td' fixtures and a real
 * stacking order this way means the actual layer-ordering logic in
 * 'ccmd_desktop_enforce_layers' (which of two clients this file's own
 * window-stacking stand-ins are asked to place above the other) is
 * what gets checked, not just a recording of which stand-in ran.
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
#include <cmds/client/layer.h>
#include <cmds/client/transient.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <policy/stacking.h>


/**
 * @brief Recording stand-in for @a ccmd_client_sync_states
 * @note Complexity: @e O(1)
 */
static int s_sync_states_calls;

void ccmd_client_sync_states(client_td *client)
{
    (void) client;
    s_sync_states_calls++;
}


/**
 * @brief Link-only stand-in for @a ccmd_target_win
 *
 * Answers the client's own window unconditionally: every client this
 * file makes leaves no decoration frame to prefer instead.
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client)
{
    return client->window;
}


/**
 * @brief Link-only stand-in for @a client_group_transient_anchor
 *
 * Answers @c NULL unconditionally: no client this file makes is
 * transient for its whole group (ICCCM section 4.1.2.6), only ever a
 * specific parent, set directly through 'transient_parent' instead.
 *
 * @note Complexity: @e O(1)
 */
client_td *client_group_transient_anchor(const client_td *client)
{
    (void) client;
    return NULL;
}


/**
 * @brief Recording stand-in for @a desktop_action_client_send_back
 * @note Complexity: @e O(1)
 */
static client_td *s_send_back_last;

int desktop_action_client_send_back(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    s_send_back_last = client;
    return 0;
}


/**
 * @brief Recording stand-in for @a desktop_action_client_send_front
 * @note Complexity: @e O(1)
 */
static client_td *s_send_front_last;

int desktop_action_client_send_front(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    s_send_front_last = client;
    return 0;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * Swallows every trace message the function under test emits along
 * the way: nothing under test here checks log output.
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/**
 * @brief Link-only stand-in for @a systray_below_window
 *
 * Answers @c XCB_WINDOW_NONE unconditionally, the same as a session
 * with no system tray: 'ccmd_desktop_enforce_layers' falls back to
 * anchoring the first client of a layer pass on the session's lowest
 * managed client instead, which this file's own stacking order
 * genuinely tracks.
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t systray_below_window(void)
{
    return XCB_WINDOW_NONE;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_client_desktop
 *
 * Answers a test-controlled desktop, or @c NULL by default: 'ccmd_
 * client_raise'/'_lower' only reach 'desktop_action_client_send_
 * front'/'_back' and 'ccmd_desktop_enforce_layers' when this resolves
 * to something real, falling back to a bare 'xcb_window_raise'/
 * '_lower' otherwise.
 *
 * @note Complexity: @e O(1)
 */
static desktop_td *s_owner_desktop;

desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;
    return s_owner_desktop;
}


/**
 * @brief Recording stand-in for @a wm_request_client_redraw
 * @note Complexity: @e O(1)
 */
static int s_redraw_calls;

void wm_request_client_redraw(client_td *client)
{
    (void) client;
    s_redraw_calls++;
}


/** Every window this file's own window-stacking stand-ins are asked
 *  to place, in the order they were asked, cleared by @a s_reset */
#define LOG_CAP (64)
static xcb_window_t s_stack_log[LOG_CAP];
static int s_stack_log_used;

/** Whether the corresponding @a s_stack_log entry came from a
 *  'lower'/'raise' call with no reference window, rather than a
 *  'stack_above'/'stack_below' call naming one in @a s_stack_ref_log */
static bool s_stack_is_bare_log[LOG_CAP];
static xcb_window_t s_stack_ref_log[LOG_CAP];


/**
 * @brief Recording stand-in for @a xcb_window_lower
 * @note Complexity: @e O(1)
 */
void xcb_window_lower(xcb_window_t window)
{
    if (s_stack_log_used < LOG_CAP) {
        s_stack_log[s_stack_log_used] = window;
        s_stack_is_bare_log[s_stack_log_used] = true;
        s_stack_ref_log[s_stack_log_used] = XCB_WINDOW_NONE;
        s_stack_log_used++;
    }
}


/**
 * @brief Recording stand-in for @a xcb_window_raise
 * @note Complexity: @e O(1)
 */
void xcb_window_raise(xcb_window_t window)
{
    if (s_stack_log_used < LOG_CAP) {
        s_stack_log[s_stack_log_used] = window;
        s_stack_is_bare_log[s_stack_log_used] = true;
        s_stack_ref_log[s_stack_log_used] = XCB_WINDOW_NONE;
        s_stack_log_used++;
    }
}


/**
 * @brief Recording stand-in for @a xcb_window_stack_above
 * @note Complexity: @e O(1)
 */
void xcb_window_stack_above(xcb_window_t window, xcb_window_t sibling)
{
    if (s_stack_log_used < LOG_CAP) {
        s_stack_log[s_stack_log_used] = window;
        s_stack_is_bare_log[s_stack_log_used] = false;
        s_stack_ref_log[s_stack_log_used] = sibling;
        s_stack_log_used++;
    }
}


/**
 * @brief Recording stand-in for @a xcb_window_stack_below
 * @note Complexity: @e O(1)
 */
void xcb_window_stack_below(xcb_window_t window, xcb_window_t sibling)
{
    if (s_stack_log_used < LOG_CAP) {
        s_stack_log[s_stack_log_used] = window;
        s_stack_is_bare_log[s_stack_log_used] = false;
        s_stack_ref_log[s_stack_log_used] = sibling;
        s_stack_log_used++;
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
#define MAX_TEST_CLIENTS (32)
#define MAX_TEST_DESKTOPS (4)
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

    return desktop;
}


static client_td *s_make_client(uint32_t id, uint32_t desktop_id,
        enum client_layer_e layer)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    client->desktop_id = desktop_id;
    client->properties.layer = (uint16_t) layer;
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


/* Register 'child' as one of 'parent's own direct transient children,
 * the same way real client setup links the two, so a walk of
 * 'parent->transients' (cmds/client/layer.c) actually reaches it */
static void s_client_add_transient(client_td *parent, client_td *child)
{
    if (parent->transients == NULL) {
        parent->transients = cdlist_init(NULL);
    }
    cdlist_ins_next(parent->transients, NULL, child);
    child->transient_parent = parent;
}


static void s_reset(void)
{
    s_sync_states_calls = 0;
    s_send_back_last = NULL;
    s_send_front_last = NULL;
    s_owner_desktop = NULL;
    s_redraw_calls = 0;
    s_stack_log_used = 0;
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
        if (s_owned_clients[i]->transients != NULL) {
            cdlist_destroy(s_owned_clients[i]->transients);
        }
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* A null client is a silent no-op on every function under test */
static void s_test_null_client_is_a_no_op(void)
{
    s_reset();

    ccmd_client_raise(NULL);
    ccmd_client_lower(NULL);
    ccmd_client_layer_above(NULL);
    ccmd_client_layer_normal(NULL);
    ccmd_client_layer_below(NULL);
    ccmd_client_cycle_layer(NULL);
    ccmd_desktop_enforce_layers(NULL);

    TAP_EQ_INT(s_stack_log_used, 0,
            "a null client never reaches any window-stacking call");
    TAP_EQ_INT(s_sync_states_calls, 0,
            "and never reaches the EWMH state sync either");

    s_teardown();
}


/* A locked client's layer-change request is refused outright, on all
 * three of the layer-setting entry points */
static void s_test_layer_set_locked_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u, 0u, CLIENT_LAYER_NORMAL);
    client->properties.flags |= CLIENT_FLAG_LOCKED;

    ccmd_client_layer_above(client);
    TAP_EQ_INT((int) client->properties.layer, (int) CLIENT_LAYER_NORMAL,
            "a locked client's 'layer above' request is refused");

    ccmd_client_layer_below(client);
    TAP_EQ_INT((int) client->properties.layer, (int) CLIENT_LAYER_NORMAL,
            "and so is its 'layer below' request");

    TAP_EQ_INT(s_sync_states_calls, 0,
            "neither refused request reaches the EWMH state sync");

    s_teardown();
}


/* Each of the three layer-setting entry points stores its own layer
 * and republishes state, with no desktop present to restack */
static void s_test_layer_set_stores_layer_and_syncs(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(2u, 0u, CLIENT_LAYER_NORMAL);

    ccmd_client_layer_above(client);
    TAP_EQ_INT((int) client->properties.layer, (int) CLIENT_LAYER_ABOVE,
            "'layer above' stores the above layer");

    ccmd_client_layer_below(client);
    TAP_EQ_INT((int) client->properties.layer, (int) CLIENT_LAYER_BELOW,
            "'layer below' stores the below layer");

    ccmd_client_layer_normal(client);
    TAP_EQ_INT((int) client->properties.layer, (int) CLIENT_LAYER_NORMAL,
            "'layer normal' stores the normal layer");

    TAP_EQ_INT(s_sync_states_calls, 3,
            "each of the three requests reaches the EWMH state sync"
            " once");
    TAP_EQ_INT(s_redraw_calls, 3,
            "and each one also requests a redraw, desktop or not");

    s_teardown();
}


/* Cycling layer moves 'normal -> above -> below -> normal', matching
 * the order the source's own comment states */
static void s_test_cycle_layer_advances_in_order(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(3u, 0u, CLIENT_LAYER_NORMAL);

    ccmd_client_cycle_layer(client);
    TAP_EQ_INT((int) client->properties.layer, (int) CLIENT_LAYER_ABOVE,
            "cycling from normal moves to above");

    ccmd_client_cycle_layer(client);
    TAP_EQ_INT((int) client->properties.layer, (int) CLIENT_LAYER_BELOW,
            "cycling from above moves to below");

    ccmd_client_cycle_layer(client);
    TAP_EQ_INT((int) client->properties.layer, (int) CLIENT_LAYER_NORMAL,
            "cycling from below moves back to normal");

    s_teardown();
}


/* Raising a client with no owning desktop falls back to a bare
 * 'xcb_window_raise' on its own target window, never touching the
 * desktop-action helper */
static void s_test_raise_with_no_desktop_is_a_bare_raise(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(4u, 0u, CLIENT_LAYER_NORMAL);

    ccmd_client_raise(client);

    TAP_EQ_INT(s_stack_log_used, 1,
            "exactly one window-stacking call is made");
    TAP_OK(s_stack_is_bare_log[0],
            "it is a bare raise, with no reference sibling");
    TAP_EQ_INT((long) s_stack_log[0], (long) client->window,
            "raised directly on the client's own window");
    TAP_OK(s_send_front_last == NULL,
            "the desktop-action send-to-front helper is never reached");

    s_teardown();
}


/* Lowering a client with no owning desktop falls back to a bare
 * 'xcb_window_lower' the same way raising does */
static void s_test_lower_with_no_desktop_is_a_bare_lower(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(5u, 0u, CLIENT_LAYER_NORMAL);

    ccmd_client_lower(client);

    TAP_EQ_INT(s_stack_log_used, 1,
            "exactly one window-stacking call is made");
    TAP_OK(s_stack_is_bare_log[0],
            "it is a bare lower, with no reference sibling");
    TAP_OK(s_send_back_last == NULL,
            "the desktop-action send-to-back helper is never reached");

    s_teardown();
}


/* Raising a client that does have an owning desktop goes through the
 * desktop-action helper and re-enforces layer ordering instead of a
 * bare raise */
static void s_test_raise_with_desktop_uses_send_front(void)
{
    client_td *client;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop(0u);
    client = s_make_client(6u, 0u, CLIENT_LAYER_NORMAL);
    s_desktop_add_client(desktop, client);
    s_owner_desktop = desktop;

    ccmd_client_raise(client);

    TAP_EQ_INT((long) s_send_front_last, (long) client,
            "the desktop-action send-to-front helper is called with"
            " this client");
    TAP_OK(s_stack_log_used > 0,
            "layer enforcement re-places at least this one client");

    s_teardown();
}


/* Two clients sharing one desktop and one layer are placed bottom to
 * top in the same order they were added to the stacking order: the
 * second-added client is placed above the first, not the reverse */
static void s_test_enforce_layers_orders_within_one_layer(void)
{
    client_td *bottom;
    client_td *top;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop(1u);
    bottom = s_make_client(10u, 1u, CLIENT_LAYER_NORMAL);
    top = s_make_client(11u, 1u, CLIENT_LAYER_NORMAL);
    s_desktop_add_client(desktop, bottom);
    s_desktop_add_client(desktop, top);

    ccmd_desktop_enforce_layers(desktop);

    TAP_EQ_INT(s_stack_log_used, 2,
            "both clients of the one layer are placed once each");
    TAP_EQ_INT((long) s_stack_log[0], (long) bottom->window,
            "the first-added client is placed first");
    TAP_OK(s_stack_is_bare_log[0],
            "the very first client of the whole pass is anchored"
            " bare, with no tray or lower sibling present");
    TAP_EQ_INT((long) s_stack_log[1], (long) top->window,
            "the second-added client is placed second");
    TAP_OK(!s_stack_is_bare_log[1],
            "and is stacked above a named sibling rather than bare");
    TAP_EQ_INT((long) s_stack_ref_log[1], (long) bottom->window,
            "that named sibling is the first client, keeping it"
            " directly below the second");

    s_teardown();
}


/* A below-layer client is always placed ahead of a normal-layer one
 * on the same desktop, regardless of which was added to the desktop
 * first */
static void s_test_enforce_layers_orders_across_layers(void)
{
    client_td *normal_client;
    client_td *below_client;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop(2u);
    normal_client = s_make_client(20u, 2u, CLIENT_LAYER_NORMAL);
    below_client = s_make_client(21u, 2u, CLIENT_LAYER_BELOW);
    s_desktop_add_client(desktop, normal_client);
    s_desktop_add_client(desktop, below_client);

    ccmd_desktop_enforce_layers(desktop);

    TAP_EQ_INT(s_stack_log_used, 2,
            "both clients are placed once each");
    TAP_EQ_INT((long) s_stack_log[0], (long) below_client->window,
            "the below-layer client is placed first, despite being"
            " added to the desktop second");
    TAP_EQ_INT((long) s_stack_log[1], (long) normal_client->window,
            "the normal-layer client is placed second, directly"
            " above it");

    s_teardown();
}


/* A transient child sharing its parent's desktop and layer is placed
 * directly above its parent, immediately after it, rather than
 * wherever it would otherwise fall in stacking order */
static void s_test_enforce_layers_clusters_transient_family(void)
{
    client_td *parent;
    client_td *child;
    client_td *unrelated;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop(3u);
    parent = s_make_client(30u, 3u, CLIENT_LAYER_NORMAL);
    unrelated = s_make_client(31u, 3u, CLIENT_LAYER_NORMAL);
    child = s_make_client(32u, 3u, CLIENT_LAYER_NORMAL);
    s_client_add_transient(parent, child);
    s_desktop_add_client(desktop, parent);
    s_desktop_add_client(desktop, unrelated);
    s_desktop_add_client(desktop, child);

    ccmd_desktop_enforce_layers(desktop);

    TAP_EQ_INT(s_stack_log_used, 3,
            "all three clients of the one layer are placed");
    TAP_EQ_INT((long) s_stack_log[1], (long) child->window,
            "the child is placed immediately after its parent,"
            " ahead of the unrelated client added between them");
    TAP_EQ_INT((long) s_stack_ref_log[1], (long) parent->window,
            "stacked directly above the parent by name");

    s_teardown();
}


/* A fullscreen client that currently holds its desktop's focus is
 * raised once more after every layer is placed, even though its own
 * layer was already handled in the loop above */
static void s_test_enforce_layers_raises_focused_fullscreen(void)
{
    client_td *client;
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop(4u);
    client = s_make_client(40u, 4u, CLIENT_LAYER_NORMAL);
    client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    s_desktop_add_client(desktop, client);
    desktop->client_active_id = client->id;

    ccmd_desktop_enforce_layers(desktop);

    TAP_EQ_INT(s_stack_log_used, 2,
            "the client is placed once by the layer pass, then"
            " raised once more for being the focused fullscreen"
            " client");
    TAP_OK(s_stack_is_bare_log[1],
            "the extra raise is a bare 'xcb_window_raise'");
    TAP_EQ_INT((long) s_stack_log[1], (long) client->window,
            "issued on the fullscreen client's own window");

    s_teardown();
}


/* An empty desktop's layer enforcement is a silent no-op: nothing is
 * placed and nothing crashes walking a stacking order of zero */
static void s_test_enforce_layers_empty_desktop_is_a_no_op(void)
{
    desktop_td *desktop;

    s_reset();
    desktop = s_make_desktop(5u);

    ccmd_desktop_enforce_layers(desktop);

    TAP_EQ_INT(s_stack_log_used, 0,
            "an empty desktop's layer enforcement places nothing");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(38);

    s_test_null_client_is_a_no_op();
    s_test_layer_set_locked_is_a_no_op();
    s_test_layer_set_stores_layer_and_syncs();
    s_test_cycle_layer_advances_in_order();
    s_test_raise_with_no_desktop_is_a_bare_raise();
    s_test_lower_with_no_desktop_is_a_bare_lower();
    s_test_raise_with_desktop_uses_send_front();
    s_test_enforce_layers_orders_within_one_layer();
    s_test_enforce_layers_orders_across_layers();
    s_test_enforce_layers_clusters_transient_family();
    s_test_enforce_layers_raises_focused_fullscreen();
    s_test_enforce_layers_empty_desktop_is_a_no_op();

    return TAP_DONE();
}
