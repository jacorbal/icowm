/**
 * @file tests/cmds/client/test_ewmh.c
 *
 * @brief Test battery for EWMH and ICCCM window-property management
 *        (cmds/client/ewmh.c)
 *
 * Every function under test is exercised through the real source
 * file, linked directly.  'xcb_ewmh_connection_get' is a test-
 * controlled stand-in answering either 'NULL' (exercising every
 * function's own null-EWMH-connection tolerance) or the address of
 * a real, fully-initialized 'xcb_ewmh_connection_t': a plain struct
 * this project's own headers already expose as a genuine complete
 * type (not opaque), so every atom field the real implementation
 * reads off it can be filled in directly with distinct, recognizable
 * values, and every assertion below can check exactly which atom(s)
 * ended up published.  'xcb_connection_get' is a link-only stand-in
 * answering a fixed non-null, never-dereferenced value: nothing in
 * this file ever reads through the pointer itself, since every
 * genuine libxcb call it reaches ('xcb_change_property',
 * 'xcb_delete_property', 'xcb_send_event', 'xcb_ewmh_set_wm_state')
 * is itself a recording stand-in rather than the real libxcb
 * function, the same reasoning 'test_move.c' and 'test_resize.c'
 * both already give for doing the same.  'atom_intern' is a test-
 * controlled stand-in answering a fixed, recognizable atom for any
 * name (by default 'XCB_ATOM_NONE', exercising the two ICCCM
 * functions' own missing-atom refusal, unless a test registers a
 * real value through 's_set_wm_state_atom'), and additionally
 * records the exact name string it was asked to intern, letting the
 * '_NET_WM_STATE_FOCUSED' lookup 'ccmd_client_sync_states' makes
 * through this same function, rather than through the real EWMH
 * connection struct, be told apart from the two ICCCM 'WM_STATE'
 * lookups the other functions under test make.  'client_last_user_
 * time' is a test-controlled stand-in answering whatever a test
 * registers, by default zero, matching the real function's own
 * fallback behavior in @a ccmd_client_ping_send.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <client.h>
#include <cmds/client/ewmh.h>
#include <defs/desktop.h>
#include <harness/tap.h>


/** Real, fully-initialized EWMH connection this file's own
 *  @a xcb_ewmh_connection_get stand-in answers, filled with
 *  distinct, recognizable atom numbers so a test can tell exactly
 *  which one a call published */
static xcb_ewmh_connection_t s_ewmh;
static bool s_ewmh_present;


/**
 * @brief Test-controlled stand-in for @a xcb_ewmh_connection_get
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return (s_ewmh_present) ? &s_ewmh : NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Answers a fixed non-null, never-dereferenced value: nothing in
 * this file ever reads through the pointer itself, since every
 * genuine libxcb call reached through it is itself a stand-in
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return (xcb_connection_t *) (uintptr_t) 1;
}


/** The fixed atom @a atom_intern answers for the 'WM_STATE' name, by
 *  default 'XCB_ATOM_NONE', registered through @a s_set_wm_state_atom */
static xcb_atom_t s_wm_state_atom;

/** Name of the last @a atom_intern call, letling a test tell apart
 *  a 'WM_STATE' lookup from a '_NET_WM_STATE_FOCUSED' one */
static char s_last_interned_name[64];
static int s_intern_count;


/**
 * @brief Test-controlled, recording stand-in for @a atom_intern
 * @note Complexity: @e O(1)
 */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) only_if_exists;

    strncpy(s_last_interned_name, name, sizeof(s_last_interned_name) - 1);
    s_last_interned_name[sizeof(s_last_interned_name) - 1] = '\0';
    s_intern_count++;

    if (strcmp(name, "WM_STATE") == 0) {
        return s_wm_state_atom;
    }

    /* '_NET_WM_STATE_FOCUSED': a fixed, recognizable atom distinct
     * from every field 's_ewmh' itself carries */
    return (xcb_atom_t) 9001u;
}


static void s_set_wm_state_atom(xcb_atom_t atom)
{
    s_wm_state_atom = atom;
}


