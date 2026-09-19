
/**
 * @file tests/cmds/client/test_flags.c
 *
 * @brief Test battery for a client's pin, urgency, opacity, border,
 *        and allowed-actions flag toggles
 *
 * Exercises 'ccmd_client_pin', 'ccmd_client_unpin',
 * 'ccmd_client_toggle_pin', 'ccmd_client_set_opacity_active',
 * 'ccmd_client_set_opacity_inactive',
 * 'ccmd_client_set_border_override', 'ccmd_client_urge', and
 * 'ccmd_client_unurge' (cmds/client/flags.c) linked against the real
 * flag mutators ('utils/safe/safeflg.c'), so the exact bit twiddling
 * behind 'client_pin'/'client_unpin'/ 'client_urge'/'client_unurge'
 * runs for real.  Everything else the file under test calls
 * (transient-family lookups, EWMH state sync, focus fallback, IPC
 * broadcasting, and the X server itself) is a link-only or recording
 * stand-in, so what actually gets exercised here is this file's own
 * flag-toggling and cascading logic, not the rest of the window
 * manager.
 *
 * 'ccmd_client_update_allowed_actions' is deliberately left untested:
 * its only observable effect is an 'xcb_change_property' call this file
 * would have to capture and decode by hand, which would test this
 * stand-in's own bookkeeping far more than the function itself.
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
#include <xcb/xcb_ewmh.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <client.h>
#include <cmds/client/flags.h>
#include <cmds/client/transient.h>
#include <desktop.h>
#include <harness/tap.h>
#include <ipc.h>


/**
 * @brief Link-only stand-in for @a ccmd_client_focus_fallback
 *
 * Reached only through the "moved off the current desktop" branch of
 * 'ccmd_client_unpin', which none of these tests take: every desktop
 * this file builds is either not the stage's current one, or has
 * no matching stage at all.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_focus_fallback(client_td *client)
{
    (void) client;
}


/**
 * @brief Recording stand-in for @a ccmd_client_sync_states
 *
 * Records how many times this ran instead of touching any EWMH
 * property: what is under test here is which clients get pinned,
 * unpinned, or marked urgent, and how the cascade reaches them, not
 * the wire format of '_NET_WM_STATE' itself (already covered on its
 * own elsewhere).
 *
 * @note Complexity: @e O(1)
 */
static int s_sync_states_calls;

