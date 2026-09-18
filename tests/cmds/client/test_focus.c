/**
 * @file tests/cmds/client/test_focus.c
 *
 * @brief Test battery for close/kill, focus/unfocus, make-active, and
 *        focus-fallback actions over a client (cmds/client/focus.c)
 *
 * Covers 'ccmd_client_close', 'ccmd_client_kill', 'ccmd_client_focus',
 * 'ccmd_client_unfocus', 'ccmd_client_make_active',
 * 'ccmd_client_focus_fallback', and 'client_focus_fallback', linked
 * against the real 'cmds/client/focus.c' and the real flag mutators
 * ('utils/safe/safeflg.c') 'client_focus_mark'/'client_unfocus_mark'
 * (client/predicates.h) resolve to, so a genuine focused-flag
 * transition runs on every call, the same as 'test_flags.c' already
 * does for the same macros.  'ccmd_client_restore' and its static
 * helper 's_ccmd_client_restore_one' are deliberately left untested
 * here: both reach deep into the maximize/fullscreen command set and
 * the transient-family snapshot machinery, a second, independent
 * dependency stage entirely apart from focus itself, better suited to
 * its own battery once 'cmds/client/maximize.c' and 'cmds/state.c' have
 * test coverage of their own to build on, per this file's own report.
 *
 * Every other external dependency is a test-controlled or recording
 * stand-in: 'wm_get_stage_by_id', 'wm_get_client_desktop',
 * 'wm_get_stages', 'wm_get_config', 'wm_request_client_redraw',
 * 'focus_order_best', 'focus_order_to_top', 'focus_apply',
 * 'ccmd_client_focus_target' (identity by default),
 * 'client_theme_layout_resync', 'client_last_user_time',
 * 'xcb_connection_get', 'xcb_ewmh_connection_get', and the real libxcb
 * entry points 'xcb_send_event', 'xcb_kill_client',
 * 'xcb_set_input_focus', 'xcb_install_colormap',
 * 'xcb_ewmh_set_active_window', and the project's own
 * 'xcb_window_destroy'/'xcb_window_show' wrappers (utils/xcb/window.h),
 * none of which are linked against a live X connection or the real
 * libxcb.  'cctl_kill_register' is a recording stand-in for the same
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <cctl/kill.h>
#include <client.h>
#include <cmds/client/ewmh.h>
#include <cmds/client/flags.h>
#include <cmds/client/focus.h>
#include <cmds/client/maximize.h>
#include <cmds/client/screen.h>
#include <cmds/client/state.h>
#include <cmds/client/transient.h>
#include <config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <policy/focus.h>
#include <stage.h>
#include <wm.h>


/**
 * @brief Test-controlled stand-in for @a wm_get_stage_by_id
 * @note Complexity: @e O(1)
 */
static stage_td *s_stage_by_id;

stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return s_stage_by_id;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_client_desktop
 * @note Complexity: @e O(1)
 */
static desktop_td *s_client_desktop;

desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;
    return s_client_desktop;
}


/**
 * @brief Link-only stand-in for @a wm_get_stages
 *
 * Handed straight through to @a focus_apply's own stand-in below,
 * which ignores it entirely; never dereferenced here.
 *
 * @note Complexity: @e O(1)
 */
list_td *wm_get_stages(void)
{
    return NULL;
}


/**
 * @brief Stand-in for @a wm_get_config, returning @c NULL unless a test
 *        points it at a configuration of its own
 * @note Complexity: @e O(1)
 */
static config_td *s_config;

config_td *wm_get_config(void)
{
    return s_config;
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
 * @brief Test-controlled stand-in for @a focus_order_best
 *
 * Answers whichever client a test placed in @a s_focus_order_best_
 * result, recording how many times it ran and the last predicate
 * handed to it, without walking any real desktop stacking order:
 * what is under test in this file is 'client_focus_fallback''s own
 * two-pass sequencing, not 'focus_order_best''s own search (already
 * covered on its own elsewhere).
 *
 * @note Complexity: @e O(1)
 */
static int s_focus_order_best_calls;
static client_td *s_focus_order_best_result;

client_td *focus_order_best(const desktop_td *desktop,
        bool (*is_valid)(const client_td *candidate, void *data),
        void *data)
{
    (void) desktop;
    (void) is_valid;
    (void) data;

    s_focus_order_best_calls++;
    return s_focus_order_best_result;
}


/**
 * @brief Recording stand-in for @a focus_order_to_top
 * @note Complexity: @e O(1)
 */
static int s_focus_order_to_top_calls;
static client_td *s_focus_order_to_top_last;

void focus_order_to_top(client_td *client)
{
    s_focus_order_to_top_calls++;
    s_focus_order_to_top_last = client;
}


/**
 * @brief Recording stand-in for @a focus_apply
 * @note Complexity: @e O(1)
 */
static int s_focus_apply_calls;
static client_td *s_focus_apply_last_client;
static stage_td *s_focus_apply_last_stage;
static desktop_td *s_focus_apply_last_desktop;
static bool s_focus_apply_last_raise;

void focus_apply(list_td *stages, stage_td *stage,
        desktop_td *desktop, client_td *client,
        bool raise, const config_td *cfg)
{
    (void) stages;
    (void) cfg;

    s_focus_apply_calls++;
    s_focus_apply_last_stage = stage;
    s_focus_apply_last_desktop = desktop;
    s_focus_apply_last_client = client;
    s_focus_apply_last_raise = raise;
}


