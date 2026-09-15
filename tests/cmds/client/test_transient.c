/**
 * @file tests/cmds/client/test_transient.c
 *
 * @brief Test battery for transient-family resolution and snapshot
 *        collection (cmds/client/transient.c)
 *
 * Every function under test is exercised linked against the real
 * 'cdlist' tree ('adt/cdlist.c') a client's own 'transients' and
 * 'transient_parent'/'transient_node' fields are built from, and the
 * real open-addressed hash table ('adt/ohtbl.c') a desktop's own
 * 'clients' table is, so a family walk, an 'ohtbl_foreach' group
 * search, and an @c O(1) unlink all run through the exact same real
 * data structures they do in the window manager itself.
 * 'lookup_find_client' is a test-controlled stand-in answering from
 * a small table this file fills directly, so 'client_link_transient'
 * never has to build a well-formed surfaces list just to resolve a
 * parent by window id.  'wm_get_client_desktop', 'wm_get_surface_by_
 * id', and 'surface_desktop_get' are test-controlled stand-ins too,
 * each answering from its own small table.  'desktop_action_client_
 * move', 'ccmd_client_restore', and 'ccmd_client_unhide' are
 * recording stand-ins, capturing what was actually asked for so a
 * test can check it directly rather than needing a live desktop-move
 * or map/unmap side effect.  'wm_get_surfaces' is a link-only
 * stand-in, reached only to be handed straight through to
 * 'lookup_find_client', which ignores it entirely.
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
#include <cmds/client/transient.h>
#include <desktop.h>
#include <harness/tap.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>


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
#define MAX_TEST_CLIENTS (80)
#define MAX_TEST_DESKTOPS (4)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;
static desktop_td *s_owned_desktops[MAX_TEST_DESKTOPS];
static int s_owned_desktops_used;

/** Table @a wm_get_surface_by_id answers from, filled by
 *  @a s_set_surface */
static surface_td *s_surfaces_by_screen[MAX_TEST_DESKTOPS];
static uint32_t s_surface_screen_ids[MAX_TEST_DESKTOPS];
static int s_surfaces_registered;

/** Table @a surface_desktop_get answers from, filled by
 *  @a s_link_surface_desktop */
static surface_td *s_sd_surfaces[MAX_TEST_DESKTOPS];
static uint32_t s_sd_desktop_ids[MAX_TEST_DESKTOPS];
static desktop_td *s_sd_desktops[MAX_TEST_DESKTOPS];
static int s_sd_registered;

/** Table @a wm_get_client_desktop answers from, filled by
 *  @a s_set_client_home */
static client_td *s_home_clients[MAX_TEST_CLIENTS];
static desktop_td *s_home_desktops[MAX_TEST_CLIENTS];
static int s_home_registered;


static client_td *s_make_client(xcb_window_t id)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = id;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


static desktop_td *s_make_desktop(xcb_window_t id)
{
    desktop_td *desktop = calloc(1, sizeof(*desktop));

    desktop->id = id;
    desktop->clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    s_owned_desktops[s_owned_desktops_used] = desktop;
    s_owned_desktops_used++;

    return desktop;
}


static void s_desktop_add_client(desktop_td *desktop, client_td *client)
{
    ohtbl_insert(desktop->clients, client);
}


static void s_set_surface(uint32_t screen_id, surface_td *surface)
{
    s_surface_screen_ids[s_surfaces_registered] = screen_id;
    s_surfaces_by_screen[s_surfaces_registered] = surface;
    s_surfaces_registered++;
}


static void s_link_surface_desktop(surface_td *surface, uint32_t desktop_id,
        desktop_td *desktop)
{
    s_sd_surfaces[s_sd_registered] = surface;
    s_sd_desktop_ids[s_sd_registered] = desktop_id;
    s_sd_desktops[s_sd_registered] = desktop;
    s_sd_registered++;
}