void ccmd_client_sync_states(client_td *client)
{
    (void) client;
    s_sync_states_calls++;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_transient_top_parent
 *
 * Every test client here has no transient family of its own, so its
 * top parent is always itself.
 *
 * @note Complexity: @e O(1)
 */
client_td *ccmd_client_transient_top_parent(client_td *client)
{
    return client;
}


/**
 * @brief Recording stand-in for @a ccmd_client_family_apply
 *
 * Calls @p fn once for @p top and once per entry in a small,
 * test-owned sibling table, the same shape a real transient family's
 * membership walk would present to its own visitor callback, without
 * this file needing 'cmds/client/transient.c''s own cdlist-walking
 * machinery just to prove one visitor runs over every member.
 *
 * @note Complexity: @e O(n), where @e n is the number of registered
 *       siblings
 */
#define MAX_TEST_SIBLINGS (4)

static client_td *s_siblings[MAX_TEST_SIBLINGS];
static int s_siblings_used;

void ccmd_client_family_apply(client_td *top, ccmd_family_fn fn,
        void *ctx)
{
    fn(top, ctx);
    for (int i = 0; i < s_siblings_used; ++i) {
        fn(s_siblings[i], ctx);
    }
}


/**
 * @brief Link-only stand-in for @a ccmd_client_unmap_decorated
 *
 * Reached only through the "moved off the current desktop" branch of
 * 'ccmd_client_unpin', which none of these tests take.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unmap_decorated(client_td *client, xcb_window_t target)
{
    (void) client;
    (void) target;
}


static int s_publish_calls;
static uint32_t s_publish_last_desktop;


/**
 * @brief Recording stand-in for @a ccmd_publish_wm_desktop
 * @note Complexity: @e O(1)
 */
void ccmd_publish_wm_desktop(client_td *client, uint32_t desktop_id)
{
    (void) client;
    s_publish_calls++;
    s_publish_last_desktop = desktop_id;
}


/**
 * @brief Link-only stand-in for @a ccmd_target_win
 *
 * Reached only through the "moved off the current desktop" branch of
 * 'ccmd_client_unpin', which none of these tests take.
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client)
{
    (void) client;
    return XCB_WINDOW_NONE;
}


/**
 * @brief Recording stand-in for @a desktop_action_recompute_urgent
 * @note Complexity: @e O(1)
 */
static int s_recompute_urgent_calls;

void desktop_action_recompute_urgent(desktop_td *desktop)
{
    (void) desktop;
    s_recompute_urgent_calls++;
}


/**
 * @brief Recording stand-in for @a ipc_broadcast_event
 *
 * Frees @p fields itself, exactly as the real IPC layer eventually
 * would once it finished serializing them, so a test enabling this
 * path never leaks the 'cJSON' object the function under test built.
 *
 * @note Complexity: @e O(1)
 */
static int s_ipc_broadcast_calls;
static uint32_t s_ipc_broadcast_last_type;

void ipc_broadcast_event(uint32_t type, cJSON *fields)
{
    s_ipc_broadcast_calls++;
    s_ipc_broadcast_last_type = type;
    cJSON_Delete(fields);
}


/**
 * @brief Link-only stand-in for @a lookup_current_desktop
 *
 * Reached only when 'ccmd_client_unpin' finds a non-null stage;
 * none of these tests attach one, so this is unreachable in practice.
 *
 * @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a wm_get_client_desktop
 *
 * Reached by 'ccmd_client_unpin' (always) and by 'ccmd_client_urge'/
 * 'ccmd_client_unurge' (to decide whether to recompute urgency);
 * answering @c NULL keeps every desktop-owning branch untaken except
 * where a test opts in through @a s_owner_desktop below.
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
 * @brief Test-controlled stand-in for @a wm_get_stage_by_id
 *
 * Reached by 'ccmd_client_unpin', 'ccmd_client_toggle_pin' and
 * 'ccmd_client_toggle_stick'.  @c NULL, which 's_reset' restores, keeps
 * 'ccmd_client_unpin''s desktop-move branch untaken and makes both
 * toggles fall through to their actual work instead of returning early
 * for a stage that cannot host the flag.
 *
 * @note Complexity: @e O(1)
 */
static stage_td *s_stub_stage;

stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return s_stub_stage;
}


/** Stand-ins for what pinning a client of a desktop not shown uses to
 *  bring it onto the one that is, recording only how often it ran */
static int s_pinned_transfer_calls;
static int s_show_one_calls;
static int s_enforce_layers_calls;
static desktop_td s_current_desktop;

desktop_td *stage_desktop_get(const stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
    return &s_current_desktop;
}

client_td **ccmd_client_transient_family_snapshot(const desktop_td *desktop,
        client_td *top, size_t *count_out)
{
    (void) desktop;
    (void) top;
    *count_out = 0u;
    return NULL;
}

void stage_client_pinned_transfer_all(stage_td *stage, uint32_t to_id)
{
    (void) stage;
    (void) to_id;
    s_pinned_transfer_calls++;
    s_current_desktop.client_active_id = 0x77u;  /* as a switch would */
}

void stage_client_show_one(stage_td *stage, client_td *client)
{
    (void) stage;
    (void) client;
    s_show_one_calls++;
}

void ccmd_desktop_enforce_layers(desktop_td *desktop)
{
    (void) desktop;
    s_enforce_layers_calls++;
}


/**
 * @brief Test-controlled stand-in for @a stage_viewport_has_room
 *
 * Reproduced here rather than linking stage/viewport.c for one
 * predicate; only consulted once 's_stub_stage' above is non-null.
 *
 * @note Complexity: @e O(1)
 */
static bool s_stub_viewport_has_room;