/** Recorded arguments from the last @a xcb_change_property call */
static xcb_window_t s_cp_window;
static xcb_atom_t s_cp_property;
static xcb_atom_t s_cp_type;
static uint32_t s_cp_data_len;
static uint32_t s_cp_data[4];
static int s_cp_count;


/**
 * @brief Recording stand-in for @a xcb_change_property
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *connection,
        uint8_t mode, xcb_window_t window, xcb_atom_t property,
        xcb_atom_t type, uint8_t format, uint32_t data_len,
        const void *data)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    size_t i;

    (void) connection;
    (void) mode;
    (void) format;

    s_cp_window = window;
    s_cp_property = property;
    s_cp_type = type;
    s_cp_data_len = data_len;
    memset(s_cp_data, 0, sizeof(s_cp_data));
    for (i = 0u; i < data_len && i < 4u; i++) {
        s_cp_data[i] = ((const uint32_t *) data)[i];
    }
    s_cp_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_delete_property call */
static xcb_window_t s_dp_window;
static xcb_atom_t s_dp_property;
static int s_dp_count;


/**
 * @brief Recording stand-in for @a xcb_delete_property
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_delete_property(xcb_connection_t *connection,
        xcb_window_t window, xcb_atom_t property)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) connection;

    s_dp_window = window;
    s_dp_property = property;
    s_dp_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_send_event call */
static xcb_client_message_event_t s_sent_event;
static int s_send_event_count;


/**
 * @brief Recording stand-in for @a xcb_send_event
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_send_event(xcb_connection_t *connection,
        uint8_t propagate, xcb_window_t destination,
        uint32_t event_mask, const char *event)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) connection;
    (void) propagate;
    (void) destination;
    (void) event_mask;

    memcpy(&s_sent_event, event, sizeof(s_sent_event));
    s_send_event_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_ewmh_set_wm_state call */
static xcb_window_t s_state_window;
static xcb_atom_t s_state_list[13];
static uint32_t s_state_len;
static int s_state_count;


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_wm_state
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_wm_state(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};
    int i;

    (void) ewmh;

    s_state_window = window;
    s_state_len = list_len;
    memset(s_state_list, 0, sizeof(s_state_list));
    for (i = 0; (uint32_t) i < list_len && (size_t) i < 13u; i++) {
        s_state_list[i] = list[i];
    }
    s_state_count++;

    return cookie;
}


static bool s_state_has(xcb_atom_t atom)
{
    uint32_t i;

    for (i = 0u; i < s_state_len; i++) {
        if (s_state_list[i] == atom) {
            return true;
        }
    }
    return false;
}


/** The fixed value @a client_last_user_time answers, registered
 *  through @a s_set_last_user_time */
static uint32_t s_last_user_time;


/**
 * @brief Test-controlled stand-in for @a client_last_user_time
 * @note Complexity: @e O(1)
 */
uint32_t client_last_user_time(void)
{
    return s_last_user_time;
}


static void s_set_last_user_time(uint32_t t)
{
    s_last_user_time = t;
}


static void s_set_ewmh_present(bool present)
{
    xcb_atom_t atom;

    memset(&s_ewmh, 0, sizeof(s_ewmh));

    /* Every field read by any function under test gets a distinct
     * atom number, ten apart, so a wrong field showing up in a
     * recorded call is never mistaken for the right one */
    atom = 100u;
    s_ewmh.WM_PROTOCOLS = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_MAXIMIZED_HORZ = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_MAXIMIZED_VERT = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_FULLSCREEN = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_HIDDEN = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_STICKY = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_DEMANDS_ATTENTION = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_SHADED = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_ABOVE = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_BELOW = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_MODAL = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_SKIP_TASKBAR = atom;
    atom += 10u;
    s_ewmh._NET_WM_STATE_SKIP_PAGER = atom;
    atom += 10u;
    s_ewmh._NET_FRAME_EXTENTS = atom;
    atom += 10u;
    s_ewmh._NET_WM_DESKTOP = atom;
    atom += 10u;
    s_ewmh._NET_WM_PING = atom;

    s_ewmh_present = present;
}