static void s_set_client_home(client_td *client, desktop_td *desktop)
{
    s_home_clients[s_home_registered] = client;
    s_home_desktops[s_home_registered] = desktop;
    s_home_registered++;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_surface_by_id
 * @note Complexity: @e O(n)
 */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    int i;

    for (i = 0; i < s_surfaces_registered; i++) {
        if (s_surface_screen_ids[i] == surface_id) {
            return s_surfaces_by_screen[i];
        }
    }

    return NULL;
}


/**
 * @brief Test-controlled stand-in for @a surface_desktop_get
 * @note Complexity: @e O(n)
 */
desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    int i;

    for (i = 0; i < s_sd_registered; i++) {
        if (s_sd_surfaces[i] == surface &&
                s_sd_desktop_ids[i] == desktop_id) {
            return s_sd_desktops[i];
        }
    }

    return NULL;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_client_desktop
 * @note Complexity: @e O(n)
 */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    int i;

    for (i = 0; i < s_home_registered; i++) {
        if (s_home_clients[i] == client) {
            return s_home_desktops[i];
        }
    }

    return NULL;
}


/** Table @a lookup_find_client answers from, filled by
 *  @a s_register_lookup */
static xcb_window_t s_lookup_ids[MAX_TEST_CLIENTS];
static client_td *s_lookup_clients[MAX_TEST_CLIENTS];
static int s_lookup_registered;


static void s_register_lookup(client_td *client)
{
    s_lookup_ids[s_lookup_registered] = client->id;
    s_lookup_clients[s_lookup_registered] = client;
    s_lookup_registered++;
}


/**
 * @brief Test-controlled stand-in for @a lookup_find_client
 *
 * Ignores @p surfaces entirely, answering from a small table this
 * file fills directly through @a s_register_lookup instead
 *
 * @note Complexity: @e O(n)
 */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    int i;

    (void) surfaces;

    if (out_surface != NULL) {
        *out_surface = NULL;
    }
    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }

    for (i = 0; i < s_lookup_registered; i++) {
        if (s_lookup_ids[i] == window) {
            return s_lookup_clients[i];
        }
    }

    return NULL;
}


/**
 * @brief Link-only stand-in for @a wm_get_surfaces
 *
 * 'client_link_transient' only ever passes what this answers
 * straight through to @a lookup_find_client, itself a test-controlled
 * stand-in that ignores it entirely
 *
 * @note Complexity: @e O(1)
 */
list_td *wm_get_surfaces(void)
{
    return NULL;
}


/** Recorded calls to @a desktop_action_client_move */
#define MAX_MOVE_LOG (16)
static desktop_td *s_move_from_log[MAX_MOVE_LOG];
static desktop_td *s_move_to_log[MAX_MOVE_LOG];
static client_td *s_move_client_log[MAX_MOVE_LOG];
static int s_move_log_used;


/**
 * @brief Recording stand-in for @a desktop_action_client_move
 * @note Complexity: @e O(1)
 */
int desktop_action_client_move(desktop_td *from, desktop_td *to,
        client_td *client)
{
    if (s_move_log_used < MAX_MOVE_LOG) {
        s_move_from_log[s_move_log_used] = from;
        s_move_to_log[s_move_log_used] = to;
        s_move_client_log[s_move_log_used] = client;
        s_move_log_used++;
    }
    client->desktop_id = to->id;

    return 0;
}


/** Recorded calls to @a ccmd_client_restore and @a ccmd_client_unhide */
#define MAX_REVEAL_LOG (16)
static client_td *s_restore_log[MAX_REVEAL_LOG];
static int s_restore_log_used;
static client_td *s_unhide_log[MAX_REVEAL_LOG];
static int s_unhide_log_used;


/**
 * @brief Recording stand-in for @a ccmd_client_restore
 * @note Complexity: @e O(1)
 */
void ccmd_client_restore(client_td *client)
{
    if (s_restore_log_used < MAX_REVEAL_LOG) {
        s_restore_log[s_restore_log_used] = client;
        s_restore_log_used++;
    }
    client->properties.state &= (uint16_t) ~CLIENT_STATE_ICONIFIED;
}


