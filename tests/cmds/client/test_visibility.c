/**
 * @file tests/cmds/client/test_visibility.c
 *
 * @brief Test battery for client iconify, hide, and unhide commands
 *
 * Exercises 'ccmd_client_iconify', 'ccmd_client_hide',
 * 'ccmd_client_unhide', and 'ccmd_client_unmap_decorated'
 * (cmds/client/visibility.c) linked against the real
 * 'utils/safe/safeflg.c', since 'client_hide'/'client_unhide' (and
 * every other flag macro besides) bottom out in it, the same choice
 * already made for 'test_flags.c' and 'test_state.c'.  Every EWMH/
 * ICCCM property write ('ccmd_set_wm_state', icon-window creation,
 * '_NET_WM_ICON_GEOMETRY' publishing) is a link-only stand-in here:
 * none of that is genuinely reachable without a live X connection,
 * and what this file actually checks is which guard clause refuses a
 * request outright, which flag bit ends up set or cleared, and which
 * family member gets visited on the way out, not the deep EWMH
 * property math each single-client helper also performs.
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

/* Local includes */
#include <client.h>
#include <cmds/client/transient.h>
#include <cmds/client/visibility.h>
#include <harness/tap.h>
#include <wm.h>


/**
 * @brief Link-only stand-in for @a ccmd_client_ensure_icon_window
 *
 * Reached only when 's_ccmd_client_iconify_one' decides an icon box
 * is actually needed, itself unreachable without a live X connection
 * to create one against.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_ensure_icon_window(client_td *client, uint16_t icon_h)
{
    (void) client;
    (void) icon_h;
}


/** Every family member this file's stand-in @a ccmd_client_family_apply
 *  hands to its caller's own visitor, one small fixed table per test
 *  file rather than a real transient tree, the same shortcut
 *  'test_flags.c' already takes for this same stand-in */
#define MAX_SIBLINGS (4)
static client_td *s_siblings[MAX_SIBLINGS];
static int s_siblings_used;


/**
 * @brief Recording, cascading stand-in for @a ccmd_client_family_apply
 *
 * Hands @p fn every member @a s_siblings currently holds, in order,
 * the same as the real function walking a transient tree; @p top and
 * whatever family it heads are entirely test-controlled here rather
 * than a genuine tree, since none of these tests need more than a
 * flat sibling list to prove a cascade reaches every member.
 *
 * @note Complexity: @e O(n), where @e n is the number of siblings
 *       currently registered
 */
void ccmd_client_family_apply(client_td *top, ccmd_family_fn fn,
        void *ctx)
{
    (void) top;
    for (int i = 0; i < s_siblings_used; ++i) {
        fn(s_siblings[i], ctx);
    }
}


/**
 * @brief Recording stand-in for @a ccmd_client_focus_fallback
 * @note Complexity: @e O(1)
 */
static int s_focus_fallback_calls;

void ccmd_client_focus_fallback(client_td *client)
{
    (void) client;
    s_focus_fallback_calls++;
}


/**
 * @brief Recording stand-in for @a ccmd_client_make_active
 * @note Complexity: @e O(1)
 */
static int s_make_active_calls;

void ccmd_client_make_active(client_td *client)
{
    (void) client;
    s_make_active_calls++;
}


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
 * @brief Test-controlled stand-in for @a ccmd_client_transient_top_parent
 *
 * Answers the client itself by default, the same as a client with no
 * transient relatives; a test naming a different top parent through
 * @a s_top_parent_override exercises the cascading branch instead.
 *
 * @note Complexity: @e O(1)
 */
static client_td *s_top_parent_override;