bool stage_viewport_has_room(const stage_td *stage)
{
    (void) stage;

    return s_stub_viewport_has_room;
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
 * Reached only by 'ccmd_client_update_allowed_actions', which none of
 * these tests call.
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_connection_get
 *
 * Reached only by 'ccmd_client_update_allowed_actions', which none of
 * these tests call.
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_window_hide
 *
 * Reached only through the "moved off the current desktop" branch of
 * 'ccmd_client_unpin', which none of these tests take.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_hide(xcb_window_t window)
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
    s_pinned_transfer_calls = 0;
    s_show_one_calls = 0;
    s_enforce_layers_calls = 0;
    memset(&s_current_desktop, 0, sizeof(s_current_desktop));
    s_sync_states_calls = 0;
    s_publish_calls = 0;
    s_publish_last_desktop = 0u;
    s_recompute_urgent_calls = 0;
    s_ipc_broadcast_calls = 0;
    s_ipc_broadcast_last_type = 0u;
    s_owner_desktop = NULL;
    s_redraw_calls = 0;
    s_siblings_used = 0;
    s_stub_stage = NULL;
    s_stub_viewport_has_room = true;
    memset(s_siblings, 0, sizeof(s_siblings));
}


static void s_teardown(void)
{
    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* A null client is a silent no-op on every flag toggle: none of the
 * stand-ins above run, and nothing crashes */
static void s_test_null_client_is_a_no_op(void)
{
    s_reset();

    ccmd_client_pin(NULL);
    ccmd_client_unpin(NULL);
    ccmd_client_toggle_pin(NULL);
    ccmd_client_stick(NULL);
    ccmd_client_unstick(NULL);
    ccmd_client_toggle_stick(NULL);
    ccmd_client_urge(NULL);
    ccmd_client_unurge(NULL);
    ccmd_client_set_opacity_active(NULL, 50u);
    ccmd_client_set_opacity_inactive(NULL, 50u);
    ccmd_client_set_border_override(NULL, 0xffffffu, 2u);

    TAP_EQ_INT(s_sync_states_calls, 0,
            "a null client never reaches the EWMH state sync");
    TAP_EQ_INT(s_redraw_calls, 0,
            "and never reaches the redraw request either");

    s_teardown();
}


/* Pinning an unpinned client with no transient family sets its own
 * pin flag, syncs state once, publishes its desktop once, and
 * requests a redraw */
static void s_test_pin_sets_flag_and_side_effects(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);
    client->desktop_id = 3u;

    ccmd_client_pin(client);

    TAP_OK(client_is_pinned(client) != 0,
            "pinning an unpinned client sets its pin flag");
    TAP_EQ_INT(s_sync_states_calls, 1,
            "state is synced exactly once for the one client pinned");
    TAP_EQ_INT(s_publish_calls, 1,
            "the desktop-changed ping is sent exactly once");
    TAP_EQ_INT((long) s_publish_last_desktop, 3,
            "and it names the client's own desktop");
    TAP_EQ_INT(s_redraw_calls, 1, "a redraw is requested once");

    s_teardown();
}


/* Pinning a client that lives on a desktop not shown brings it onto
 * the one shown and maps it there at once; one already on the desktop
 * shown is left where it is */
static void s_test_pin_brings_client_from_desktop_not_shown(void)
{
    client_td *client;
    stage_td stage;
    desktop_td home;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&home, 0, sizeof(home));
    stage.desktop_cur = 0u;
    stage.desktop_count = 4u;
    home.id = 1u;
    s_stub_stage = &stage;
    s_owner_desktop = &home;
    client = s_make_client(2u);

    s_current_desktop.client_active_id = 0x55u;
    ccmd_client_pin(client);
    TAP_OK(s_pinned_transfer_calls == 1 && s_show_one_calls == 1 &&
            s_enforce_layers_calls == 1,
            "a client pinned from a desktop not shown is moved to the"
            " one shown and mapped there at once");
    TAP_EQ_INT((int) s_current_desktop.client_active_id, 0x55,
            "...and the desktop shown keeps its own active client");

    s_reset();
    memset(&home, 0, sizeof(home));
    home.id = 0u;
    s_stub_stage = &stage;
    s_owner_desktop = &home;
    client = s_make_client(3u);

    ccmd_client_pin(client);
    TAP_OK(s_pinned_transfer_calls == 0 && s_show_one_calls == 0,
            "a client pinned on the desktop shown is left where it is");

    s_stub_stage = NULL;
    s_owner_desktop = NULL;
    s_teardown();
}