/**
 * @brief Recording stand-in for @a ccmd_client_unhide
 * @note Complexity: @e O(1)
 */
void ccmd_client_unhide(client_td *client)
{
    if (s_unhide_log_used < MAX_REVEAL_LOG) {
        s_unhide_log[s_unhide_log_used] = client;
        s_unhide_log_used++;
    }
    client->properties.flags &= (uint32_t) ~CLIENT_FLAG_HIDDEN;
}


static void s_reset(void)
{
    s_surfaces_registered = 0;
    s_sd_registered = 0;
    s_home_registered = 0;
    s_lookup_registered = 0;
    s_move_log_used = 0;
    s_restore_log_used = 0;
    s_unhide_log_used = 0;
    memset(s_surfaces_by_screen, 0, sizeof(s_surfaces_by_screen));
    memset(s_sd_desktops, 0, sizeof(s_sd_desktops));
    memset(s_home_desktops, 0, sizeof(s_home_desktops));
}


static void s_teardown(void)
{
    int i;

    for (i = 0; i < s_owned_desktops_used; i++) {
        ohtbl_destroy(s_owned_desktops[i]->clients);
        free(s_owned_desktops[i]);
    }
    s_owned_desktops_used = 0;

    for (i = 0; i < s_owned_clients_used; i++) {
        if (s_owned_clients[i]->transients != NULL) {
            cdlist_destroy(s_owned_clients[i]->transients);
        }
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* A null client's top parent is null, never a crash */
static void s_test_top_parent_null_client(void)
{
    s_reset();

    TAP_OK((ccmd_client_transient_top_parent(NULL)) == NULL,
            "a null client's top parent is null");

    s_teardown();
}


/* A client with no parent at all is its own top parent */
static void s_test_top_parent_no_parent(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);

    TAP_OK(ccmd_client_transient_top_parent(client) == client,
            "a client transient for nothing is its own top parent");

    s_teardown();
}


/* A three-generation chain resolves all the way up to the grandparent */
static void s_test_top_parent_chain(void)
{
    client_td *grandparent;
    client_td *parent;
    client_td *child;

    s_reset();
    grandparent = s_make_client(1u);
    parent = s_make_client(2u);
    child = s_make_client(3u);
    parent->transient_parent = grandparent;
    child->transient_parent = parent;

    TAP_OK(ccmd_client_transient_top_parent(child) == grandparent,
            "a three-generation chain resolves to the grandparent");

    s_teardown();
}


/* A chain artificially made to exceed the maximum depth stops at the
 * depth limit rather than looping forever on a cycle */
static void s_test_top_parent_depth_guard(void)
{
    client_td *nodes[WM_TRANSIENT_CHAIN_MAX_DEPTH + 4];
    uint32_t i;

    s_reset();
    for (i = 0; i < WM_TRANSIENT_CHAIN_MAX_DEPTH + 4; i++) {
        nodes[i] = s_make_client((xcb_window_t) (i + 1u));
    }
    /* A cycle: every node points back to the very first one */
    for (i = 1; i < WM_TRANSIENT_CHAIN_MAX_DEPTH + 4; i++) {
        nodes[i]->transient_parent = nodes[i - 1u];
    }
    nodes[0]->transient_parent = nodes[WM_TRANSIENT_CHAIN_MAX_DEPTH + 3u];

    TAP_OK(ccmd_client_transient_top_parent(
                nodes[WM_TRANSIENT_CHAIN_MAX_DEPTH + 3u]) != NULL,
            "a cyclic chain still returns some node rather than"
            " looping forever, thanks to the depth guard");

    s_teardown();
}


/* A group-transient client with no other sibling present resolves to
 * no anchor at all */
static void s_test_group_anchor_no_sibling(void)
{
    client_td *dialog;

    s_reset();
    dialog = s_make_client(1u);
    dialog->is_transient_for_group = true;
    dialog->hints_icccm.hints.group_leader = (xcb_window_t) 42u;

    TAP_OK((client_group_transient_anchor(dialog)) == NULL,
            "a group-transient client with no sibling anywhere has"
            " no anchor");

    s_teardown();
}


/* A group-transient client finds its mapped, non-group-transient
 * sibling on its own desktop as its anchor */
static void s_test_group_anchor_finds_sibling(void)
{
    desktop_td *desktop;
    client_td *dialog;
    client_td *sibling;

    s_reset();
    desktop = s_make_desktop(1u);
    dialog = s_make_client(1u);
    dialog->is_transient_for_group = true;
    dialog->hints_icccm.hints.group_leader = (xcb_window_t) 42u;
    sibling = s_make_client(2u);
    sibling->hints_icccm.hints.group_leader = (xcb_window_t) 42u;
    s_desktop_add_client(desktop, dialog);
    s_desktop_add_client(desktop, sibling);
    s_set_client_home(dialog, desktop);

    TAP_OK(client_group_transient_anchor(dialog) == sibling,
            "the plain sibling sharing the same group leader is"
            " found as the anchor");

    s_teardown();
}


/* A group-transient client never resolves another group-transient
 * dialog as its own anchor, even when it is the only other member
 * present */
static void s_test_group_anchor_excludes_other_dialog(void)
{
    desktop_td *desktop;
    client_td *dialog_a;
    client_td *dialog_b;

    s_reset();
    desktop = s_make_desktop(1u);
    dialog_a = s_make_client(1u);
    dialog_a->is_transient_for_group = true;
    dialog_a->hints_icccm.hints.group_leader = (xcb_window_t) 42u;
    dialog_b = s_make_client(2u);
    dialog_b->is_transient_for_group = true;
    dialog_b->hints_icccm.hints.group_leader = (xcb_window_t) 42u;
    s_desktop_add_client(desktop, dialog_a);
    s_desktop_add_client(desktop, dialog_b);
    s_set_client_home(dialog_a, desktop);

    TAP_OK((client_group_transient_anchor(dialog_a)) == NULL,
            "another group-transient dialog is never chosen as the"
            " anchor");

    s_teardown();
}


/* A client that is not itself transient for its whole group never
 * resolves an anchor, regardless of what else exists */
static void s_test_group_anchor_not_group_transient(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);

    TAP_OK((client_group_transient_anchor(client)) == NULL,
            "a client not transient for its group has no anchor at"
            " all");

    s_teardown();
}