/**
 * @brief Test-controlled stand-in for @a ccmd_client_focus_target
 *
 * Identity by default (answers @p client unchanged), matching the
 * overwhelming majority of clients, which have no transient dialog to
 * redirect to; a test opts into a redirect by setting
 * @a s_focus_target_override.
 *
 * @note Complexity: @e O(1)
 */
static client_td *s_focus_target_override;

client_td *ccmd_client_focus_target(client_td *client)
{
    return (s_focus_target_override != NULL)
        ? s_focus_target_override : client;
}


/**
 * @brief Link-only stand-ins for the transient-family functions
 *        'ccmd_client_restore' would need; never reached by any test
 *        in this file, since 'ccmd_client_restore' itself is not
 *        under test here
 *
 * @note Complexity: @e O(1)
 */
client_td *ccmd_client_transient_top_parent(client_td *client)
{
    return client;
}


client_td **ccmd_client_transient_family_snapshot_anywhere(
        client_td *top, size_t *count)
{
    (void) top;
    if (count != NULL) {
        *count = 0u;
    }
    return NULL;
}


/** Link-only stand-in for @a ccmd_client_unhide; never reached, same
 *  reason as the two functions just above */
void ccmd_client_unhide(client_td *client)
{
    (void) client;
}


/**
 * @brief Recording stand-in for @a client_theme_layout_resync
 * @note Complexity: @e O(1)
 */
static int s_theme_resync_calls;
static bool s_theme_resync_last_is_active;

void client_theme_layout_resync(client_td *client, bool is_active)
{
    (void) client;
    s_theme_resync_calls++;
    s_theme_resync_last_is_active = is_active;
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
 * @brief Recording stand-in for @a ccmd_client_unurge
 * @note Complexity: @e O(1)
 */
static int s_unurge_calls;

void ccmd_client_unurge(client_td *client)
{
    if (client != NULL) {
        client->properties.flags &= (uint16_t) ~CLIENT_FLAG_URGENT;
    }
    s_unurge_calls++;
}


/**
 * @brief Link-only stand-ins for every function only
 * 's_ccmd_client_restore_one' (through 'ccmd_client_restore', not
 * under test in this file) reaches: the maximize/fullscreen command
 * set, WM_STATE bookkeeping, and atom interning
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unfullscreen(client_td *client)
{
    (void) client;
}


void ccmd_client_maximize(client_td *client)
{
    (void) client;
}


void ccmd_client_maximize_horz(client_td *client)
{
    (void) client;
}


void ccmd_client_maximize_vert(client_td *client)
{
    (void) client;
}


void ccmd_client_refill_maximized(client_td *client)
{
    (void) client;
}


void ccmd_client_fullscreen(client_td *client)
{
    (void) client;
}


xcb_window_t ccmd_target_win(client_td *client)
{
    return (client != NULL) ? client->window : XCB_WINDOW_NONE;
}


void ccmd_set_wm_state(client_td *client, uint32_t state,
        xcb_window_t icon_window)
{
    (void) client;
    (void) state;
    (void) icon_window;
}


xcb_atom_t ccmd_intern_atom(xcb_connection_t *connection,
        const char *name)
{
    (void) connection;
    (void) name;
    return (xcb_atom_t) XCB_ATOM_NONE;
}


static int s_delete_property_calls;

xcb_void_cookie_t xcb_delete_property(xcb_connection_t *c,
        xcb_window_t window, xcb_atom_t property)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) window;
    (void) property;

    s_delete_property_calls++;

    cookie.sequence = 0u;
    return cookie;
}


/**
 * @brief Test-controlled stand-in for @a client_last_user_time
 *
 * Zero by default, matching a window manager that has seen no real
 * input yet, so every focus-granting call under test falls back to
 * 'XCB_CURRENT_TIME' unless a test sets a real timestamp.
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_last_user_time;

uint32_t client_last_user_time(void)
{
    return s_last_user_time;
}


/**
 * @brief Recording stand-in for @a cctl_kill_register
 * @note Complexity: @e O(1)
 */
static int s_kill_register_calls;
static pid_t s_kill_register_last_pid;

void cctl_kill_register(pid_t pid)
{
    s_kill_register_calls++;
    s_kill_register_last_pid = pid;
}


/**
 * @brief Test-controlled stand-in for @a xcb_connection_get
 * @note Complexity: @e O(1)
 */
static xcb_connection_t *s_connection;

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection;
}


/**
 * @brief Test-controlled stand-in for @a xcb_ewmh_connection_get
 * @note Complexity: @e O(1)
 */
static xcb_ewmh_connection_t *s_ewmh_connection;

xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return s_ewmh_connection;
}


/* Recording stand-ins for every real libxcb entry point reached: not
 * linked against the real libxcb, since none of these tests run against
 * a live X connection (the same approach test_grab.c already takes for
 * 'xcb_grab_button'/'xcb_ungrab_button') */
static int s_send_event_calls;
static xcb_window_t s_send_event_last_window;
static xcb_client_message_event_t s_send_event_last_event;