static void s_reset(void)
{
    s_set_wm_state_atom(XCB_ATOM_NONE);
    memset(s_last_interned_name, 0, sizeof(s_last_interned_name));
    s_intern_count = 0;
    s_cp_window = XCB_WINDOW_NONE;
    s_cp_property = XCB_ATOM_NONE;
    s_cp_type = XCB_ATOM_NONE;
    s_cp_data_len = 0u;
    memset(s_cp_data, 0, sizeof(s_cp_data));
    s_cp_count = 0;
    s_dp_window = XCB_WINDOW_NONE;
    s_dp_property = XCB_ATOM_NONE;
    s_dp_count = 0;
    memset(&s_sent_event, 0, sizeof(s_sent_event));
    s_send_event_count = 0;
    s_state_window = XCB_WINDOW_NONE;
    memset(s_state_list, 0, sizeof(s_state_list));
    s_state_len = 0u;
    s_state_count = 0;
    s_set_last_user_time(0u);
    s_ewmh_present = false;
    memset(&s_ewmh, 0, sizeof(s_ewmh));
}


/* ccmd_intern_atom is a thin, direct wrapper over atom_intern with
 * only_if_exists forced to false */
static void s_test_intern_atom_wraps_atom_intern(void)
{
    xcb_atom_t result;

    s_reset();
    s_set_wm_state_atom((xcb_atom_t) 555u);

    result = ccmd_intern_atom(xcb_connection_get(), "WM_STATE");

    TAP_EQ_INT((long) result, 555,
            "ccmd_intern_atom answers whatever atom_intern itself"
            " answers");
    TAP_EQ_STR(s_last_interned_name, "WM_STATE",
            "the exact name string is passed through unchanged");
}


/* A null client is a silent no-op for every entry point in this
 * file, never a crash */
static void s_test_null_client_is_noop(void)
{
    s_reset();
    s_set_ewmh_present(true);

    ccmd_client_sync_states(NULL);
    ccmd_set_wm_state(NULL, CCMD_WM_STATE_NORMAL, XCB_WINDOW_NONE);
    ccmd_clear_wm_state(NULL);
    ccmd_publish_frame_extents(NULL, 1u, 1u, 1u, 1u);
    ccmd_publish_wm_desktop(NULL, 0u);
    ccmd_client_ping_send(NULL);

    TAP_OK(true, "every entry point tolerates a null client without"
            " crashing");
}


/* Every EWMH-touching entry point is a silent no-op when there is no
 * EWMH connection at all, without touching any of the stand-ins */
static void s_test_no_ewmh_connection_is_noop(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 42u;
    client.hints_ewmh.ping.is_supported = true;

    ccmd_client_sync_states(&client);
    ccmd_publish_frame_extents(&client, 1u, 1u, 1u, 1u);
    ccmd_publish_wm_desktop(&client, 3u);
    ccmd_client_ping_send(&client);

    TAP_EQ_INT(s_state_count, 0,
            "no _NET_WM_STATE write happens without an EWMH"
            " connection");
    TAP_EQ_INT(s_cp_count, 0,
            "no xcb_change_property write happens without one"
            " either");
    TAP_EQ_INT(s_send_event_count, 0,
            "no ping is sent without one");
}


/* A plain, stateless client publishes an empty _NET_WM_STATE list */
static void s_test_sync_states_plain_client_is_empty(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 7u;
    client.properties.layer = (uint16_t) CLIENT_LAYER_NORMAL;

    ccmd_client_sync_states(&client);

    TAP_EQ_INT(s_state_count, 1,
            "exactly one _NET_WM_STATE write happens");
    TAP_EQ_INT((long) s_state_window, 7,
            "it targets the client's own window");
    TAP_EQ_INT((long) s_state_len, 0,
            "a plain client with nothing set publishes an empty"
            " list");
}


/* Both maximized axes, fullscreen, hidden-via-iconified, pinned,
 * urgent, and shaded each publish their own single matching atom,
 * all independent of one another */