/* Snapshotting a top parent with no transient children at all
 * returns null and a zero count */
static void s_test_snapshot_empty_family(void)
{
    desktop_td *desktop;
    client_td *top;
    size_t count = 999u;

    s_reset();
    desktop = s_make_desktop(1u);
    top = s_make_client(1u);

    TAP_OK(ccmd_client_transient_family_snapshot(desktop, top, &count)
                == NULL,
            "a top parent with no children snapshots to null");
    TAP_EQ_INT((int) count, 0, "and the count comes back zero");

    s_teardown();
}


/* A two-generation family (child and grandchild) both appear in the
 * snapshot, but the top parent itself never does */
static void s_test_snapshot_two_generations(void)
{
    desktop_td *desktop;
    client_td *top;
    client_td *child;
    client_td *grandchild;
    client_td **members;
    size_t count;
    bool found_child;
    bool found_grandchild;
    bool found_top;
    size_t i;

    s_reset();
    desktop = s_make_desktop(1u);
    top = s_make_client(1u);
    child = s_make_client(2u);
    child->desktop_id = 1u;
    grandchild = s_make_client(3u);
    grandchild->desktop_id = 1u;
    top->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(top->transients, NULL, child);
    child->transient_parent = top;
    child->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(child->transients, NULL, grandchild);
    grandchild->transient_parent = child;

    members = ccmd_client_transient_family_snapshot(desktop, top, &count);

    TAP_EQ_INT((int) count, 2, "both the child and grandchild are"
            " collected");
    found_child = false;
    found_grandchild = false;
    found_top = false;
    for (i = 0; i < count; i++) {
        found_child = (found_child || members[i] == child);
        found_grandchild = (found_grandchild || members[i] == grandchild);
        found_top = (found_top || members[i] == top);
    }
    TAP_OK(found_child, "the direct child is present");
    TAP_OK(found_grandchild, "the grandchild is present");
    TAP_OK(!found_top, "the top parent itself is never included");

    free(members);
    s_teardown();
}