xcb_void_cookie_t xcb_send_event(xcb_connection_t *c, uint8_t propagate,
        xcb_window_t destination, uint32_t event_mask,
        const char *event)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) propagate;
    (void) event_mask;

    s_send_event_calls++;
    s_send_event_last_window = destination;
    memcpy(&s_send_event_last_event, event,
            sizeof(s_send_event_last_event));

    cookie.sequence = 0u;
    return cookie;
}


static int s_kill_client_calls;
static xcb_window_t s_kill_client_last_window;

xcb_void_cookie_t xcb_kill_client(xcb_connection_t *c,
        uint32_t resource)
{
    xcb_void_cookie_t cookie;

    (void) c;

    s_kill_client_calls++;
    s_kill_client_last_window = resource;

    cookie.sequence = 0u;
    return cookie;
}


static int s_set_input_focus_calls;
static xcb_window_t s_set_input_focus_last_focus;
static uint8_t s_set_input_focus_last_revert_to;
static uint32_t s_set_input_focus_last_time;

xcb_void_cookie_t xcb_set_input_focus(xcb_connection_t *c,
        uint8_t revert_to, xcb_window_t focus, xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie;

    (void) c;

    s_set_input_focus_calls++;
    s_set_input_focus_last_revert_to = revert_to;
    s_set_input_focus_last_focus = focus;
    s_set_input_focus_last_time = time;

    cookie.sequence = 0u;
    return cookie;
}


static int s_install_colormap_calls;
static xcb_colormap_t s_install_colormap_last;

xcb_void_cookie_t xcb_install_colormap(xcb_connection_t *c,
        xcb_colormap_t cmap)
{
    xcb_void_cookie_t cookie;

    (void) c;

    s_install_colormap_calls++;
    s_install_colormap_last = cmap;

    cookie.sequence = 0u;
    return cookie;
}


static int s_active_window_calls;
static xcb_window_t s_active_window_last;

xcb_void_cookie_t xcb_ewmh_set_active_window(xcb_ewmh_connection_t *ewmh,
        int screen_nbr, xcb_window_t window)
{
    xcb_void_cookie_t cookie;

    (void) ewmh;
    (void) screen_nbr;

    s_active_window_calls++;
    s_active_window_last = window;

    cookie.sequence = 0u;
    return cookie;
}


static int s_window_destroy_calls;
static xcb_window_t s_window_destroy_last;

void xcb_window_destroy(xcb_window_t window)
{
    s_window_destroy_calls++;
    s_window_destroy_last = window;
}


static int s_window_show_calls;
static xcb_window_t s_window_show_last;

void xcb_window_show(xcb_window_t window)
{
    s_window_show_calls++;
    s_window_show_last = window;
}


#define MAX_TEST_CLIENTS (32)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;


static client_td *s_make_client(uint32_t id)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    client->properties.flags = (uint16_t) CLIENT_FLAG_FOCUSABLE;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


/* The focused bit of a client, as a long for TAP_EQ_INT: read back
 * often enough below, and from enough different clients, that spelling
 * out the mask each time buried what the assertion was about */
static long s_focused_bit(const client_td *client)
{
    return (long) (client->properties.flags &
            (uint16_t) CLIENT_FLAG_FOCUSED);
}


static void s_reset(void)
{
    static int s_fake_connection_storage;
    static xcb_ewmh_connection_t s_fake_ewmh_storage;

    s_stage_by_id = NULL;
    s_client_desktop = NULL;
    s_redraw_calls = 0;
    s_focus_order_best_calls = 0;
    s_focus_order_best_result = NULL;
    s_focus_order_to_top_calls = 0;
    s_focus_order_to_top_last = NULL;
    s_focus_apply_calls = 0;
    s_focus_apply_last_client = NULL;
    s_focus_apply_last_stage = NULL;
    s_focus_apply_last_desktop = NULL;
    s_focus_apply_last_raise = false;
    s_focus_target_override = NULL;
    s_theme_resync_calls = 0;
    s_theme_resync_last_is_active = false;
    s_last_user_time = 0u;
    s_kill_register_calls = 0;
    s_kill_register_last_pid = 0;
    s_connection = (xcb_connection_t *) &s_fake_connection_storage;
    memset(&s_fake_ewmh_storage, 0, sizeof(s_fake_ewmh_storage));
    s_fake_ewmh_storage.WM_PROTOCOLS = (xcb_atom_t) 111u;
    s_ewmh_connection = &s_fake_ewmh_storage;
    s_send_event_calls = 0;
    s_send_event_last_window = XCB_WINDOW_NONE;
    memset(&s_send_event_last_event, 0, sizeof(s_send_event_last_event));
    s_kill_client_calls = 0;
    s_kill_client_last_window = XCB_WINDOW_NONE;
    s_set_input_focus_calls = 0;
    s_set_input_focus_last_focus = XCB_WINDOW_NONE;
    s_set_input_focus_last_revert_to = 0u;
    s_set_input_focus_last_time = 0u;
    s_install_colormap_calls = 0;
    s_install_colormap_last = (xcb_colormap_t) XCB_NONE;
    s_active_window_calls = 0;
    s_active_window_last = XCB_WINDOW_NONE;
    s_window_destroy_calls = 0;
    s_window_destroy_last = XCB_WINDOW_NONE;
    s_window_show_calls = 0;
    s_window_show_last = XCB_WINDOW_NONE;
}