static void s_test_sync_states_maximized_fullscreen_hidden(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED |
        (uint16_t) CLIENT_STATE_FULLSCREEN;

    ccmd_client_sync_states(&client);

    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_MAXIMIZED_HORZ),
            "a fully maximized client publishes MAXIMIZED_HORZ");
    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_MAXIMIZED_VERT),
            "and MAXIMIZED_VERT too");
    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_FULLSCREEN),
            "a fullscreen client publishes FULLSCREEN");
}


/* Iconified alone, without CLIENT_FLAG_HIDDEN, still publishes
 * HIDDEN: EWMH does not distinguish the two internal concepts */
static void s_test_sync_states_iconified_publishes_hidden(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) CLIENT_STATE_ICONIFIED;
    client.properties.layer = (uint16_t) CLIENT_LAYER_NORMAL;

    ccmd_client_sync_states(&client);

    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_HIDDEN),
            "an iconified client alone still publishes HIDDEN");
    TAP_EQ_INT((long) s_state_len, 1,
            "and nothing else, since no other field is set");
}


/* CLIENT_FLAG_HIDDEN alone, without being iconified, also publishes
 * HIDDEN, the second of the two internal concepts EWMH does not
 * distinguish */
static void s_test_sync_states_flag_hidden_publishes_hidden(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.properties.flags = (uint32_t) CLIENT_FLAG_HIDDEN;

    ccmd_client_sync_states(&client);

    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_HIDDEN),
            "a client hidden via the flag alone still publishes"
            " HIDDEN");
}


/* Pinned, urgent, and shaded each publish their own single matching
 * atom */
static void s_test_sync_states_pinned_urgent_shaded(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.properties.flags = (uint32_t) CLIENT_FLAG_PIN |
        (uint32_t) CLIENT_FLAG_URGENT |
        (uint32_t) CLIENT_FLAG_SHADED;

    ccmd_client_sync_states(&client);

    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_STICKY),
            "a pinned client publishes STICKY");
    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_DEMANDS_ATTENTION),
            "an urgent client publishes DEMANDS_ATTENTION");
    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_SHADED),
            "a shaded client publishes SHADED");
}


/* The above and below layers each publish their own single matching
 * atom, and the normal layer, the third possible value, publishes
 * neither */
static void s_test_sync_states_layer_above_below_normal(void)
{
    client_td above;
    client_td below;
    client_td normal;

    s_reset();
    s_set_ewmh_present(true);
    memset(&above, 0, sizeof(above));
    above.properties.layer = (uint16_t) CLIENT_LAYER_ABOVE;
    ccmd_client_sync_states(&above);
    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_ABOVE),
            "a client on the above layer publishes ABOVE");

    s_reset();
    s_set_ewmh_present(true);
    memset(&below, 0, sizeof(below));
    below.properties.layer = (uint16_t) CLIENT_LAYER_BELOW;
    ccmd_client_sync_states(&below);
    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_BELOW),
            "a client on the below layer publishes BELOW");

    s_reset();
    s_set_ewmh_present(true);
    memset(&normal, 0, sizeof(normal));
    normal.properties.layer = (uint16_t) CLIENT_LAYER_NORMAL;
    ccmd_client_sync_states(&normal);
    TAP_EQ_INT((long) s_state_len, 0,
            "a client on the normal layer publishes neither ABOVE"
            " nor BELOW");
}


/* A modal client publishes MODAL */
static void s_test_sync_states_modal(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.properties.flags = (uint32_t) CLIENT_FLAG_MODAL;

    ccmd_client_sync_states(&client);

    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_MODAL),
            "a modal client publishes MODAL");
}


/* A focused client is the one atom resolved through ccmd_intern_atom
 * rather than read straight off the EWMH connection struct */
static void s_test_sync_states_focused_uses_intern_atom(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.properties.flags = (uint32_t) CLIENT_FLAG_FOCUSED;

    ccmd_client_sync_states(&client);

    TAP_EQ_STR(s_last_interned_name, "_NET_WM_STATE_FOCUSED",
            "a focused client's atom is resolved by name through"
            " ccmd_intern_atom");
    TAP_OK(s_state_has((xcb_atom_t) 9001u),
            "and that resolved atom is the one published");
}