/* A desktop filter genuinely restricts the snapshot to only the
 * members whose own 'desktop_id' matches, unlike the '_anywhere'
 * counterpart */
static void s_test_snapshot_desktop_filter(void)
{
    desktop_td *desktop;
    client_td *top;
    client_td *same_desktop;
    client_td *other_desktop;
    client_td **members;
    size_t count;

    s_reset();
    desktop = s_make_desktop(7u);
    top = s_make_client(1u);
    same_desktop = s_make_client(2u);
    same_desktop->desktop_id = 7u;
    other_desktop = s_make_client(3u);
    other_desktop->desktop_id = 9u;
    top->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(top->transients, NULL, same_desktop);
    (void) cdlist_ins_next(top->transients, NULL, other_desktop);

    members = ccmd_client_transient_family_snapshot(desktop, top, &count);

    TAP_EQ_INT((int) count, 1, "only the matching-desktop member is"
            " collected");
    TAP_OK(members[0] == same_desktop, "and it is the right one");

    free(members);
    s_teardown();
}


/* The '_anywhere' counterpart finds every family member regardless of
 * which desktop each one is registered under */
static void s_test_snapshot_anywhere_ignores_desktop(void)
{
    client_td *top;
    client_td *same_desktop;
    client_td *other_desktop;
    client_td **members;
    size_t count;

    s_reset();
    top = s_make_client(1u);
    same_desktop = s_make_client(2u);
    same_desktop->desktop_id = 7u;
    other_desktop = s_make_client(3u);
    other_desktop->desktop_id = 9u;
    top->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(top->transients, NULL, same_desktop);
    (void) cdlist_ins_next(top->transients, NULL, other_desktop);

    members = ccmd_client_transient_family_snapshot_anywhere(top, &count);

    TAP_EQ_INT((int) count, 2, "both members are collected regardless"
            " of desktop");

    free(members);
    s_teardown();
}


/* A snapshot with a null desktop, a null top, or a null count_out is
 * refused outright */
static void s_test_snapshot_null_arguments_refused(void)
{
    desktop_td *desktop;
    client_td *top;
    size_t count = 0u;

    s_reset();
    desktop = s_make_desktop(1u);
    top = s_make_client(1u);

    TAP_OK(ccmd_client_transient_family_snapshot(NULL, top, &count) == NULL,
            "a null desktop is refused outright");
    TAP_OK(ccmd_client_transient_family_snapshot(desktop, NULL, &count)
                == NULL,
            "a null top is refused outright");
    TAP_OK(ccmd_client_transient_family_snapshot(desktop, top, NULL) == NULL,
            "a null count_out is refused outright");
    TAP_OK(ccmd_client_transient_family_snapshot_anywhere(NULL, &count)
                == NULL,
            "the anywhere counterpart refuses a null top too");

    s_teardown();
}


/* A family large enough to have once exceeded an old, fixed 32-slot
 * cap is still collected in full, none dropped */
static void s_test_snapshot_past_old_fixed_cap(void)
{
    client_td *top;
    client_td *children[40];
    client_td **members;
    size_t count;
    int i;

    s_reset();
    top = s_make_client(1u);
    top->transients = cdlist_init(NULL);
    for (i = 0; i < 40; i++) {
        children[i] = s_make_client((xcb_window_t) (i + 2));
        (void) cdlist_ins_next(top->transients, NULL, children[i]);
    }

    members = ccmd_client_transient_family_snapshot_anywhere(top, &count);

    TAP_EQ_INT((int) count, 40,
            "all 40 children are collected, past the old 32-slot cap");

    free(members);
    s_teardown();
}