/* Pinning a client already pinned is a no-op: the flag stays set, but
 * none of the side effects fire again */
static void s_test_pin_already_pinned_is_idempotent(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(2u);
    client_pin(client);

    ccmd_client_pin(client);

    TAP_OK(client_is_pinned(client) != 0,
            "the already-pinned client stays pinned");
    TAP_EQ_INT(s_sync_states_calls, 0,
            "no state sync fires for a client already in the target"
            " state");

    s_teardown();
}


/* Pinning one member of a transient family pins every other member
 * still unpinned too, cascading through the recording family-apply
 * stand-in */
static void s_test_pin_cascades_to_family(void)
{
    client_td *top;
    client_td *sibling;

    s_reset();
    top = s_make_client(10u);
    sibling = s_make_client(11u);
    s_siblings[0] = sibling;
    s_siblings_used = 1;

    ccmd_client_pin(top);

    TAP_OK(client_is_pinned(top) != 0, "the top parent is pinned");
    TAP_OK(client_is_pinned(sibling) != 0,
            "and its family member is pinned along with it");
    TAP_EQ_INT(s_sync_states_calls, 2,
            "state is synced once for each of the two clients pinned");

    s_teardown();
}


/* Unpinning a pinned, unlocked client with no transient family clears
 * its own pin flag and runs the same side effects as pin does */
static void s_test_unpin_clears_flag_and_side_effects(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(20u);
    client_pin(client);

    ccmd_client_unpin(client);

    TAP_OK(client_is_unpinned(client),
            "unpinning a pinned client clears its pin flag");
    TAP_EQ_INT(s_sync_states_calls, 1,
            "state is synced exactly once for the one client unpinned");
    TAP_EQ_INT(s_publish_calls, 1,
            "the desktop-changed ping is sent exactly once");
    TAP_EQ_INT(s_redraw_calls, 1, "a redraw is requested once");

    s_teardown();
}


/* A locked client is never unpinned, whatever its own pin flag says,
 * and no side effect fires for it at all */
static void s_test_unpin_locked_client_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(21u);
    client_pin(client);
    client->properties.flags |= CLIENT_FLAG_LOCKED;

    ccmd_client_unpin(client);

    TAP_OK(client_is_pinned(client) != 0,
            "a locked client stays pinned regardless of an unpin"
            " request");
    TAP_EQ_INT(s_sync_states_calls, 0,
            "no state sync fires for a locked client");

    s_teardown();
}


/* Unpinning cascades to every other pinned, unlocked family member,
 * but skips one that is locked */
static void s_test_unpin_cascades_but_skips_locked(void)
{
    client_td *top;
    client_td *unlocked_sibling;
    client_td *locked_sibling;

    s_reset();
    top = s_make_client(30u);
    unlocked_sibling = s_make_client(31u);
    locked_sibling = s_make_client(32u);
    client_pin(top);
    client_pin(unlocked_sibling);
    client_pin(locked_sibling);
    locked_sibling->properties.flags |= CLIENT_FLAG_LOCKED;
    s_siblings[0] = unlocked_sibling;
    s_siblings[1] = locked_sibling;
    s_siblings_used = 2;

    ccmd_client_unpin(top);

    TAP_OK(client_is_unpinned(top), "the top parent is unpinned");
    TAP_OK(client_is_unpinned(unlocked_sibling),
            "the unlocked family member is unpinned along with it");
    TAP_OK(client_is_pinned(locked_sibling) != 0,
            "but the locked family member stays pinned");

    s_teardown();
}


/* Toggling pin on a currently-unpinned client with no attached
 * stage pins it, and toggling again unpins it */