/* Skip-taskbar and skip-pager each publish their own single matching
 * atom, independent of every other flag */
static void s_test_sync_states_skip_taskbar_and_pager(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.properties.flags = (uint32_t) CLIENT_FLAG_SKIP_TASKBAR |
        (uint32_t) CLIENT_FLAG_SKIP_PAGER;

    ccmd_client_sync_states(&client);

    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_SKIP_TASKBAR),
            "skip-taskbar publishes SKIP_TASKBAR");
    TAP_OK(s_state_has(s_ewmh._NET_WM_STATE_SKIP_PAGER),
            "skip-pager publishes SKIP_PAGER");
}


/* Every single state at once fills the full 13-entry list, none
 * overwriting another */
static void s_test_sync_states_everything_at_once(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED |
        (uint16_t) CLIENT_STATE_FULLSCREEN |
        (uint16_t) CLIENT_STATE_ICONIFIED;
    client.properties.layer = (uint16_t) CLIENT_LAYER_ABOVE;
    client.properties.flags = (uint32_t) CLIENT_FLAG_PIN |
        (uint32_t) CLIENT_FLAG_URGENT |
        (uint32_t) CLIENT_FLAG_SHADED |
        (uint32_t) CLIENT_FLAG_MODAL |
        (uint32_t) CLIENT_FLAG_FOCUSED |
        (uint32_t) CLIENT_FLAG_SKIP_TASKBAR |
        (uint32_t) CLIENT_FLAG_SKIP_PAGER;

    ccmd_client_sync_states(&client);

    TAP_EQ_INT((long) s_state_len, 12,
            "every one of the twelve independent states this"
            " combination triggers ends up in the published list"
            " (thirteen possible atoms total, minus BELOW, which"
            " this combination's ABOVE layer excludes)");
}


/* ccmd_set_wm_state writes format-32 WM_STATE with the state and
 * icon window packed into its two cardinal values */
static void s_test_set_wm_state_writes_values(void)
{
    client_td client;

    s_reset();
    s_set_wm_state_atom((xcb_atom_t) 33u);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 9u;

    ccmd_set_wm_state(&client, CCMD_WM_STATE_ICONIC,
            (xcb_window_t) 55u);

    TAP_EQ_INT(s_cp_count, 1,
            "exactly one xcb_change_property write happens");
    TAP_EQ_INT((long) s_cp_window, 9,
            "it targets the client's own window");
    TAP_EQ_INT((long) s_cp_property, 33,
            "the property is the interned WM_STATE atom");
    TAP_EQ_INT((long) s_cp_type, 33,
            "WM_STATE's own type is itself, ICCCM's self-referential"
            " convention");
    TAP_EQ_INT((long) s_cp_data_len, 2,
            "exactly two cardinal values are written");
    TAP_EQ_INT((long) s_cp_data[0], CCMD_WM_STATE_ICONIC,
            "the first value is the requested ICCCM state");
    TAP_EQ_INT((long) s_cp_data[1], 55,
            "the second value is the requested icon window");
}


/* ccmd_set_wm_state is a no-op when WM_STATE fails to resolve to a
 * real atom */
static void s_test_set_wm_state_missing_atom_is_noop(void)
{
    client_td client;

    s_reset();
    s_set_wm_state_atom(XCB_ATOM_NONE);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 9u;

    ccmd_set_wm_state(&client, CCMD_WM_STATE_NORMAL, XCB_WINDOW_NONE);

    TAP_EQ_INT(s_cp_count, 0,
            "no write happens when WM_STATE itself fails to"
            " resolve");
}


/* ccmd_set_wm_state is a no-op for a client with no window yet */
static void s_test_set_wm_state_no_window_is_noop(void)
{
    client_td client;

    s_reset();
    s_set_wm_state_atom((xcb_atom_t) 33u);
    memset(&client, 0, sizeof(client));
    client.window = XCB_WINDOW_NONE;

    ccmd_set_wm_state(&client, CCMD_WM_STATE_NORMAL, XCB_WINDOW_NONE);

    TAP_EQ_INT(s_cp_count, 0,
            "no write happens for a client with no window yet");
}


