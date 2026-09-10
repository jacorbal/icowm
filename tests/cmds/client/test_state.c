/**
 * @file tests/cmds/client/test_state.c
 *
 * @brief Test battery for client shade, fullscreen, and decoration
 *        toggle guard logic
 *
 * Exercises 'ccmd_client_toggle_shade', 'ccmd_client_fullscreen',
 * 'ccmd_client_unfullscreen', 'ccmd_client_toggle_fullscreen', and
 * 'ccmd_client_toggle_decorate' (cmds/client/state.c) linked against
 * nothing but this file's own stand-ins for every XCB, layer, and
 * desktop-lookup dependency: none of these functions' real work
 * (window creation, reparenting, or the X server round trips
 * 'ccmd_client_shade' and 's_client_enable_decoration' make) can run
 * without a live X connection, so every one of them is a link-only or
 * recording stand-in here.  What this file actually exercises is the
 * decision logic each public entry point makes before and after that
 * XCB work: which guard clauses refuse a request outright, which
 * state bit ends up set or cleared, and which follow-up call (raise,
 * focus, layer enforcement, allowed-actions republish) happens on the
 * way out.
 *
 * 'ccmd_client_shade' and 'ccmd_client_unshade' themselves are
 * exercised only through 'ccmd_client_toggle_shade', never directly:
 * both call 'xcb_get_geometry'/'_reply' or 'ccmd_client_resolve_
 * workarea' partway through, so this file's toggle-level guard
 * checks (decorated, already-shaded, fullscreen) are what stays
 * testable without those calls' own, X-shaped return values driving
 * the rest of either function's geometry math.
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
#include <cmds/client/state.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <monitor.h>


/**
 * @brief Recording stand-in for @a ccmd_client_apply_geometry
 *
 * Counts calls and remembers the width/height of the last one, so
 * a test can confirm 'ccmd_client_unfullscreen' folds a still-
 * maximized axis into 'layout.geometry.cur' before this, its one
 * real geometry-applying call, rather than applying the restored
 * (pre-maximize) size here and only fixing it up afterward
 *
 * @note Complexity: @e O(1)
 */
static int s_apply_geometry_calls;
static uint32_t s_last_apply_w;
static uint32_t s_last_apply_h;

void ccmd_client_apply_geometry(client_td *client,
        xcb_window_t target, uint16_t mask,
        int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    (void) client;
    (void) target;
    (void) mask;
    (void) x;
    (void) y;
    (void) border_width;
    s_apply_geometry_calls++;
    s_last_apply_w = w;
    s_last_apply_h = h;
}


/**
 * @brief Recording stand-in for @a ccmd_client_focus
 * @note Complexity: @e O(1)
 */
static int s_focus_calls;