static void s_test_toggle_pin_flips_both_ways(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(40u);

    ccmd_client_toggle_pin(client);
    TAP_OK(client_is_pinned(client) != 0,
            "toggling an unpinned client with no stage pins it");

    ccmd_client_toggle_pin(client);
    TAP_OK(client_is_unpinned(client),
            "toggling it again unpins it back");

    s_teardown();
}


/* A locked client's toggle never even reaches the pin/unpin split: it
 * is refused outright, same as a direct unpin request would be */
static void s_test_toggle_pin_locked_client_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(41u);
    client->properties.flags |= CLIENT_FLAG_LOCKED;

    ccmd_client_toggle_pin(client);

    TAP_OK(client_is_unpinned(client),
            "a locked, unpinned client stays unpinned through a"
            " toggle request");

    s_teardown();
}


/* Sticking an unstuck client with no transient family sets its own
 * sticky flag and runs the same side effects pin does: state synced,
 * a redraw requested.  Unlike pin, no desktop-changed ping, sticky
 * never moving a client between desktops. */
static void s_test_stick_sets_flag_and_side_effects(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(50u);

    ccmd_client_stick(client);

    TAP_OK(client_is_sticky(client) != 0,
            "sticking an unstuck client sets its sticky flag");
    TAP_EQ_INT(s_sync_states_calls, 1,
            "state is synced exactly once for the one client stuck");
    TAP_EQ_INT(s_publish_calls, 0,
            "sticky does not move the client between desktops");
    TAP_EQ_INT(s_redraw_calls, 1, "a redraw is requested once");

    s_teardown();
}


/* Sticking one member of a transient family sticks every other
 * member still unstuck too, cascading through the recording
 * family-apply stand-in, the same as pin does */
static void s_test_stick_cascades_to_family(void)
{
    client_td *top;
    client_td *sibling;

    s_reset();
    top = s_make_client(58u);
    sibling = s_make_client(59u);
    s_siblings[0] = sibling;
    s_siblings_used = 1;

    ccmd_client_stick(top);

    TAP_OK(client_is_sticky(top) != 0, "the top parent is stuck");
    TAP_OK(client_is_sticky(sibling) != 0,
            "and its family member is stuck along with it");
    TAP_EQ_INT(s_sync_states_calls, 2,
            "state is synced once for each of the two clients stuck");

    s_teardown();
}


/* A locked client is never stuck, whatever its own sticky flag says */
static void s_test_stick_locked_client_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(51u);
    client->properties.flags |= CLIENT_FLAG_LOCKED;

    ccmd_client_stick(client);

    TAP_OK(client_is_unsticky(client),
            "a locked client stays unstuck regardless of a stick"
            " request");
    TAP_EQ_INT(s_sync_states_calls, 0,
            "no state sync fires for a locked client");

    s_teardown();
}


/* Unsticking a stuck, unlocked client clears its own sticky flag and
 * runs the same side effects stick does */
static void s_test_unstick_clears_flag_and_side_effects(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(52u);
    client_stick(client);

    ccmd_client_unstick(client);

    TAP_OK(client_is_unsticky(client),
            "unsticking a stuck client clears its sticky flag");
    TAP_EQ_INT(s_sync_states_calls, 1,
            "state is synced exactly once for the one client"
            " unstuck");
    TAP_EQ_INT(s_redraw_calls, 1, "a redraw is requested once");

    s_teardown();
}


/* Unsticking cascades to every other stuck, unlocked family member,
 * but skips one that is locked */
static void s_test_unstick_cascades_but_skips_locked(void)
{
    client_td *top;
    client_td *unlocked_sibling;
    client_td *locked_sibling;

    s_reset();
    top = s_make_client(60u);
    unlocked_sibling = s_make_client(61u);
    locked_sibling = s_make_client(62u);
    client_stick(top);
    client_stick(unlocked_sibling);
    client_stick(locked_sibling);
    locked_sibling->properties.flags |= CLIENT_FLAG_LOCKED;
    s_siblings[0] = unlocked_sibling;
    s_siblings[1] = locked_sibling;
    s_siblings_used = 2;

    ccmd_client_unstick(top);

    TAP_OK(client_is_unsticky(top), "the top parent is unstuck");
    TAP_OK(client_is_unsticky(unlocked_sibling),
            "the unlocked family member is unstuck along with it");
    TAP_OK(client_is_sticky(locked_sibling) != 0,
            "but the locked family member stays stuck");

    s_teardown();
}