/** Recorded calls a @a ccmd_family_fn test double leaves behind */
static client_td *s_apply_log[64];
static int s_apply_log_used;


static void s_apply_recorder(client_td *member, void *ctx)
{
    (void) ctx;
    if (s_apply_log_used < 64) {
        s_apply_log[s_apply_log_used] = member;
        s_apply_log_used++;
    }
}


/* ccmd_client_family_apply visits every descendant exactly once, top
 * excluded */
static void s_test_family_apply_visits_all(void)
{
    client_td *top;
    client_td *child;
    client_td *grandchild;

    s_reset();
    s_apply_log_used = 0;
    top = s_make_client(1u);
    child = s_make_client(2u);
    grandchild = s_make_client(3u);
    top->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(top->transients, NULL, child);
    child->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(child->transients, NULL, grandchild);

    ccmd_client_family_apply(top, s_apply_recorder, NULL);

    TAP_EQ_INT(s_apply_log_used, 2, "exactly the child and grandchild"
            " are visited");

    s_teardown();
}


/* ccmd_client_family_apply on a top with no transients at all visits
 * nothing, a silent no-op */
static void s_test_family_apply_empty_is_noop(void)
{
    client_td *top;

    s_reset();
    s_apply_log_used = 0;
    top = s_make_client(1u);

    ccmd_client_family_apply(top, s_apply_recorder, NULL);

    TAP_EQ_INT(s_apply_log_used, 0, "nothing is visited");

    s_teardown();
}


/* Linking a client whose declared parent is not (or not yet) managed
 * is a silent no-op: it is never linked into any tree */
static void s_test_link_unmanaged_parent_is_noop(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);
    client->transient_for = (xcb_window_t) 999u;

    client_link_transient(client);

    TAP_OK((client->transient_parent) == NULL,
            "a client whose parent is not managed stays unlinked");

    s_teardown();
}


/* Linking a client transient for itself is a silent no-op too */
static void s_test_link_self_transient_is_noop(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);
    client->transient_for = client->id;
    s_register_lookup(client);

    client_link_transient(client);

    TAP_OK((client->transient_parent) == NULL,
            "a client transient for itself stays unlinked");

    s_teardown();
}


/* A null client is a silent no-op for every link/unlink entry point */
static void s_test_link_unlink_null_is_noop(void)
{
    s_reset();

    client_link_transient(NULL);
    client_unlink_transient(NULL);

    TAP_OK(true, "linking and unlinking a null client never crashes");

    s_teardown();
}


/* Linking a client whose parent is already managed attaches it at
 * the head of that parent's own transients list, both directions
 * of the pointer set */
static void s_test_link_attaches_to_parent(void)
{
    client_td *parent;
    client_td *child;

    s_reset();
    parent = s_make_client(10u);
    child = s_make_client(11u);
    child->transient_for = parent->id;
    s_register_lookup(parent);

    client_link_transient(child);

    TAP_OK(child->transient_parent == parent,
            "the child's transient_parent now points at the parent");
    TAP_OK((parent->transients) != NULL,
            "the parent's own transients list now exists");
    TAP_OK((client_td *) cdlist_data(cdlist_head(parent->transients))
                == child,
            "the child is the head of the parent's transients list");

    s_teardown();
}


/* Unlinking a client removes it from its parent's transients list in
 * one step and orphans every one of its own children */
static void s_test_unlink_removes_and_orphans(void)
{
    client_td *parent;
    client_td *child;
    client_td *grandchild;

    s_reset();
    parent = s_make_client(10u);
    child = s_make_client(11u);
    grandchild = s_make_client(12u);
    child->transient_for = parent->id;
    s_register_lookup(parent);
    client_link_transient(child);
    grandchild->transient_for = child->id;
    s_register_lookup(child);
    client_link_transient(grandchild);

    client_unlink_transient(child);

    TAP_OK((child->transient_parent) == NULL,
            "the unlinked child's own parent pointer is cleared");
    TAP_OK(cdlist_is_empty(parent->transients),
            "the parent's transients list no longer holds it");
    TAP_OK((grandchild->transient_parent) == NULL,
            "the grandchild is orphaned, its parent pointer cleared");
    TAP_OK((child->transients) == NULL,
            "the unlinked child's own transients list is torn down");

    s_teardown();
}