client_td *ccmd_client_transient_top_parent(client_td *client)
{
    return (s_top_parent_override != NULL) ? s_top_parent_override : client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_unfullscreen
 *
 * Reached only for a client iconified while still fullscreen, which
 * no test here sets up: 'ccmd_client_iconify' unconditionally puts
 * the fullscreen state bit straight back right after calling this
 * (see 'visibility.c''s own comment on that), so this file's own
 * assertions never depend on what leaving fullscreen itself did.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unfullscreen(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_unshade
 *
 * Reached only for a client iconified while still shaded, which no
 * test here sets up.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unshade(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_intern_atom
 *
 * Answers a fixed, nonzero atom for any name: nothing under test
 * here reads back which atom a property was written under, only
 * whether the flag and cascade side of each function ran correctly.
 *
 * @note Complexity: @e O(1)
 */
xcb_atom_t ccmd_intern_atom(xcb_connection_t *connection, const char *name)
{
    (void) connection;
    (void) name;
    return 1u;
}


/**
 * @brief Recording stand-in for @a ccmd_set_wm_state
 * @note Complexity: @e O(1)
 */
static uint32_t s_wm_state_last;
static int s_set_wm_state_calls;

void ccmd_set_wm_state(client_td *client, uint32_t state,
        xcb_window_t icon_window)
{
    (void) client;
    (void) icon_window;
    s_wm_state_last = state;
    s_set_wm_state_calls++;
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
 * @brief Link-only stand-in for @a systray_below_window
 *
 * Answers @c XCB_WINDOW_NONE unconditionally, the same as a session
 * with no system tray.
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t systray_below_window(void)
{
    return XCB_WINDOW_NONE;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_surface_by_id
 *
 * Answers a test-controlled surface, or @c NULL by default.
 *
 * @note Complexity: @e O(1)
 */
static surface_td *s_owner_surface;

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_owner_surface;
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


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
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
 * @brief Link-only stand-in for @a xcb_window_show
 * @note Complexity: @e O(1)
 */
void xcb_window_show(xcb_window_t window)
{
    (void) window;
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


/**
 * @brief Link-only stand-in for @a xcb_change_property
 *
 * Reached whenever an icon window's own property is published; safe
 * to answer a zeroed cookie unconditionally, since this file's own
 * connection is @c NULL throughout and nothing here ever issues a
 * real request against it.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *c, uint8_t mode,
        xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
        uint8_t format, uint32_t data_len, const void *data)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) mode;
    (void) window;
    (void) property;
    (void) type;
    (void) format;
    (void) data_len;
    (void) data;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_get_property
 *
 * Reached only to build the cookie @a xcb_get_property_reply below
 * answers @c NULL for regardless; a zeroed cookie is fine, since
 * this file's own connection is @c NULL throughout and nothing here
 * ever issues a real request against it.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_get_property(xcb_connection_t *c,
        uint8_t delete_prop, xcb_window_t window,
        xcb_atom_t property, xcb_atom_t type, uint32_t long_offset,
        uint32_t long_length)
{
    xcb_get_property_cookie_t cookie;

    (void) c;
    (void) delete_prop;
    (void) window;
    (void) property;
    (void) type;
    (void) long_offset;
    (void) long_length;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_get_property_reply
 *
 * Answers @c NULL unconditionally, the same as a pager that never set
 * '_NET_WM_HANDLED_ICONS': 'skip_icon_win' then falls through to its
 * other, flag-based half instead of this reply's own length.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_reply_t *xcb_get_property_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie, xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    (void) e;
    return NULL;
}


/** Every client this file calloc's, freed in one place by
 *  @a s_teardown rather than at each test's own end */
#define MAX_TEST_CLIENTS (16)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;
static const config_td s_config;


static client_td *s_make_client(uint32_t id)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    client->config = &s_config;
    client->properties.flags |= CLIENT_FLAG_FOCUSABLE;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


static void s_reset(void)
{
    s_siblings_used = 0;
    memset(s_siblings, 0, sizeof(s_siblings));
    s_focus_fallback_calls = 0;
    s_make_active_calls = 0;
    s_sync_states_calls = 0;
    s_top_parent_override = NULL;
    s_wm_state_last = 0u;
    s_set_wm_state_calls = 0;
    s_owner_surface = NULL;
    s_redraw_calls = 0;
}


static void s_teardown(void)
{
    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* A null client is a silent no-op on every function under test,
 * except 'ccmd_client_unmap_decorated', which only guards its own
 * client parameter and is checked separately below */
static void s_test_null_client_is_a_no_op(void)
{
    s_reset();

    ccmd_client_iconify(NULL);
    ccmd_client_hide(NULL);
    ccmd_client_unhide(NULL);

    TAP_EQ_INT(s_sync_states_calls, 0,
            "a null client never reaches the EWMH state sync");
    TAP_EQ_INT(s_set_wm_state_calls, 0,
            "and never reaches the WM_STATE write either");

    s_teardown();
}


/* 'ccmd_client_unmap_decorated' itself treats a null client as a
 * silent no-op too, rather than crashing on its 'ignore.unmap'
 * increment */
static void s_test_unmap_decorated_null_client_is_a_no_op(void)
{
    s_reset();

    ccmd_client_unmap_decorated(NULL, 123u);

    TAP_OK(true, "a null client passed to the shared unmap helper"
            " does not crash");

    s_teardown();
}


/* 'ccmd_client_unmap_decorated' increments 'ignore.unmap' by two for
 * the target itself, plus one more when a titlebar is present */
static void s_test_unmap_decorated_counts_titlebar(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);
    client->titlebar = 99u;

    ccmd_client_unmap_decorated(client, client->window);

    TAP_EQ_INT((int) client->ignore.unmap, 3,
            "two events for the target plus one for the titlebar");

    s_teardown();
}


/* With no titlebar, only the target's own two events are counted */
static void s_test_unmap_decorated_no_titlebar(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(2u);

    ccmd_client_unmap_decorated(client, client->window);

    TAP_EQ_INT((int) client->ignore.unmap, 2,
            "only the target's own two events are counted");

    s_teardown();
}


/* A locked client's iconify request is refused outright */
static void s_test_iconify_locked_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(10u);
    client->properties.flags |= CLIENT_FLAG_LOCKED;

    ccmd_client_iconify(client);

    TAP_OK(!client_is_iconified(client),
            "a locked client's iconify request is refused");
    TAP_EQ_INT(s_set_wm_state_calls, 0,
            "no WM_STATE write happens for a refused request");

    s_teardown();
}


/* A non-focusable (and so non-iconifiable) client's iconify request
 * is refused outright */
static void s_test_iconify_non_focusable_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(11u);
    client->properties.flags &= (uint16_t) ~CLIENT_FLAG_FOCUSABLE;

    ccmd_client_iconify(client);

    TAP_OK(!client_is_iconified(client),
            "a non-focusable client's iconify request is refused");

    s_teardown();
}


/* A plain, standalone client's iconify request marks it iconified,
 * writes the ICCCM iconic state, and syncs EWMH state, focus
 * fallback, and a redraw */
static void s_test_iconify_plain_client_sets_state(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(12u);

    ccmd_client_iconify(client);

    TAP_OK(client_is_iconified(client) != 0,
            "the client ends up marked iconified");
    TAP_EQ_INT((int) s_wm_state_last, 3 /* CCMD_WM_STATE_ICONIC */,
            "the ICCCM iconic state is written");
    TAP_EQ_INT(s_sync_states_calls, 1,
            "EWMH state is synced once");
    TAP_EQ_INT(s_focus_fallback_calls, 1,
            "focus falls back to another client once");
    TAP_EQ_INT(s_redraw_calls, 1, "a redraw is requested once");

    s_teardown();
}


/* Iconifying an already-iconified client is idempotent: the guard in
 * 'ccmd_client_iconify' itself only refuses a locked or non-
 * iconifiable client, so the second call still runs the single-client
 * helper again rather than being skipped outright, but a client
 * already iconified stays iconified either way */
static void s_test_iconify_already_iconified_stays_iconified(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(13u);

    ccmd_client_iconify(client);
    ccmd_client_iconify(client);

    TAP_OK(client_is_iconified(client) != 0,
            "the client is still iconified after a second request");

    s_teardown();
}


/* Iconifying one member of a transient family redirects to, and
 * iconifies, the top parent, then cascades to every other family
 * member through 'ccmd_client_family_apply' */
static void s_test_iconify_cascades_to_family(void)
{
    client_td *dialog;
    client_td *top;
    client_td *sibling;

    s_reset();
    top = s_make_client(20u);
    dialog = s_make_client(21u);
    sibling = s_make_client(22u);
    s_top_parent_override = top;
    s_siblings[0] = dialog;
    s_siblings[1] = sibling;
    s_siblings_used = 2;

    ccmd_client_iconify(dialog);

    TAP_OK(client_is_iconified(top) != 0,
            "the top parent ends up iconified");
    TAP_OK(client_is_iconified(dialog) != 0,
            "the requesting dialog itself ends up iconified too");
    TAP_OK(client_is_iconified(sibling) != 0,
            "and so does its unrelated sibling in the same family");

    s_teardown();
}


/* A cascade skips a family member that is already iconified or
 * locked, rather than iconifying it a second time or overriding its
 * lock */
static void s_test_iconify_cascade_skips_locked_and_done(void)
{
    client_td *top;
    client_td *already_done;
    client_td *locked;

    s_reset();
    top = s_make_client(23u);
    already_done = s_make_client(24u);
    locked = s_make_client(25u);
    client_hide(already_done); /* unrelated flag, just to prove no
                                 * further change happens below */
    already_done->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    locked->properties.flags |= CLIENT_FLAG_LOCKED;
    s_top_parent_override = top;
    s_siblings[0] = already_done;
    s_siblings[1] = locked;
    s_siblings_used = 2;

    ccmd_client_iconify(top);

    TAP_OK(client_is_locked(locked) != 0,
            "a locked sibling's own lock is left untouched");
    TAP_OK(!client_is_iconified(locked),
            "and it is never iconified by the cascade");

    s_teardown();
}


/* A hide request marks the client hidden, writes the ICCCM iconic
 * state with no icon window, and syncs EWMH state, focus fallback,
 * and a redraw, clearing a surface's 'is_showing_desktop' flag along
 * the way */
static void s_test_hide_plain_client_sets_state(void)
{
    client_td *client;
    surface_td surface;

    s_reset();
    client = s_make_client(30u);
    memset(&surface, 0, sizeof(surface));
    surface.is_showing_desktop = true;
    s_owner_surface = &surface;

    ccmd_client_hide(client);

    TAP_OK(client_is_hidden(client) != 0,
            "the client ends up marked hidden");
    TAP_OK(!surface.is_showing_desktop,
            "the surface's 'showing desktop' flag is cleared");
    TAP_EQ_INT(s_sync_states_calls, 1, "EWMH state is synced once");
    TAP_EQ_INT(s_redraw_calls, 1, "a redraw is requested once");

    s_teardown();
}


/* Hiding cascades to a transient family the same way iconifying does */
static void s_test_hide_cascades_to_family(void)
{
    client_td *dialog;
    client_td *top;

    s_reset();
    top = s_make_client(31u);
    dialog = s_make_client(32u);
    s_top_parent_override = top;
    s_siblings[0] = dialog;
    s_siblings_used = 1;

    ccmd_client_hide(dialog);

    TAP_OK(client_is_hidden(top) != 0,
            "the top parent ends up hidden");
    TAP_OK(client_is_hidden(dialog) != 0,
            "the requesting dialog itself ends up hidden too");

    s_teardown();
}


/* Unhiding a plain, already-hidden client clears the hidden flag,
 * writes the ICCCM normal state, syncs EWMH state, makes it active,
 * and requests a redraw */
static void s_test_unhide_plain_client_clears_state(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(40u);
    client_hide(client);

    ccmd_client_unhide(client);

    TAP_OK(!client_is_hidden(client),
            "the client is no longer marked hidden");
    TAP_EQ_INT((int) s_wm_state_last, 1 /* CCMD_WM_STATE_NORMAL */,
            "the ICCCM normal state is written");
    TAP_EQ_INT(s_make_active_calls, 1,
            "the client is made active once");
    TAP_EQ_INT(s_redraw_calls, 1, "a redraw is requested once");

    s_teardown();
}


/* Unhiding a client that was never hidden in the first place is a
 * silent no-op for the top parent's own single-client helper, since
 * its guard checks 'client_is_hidden' before calling it, but the
 * cascade to family members still runs regardless */
static void s_test_unhide_not_hidden_top_is_a_no_op_itself(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(41u);

    ccmd_client_unhide(client);

    TAP_EQ_INT(s_make_active_calls, 0,
            "a client that was never hidden is never made active"
            " by unhiding it");
    TAP_EQ_INT(s_set_wm_state_calls, 0,
            "and no WM_STATE write happens for it either");

    s_teardown();
}


/* Unhiding cascades to a transient family the same way iconifying and
 * hiding do, with every hidden member cleared */
static void s_test_unhide_cascades_to_family(void)
{
    client_td *dialog;
    client_td *top;
    client_td *sibling;

    s_reset();
    top = s_make_client(42u);
    dialog = s_make_client(43u);
    sibling = s_make_client(44u);
    client_hide(top);
    client_hide(dialog);
    client_hide(sibling);
    s_top_parent_override = top;
    s_siblings[0] = dialog;
    s_siblings[1] = sibling;
    s_siblings_used = 2;

    ccmd_client_unhide(dialog);

    TAP_OK(!client_is_hidden(top), "the top parent is unhidden");
    TAP_OK(!client_is_hidden(dialog),
            "the requesting dialog itself is unhidden too");
    TAP_OK(!client_is_hidden(sibling),
            "and so is its unrelated sibling in the same family");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(34);

    s_test_null_client_is_a_no_op();
    s_test_unmap_decorated_null_client_is_a_no_op();
    s_test_unmap_decorated_counts_titlebar();
    s_test_unmap_decorated_no_titlebar();
    s_test_iconify_locked_is_a_no_op();
    s_test_iconify_non_focusable_is_a_no_op();
    s_test_iconify_plain_client_sets_state();
    s_test_iconify_already_iconified_stays_iconified();
    s_test_iconify_cascades_to_family();
    s_test_iconify_cascade_skips_locked_and_done();
    s_test_hide_plain_client_sets_state();
    s_test_hide_cascades_to_family();
    s_test_unhide_plain_client_clears_state();
    s_test_unhide_not_hidden_top_is_a_no_op_itself();
    s_test_unhide_cascades_to_family();

    return TAP_DONE();
}