static void s_teardown(void)
{
    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* ccmd_client_close: a null client is a silent no-op */
static void s_test_close_null_is_a_no_op(void)
{
    s_reset();

    ccmd_client_close(NULL);

    TAP_EQ_INT(s_send_event_calls, 0,
            "a null client never sends a delete message");
    TAP_EQ_INT(s_window_destroy_calls, 0,
            "nor destroys any window");

    s_teardown();
}


/* ccmd_client_close: a client advertising WM_DELETE_WINDOW, with a
 * live EWMH connection, gets a ClientMessage instead of being
 * destroyed outright */
static void s_test_close_sends_delete_message_when_supported(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);
    client->hints_icccm.protocols.has_delete = true;
    client->hints_icccm.protocols.delete_atom = (xcb_atom_t) 42u;

    ccmd_client_close(client);

    TAP_EQ_INT(s_send_event_calls, 1,
            "a client supporting WM_DELETE_WINDOW is sent exactly one"
            " ClientMessage");
    TAP_EQ_INT((long) s_send_event_last_window, (long) client->window,
            "targeted at the client's own window");
    TAP_EQ_INT((long) s_send_event_last_event.data.data32[0], 42,
            "carrying its cached WM_DELETE_WINDOW atom");
    TAP_EQ_INT(s_window_destroy_calls, 0,
            "and the window is never destroyed directly");

    s_teardown();
}


/* ccmd_client_close: a client with no WM_DELETE_WINDOW support falls
 * back to a direct destroy */
static void s_test_close_destroys_window_when_unsupported(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(2u);
    client->hints_icccm.protocols.has_delete = false;

    ccmd_client_close(client);

    TAP_EQ_INT(s_send_event_calls, 0,
            "a client with no delete protocol receives no message");
    TAP_EQ_INT(s_window_destroy_calls, 1,
            "and is destroyed directly exactly once");
    TAP_EQ_INT((long) s_window_destroy_last, (long) client->window,
            "targeting its own window");

    s_teardown();
}


/* ccmd_client_close: WM_DELETE_WINDOW support with no live EWMH
 * connection also falls back to a direct destroy */
static void s_test_close_destroys_window_with_no_ewmh(void)
{
    client_td *client;

    s_reset();
    s_ewmh_connection = NULL;
    client = s_make_client(3u);
    client->hints_icccm.protocols.has_delete = true;

    ccmd_client_close(client);

    TAP_EQ_INT(s_send_event_calls, 0,
            "no EWMH connection means no ClientMessage is sent"
            " regardless of protocol support");
    TAP_EQ_INT(s_window_destroy_calls, 1,
            "the window is destroyed directly instead");

    s_teardown();
}


/* ccmd_client_kill: a null client is a silent no-op */
static void s_test_kill_null_is_a_no_op(void)
{
    s_reset();

    ccmd_client_kill(NULL);

    TAP_EQ_INT(s_kill_client_calls, 0,
            "a null client's connection is never killed");
    TAP_EQ_INT(s_kill_register_calls, 0,
            "nor is it ever registered for a bounded SIGKILL");

    s_teardown();
}


/* ccmd_client_kill: a local client's connection is killed and its
 * process is registered for a bounded forced kill */
static void s_test_kill_local_client_registers_pid(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(4u);
    client->process.pid_is_local = true;
    client->process.pid = 12345;

    ccmd_client_kill(client);

    TAP_EQ_INT(s_kill_client_calls, 1,
            "the client's X connection is killed exactly once");
    TAP_EQ_INT((long) s_kill_client_last_window, (long) client->window,
            "naming its own window as the resource");
    TAP_EQ_INT(s_kill_register_calls, 1,
            "a local client's pid is registered for a bounded"
            " SIGKILL exactly once");
    TAP_EQ_INT((long) s_kill_register_last_pid, 12345,
            "under its own pid");

    s_teardown();
}


/* ccmd_client_kill: a remote (or unknown-host) client's connection is
 * still killed at the X protocol level, but its pid is never touched */
static void s_test_kill_remote_client_skips_pid_register(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(5u);
    client->process.pid_is_local = false;
    client->process.pid = 99999;

    ccmd_client_kill(client);

    TAP_EQ_INT(s_kill_client_calls, 1,
            "the X connection is killed regardless of host");
    TAP_EQ_INT(s_kill_register_calls, 0,
            "but a non-local pid is never registered for a signal");

    s_teardown();
}


/* ccmd_client_focus: a null client is a silent no-op */
static void s_test_focus_null_is_a_no_op(void)
{
    s_reset();

    ccmd_client_focus(NULL);

    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "a null client never receives real input focus");
    TAP_EQ_INT(s_send_event_calls, 0,
            "nor any ClientMessage");

    s_teardown();
}


/* ccmd_client_focus: a Passive-model client (accepts_input true, no
 * WM_TAKE_FOCUS) receives a direct SetInputFocus and no message */