void ccmd_client_focus(client_td *client)
{
    (void) client;
    s_focus_calls++;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_grab_buttons
 * @note Complexity: @e O(1)
 */
void ccmd_client_grab_buttons(client_td *client)
{
    (void) client;
}


/**
 * @brief Test-controlled stand-in for @a ccmd_client_monitor
 *
 * Answers @c false unconditionally, the same as a client whose
 * surface cannot be found: 'ccmd_client_fullscreen' falls through to
 * 'ccmd_screen_dim' instead, which this file also controls.
 *
 * @note Complexity: @e O(1)
 */
bool ccmd_client_monitor(client_td *client, surface_td **out_surface,
        monitor_td *out_monitor)
{
    (void) client;
    (void) out_monitor;

    if (out_surface != NULL) {
        *out_surface = NULL;
    }

    return false;
}


/**
 * @brief Recording stand-in for @a ccmd_client_raise
 * @note Complexity: @e O(1)
 */
static int s_raise_calls;

void ccmd_client_raise(client_td *client)
{
    (void) client;
    s_raise_calls++;
}


/**
 * @brief Configurable stand-in for
 *        @a ccmd_client_refill_maximized_geometry
 *
 * Defaults to @c false, untouched, matching what the real function
 * does for a client maximized on neither axis, which is every client
 * every other test here ever sets up.  A test exercising the one
 * still-maximized case sets @c s_refill_geometry_should_touch first,
 * so this writes deterministic sentinel dimensions to
 * @c layout.geometry.cur the same way the real function would write
 * its own freshly resolved ones, and returns @c true.
 *
 * @note Complexity: @e O(1)
 */
#define S_REFILL_SENTINEL_W (777u)
#define S_REFILL_SENTINEL_H (888u)
static bool s_refill_geometry_should_touch;

bool ccmd_client_refill_maximized_geometry(client_td *client)
{
    if (!s_refill_geometry_should_touch) {
        return false;
    }
    client->layout.geometry.cur.dim.w = S_REFILL_SENTINEL_W;
    client->layout.geometry.cur.dim.h = S_REFILL_SENTINEL_H;
    return true;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_refill_maximized
 *
 * Reached only when a client leaving fullscreen is also maximized,
 * which no test here sets up.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_refill_maximized(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_resolve_workarea
 *
 * Reached only by 'ccmd_client_shade'/'_unshade' (through
 * 'ccmd_client_toggle_shade', never exercised past its own guard
 * clauses here) and by 's_ccmd_decorate_remaximize' (through
 * 'ccmd_client_toggle_decorate', reached only for a client already
 * maximized, which no test here sets up).
 *
 * @note Complexity: @e O(1)
 */
bool ccmd_client_resolve_workarea(client_td *client,
        int32_t *out_x, int32_t *out_y,
        uint16_t *out_w, uint16_t *out_h)
{
    (void) client;

    if (out_x != NULL) {
        *out_x = 0;
    }
    if (out_y != NULL) {
        *out_y = 0;
    }
    if (out_w != NULL) {
        *out_w = 0u;
    }
    if (out_h != NULL) {
        *out_h = 0u;
    }

    return false;
}


/**
 * @brief Recording stand-in for @a ccmd_client_restore
 *
 * Reached whenever an iconified client is asked to shade or go
 * fullscreen; records that it ran, and clears the iconified state, the
 * one part of the real function's effect the caller's own guard
 * clauses depend on afterward.
 *
 * @note Complexity: @e O(1)
 */
static int s_restore_calls;

void ccmd_client_restore(client_td *client)
{
    if (client != NULL) {
        client->properties.state &=
            (uint16_t) ~CLIENT_STATE_ICONIFIED;
    }
    s_restore_calls++;
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
 * @brief Link-only stand-in for @a ccmd_client_ungrab_buttons
 *
 * Reached only by 's_client_enable_decoration', unreachable without a
 * live X connection.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_ungrab_buttons(client_td *client)
{
    (void) client;
}


/**
 * @brief Recording stand-in for @a ccmd_client_update_allowed_actions
 * @note Complexity: @e O(1)
 */
static int s_update_allowed_actions_calls;

void ccmd_client_update_allowed_actions(client_td *client)
{
    (void) client;
    s_update_allowed_actions_calls++;
}


/**
 * @brief Recording stand-in for @a ccmd_desktop_enforce_layers
 * @note Complexity: @e O(1)
 */
static int s_enforce_layers_calls;

void ccmd_desktop_enforce_layers(desktop_td *desktop)
{
    (void) desktop;
    s_enforce_layers_calls++;
}


/**
 * @brief Link-only stand-in for @a ccmd_publish_frame_extents
 * @note Complexity: @e O(1)
 */
void ccmd_publish_frame_extents(client_td *client,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom)
{
    (void) client;
    (void) left;
    (void) right;
    (void) top;
    (void) bottom;
}


/**
 * @brief Link-only stand-in for @a ccmd_screen_dim
 *
 * Answers a fixed, nonzero screen size: 'ccmd_client_fullscreen'
 * needs one or the other of this and @a ccmd_client_monitor to
 * succeed to ever reach its own state-setting tail, and this file's
 * monitor stand-in always fails on purpose (see its own comment).
 *
 * @note Complexity: @e O(1)
 */
bool ccmd_screen_dim(const client_td *client,
        uint16_t *out_w, uint16_t *out_h)
{
    (void) client;

    if (out_w != NULL) {
        *out_w = 1920u;
    }
    if (out_h != NULL) {
        *out_h = 1080u;
    }

    return true;
}


/**
 * @brief Link-only stand-in for @a client_decoration_layout_sync
 *
 * Reached only for a client with a nonzero @c frame, which no test
 * here sets up: every client tested is undecorated ('frame' left at
 * @c 0 by @c calloc), so 'ccmd_client_toggle_decorate' always takes
 * its "restore decoration" branch through
 * 's_client_enable_decoration' instead, itself unreachable without a
 * live X connection.
 *
 * @note Complexity: @e O(1)
 */
void client_decoration_layout_sync(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a client_gravity_adjust_pos
 *
 * Reached only inside 's_ccmd_decorate_remove'/'_restore', both
 * themselves unreachable without a live X connection creating or
 * destroying real frame windows.
 *
 * @note Complexity: @e O(1)
 */
void client_gravity_adjust_pos(int32_t *out_x, int32_t *out_y,
        uint32_t old_w, uint32_t old_h,
        uint32_t new_w, uint32_t new_h, uint16_t gravity)
{
    (void) out_x;
    (void) out_y;
    (void) old_w;
    (void) old_h;
    (void) new_w;
    (void) new_h;
    (void) gravity;
}


/**
 * @brief Recording stand-in for @a client_send_synthetic_configure_notify
 * @note Complexity: @e O(1)
 */
static int s_synthetic_notify_calls;

void client_send_synthetic_configure_notify(
        xcb_connection_t *connection, const client_td *client)
{
    (void) connection;
    (void) client;
    s_synthetic_notify_calls++;
}


/**
 * @brief Link-only stand-in for @a desktop_action_client_send_front
 * @note Complexity: @e O(1)
 */
int desktop_action_client_send_front(desktop_td *desktop,
        client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}


/**
 * @brief Link-only stand-in for @a mouse_hover_poll_clear
 * @note Complexity: @e O(1)
 */
void mouse_hover_poll_clear(xcb_window_t window)
{
    (void) window;
}


/**
 * @brief Recording stand-in for @a systray_restack
 * @note Complexity: @e O(1)
 */
static int s_systray_restack_calls;

void systray_restack(void)
{
    s_systray_restack_calls++;
}


/**
 * @brief Link-only stand-in for @a ccmd_target_win
 *
 * Answers the client's own window unconditionally, the same as a
 * client with no decoration frame; every path under test leaves
 * @c frame at @c 0 (see this file's own top comment).
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client)
{
    return client->window;
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
 * @brief Link-only stand-in for @a wm_get_client_desktop
 *
 * Answers a test-controlled desktop, or @c NULL by default: several
 * of the paths under test only take their "update the owning
 * desktop" branch when this resolves to something real.
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


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Reached wherever the function under test would otherwise need a
 * live connection; @c NULL is fine, since every real XCB call this
 * file's own stand-ins wrap never dereferences it.
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_window_destroy
 *
 * Reached only by 's_ccmd_decorate_remove', unreachable without a
 * live X connection creating the frame it would destroy.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_destroy(xcb_window_t window)
{
    (void) window;
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
 * @brief Link-only stand-in for @a xcb_window_reparent
 *
 * Reached only by 's_ccmd_decorate_remove', unreachable without a
 * live X connection.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_reparent(xcb_window_t window, xcb_window_t parent,
        int16_t x, int16_t y)
{
    (void) window;
    (void) parent;
    (void) x;
    (void) y;
}


/**
 * @brief Link-only stand-in for @a xcb_window_save_set
 *
 * Reached only by 's_ccmd_decorate_remove', unreachable without a
 * live X connection.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_save_set(xcb_window_t window, bool add)
{
    (void) window;
    (void) add;
}


/**
 * @brief Link-only stand-in for @a xcb_window_show
 * @note Complexity: @e O(1)
 */
void xcb_window_show(xcb_window_t window)
{
    (void) window;
}


/** Every client this file calloc's, freed in one place by @a
 *  s_teardown rather than at each test's own end */
#define MAX_TEST_CLIENTS (16)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;


static client_td *s_make_client(uint32_t id)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


static void s_reset(void)
{
    s_focus_calls = 0;
    s_raise_calls = 0;
    s_restore_calls = 0;
    s_sync_states_calls = 0;
    s_update_allowed_actions_calls = 0;
    s_enforce_layers_calls = 0;
    s_systray_restack_calls = 0;
    s_owner_desktop = NULL;
    s_redraw_calls = 0;
    s_apply_geometry_calls = 0;
    s_last_apply_w = 0u;
    s_last_apply_h = 0u;
    s_synthetic_notify_calls = 0;
    s_refill_geometry_should_touch = false;
}


static void s_teardown(void)
{
    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* A null client is a silent no-op on every function under test: none
 * of the stand-ins above run, and nothing crashes */
static void s_test_null_client_is_a_no_op(void)
{
    s_reset();

    ccmd_client_toggle_shade(NULL);
    ccmd_client_fullscreen(NULL);
    ccmd_client_unfullscreen(NULL);
    ccmd_client_toggle_fullscreen(NULL);
    ccmd_client_toggle_decorate(NULL);

    TAP_EQ_INT(s_sync_states_calls, 0,
            "a null client never reaches the EWMH state sync");
    TAP_EQ_INT(s_redraw_calls, 0,
            "and never reaches the redraw request either");

    s_teardown();
}


/* An undecorated client's toggle-shade request is refused outright:
 * shading only ever applies to a decorated client */
static void s_test_toggle_shade_undecorated_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);

    ccmd_client_toggle_shade(client);

    TAP_OK(!client_is_shaded(client),
            "an undecorated client's shade request is refused");
    TAP_EQ_INT(s_sync_states_calls, 0,
            "no state sync fires for a refused shade request");

    s_teardown();
}


/* A fullscreen client's toggle-shade request is refused outright,
 * even when it is otherwise decorated */
static void s_test_toggle_shade_fullscreen_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(2u);
    client_decorate(client);
    client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;

    ccmd_client_toggle_shade(client);

    TAP_OK(!client_is_shaded(client),
            "a fullscreen client's shade request is refused");

    s_teardown();
}


/* Toggling shade on an already-shaded, decorated client takes the
 * unshade branch, clearing the shaded flag rather than leaving it set */
static void s_test_toggle_shade_already_shaded_unshades(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(3u);
    client_decorate(client);
    client_shade(client);

    ccmd_client_toggle_shade(client);

    TAP_OK(!client_is_shaded(client),
            "toggling an already-shaded client unshades it");
    TAP_EQ_INT(s_sync_states_calls, 1,
            "state is synced once for the one transition made");

    s_teardown();
}


/* A modal client's fullscreen request is refused outright: it must
 * stay in view alongside the window it answers for */
static void s_test_fullscreen_modal_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(10u);
    client->properties.flags |= CLIENT_FLAG_MODAL;

    ccmd_client_fullscreen(client);

    TAP_OK(!client_is_fullscreen(client),
            "a modal client's fullscreen request is refused");
    TAP_EQ_INT(s_sync_states_calls, 0,
            "no state sync fires for a refused fullscreen request");

    s_teardown();
}


/* An iconified client asked to go fullscreen is restored first, then
 * genuinely enters fullscreen */
static void s_test_fullscreen_restores_iconified_first(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(11u);
    client->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;

    ccmd_client_fullscreen(client);

    TAP_EQ_INT(s_restore_calls, 1,
            "the iconified client is restored before going"
            " fullscreen");
    TAP_OK(client_is_fullscreen(client) != 0,
            "and it genuinely ends up fullscreen afterward");

    s_teardown();
}


/* An ordinary, non-modal client entering fullscreen ends up with the
 * fullscreen state bit set, keeps focus on its own desktop, and
 * enforces that desktop's layer ordering right away */
static void s_test_fullscreen_sets_state_and_keeps_focus(void)
{
    client_td *client;
    desktop_td owner;

    s_reset();
    client = s_make_client(12u);
    memset(&owner, 0, sizeof(owner));
    s_owner_desktop = &owner;

    ccmd_client_fullscreen(client);

    TAP_OK(client_is_fullscreen(client) != 0,
            "the client ends up marked fullscreen");
    TAP_EQ_INT((long) owner.client_active_id, (long) client->id,
            "the owning desktop's active client becomes this one");
    TAP_OK(owner.is_focus_dirty,
            "the owning desktop is marked to refresh focus decoration");
    TAP_EQ_INT(s_focus_calls, 1, "the client is given real input focus");
    TAP_EQ_INT(s_enforce_layers_calls, 1,
            "layer ordering is enforced once right away");
    TAP_EQ_INT(s_systray_restack_calls, 1,
            "the systray is let reconsider its own stacking");

    s_teardown();
}


/* Leaving fullscreen clears the fullscreen state bit and, with an
 * owning desktop present, re-enforces its layer ordering on the way
 * out too */
static void s_test_unfullscreen_clears_state(void)
{
    client_td *client;
    desktop_td owner;

    s_reset();
    client = s_make_client(13u);
    client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    memset(&owner, 0, sizeof(owner));
    s_owner_desktop = &owner;

    ccmd_client_unfullscreen(client);

    TAP_OK(!client_is_fullscreen(client),
            "leaving fullscreen clears the fullscreen state bit");
    TAP_EQ_INT(s_enforce_layers_calls, 1,
            "layer ordering is re-enforced once on the way out");
    TAP_EQ_INT(s_systray_restack_calls, 1,
            "the systray is let reconsider its own stacking again");

    s_teardown();
}


/* A client still maximized (on one axis, the common partial case)
 * when it leaves fullscreen has that axis folded into
 * 'layout.geometry.cur' before the one real geometry-applying call
 * this function makes, rather than applying the restored
 * (pre-maximize) geometry for real first and correcting it with
 * a second real apply and a second synthetic notify moments later */
static void s_test_unfullscreen_maximized_applies_geometry_once(void)
{
    client_td *client;
    desktop_td owner;

    s_reset();
    client = s_make_client(15u);
    client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN |
        (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    memset(&owner, 0, sizeof(owner));
    s_owner_desktop = &owner;
    s_refill_geometry_should_touch = true;

    ccmd_client_unfullscreen(client);

    TAP_OK(!client_is_fullscreen(client),
            "leaving fullscreen clears the fullscreen state bit");
    TAP_OK(client_is_maximized_horz(client),
            "the maximized-horizontal state bit survives leaving" \
            " fullscreen, unaffected by any of this");
    TAP_EQ_INT(s_synthetic_notify_calls, 1,
            "the client is told its geometry exactly once, not once" \
            " for the restored size and again for the corrected one");
    TAP_EQ_INT((long) s_last_apply_w, (long) S_REFILL_SENTINEL_W,
            "the one real geometry apply already carries the" \
            " refilled maximized width, not the pre-maximize one" \
            " 'client_geometry_restore' set moments earlier");
    TAP_EQ_INT((long) s_last_apply_h, (long) S_REFILL_SENTINEL_H,
            "same for height");

    s_teardown();
}


/* Toggling fullscreen on a plain client enters it, and toggling again
 * on the very same client leaves it */
static void s_test_toggle_fullscreen_flips_both_ways(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(14u);

    ccmd_client_toggle_fullscreen(client);
    TAP_OK(client_is_fullscreen(client) != 0,
            "toggling a plain client enters fullscreen");

    ccmd_client_toggle_fullscreen(client);
    TAP_OK(!client_is_fullscreen(client),
            "toggling it again leaves fullscreen");

    s_teardown();
}


/* A fullscreen client's decoration toggle is refused outright: only
 * 'ccmd_client_unfullscreen' is allowed to change its framing */
static void s_test_toggle_decorate_fullscreen_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(20u);
    client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;

    ccmd_client_toggle_decorate(client);

    TAP_EQ_INT(s_update_allowed_actions_calls, 0,
            "a fullscreen client's decoration toggle never reaches"
            " the allowed-actions republish");

    s_teardown();
}


/* A locked client's decoration toggle is refused outright */
static void s_test_toggle_decorate_locked_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(21u);
    client->properties.flags |= CLIENT_FLAG_LOCKED;

    ccmd_client_toggle_decorate(client);

    TAP_EQ_INT(s_redraw_calls, 0,
            "a locked client's decoration toggle never reaches the"
            " redraw request");

    s_teardown();
}


/* Toggling decoration on an ordinary, undecorated client republishes
 * its allowed actions, raises and refocuses it on its own desktop,
 * and requests a redraw */
static void s_test_toggle_decorate_plain_client_runs_side_effects(void)
{
    client_td *client;
    desktop_td owner;

    s_reset();
    client = s_make_client(22u);
    memset(&owner, 0, sizeof(owner));
    s_owner_desktop = &owner;

    ccmd_client_toggle_decorate(client);

    TAP_EQ_INT((long) owner.client_active_id, (long) client->id,
            "the owning desktop's active client becomes this one");
    TAP_EQ_INT(s_raise_calls, 1, "the client is raised once");
    TAP_EQ_INT(s_focus_calls, 1, "the client is refocused once");
    TAP_EQ_INT(s_update_allowed_actions_calls, 1,
            "its allowed actions are republished exactly once");
    TAP_EQ_INT(s_redraw_calls, 1, "a redraw is requested once");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(34);

    s_test_null_client_is_a_no_op();
    s_test_toggle_shade_undecorated_is_a_no_op();
    s_test_toggle_shade_fullscreen_is_a_no_op();
    s_test_toggle_shade_already_shaded_unshades();
    s_test_fullscreen_modal_is_a_no_op();
    s_test_fullscreen_restores_iconified_first();
    s_test_fullscreen_sets_state_and_keeps_focus();
    s_test_unfullscreen_clears_state();
    s_test_unfullscreen_maximized_applies_geometry_once();
    s_test_toggle_fullscreen_flips_both_ways();
    s_test_toggle_decorate_fullscreen_is_a_no_op();
    s_test_toggle_decorate_locked_is_a_no_op();
    s_test_toggle_decorate_plain_client_runs_side_effects();

    return TAP_DONE();
}