/* Focus target on a client with no mapped transient child at all is
 * simply itself */
static void s_test_focus_target_no_children(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(1u);

    TAP_OK(ccmd_client_focus_target(client) == client,
            "a childless client is its own focus target");

    s_teardown();
}


/* Focus target descends to the single mapped, non-iconified child */
static void s_test_focus_target_descends_one_level(void)
{
    client_td *parent;
    client_td *child;

    s_reset();
    parent = s_make_client(1u);
    child = s_make_client(2u);
    parent->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(parent->transients, NULL, child);

    TAP_OK(ccmd_client_focus_target(parent) == child,
            "focus descends to the single mapped child");

    s_teardown();
}


/* An iconified child is skipped, leaving the parent itself as the
 * focus target */
static void s_test_focus_target_skips_iconified_child(void)
{
    client_td *parent;
    client_td *child;

    s_reset();
    parent = s_make_client(1u);
    child = s_make_client(2u);
    child->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    parent->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(parent->transients, NULL, child);

    TAP_OK(ccmd_client_focus_target(parent) == parent,
            "an iconified child is skipped, leaving the parent"
            " itself as the target");

    s_teardown();
}


/* A null client's focus target is null */
static void s_test_focus_target_null_client(void)
{
    s_reset();

    TAP_OK((ccmd_client_focus_target(NULL)) == NULL,
            "a null client's focus target is null");

    s_teardown();
}


/* ccmd_client_bring_family relocates a sibling found on a different
 * desktop than the one currently being looked at, onto that one */
static void s_test_bring_family_relocates_sibling(void)
{
    surface_td surface;
    desktop_td *home_desktop;
    desktop_td *current_desktop;
    client_td *top;
    client_td *sibling;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.desktop_cur = 2u;
    home_desktop = s_make_desktop(1u);
    current_desktop = s_make_desktop(2u);
    top = s_make_client(1u);
    top->screen_id = 5u;
    sibling = s_make_client(2u);
    sibling->desktop_id = 1u;
    top->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(top->transients, NULL, sibling);
    s_set_surface(5u, &surface);
    s_link_surface_desktop(&surface, 2u, current_desktop);
    s_set_client_home(sibling, home_desktop);

    ccmd_client_bring_family(top);

    TAP_EQ_INT(s_move_log_used, 1, "exactly one relocation is recorded");
    TAP_OK(s_move_to_log[0] == current_desktop,
            "the sibling is moved onto the desktop being looked at");
    TAP_EQ_INT((int) sibling->desktop_id, 2,
            "the sibling's own desktop_id now reflects the move");

    s_teardown();
}


/* An iconified sibling already on the right desktop is restored
 * rather than relocated */
static void s_test_bring_family_restores_iconified(void)
{
    surface_td surface;
    desktop_td *current_desktop;
    client_td *top;
    client_td *sibling;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.desktop_cur = 3u;
    current_desktop = s_make_desktop(3u);
    top = s_make_client(1u);
    top->screen_id = 5u;
    sibling = s_make_client(2u);
    sibling->desktop_id = 3u;
    sibling->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    top->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(top->transients, NULL, sibling);
    s_set_surface(5u, &surface);
    s_link_surface_desktop(&surface, 3u, current_desktop);

    ccmd_client_bring_family(top);

    TAP_EQ_INT(s_move_log_used, 0, "no relocation happens, it is"
            " already on the right desktop");
    TAP_EQ_INT(s_restore_log_used, 1, "the iconified sibling is"
            " restored instead");
    TAP_OK(s_restore_log[0] == sibling, "and it is the right client");

    s_teardown();
}