/* ccmd_clear_wm_state deletes the WM_STATE property from the
 * client's window */
static void s_test_clear_wm_state_deletes_property(void)
{
    client_td client;

    s_reset();
    s_set_wm_state_atom((xcb_atom_t) 44u);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 3u;

    ccmd_clear_wm_state(&client);

    TAP_EQ_INT(s_dp_count, 1,
            "exactly one xcb_delete_property call happens");
    TAP_EQ_INT((long) s_dp_window, 3,
            "it targets the client's own window");
    TAP_EQ_INT((long) s_dp_property, 44,
            "and removes the interned WM_STATE atom");
}


/* ccmd_clear_wm_state is a no-op when WM_STATE fails to resolve, and
 * when the client has no window yet */
static void s_test_clear_wm_state_noop_cases(void)
{
    client_td client;

    s_reset();
    s_set_wm_state_atom(XCB_ATOM_NONE);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 3u;
    ccmd_clear_wm_state(&client);
    TAP_EQ_INT(s_dp_count, 0,
            "no delete happens when WM_STATE fails to resolve");

    s_reset();
    s_set_wm_state_atom((xcb_atom_t) 44u);
    memset(&client, 0, sizeof(client));
    client.window = XCB_WINDOW_NONE;
    ccmd_clear_wm_state(&client);
    TAP_EQ_INT(s_dp_count, 0,
            "no delete happens for a client with no window yet"
            " either");
}


/* ccmd_publish_frame_extents writes all four sides, in the
 * left/right/top/bottom order EWMH itself mandates */
static void s_test_publish_frame_extents_writes_all_four_sides(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 5u;

    ccmd_publish_frame_extents(&client, 1u, 2u, 30u, 4u);

    TAP_EQ_INT(s_cp_count, 1,
            "exactly one xcb_change_property write happens");
    TAP_EQ_INT((long) s_cp_window, 5,
            "it targets the client's own window");
    TAP_EQ_INT((long) s_cp_property, (long) s_ewmh._NET_FRAME_EXTENTS,
            "the property is _NET_FRAME_EXTENTS");
    TAP_EQ_INT((long) s_cp_data_len, 4,
            "all four cardinal values are written");
    TAP_EQ_INT((long) s_cp_data[0], 1, "left comes first");
    TAP_EQ_INT((long) s_cp_data[1], 2, "right comes second");
    TAP_EQ_INT((long) s_cp_data[2], 30, "top comes third");
    TAP_EQ_INT((long) s_cp_data[3], 4, "bottom comes fourth");
}


/* ccmd_publish_wm_desktop writes the requested desktop id verbatim
 * for a plain, unpinned client */
static void s_test_publish_wm_desktop_plain_client(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 8u;

    ccmd_publish_wm_desktop(&client, 4u);

    TAP_EQ_INT((long) s_cp_property, (long) s_ewmh._NET_WM_DESKTOP,
            "the property is _NET_WM_DESKTOP");
    TAP_EQ_INT((long) s_cp_data[0], 4,
            "a plain client publishes exactly the requested desktop"
            " id");
}


/* ccmd_publish_wm_desktop publishes the EWMH "all desktops" sentinel
 * for a pinned client, regardless of the desktop id requested */
static void s_test_publish_wm_desktop_pinned_client_publishes_all(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 8u;
    client.properties.flags = (uint32_t) CLIENT_FLAG_PIN;

    ccmd_publish_wm_desktop(&client, 4u);

    TAP_EQ_INT((long) s_cp_data[0], (long) WM_DESKTOP_ID_ALL,
            "a pinned client always publishes the EWMH \"all"
            " desktops\" sentinel, ignoring the requested desktop"
            " id entirely");
}


/* ccmd_client_ping_send is a no-op when the client does not
 * advertise _NET_WM_PING support at all */