static void s_test_focus_passive_model_sets_input_focus(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(10u);
    client->hints_icccm.hints.accepts_input = true;
    client->hints_icccm.protocols.has_take_focus = false;

    ccmd_client_focus(client);

    TAP_EQ_INT(s_set_input_focus_calls, 1,
            "a Passive client gets exactly one SetInputFocus call");
    TAP_EQ_INT((long) s_set_input_focus_last_focus, (long) client->window,
            "targeting its own window, unshaded");
    TAP_EQ_INT(s_send_event_calls, 0,
            "and no WM_TAKE_FOCUS message, since it never registered"
            " the protocol");
    TAP_EQ_INT(s_focused_bit(client),
            (long) CLIENT_FLAG_FOCUSED,
            "the client's own focused flag is marked set");
    TAP_EQ_INT(s_active_window_calls, 1,
            "and _NET_ACTIVE_WINDOW is published exactly once");
    TAP_EQ_INT((long) s_active_window_last, (long) client->window,
            "naming this client's window");

    s_teardown();
}


/* ccmd_client_focus: a Locally/Globally Active client (registered
 * WM_TAKE_FOCUS) receives only the message, never a direct
 * SetInputFocus, whatever its own input field says */
static void s_test_focus_take_focus_model_sends_message_only(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(11u);
    client->hints_icccm.hints.accepts_input = false;
    client->hints_icccm.protocols.has_take_focus = true;
    client->hints_icccm.protocols.take_focus_atom = (xcb_atom_t) 77u;

    ccmd_client_focus(client);

    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "a Globally Active client never gets a direct"
            " SetInputFocus");
    TAP_EQ_INT(s_send_event_calls, 1,
            "it receives exactly one WM_TAKE_FOCUS message instead");
    TAP_EQ_INT((long) s_send_event_last_event.data.data32[0], 77,
            "carrying its own cached WM_TAKE_FOCUS atom");

    s_teardown();
}


/* ccmd_client_focus: a No-Input client (neither Passive nor
 * registered WM_TAKE_FOCUS) gets neither a SetInputFocus nor a
 * message, exactly as ICCCM says it never takes real focus at all */
static void s_test_focus_no_input_model_gets_nothing(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(12u);
    client->hints_icccm.hints.accepts_input = false;
    client->hints_icccm.protocols.has_take_focus = false;

    ccmd_client_focus(client);

    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "no SetInputFocus for a No-Input client");
    TAP_EQ_INT(s_send_event_calls, 0,
            "and no WM_TAKE_FOCUS message either");
    TAP_EQ_INT(s_active_window_calls, 1,
            "the active window is still published regardless of input"
            " model");

    s_teardown();
}


/* ccmd_client_focus: a shaded client with a frame is focused through
 * its frame rather than its unmapped content window */
static void s_test_focus_shaded_client_targets_frame(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(13u);
    client->frame = 900u;
    client->properties.flags |= (uint16_t) CLIENT_FLAG_SHADED;
    client->hints_icccm.hints.accepts_input = true;

    ccmd_client_focus(client);

    TAP_EQ_INT((long) s_set_input_focus_last_focus, (long) client->frame,
            "a shaded client's frame stands in for its unmapped"
            " content window");
    TAP_EQ_INT(s_window_show_calls, 0,
            "and its content window is never remapped while shaded");

    s_teardown();
}


/* ccmd_client_focus: an unshaded, decorated, framed client is shown
 * and has its theme layout resynced; a client with neither decoration
 * nor a frame skips the resync entirely */
static void s_test_focus_decorated_framed_client_resyncs_theme(void)
{
    client_td *decorated;
    client_td *plain;

    s_reset();
    decorated = s_make_client(14u);
    decorated->frame = 901u;
    decorated->properties.flags |= (uint16_t) CLIENT_FLAG_DECORATED;

    ccmd_client_focus(decorated);

    TAP_EQ_INT(s_window_show_calls, 1,
            "an unshaded client's content window is shown exactly"
            " once");
    TAP_EQ_INT((long) s_window_show_last, (long) decorated->window,
            "the client's own window");
    TAP_EQ_INT(s_theme_resync_calls, 1,
            "a decorated, framed client resyncs its theme layout"
            " exactly once");
    TAP_OK(s_theme_resync_last_is_active,
            "marked as becoming the active one");

    s_reset();
    plain = s_make_client(15u);

    ccmd_client_focus(plain);

    TAP_EQ_INT(s_theme_resync_calls, 0,
            "an undecorated, frameless client never resyncs a theme"
            " layout on focus");

    s_teardown();
}


/* ccmd_client_focus: urgency is cleared on receiving real focus */
static void s_test_focus_clears_urgency(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(16u);
    client->properties.flags |= (uint16_t) CLIENT_FLAG_URGENT;

    ccmd_client_focus(client);

    TAP_EQ_INT((long) (client->properties.flags &
            (uint16_t) CLIENT_FLAG_URGENT),
            0,
            "a client receiving real focus has its urgency hint"
            " cleared");

    s_teardown();
}


/* ccmd_client_focus: colormap windows with a real, non-default
 * colormap are installed; a XCB_NONE entry is skipped */
static void s_test_focus_installs_colormap_windows(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(17u);
    client->colormap_windows.count = 2u;
    client->colormap_windows.windows[0] = 501u;
    client->colormap_windows.colormap_ids[0] = (xcb_colormap_t) 33u;
    client->colormap_windows.windows[1] = 502u;
    client->colormap_windows.colormap_ids[1] = (xcb_colormap_t) XCB_NONE;

    ccmd_client_focus(client);

    TAP_EQ_INT(s_install_colormap_calls, 1,
            "only the entry with a real colormap id is installed");
    TAP_EQ_INT((long) s_install_colormap_last, 33,
            "installing exactly that colormap");

    s_teardown();
}