/* A locked client is never unstuck, whatever its own sticky flag
 * says */
static void s_test_unstick_locked_client_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(53u);
    client_stick(client);
    client->properties.flags |= CLIENT_FLAG_LOCKED;

    ccmd_client_unstick(client);

    TAP_OK(client_is_sticky(client) != 0,
            "a locked client stays stuck regardless of an unstick"
            " request");
    TAP_EQ_INT(s_sync_states_calls, 0,
            "no state sync fires for a locked client");

    s_teardown();
}


/* Toggling sticky on a currently-unstuck client sticks it, and
 * toggling again unsticks it */
static void s_test_toggle_stick_flips_both_ways(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(54u);

    ccmd_client_toggle_stick(client);
    TAP_OK(client_is_sticky(client) != 0,
            "toggling an unstuck client sticks it");

    ccmd_client_toggle_stick(client);
    TAP_OK(client_is_unsticky(client),
            "toggling it again unsticks it back");

    s_teardown();
}


/* A stage whose configured viewport is a single screen refuses the
 * toggle outright: there is nothing for the flag to hold a client
 * still against there, so the keyboard shortcut is cut off at the same
 * choke point every other caller passes through */
static void s_test_toggle_stick_single_cell_viewport_is_a_no_op(void)
{
    client_td *client;
    stage_td stage;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    s_stub_stage = &stage;
    s_stub_viewport_has_room = false;
    client = s_make_client(56u);

    ccmd_client_toggle_stick(client);
    TAP_OK(client_is_unsticky(client),
            "a 1x1 viewport leaves an unstuck client unstuck through"
            " a toggle request");

    s_stub_viewport_has_room = true;
    ccmd_client_toggle_stick(client);
    TAP_OK(client_is_sticky(client) != 0,
            "the very same client sticks once its viewport can pan");

    s_teardown();
}


/* A locked client's toggle never even reaches the stick/unstick
 * split: it is refused outright, same as a direct unstick request
 * would be */
static void s_test_toggle_stick_locked_client_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(55u);
    client->properties.flags |= CLIENT_FLAG_LOCKED;

    ccmd_client_toggle_stick(client);

    TAP_OK(client_is_unsticky(client),
            "a locked, unstuck client stays unstuck through a"
            " toggle request");

    s_teardown();
}


/* Setting the active-state opacity override marks it set, stores the
 * given percentage, and requests a redraw */
static void s_test_set_opacity_active_stores_value(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(50u);

    ccmd_client_set_opacity_active(client, 75u);

    TAP_OK(client->opacity_override.is_set_active,
            "the active-state opacity override is marked set");
    TAP_EQ_INT((long) client->opacity_override.active, 75,
            "and holds the requested percentage");
    TAP_EQ_INT(s_redraw_calls, 1,
            "a redraw is requested after the override is set");

    s_teardown();
}


/* Setting the inactive-state opacity override is independent of the
 * active-state one: only its own half of the struct changes */
static void s_test_set_opacity_inactive_stores_value(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(51u);

    ccmd_client_set_opacity_inactive(client, 30u);

    TAP_OK(client->opacity_override.is_set_inactive,
            "the inactive-state opacity override is marked set");
    TAP_EQ_INT((long) client->opacity_override.inactive, 30,
            "and holds the requested percentage");
    TAP_OK(!client->opacity_override.is_set_active,
            "the active-state half is left untouched");

    s_teardown();
}


/* Setting the border override stores color and width together and
 * marks it set */