static void s_test_ping_send_unsupported_is_noop(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.ping.is_supported = false;

    ccmd_client_ping_send(&client);

    TAP_EQ_INT(s_send_event_count, 0,
            "no ping is sent to a client without _NET_WM_PING"
            " support");
}


/* ccmd_client_ping_send builds and sends a genuine WM_PROTOCOLS
 * ClientMessage carrying _NET_WM_PING, the client's own window
 * twice over (as both the message target and its own data32[2]
 * echo, per EWMH), and marks the ping outstanding */
static void s_test_ping_send_builds_message_and_marks_waiting(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    s_set_last_user_time(12345u);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 21u;
    client.hints_ewmh.ping.is_supported = true;

    ccmd_client_ping_send(&client);

    TAP_EQ_INT(s_send_event_count, 1,
            "exactly one ClientMessage is sent");
    TAP_EQ_INT((long) s_sent_event.response_type, XCB_CLIENT_MESSAGE,
            "it is a genuine ClientMessage");
    TAP_EQ_INT((long) s_sent_event.window, 21,
            "it targets the client's own window");
    TAP_EQ_INT((long) s_sent_event.type, (long) s_ewmh.WM_PROTOCOLS,
            "the ClientMessage type is WM_PROTOCOLS");
    TAP_EQ_INT((long) s_sent_event.data.data32[0],
            (long) s_ewmh._NET_WM_PING,
            "data32[0] names _NET_WM_PING");
    TAP_EQ_INT((long) s_sent_event.data.data32[1], 12345,
            "data32[1] carries the last known user-interaction"
            " timestamp");
    TAP_EQ_INT((long) s_sent_event.data.data32[2], 21,
            "data32[2] echoes the client's own window, per EWMH"
            " section 4.6");
    TAP_EQ_INT((long) client.hints_ewmh.ping.last_sent, 12345,
            "the timestamp actually sent is recorded on the client"
            " itself");
    TAP_OK(client.hints_ewmh.ping.is_waiting,
            "the ping is marked outstanding on the client");
}


/* When no real user interaction has ever been observed,
 * ccmd_client_ping_send falls back to XCB_CURRENT_TIME rather than
 * a genuine zero timestamp */
static void s_test_ping_send_falls_back_to_current_time(void)
{
    client_td client;

    s_reset();
    s_set_ewmh_present(true);
    s_set_last_user_time(0u);
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 1u;
    client.hints_ewmh.ping.is_supported = true;

    ccmd_client_ping_send(&client);

    TAP_EQ_INT((long) s_sent_event.data.data32[1], XCB_CURRENT_TIME,
            "with no last user time recorded, the ping falls back"
            " to XCB_CURRENT_TIME rather than a genuine zero"
            " timestamp");
}


int main(void)
{
    TAP_PLAN(63);

    s_test_intern_atom_wraps_atom_intern();
    s_test_null_client_is_noop();
    s_test_no_ewmh_connection_is_noop();
    s_test_sync_states_plain_client_is_empty();
    s_test_sync_states_maximized_fullscreen_hidden();
    s_test_sync_states_iconified_publishes_hidden();
    s_test_sync_states_flag_hidden_publishes_hidden();
    s_test_sync_states_pinned_urgent_shaded();
    s_test_sync_states_layer_above_below_normal();
    s_test_sync_states_modal();
    s_test_sync_states_focused_uses_intern_atom();
    s_test_sync_states_skip_taskbar_and_pager();
    s_test_sync_states_everything_at_once();
    s_test_set_wm_state_writes_values();
    s_test_set_wm_state_missing_atom_is_noop();
    s_test_set_wm_state_no_window_is_noop();
    s_test_clear_wm_state_deletes_property();
    s_test_clear_wm_state_noop_cases();
    s_test_publish_frame_extents_writes_all_four_sides();
    s_test_publish_wm_desktop_plain_client();
    s_test_publish_wm_desktop_pinned_client_publishes_all();
    s_test_ping_send_unsupported_is_noop();
    s_test_ping_send_builds_message_and_marks_waiting();
    s_test_ping_send_falls_back_to_current_time();

    return TAP_DONE();
}