/* ccmd_client_focus: the redirect through ccmd_client_focus_target
 * sends every side effect to the redirected client, not the one
 * originally named */
static void s_test_focus_redirects_through_focus_target(void)
{
    client_td *parent;
    client_td *dialog;

    s_reset();
    parent = s_make_client(18u);
    dialog = s_make_client(19u);
    dialog->hints_icccm.hints.accepts_input = true;
    s_focus_target_override = dialog;

    ccmd_client_focus(parent);

    TAP_EQ_INT((long) s_set_input_focus_last_focus, (long) dialog->window,
            "real input focus targets the redirected dialog, not the"
            " originally named parent");
    TAP_EQ_INT((long) s_active_window_last, (long) dialog->window,
            "as does the published active window");
    TAP_EQ_INT(s_focused_bit(parent),
            0,
            "the originally named client's own focused flag is never"
            " marked");

    s_teardown();
}


/* ccmd_client_unfocus: a null client is a silent no-op */
static void s_test_unfocus_null_is_a_no_op(void)
{
    s_reset();

    ccmd_client_unfocus(NULL);

    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "a null client never relinquishes real input focus");

    s_teardown();
}


/* ccmd_client_unfocus: clears the focused mark, relinquishes real
 * input focus to PointerRoot, and clears the published active window */
static void s_test_unfocus_relinquishes_focus(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(20u);
    client->properties.flags |= (uint16_t) CLIENT_FLAG_FOCUSED;

    ccmd_client_unfocus(client);

    TAP_EQ_INT(s_focused_bit(client),
            0,
            "the client's own focused flag is cleared");
    TAP_EQ_INT(s_set_input_focus_calls, 1,
            "real input focus is relinquished exactly once");
    TAP_EQ_INT((long) s_set_input_focus_last_focus,
            (long) XCB_INPUT_FOCUS_POINTER_ROOT,
            "handed to PointerRoot");
    TAP_EQ_INT(s_active_window_calls, 1,
            "the published active window is updated exactly once");
    TAP_EQ_INT((long) s_active_window_last, (long) XCB_NONE,
            "cleared to XCB_NONE");

    s_teardown();
}


/* ccmd_client_unfocus: with no live connection, the client's own
 * bookkeeping still clears, but nothing reaches the (absent) server */
static void s_test_unfocus_with_no_connection_skips_wire_update(void)
{
    client_td *client;

    s_reset();
    s_connection = NULL;
    client = s_make_client(21u);
    client->properties.flags |= (uint16_t) CLIENT_FLAG_FOCUSED;

    ccmd_client_unfocus(client);

    TAP_EQ_INT(s_focused_bit(client),
            0,
            "the focused flag still clears locally");
    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "but no SetInputFocus reaches the absent server");

    s_teardown();
}


/* ccmd_client_make_active: a null client is a silent no-op */
static void s_test_make_active_null_is_a_no_op(void)
{
    s_reset();

    ccmd_client_make_active(NULL);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "a null client never reaches focus_apply");
    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "nor any direct focus call");

    s_teardown();
}


/* ccmd_client_make_active: a non-focusable client is left alone
 * entirely */
static void s_test_make_active_non_focusable_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(22u);
    client->properties.flags &= (uint16_t) ~CLIENT_FLAG_FOCUSABLE;

    ccmd_client_make_active(client);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "a non-focusable client never reaches focus_apply");
    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "nor is it ever given direct input focus");

    s_teardown();
}


/* ccmd_client_make_active: with no resolvable stage or desktop,
 * falls back to a direct ccmd_client_focus rather than focus_apply */
static void s_test_make_active_falls_back_without_stage_or_desktop(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(23u);
    client->hints_icccm.hints.accepts_input = true;
    s_stage_by_id = NULL;
    s_client_desktop = NULL;

    ccmd_client_make_active(client);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "no stage or desktop means focus_apply is never reached");
    TAP_EQ_INT(s_set_input_focus_calls, 1,
            "a direct focus call is made instead");

    s_teardown();
}


/* ccmd_client_make_active: with both a stage and a desktop
 * resolved, delegates to focus_apply, always raising */
static void s_test_make_active_delegates_to_focus_apply(void)
{
    client_td *client;
    stage_td stage;
    desktop_td desktop;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    client = s_make_client(24u);
    s_stage_by_id = &stage;
    s_client_desktop = &desktop;

    ccmd_client_make_active(client);

    TAP_EQ_INT(s_focus_apply_calls, 1,
            "a resolvable stage and desktop delegate to focus_apply"
            " exactly once");
    TAP_OK(s_focus_apply_last_client == client,
            "naming this exact client");
    TAP_OK(s_focus_apply_last_stage == &stage,
            "on its resolved stage");
    TAP_OK(s_focus_apply_last_desktop == &desktop,
            "and resolved desktop");
    TAP_OK(s_focus_apply_last_raise,
            "always asking to raise, regardless of any setting");
    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "no direct focus call is made when delegating");

    s_teardown();
}


/* ccmd_client_focus_fallback: a null client is a silent no-op */
static void s_test_focus_fallback_null_is_a_no_op(void)
{
    s_reset();

    ccmd_client_focus_fallback(NULL);

    TAP_EQ_INT(s_focus_order_best_calls, 0,
            "a null client never triggers a fallback search");

    s_teardown();
}