/* A genuinely hidden (not withdrawn) sibling is unhidden */
static void s_test_bring_family_unhides_hidden(void)
{
    surface_td surface;
    desktop_td *current_desktop;
    client_td *top;
    client_td *sibling;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.desktop_cur = 3u;
    current_desktop = s_make_desktop(3u);
    top = s_make_client(1u);
    top->screen_id = 5u;
    sibling = s_make_client(2u);
    sibling->desktop_id = 3u;
    client_hide(sibling);
    top->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(top->transients, NULL, sibling);
    s_set_surface(5u, &surface);
    s_link_surface_desktop(&surface, 3u, current_desktop);

    ccmd_client_bring_family(top);

    TAP_EQ_INT(s_unhide_log_used, 1,
            "the hidden sibling is unhidden");
    TAP_OK(s_unhide_log[0] == sibling, "and it is the right client");

    s_teardown();
}


/* A transient sibling is still relocated to the desktop being shown,
 * the same as any other family member, but never revealed: a dialog
 * is not something automatic discovery brings back on its own */
static void s_test_bring_family_relocates_but_skips_transient(void)
{
    surface_td surface;
    desktop_td *current_desktop;
    desktop_td *home_desktop;
    client_td *top;
    client_td *sibling;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.desktop_cur = 3u;
    current_desktop = s_make_desktop(3u);
    home_desktop = s_make_desktop(7u);
    top = s_make_client(1u);
    top->screen_id = 5u;
    sibling = s_make_client(2u);
    sibling->desktop_id = 7u;   /* deliberately on some other desktop */
    sibling->transient_for = (xcb_window_t) 1;
    client_hide(sibling);
    top->transients = cdlist_init(NULL);
    (void) cdlist_ins_next(top->transients, NULL, sibling);
    s_set_surface(5u, &surface);
    s_link_surface_desktop(&surface, 3u, current_desktop);
    s_set_client_home(sibling, home_desktop);

    ccmd_client_bring_family(top);

    TAP_EQ_INT(s_move_log_used, 1,
            "a transient sibling is still relocated like the rest of"
            " the family");
    TAP_EQ_INT(s_unhide_log_used, 0,
            "but it is not unhidden");

    s_teardown();
}


/* A null client, or one whose top parent's surface cannot be
 * resolved, is a silent no-op */
static void s_test_bring_family_null_and_unresolved_are_noop(void)
{
    client_td *lone;

    s_reset();

    ccmd_client_bring_family(NULL);
    TAP_OK(true, "a null client never crashes");

    lone = s_make_client(1u);
    lone->screen_id = 123u;
    ccmd_client_bring_family(lone);
    TAP_EQ_INT(s_move_log_used, 0,
            "an unresolvable surface leaves nothing relocated");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(50);

    s_test_top_parent_null_client();
    s_test_top_parent_no_parent();
    s_test_top_parent_chain();
    s_test_top_parent_depth_guard();
    s_test_group_anchor_no_sibling();
    s_test_group_anchor_finds_sibling();
    s_test_group_anchor_excludes_other_dialog();
    s_test_group_anchor_not_group_transient();
    s_test_snapshot_empty_family();
    s_test_snapshot_two_generations();
    s_test_snapshot_desktop_filter();
    s_test_snapshot_anywhere_ignores_desktop();
    s_test_snapshot_null_arguments_refused();
    s_test_snapshot_past_old_fixed_cap();
    s_test_family_apply_visits_all();
    s_test_family_apply_empty_is_noop();
    s_test_link_unmanaged_parent_is_noop();
    s_test_link_self_transient_is_noop();
    s_test_link_unlink_null_is_noop();
    s_test_link_attaches_to_parent();
    s_test_unlink_removes_and_orphans();
    s_test_focus_target_no_children();
    s_test_focus_target_descends_one_level();
    s_test_focus_target_skips_iconified_child();
    s_test_focus_target_null_client();
    s_test_bring_family_relocates_sibling();
    s_test_bring_family_restores_iconified();
    s_test_bring_family_unhides_hidden();
    s_test_bring_family_relocates_but_skips_transient();
    s_test_bring_family_null_and_unresolved_are_noop();

    return TAP_DONE();
}