static void s_test_set_border_override_stores_values(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(60u);

    ccmd_client_set_border_override(client, 0x00ff00u, 3u);

    TAP_OK(client->border_override.is_set,
            "the border override is marked set");
    TAP_EQ_INT((long) client->border_override.color, (long) 0x00ff00u,
            "and holds the requested color");
    TAP_EQ_INT((long) client->border_override.width, 3,
            "and the requested width");
    TAP_EQ_INT(s_redraw_calls, 1,
            "a redraw is requested after the override is set");

    s_teardown();
}


/* Marking a client urgent sets its urgency flag, syncs state, and
 * broadcasts the urgency-set IPC event, but only recomputes the
 * desktop's own urgent count when the client actually has an owning
 * desktop */
static void s_test_urge_sets_flag_and_broadcasts(void)
{
    client_td *client;
    desktop_td owner;

    s_reset();
    client = s_make_client(70u);
    memset(&owner, 0, sizeof(owner));
    s_owner_desktop = &owner;

    ccmd_client_urge(client);

    TAP_OK(client_is_urgent(client) != 0,
            "marking a client urgent sets its urgency flag");
    TAP_EQ_INT(s_sync_states_calls, 1, "state is synced once");
    TAP_EQ_INT(s_recompute_urgent_calls, 1,
            "the owning desktop's urgent count is recomputed once");
    TAP_EQ_INT(s_ipc_broadcast_calls, 1,
            "exactly one IPC event is broadcast");
    TAP_EQ_INT((long) s_ipc_broadcast_last_type,
            (long) IPC_EVENT_URGENCY_SET,
            "and it is the urgency-set event");

    s_teardown();
}


/* A client with no owning desktop still gets marked urgent and
 * broadcast, but the recompute step is skipped: there is no desktop
 * to recompute for */
static void s_test_urge_with_no_owner_desktop_skips_recompute(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(71u);

    ccmd_client_urge(client);

    TAP_OK(client_is_urgent(client) != 0,
            "the client is still marked urgent with no owning desktop");
    TAP_EQ_INT(s_recompute_urgent_calls, 0,
            "but no recompute happens with nothing to recompute for");
    TAP_EQ_INT(s_ipc_broadcast_calls, 1,
            "the IPC event still fires regardless");

    s_teardown();
}


/* Clearing urgency is the mirror of setting it: the flag drops, and
 * the urgency-cleared event is the one broadcast instead */
static void s_test_unurge_clears_flag_and_broadcasts(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(72u);
    client_urge(client);

    ccmd_client_unurge(client);

    TAP_OK(!client_is_urgent(client),
            "clearing urgency drops the urgency flag");
    TAP_EQ_INT((long) s_ipc_broadcast_last_type,
            (long) IPC_EVENT_URGENCY_CLEARED,
            "the broadcast event is the urgency-cleared one, not the"
            " urgency-set one");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(69);

    s_test_null_client_is_a_no_op();
    s_test_pin_sets_flag_and_side_effects();
    s_test_pin_brings_client_from_desktop_not_shown();
    s_test_pin_already_pinned_is_idempotent();
    s_test_pin_cascades_to_family();
    s_test_unpin_clears_flag_and_side_effects();
    s_test_unpin_locked_client_is_a_no_op();
    s_test_unpin_cascades_but_skips_locked();
    s_test_toggle_pin_flips_both_ways();
    s_test_toggle_pin_locked_client_is_a_no_op();
    s_test_stick_sets_flag_and_side_effects();
    s_test_stick_cascades_to_family();
    s_test_stick_locked_client_is_a_no_op();
    s_test_unstick_clears_flag_and_side_effects();
    s_test_unstick_cascades_but_skips_locked();
    s_test_unstick_locked_client_is_a_no_op();
    s_test_toggle_stick_flips_both_ways();
    s_test_toggle_stick_single_cell_viewport_is_a_no_op();
    s_test_toggle_stick_locked_client_is_a_no_op();
    s_test_set_opacity_active_stores_value();
    s_test_set_opacity_inactive_stores_value();
    s_test_set_border_override_stores_values();
    s_test_urge_sets_flag_and_broadcasts();
    s_test_urge_with_no_owner_desktop_skips_recompute();
    s_test_unurge_clears_flag_and_broadcasts();

    return TAP_DONE();
}