/* ccmd_client_focus_fallback: a client that is not its desktop's
 * remembered active client is left alone; it never held that focus
 * to hand off in the first place */
static void s_test_focus_fallback_not_active_is_a_no_op(void)
{
    client_td *client;
    stage_td stage;
    desktop_td desktop;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    client = s_make_client(30u);
    desktop.client_active_id = (xcb_window_t) 999u;
    s_stage_by_id = &stage;
    s_client_desktop = &desktop;

    ccmd_client_focus_fallback(client);

    TAP_EQ_INT(s_focus_order_best_calls, 0,
            "a client that is not the desktop's active one never"
            " triggers a fallback search");

    s_teardown();
}


/* ccmd_client_focus_fallback: with no resolvable stage or desktop,
 * is also a no-op */
static void s_test_focus_fallback_without_stage_or_desktop(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(31u);
    s_stage_by_id = NULL;
    s_client_desktop = NULL;

    ccmd_client_focus_fallback(client);

    TAP_EQ_INT(s_focus_order_best_calls, 0,
            "no stage or desktop means the fallback search never"
            " runs");

    s_teardown();
}


/* ccmd_client_focus_fallback: the genuinely active client on its own
 * desktop delegates into client_focus_fallback */
static void s_test_focus_fallback_delegates_when_active(void)
{
    client_td *client;
    stage_td stage;
    desktop_td desktop;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    client = s_make_client(32u);
    desktop.client_active_id = client->id;
    s_stage_by_id = &stage;
    s_client_desktop = &desktop;

    ccmd_client_focus_fallback(client);

    TAP_EQ_INT(s_focus_order_best_calls, 1,
            "the desktop's own genuinely active client triggers"
            " exactly one fallback search");

    s_teardown();
}


/* client_focus_fallback: a null desktop is a silent no-op */
static void s_test_client_focus_fallback_null_desktop_is_a_no_op(void)
{
    s_reset();

    client_focus_fallback(NULL, NULL, NULL);

    TAP_EQ_INT(s_focus_order_best_calls, 0,
            "a null desktop never triggers a fallback search");

    s_teardown();
}


/* client_focus_fallback: a winning candidate is delegated to
 * focus_apply, always raised, exactly the way ccmd_client_make_active
 * already delegates a client being made active */
static void s_test_client_focus_fallback_winner_is_focused(void)
{
    desktop_td desktop;
    stage_td stage;
    client_td *exclude;
    client_td *winner;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    exclude = s_make_client(40u);
    winner = s_make_client(41u);
    winner->hints_icccm.hints.accepts_input = true;
    s_focus_order_best_result = winner;

    client_focus_fallback(&desktop, &stage, exclude);

    TAP_EQ_INT(s_focus_apply_calls, 1,
            "the winning candidate delegates to focus_apply exactly"
            " once");
    TAP_OK(s_focus_apply_last_client == winner,
            "naming the winner itself");
    TAP_OK(s_focus_apply_last_stage == &stage,
            "on the stage it was called with");
    TAP_OK(s_focus_apply_last_desktop == &desktop,
            "and the desktop it was called with");
    TAP_OK(s_focus_apply_last_raise,
            "always asking to raise, regardless of any setting");
    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "no direct focus call is made when delegating");

    s_teardown();
}


/* client_focus_fallback: a group-mate of 'exclude' is tried first,
 * ahead of the plain MRU search, when exclude names a real group
 * leader */
static void s_test_client_focus_fallback_tries_group_first(void)
{
    desktop_td desktop;
    stage_td stage;
    client_td *exclude;
    config_td config;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    memset(&config, 0, sizeof(config));
    config.base.windows.focus.use_group_fallback = true;
    s_config = &config;
    exclude = s_make_client(42u);
    exclude->hints_icccm.hints.client_leader = (xcb_window_t) 500u;
    s_focus_order_best_result = NULL;

    client_focus_fallback(&desktop, &stage, exclude);

    TAP_EQ_INT(s_focus_order_best_calls, 2,
            "with group-fallback on, excluding a client with a real"
            " group leader runs both the group-restricted pass and the"
            " plain MRU pass when the first finds nothing");

    s_config = NULL;
    s_teardown();
}


/* client_focus_fallback on a desktop that is not the one shown: no
 * focus is given or relinquished, since no window there can take it
 * and relinquishing would take the keyboard from the desktop that is
 * shown; only the record of the active client is kept */
static void s_test_client_focus_fallback_hidden_desktop(void)
{
    desktop_td desktop;
    stage_td stage;
    client_td *exclude;
    client_td *candidate;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    desktop.id = 0u;
    stage.desktop_cur = 1u;
    exclude = s_make_client(45u);
    candidate = s_make_client(46u);

    s_focus_order_best_result = NULL;
    desktop.client_active_id = exclude->id;
    client_focus_fallback(&desktop, &stage, exclude);
    TAP_OK(s_set_input_focus_calls == 0 && s_focus_apply_calls == 0 &&
            desktop.client_active_id == 0u,
            "hidden desktop, no candidate: nothing is focused and the"
            " focus is not relinquished to PointerRoot");

    s_focus_order_best_result = candidate;
    desktop.client_active_id = exclude->id;
    client_focus_fallback(&desktop, &stage, exclude);
    TAP_OK(s_set_input_focus_calls == 0 && s_focus_apply_calls == 0 &&
            desktop.client_active_id == candidate->id,
            "hidden desktop with a candidate: it is only recorded as"
            " the desktop's active client, never given real focus");

    s_teardown();
}


/* client_focus_fallback: with 'windows.focus.group-fallback' off, the
 * default, a real group leader changes nothing and only the plain MRU
 * pass runs */
static void s_test_client_focus_fallback_group_off_is_mru_only(void)
{
    desktop_td desktop;
    stage_td stage;
    client_td *exclude;
    config_td config;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    memset(&config, 0, sizeof(config));
    s_config = &config;
    exclude = s_make_client(44u);
    exclude->hints_icccm.hints.client_leader = (xcb_window_t) 500u;
    s_focus_order_best_result = NULL;

    client_focus_fallback(&desktop, &stage, exclude);

    TAP_EQ_INT(s_focus_order_best_calls, 1,
            "with group-fallback off, a client with a real group"
            " leader still falls back through the plain MRU pass only");

    s_config = NULL;
    s_teardown();
}


/* client_focus_fallback: excluding a client with no group leader at
 * all skips straight to the plain MRU pass, running the search only
 * once */
static void
    s_test_client_focus_fallback_skips_group_pass_without_leader(void)
{
    desktop_td desktop;
    stage_td stage;
    client_td *exclude;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    exclude = s_make_client(43u);
    s_focus_order_best_result = NULL;

    client_focus_fallback(&desktop, &stage, exclude);

    TAP_EQ_INT(s_focus_order_best_calls, 1,
            "no group leader on the excluded client means only the"
            " plain MRU pass ever runs");

    s_teardown();
}


/* client_focus_fallback: no candidate qualifies but an excluded
 * client was named, so that client itself is explicitly unfocused
 * rather than left wearing a stale focused mark */
static void
    s_test_client_focus_fallback_unfocuses_exclude_when_none_found(void)
{
    desktop_td desktop;
    stage_td stage;
    client_td *exclude;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    exclude = s_make_client(44u);
    exclude->properties.flags |= (uint16_t) CLIENT_FLAG_FOCUSED;
    s_focus_order_best_result = NULL;

    client_focus_fallback(&desktop, &stage, exclude);

    TAP_EQ_INT(s_focused_bit(exclude),
            0,
            "the excluded client's own focused flag is cleared by the"
            " explicit unfocus");
    TAP_EQ_INT(s_active_window_calls, 1,
            "and the published active window is cleared along with it");

    s_teardown();
}


/* client_focus_fallback: no candidate qualifies and no exclude was
 * even named, so real input focus is simply relinquished to
 * PointerRoot */
static void
    s_test_client_focus_fallback_relinquishes_with_no_exclude(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    s_focus_order_best_result = NULL;

    client_focus_fallback(&desktop, &stage, NULL);

    TAP_EQ_INT(s_set_input_focus_calls, 1,
            "input focus is relinquished exactly once");
    TAP_EQ_INT((long) s_set_input_focus_last_focus,
            (long) XCB_INPUT_FOCUS_POINTER_ROOT,
            "handed to PointerRoot");
    TAP_EQ_INT((long) desktop.client_active_id, 0,
            "the desktop's active client is cleared to none");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(87);

    s_test_close_null_is_a_no_op();
    s_test_close_sends_delete_message_when_supported();
    s_test_close_destroys_window_when_unsupported();
    s_test_close_destroys_window_with_no_ewmh();
    s_test_kill_null_is_a_no_op();
    s_test_kill_local_client_registers_pid();
    s_test_kill_remote_client_skips_pid_register();
    s_test_focus_null_is_a_no_op();
    s_test_focus_passive_model_sets_input_focus();
    s_test_focus_take_focus_model_sends_message_only();
    s_test_focus_no_input_model_gets_nothing();
    s_test_focus_shaded_client_targets_frame();
    s_test_focus_decorated_framed_client_resyncs_theme();
    s_test_focus_clears_urgency();
    s_test_focus_installs_colormap_windows();
    s_test_focus_redirects_through_focus_target();
    s_test_unfocus_null_is_a_no_op();
    s_test_unfocus_relinquishes_focus();
    s_test_unfocus_with_no_connection_skips_wire_update();
    s_test_make_active_null_is_a_no_op();
    s_test_make_active_non_focusable_is_a_no_op();
    s_test_make_active_falls_back_without_stage_or_desktop();
    s_test_make_active_delegates_to_focus_apply();
    s_test_focus_fallback_null_is_a_no_op();
    s_test_focus_fallback_not_active_is_a_no_op();
    s_test_focus_fallback_without_stage_or_desktop();
    s_test_focus_fallback_delegates_when_active();
    s_test_client_focus_fallback_null_desktop_is_a_no_op();
    s_test_client_focus_fallback_winner_is_focused();
    s_test_client_focus_fallback_tries_group_first();
    s_test_client_focus_fallback_group_off_is_mru_only();
    s_test_client_focus_fallback_hidden_desktop();
    s_test_client_focus_fallback_skips_group_pass_without_leader();
    s_test_client_focus_fallback_unfocuses_exclude_when_none_found();
    s_test_client_focus_fallback_relinquishes_with_no_exclude();

    return TAP_DONE();
}
