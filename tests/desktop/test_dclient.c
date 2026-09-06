/**
 * @file tests/desktop/test_dclient.c
 *
 * @brief Test battery for desktop client management and action
 *        dispatchers
 *
 * 'src/policy/stacking.c', 'src/adt/ohtbl.c', 'src/adt/cdlist.c',
 * 'src/utils/safe/safestr.c', and 'src/logger.c' are all linked for
 * real: the same, small, self-contained sources
 * 'tests/desktop/test_workarea.c' already links this same way, since
 * dclient.c's own behavior genuinely depends on how those structures
 * actually behave (a client removed from the real hash table must
 * actually be gone from a real 'ohtbl_lookup', not merely reported
 * gone by a stand-in that could get that wrong without any test here
 * ever noticing).  'logger_msg' behind every 'LOGGER_*' macro is a
 * silent, safe no-op while uninitialized (its own first check is
 * 'logger == NULL'), so this file never calls 'logger_start' at all;
 * every log call below still resolves at link time, just does
 * nothing at runtime.  Every other dependency (transient family
 * resolution, layer enforcement, iconify/restore, startup
 * notification, spawning, the singleton window-manager accessors, and
 * the message dialog dclient.c can pop up on urgency) is a recording
 * stand-in below, following 'tests/rules/test_apply.c' 's pattern of
 * opaque handles and call counters.
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
#include <stdint.h>
#include <string.h>
#include <sys/types.h>  /* pid_t */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Hash includes */
#include <utils/hash/murmurhash.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <surface.h>

/* Local includes */
#include <cctl/sn.h>
#include <cmds/client/layer.h>
#include <cmds/client/transient.h>
#include <desktop.h>
#include <enact.h>
#include <harness/tap.h>
#include <menu/dialog/message.h>
#include <policy/stacking.h>
#include <utils/safe/safestr.h>
#include <utils/spawn.h>
#include <utils/xcb/connection.h>
#include <wm.h>


/** Fake, non-null XCB connection handle, standing in for a live one
 *  wherever dclient.c merely forwards it onward to another stubbed
 *  function without ever dereferencing it directly itself */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

/** Whether xcb_connection_get answers a live connection or NULL, so
 *  every scenario controls for itself whether dclient.c's own
 *  'xcb_connection_get() != NULL' guards take the XCB-touching branch
 *  or the plain fallback */
static bool s_connection_is_live;


/**
 * @brief Test-controlled stand-in for @a xcb_connection_get
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return (s_connection_is_live) ? s_fake_connection : NULL;
}


/** Whether this file's own xcb_get_window_attributes_reply stand-in
 *  answers a reply (window still exists) or NULL (BadWindow, the
 *  window is gone); read by desktop_action_client_add's stale-ghost-
 *  window check */
static bool s_probed_window_exists;
static int s_call_get_window_attributes;


/**
 * @brief Link-only stand-in for @a xcb_get_window_attributes
 * @note Complexity: @e O(1)
 */
xcb_get_window_attributes_cookie_t xcb_get_window_attributes(
        xcb_connection_t *c, xcb_window_t window)
{
    xcb_get_window_attributes_cookie_t cookie;

    (void) c;
    (void) window;
    s_call_get_window_attributes++;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Test-controlled stand-in for @a xcb_get_window_attributes_reply
 *
 * Answers a heap-allocated reply (freed by the caller, dclient.c
 * itself, exactly as for a real one) when
 * 's_probed_window_exists' is @c true, or @c NULL (matching what a
 * real server answers for a 'BadWindow') otherwise.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_window_attributes_reply_t *xcb_get_window_attributes_reply(
        xcb_connection_t *c, xcb_get_window_attributes_cookie_t cookie,
        xcb_generic_error_t **e)
{
    xcb_get_window_attributes_reply_t *reply;

    (void) c;
    (void) cookie;

    if (e != NULL) {
        *e = NULL;
    }
    if (!s_probed_window_exists) {
        return NULL;
    }

    reply = malloc(sizeof(*reply));
    memset(reply, 0, sizeof(*reply));
    return reply;
}


/** Call counter and last-seen argument for the layer-enforcement
 *  stand-in */
static int s_call_enforce_layers;
static desktop_td *s_last_enforce_layers_desktop;


/**
 * @brief Recording stand-in for @a ccmd_desktop_enforce_layers
 * @note Complexity: @e O(1)
 */
void ccmd_desktop_enforce_layers(desktop_td *desktop)
{
    s_call_enforce_layers++;
    s_last_enforce_layers_desktop = desktop;
}


/** Fixed answer this file's own client_group_transient_anchor
 *  stand-in reports; set per scenario, NULL by default */
static client_td *s_group_transient_anchor_answer;


/**
 * @brief Test-controlled stand-in for @a client_group_transient_anchor
 * @note Complexity: @e O(1)
 */
client_td *client_group_transient_anchor(const client_td *client)
{
    (void) client;
    return s_group_transient_anchor_answer;
}


/** Call counters and last-seen argument for the enact stand-ins */
static int s_call_enact_iconify;
static int s_call_enact_restore;
static client_td *s_last_enact_client;


/**
 * @brief Recording stand-in for @a enact_client_iconify
 * @note Complexity: @e O(1)
 */
void enact_client_iconify(client_td *client)
{
    s_call_enact_iconify++;
    s_last_enact_client = client;
}


/**
 * @brief Recording stand-in for @a enact_client_restore
 * @note Complexity: @e O(1)
 */
void enact_client_restore(client_td *client)
{
    s_call_enact_restore++;
    s_last_enact_client = client;
}


/** Startup-notification stand-ins' call counters and canned answers */
static int s_call_sn_begin;
static int s_call_sn_cancel;
static bool s_sn_begin_answer = true;
static char s_sn_begin_id[128] = "startup-id-42";


/**
 * @brief Test-controlled stand-in for @a cctl_sn_begin
 * @note Complexity: @e O(1)
 */
bool cctl_sn_begin(xcb_connection_t *connection, list_td *surfaces,
        const char *restrict name, uint32_t origin_desktop,
        char *restrict out_id, size_t out_id_size)
{
    (void) connection;
    (void) surfaces;
    (void) name;
    (void) origin_desktop;

    s_call_sn_begin++;
    if (!s_sn_begin_answer) {
        return false;
    }

    (void) safe_strncpy(out_id, s_sn_begin_id, out_id_size);
    return true;
}


/**
 * @brief Recording stand-in for @a cctl_sn_cancel
 * @note Complexity: @e O(1)
 */
void cctl_sn_cancel(xcb_connection_t *connection, list_td *surfaces,
        const char *id)
{
    (void) connection;
    (void) surfaces;
    (void) id;
    s_call_sn_cancel++;
}


/** Fixed answers this file's wm_get_* stand-ins report; set per
 *  scenario */
static surface_td *s_wm_desktop_surface_answer;
static config_td *s_wm_config_answer;
static list_td *s_wm_surfaces_answer;


/** Viewport page a scenario wants reported for the urgent client and
 *  for the page the desktop is showing, so the three branches of the
 *  notice can each be reached; @c s_viewport_has_pages false is the
 *  single-page case, where neither is reported at all */
static bool s_viewport_has_pages;
static uint32_t s_client_page_col;
static uint32_t s_client_page_row;
static uint32_t s_shown_page_col;
static uint32_t s_shown_page_row;


/**
 * @brief Test-controlled stand-in for
 *        @a scmd_surface_viewport_client_page
 * @note Complexity: @e O(1)
 */
bool scmd_surface_viewport_client_page(const surface_td *surface,
        const desktop_td *desktop, const client_td *client,
        uint32_t *col_out, uint32_t *row_out)
{
    (void) surface;
    (void) desktop;
    (void) client;

    if (!s_viewport_has_pages) {
        return false;
    }
    *col_out = s_client_page_col;
    *row_out = s_client_page_row;
    return true;
}


/**
 * @brief Test-controlled stand-in for
 *        @a scmd_surface_viewport_desktop_page
 * @note Complexity: @e O(1)
 */
bool scmd_surface_viewport_desktop_page(const surface_td *surface,
        const desktop_td *desktop, uint32_t *col_out, uint32_t *row_out)
{
    (void) surface;
    (void) desktop;

    if (!s_viewport_has_pages) {
        return false;
    }
    *col_out = s_shown_page_col;
    *row_out = s_shown_page_row;
    return true;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_desktop_surface
 * @note Complexity: @e O(1)
 */
surface_td *wm_get_desktop_surface(const desktop_td *desktop)
{
    (void) desktop;
    return s_wm_desktop_surface_answer;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_config
 * @note Complexity: @e O(1)
 */
config_td *wm_get_config(void)
{
    return s_wm_config_answer;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_surfaces
 * @note Complexity: @e O(1)
 */
list_td *wm_get_surfaces(void)
{
    return s_wm_surfaces_answer;
}


/** Message-dialog stand-ins' call counter and canned "is open" answer */
static int s_call_message_dialog_show;
static bool s_message_dialog_is_open_answer;


/**
 * @brief Test-controlled stand-in for @a menu_message_dialog_is_open
 * @note Complexity: @e O(1)
 */
bool menu_message_dialog_is_open(void)
{
    return s_message_dialog_is_open_answer;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_show
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *message, menu_msg_level_e level)
{
    (void) connection;
    (void) surface;
    (void) config;
    (void) message;
    (void) level;
    s_call_message_dialog_show++;
}


/** spawn_command stand-in's call counter, last-seen options (with
 *  its two string members copied into fixed local buffers below
 *  rather than the caller's own pointers kept as-is, since those
 *  point into 'desktop_action_process_launch_with_class' 's own
 *  stack frame, gone by the time a scenario reads them back), and
 *  canned answer */
static int s_call_spawn_command;
static spawn_opts_td s_last_spawn_opts;
static char s_last_spawn_startup_id[128];
static char s_last_spawn_class_name[128];
static int s_spawn_command_answer;
static pid_t s_spawn_command_pid_answer = 4242;


/**
 * @brief Test-controlled stand-in for @a spawn_command
 * @note Complexity: @e O(1)
 */
int spawn_command(const char *command, const spawn_opts_td *opts,
        pid_t *out_pid)
{
    (void) command;

    s_call_spawn_command++;
    s_last_spawn_startup_id[0] = '\0';
    s_last_spawn_class_name[0] = '\0';
    if (opts != NULL) {
        s_last_spawn_opts = *opts;
        if (opts->startup_id != NULL) {
            (void) safe_strncpy(s_last_spawn_startup_id,
                    opts->startup_id, sizeof(s_last_spawn_startup_id));
            s_last_spawn_opts.startup_id = s_last_spawn_startup_id;
        }
        if (opts->class_name != NULL) {
            (void) safe_strncpy(s_last_spawn_class_name,
                    opts->class_name, sizeof(s_last_spawn_class_name));
            s_last_spawn_opts.class_name = s_last_spawn_class_name;
        }
    } else {
        memset(&s_last_spawn_opts, 0, sizeof(s_last_spawn_opts));
    }

    if (s_spawn_command_answer == 0 && out_pid != NULL) {
        *out_pid = s_spawn_command_pid_answer;
    }
    return s_spawn_command_answer;
}


/** Call counter for the client_destroy stand-in */
static int s_call_client_destroy;


/**
 * @brief Recording stand-in for @a client_destroy
 *
 * Only ever reached by dclient.c's own stale-ghost-window branch in
 * @a desktop_action_client_add, itself only reachable when
 * @a xcb_connection_get returns non-NULL; with 's_connection_is_live'
 * left @c false, as it is for every scenario that never sets it, this
 * stand-in is link-only and never actually runs, following the same
 * precedent already set by 'tests/desktop/test_workarea.c' for this
 * exact function.
 *
 * @note Complexity: @e O(1)
 */
void client_destroy(client_td *client)
{
    (void) client;
    s_call_client_destroy++;
}


/** Fixed seeds mirroring 'src/desktop.c' 's own two private hash
 *  functions for this exact table, repeated here rather than shared,
 *  since neither one is exported and there is no common test-support
 *  translation unit this project's own tests link against */
#define S_HASH_SEED_PRIMARY (0x9E3779B9u)
#define S_HASH_SEED_SECONDARY (0x85EBCA6Bu)


/**
 * @brief Hash a client by its own identifier
 *
 * Deliberately keyed on 'client->id', exactly as the real
 * production table in 'src/desktop.c' hashes clients, rather than on
 * a client's own address: 'desktop_action_client_add' 's stale-
 * ghost-window branch looks up a stack-local 'client_td' holding
 * only a matching 'id', so only an identifier-based table, not an
 * address-based one, can ever exercise that branch faithfully.
 *
 * @note Complexity: @e O(1)
 */
static size_t s_client_hash1(const void *key)
{
    const client_td *client = (const client_td *) key;
    const uint32_t id = client->id;

    return (size_t) murmurhash3_32(&id, (int) sizeof(id),
            S_HASH_SEED_PRIMARY);
}


/**
 * @brief Second hash for the open-addressed table, by identifier
 * @note Complexity: @e O(1)
 */
static size_t s_client_hash2(const void *key)
{
    const client_td *client = (const client_td *) key;
    const uint32_t id = client->id;
    size_t hash2 = (size_t) murmurhash3_32(&id, (int) sizeof(id),
            S_HASH_SEED_SECONDARY);

    return (hash2 == 0u) ? 1u : hash2;
}


/**
 * @brief Whether two table entries are the same logical client, by
 *        identifier
 * @note Complexity: @e O(1)
 */
static bool s_client_match(const void *key1, const void *key2)
{
    const client_td *client1 = (const client_td *) key1;
    const client_td *client2 = (const client_td *) key2;

    return client1->id == client2->id;
}


/** Shared desktop fixtures, rebuilt fresh by s_reset before every
 *  scenario */
static desktop_td s_desktop_a;
static desktop_td s_desktop_b;
static config_td s_config;
static surface_td s_surface;


/**
 * @brief Give a desktop a real, empty client table and stacking order
 * @note Complexity: @e O(1)
 */
static void s_make_desktop(desktop_td *desktop, uint32_t id,
        const char *name)
{
    memset(desktop, 0, sizeof(*desktop));
    desktop->id = id;
    (void) safe_strncpy(desktop->name, name, sizeof(desktop->name));
    desktop->config = &s_config;
    desktop->clients = ohtbl_init(8, 8, s_client_hash1, s_client_hash2,
            s_client_match, NULL);
    (void) stacking_create(desktop);
}


/**
 * @brief Tear down a desktop fixture built by s_make_desktop
 * @note Complexity: @e O(n), where @e n is the number of clients still
 *       registered on @p desktop
 */
static void s_destroy_desktop(desktop_td *desktop)
{
    stacking_destroy(desktop);
    ohtbl_destroy(desktop->clients);
}


/**
 * @brief Build a zeroed client with the given ID and name, ready to
 *        be added to a desktop
 * @note Complexity: @e O(1)
 */
static void s_make_client(client_td *client, xcb_window_t id,
        const char *name)
{
    memset(client, 0, sizeof(*client));
    client->id = id;
    client->window = id;
    client->info.name = (char *) name;
}


/**
 * @brief Reset every recording stand-in, canned answer, and shared
 *        desktop fixture between scenarios
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_connection_is_live = false;
    s_probed_window_exists = false;
    s_call_get_window_attributes = 0;
    s_call_enforce_layers = 0;
    s_last_enforce_layers_desktop = NULL;
    s_group_transient_anchor_answer = NULL;
    s_call_enact_iconify = 0;
    s_call_enact_restore = 0;
    s_last_enact_client = NULL;
    s_call_sn_begin = 0;
    s_call_sn_cancel = 0;
    s_sn_begin_answer = true;
    (void) safe_strncpy(s_sn_begin_id, "startup-id-42",
            sizeof(s_sn_begin_id));
    s_wm_desktop_surface_answer = NULL;
    s_wm_config_answer = NULL;
    s_wm_surfaces_answer = NULL;
    s_call_message_dialog_show = 0;
    s_message_dialog_is_open_answer = false;
    s_viewport_has_pages = false;
    s_client_page_col = 0u;
    s_client_page_row = 0u;
    s_shown_page_col = 0u;
    s_shown_page_row = 0u;
    s_call_spawn_command = 0;
    memset(&s_last_spawn_opts, 0, sizeof(s_last_spawn_opts));
    s_last_spawn_startup_id[0] = '\0';
    s_last_spawn_class_name[0] = '\0';
    s_spawn_command_answer = 0;
    s_spawn_command_pid_answer = 4242;

    memset(&s_config, 0, sizeof(s_config));
    memset(&s_surface, 0, sizeof(s_surface));

    s_make_desktop(&s_desktop_a, 1u, "Alpha");
    s_make_desktop(&s_desktop_b, 2u, "Beta");
}


/* Adding a valid client inserts it into both the hash table and the
 * stacking order, and marks the desktop outdated for redraw */
static void s_test_add_inserts_into_table_and_stacking(void)
{
    client_td client;
    void *found;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");
    s_desktop_a.is_outdated = false;

    rc = desktop_action_client_add(&s_desktop_a, &client);

    TAP_EQ_INT(rc, 0, "adding a fresh client succeeds");
    found = &client;
    TAP_EQ_INT(ohtbl_lookup(s_desktop_a.clients, &found), 0,
            "the client is now found in the hash table");
    TAP_EQ_INT((long) stacking_count(&s_desktop_a), 1,
            "and the stacking order now holds exactly one client");
    TAP_OK(s_desktop_a.is_outdated,
            "adding a client marks the desktop outdated for redraw");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A NULL desktop or client is rejected without touching anything */
static void s_test_add_null_guards(void)
{
    client_td client;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");

    rc = desktop_action_client_add(NULL, &client);
    TAP_EQ_INT(rc, -1, "adding to a NULL desktop fails");

    rc = desktop_action_client_add(&s_desktop_a, NULL);
    TAP_EQ_INT(rc, -1, "adding a NULL client fails");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A stale ghost-window entry already occupying a client's key is
 * detected (no live XCB connection means the check is skipped, and
 * so is always treated as "still exists") and left in place when the
 * connection cannot be consulted */
static void s_test_add_existing_key_without_connection_keeps_entry(void)
{
    client_td first;
    client_td second;
    void *found;
    int rc;

    s_reset();
    s_connection_is_live = false;
    s_make_client(&first, 100u, "first");
    s_make_client(&second, 100u, "second");

    TAP_EQ_INT(desktop_action_client_add(&s_desktop_a, &first), 0,
            "the first client with this ID is added normally");

    rc = desktop_action_client_add(&s_desktop_a, &second);
    TAP_EQ_INT(rc, -1,
            "a second client reusing the same ID, with no live" \
            " connection to check staleness, fails to insert" \
            " (ohtbl already holds that exact key)");
    found = &first;
    TAP_EQ_INT(ohtbl_lookup(s_desktop_a.clients, &found), 0,
            "the original entry is still the one found afterward");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A stale ghost-window entry, this time genuinely confirmed gone via
 * a live connection's window-attributes probe, is removed and the new
 * client takes its place */
static void s_test_add_replaces_stale_ghost_entry(void)
{
    client_td stale;
    client_td fresh;
    void *found;
    int rc;

    s_reset();
    s_connection_is_live = true;
    s_probed_window_exists = false;
    s_make_client(&stale, 100u, "stale");
    s_make_client(&fresh, 100u, "fresh");

    TAP_EQ_INT(desktop_action_client_add(&s_desktop_a, &stale), 0,
            "the stale client is added first, as if it were" \
            " ordinary at the time");

    rc = desktop_action_client_add(&s_desktop_a, &fresh);
    TAP_EQ_INT(rc, 0,
            "a second client reusing the same ID, once the first" \
            " one's window is confirmed gone, is inserted" \
            " successfully");
    found = &fresh;
    TAP_EQ_INT(ohtbl_lookup(s_desktop_a.clients, &found), 0,
            "the hash table now finds the fresh client");
    TAP_OK(found == (void *) &fresh,
            "and it is genuinely the fresh client, not the stale" \
            " one still occupying that key");

    /* desktop_action_client_rem, called internally on the stale entry
     * above, deliberately never touches stacking (see its own test
     * below); that leftover stacking entry, still pointing at this
     * function's own soon-to-vanish 'stale' local, is cleaned up
     * explicitly here, exactly as a real caller destroying a client
     * would do through the stacking policy layer, rather than left
     * to dangle past this function's own return */
    (void) stacking_remove(&stale);

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Removing a client takes it out of the hash table, but deliberately
 * leaves the stacking order untouched */
static void s_test_rem_removes_from_table_only(void)
{
    client_td client;
    void *found;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");
    (void) desktop_action_client_add(&s_desktop_a, &client);

    rc = desktop_action_client_rem(&s_desktop_a, &client);

    TAP_EQ_INT(rc, 0, "removing a present client succeeds");
    found = &client;
    TAP_EQ_INT(ohtbl_lookup(s_desktop_a.clients, &found), -1,
            "the client is no longer found in the hash table");
    /* stacking_count itself cross-references each raw entry against
     * this same hash table to decide whether it still belongs to
     * 'desktop', so it now reports zero: the client removed from the
     * table above no longer counts as this desktop's, even while its
     * raw entry is still physically in the stacking list.
     * stacking_bottom, which walks that raw list with no such
     * cross-reference at all, is what actually proves the entry
     * itself was never touched */
    TAP_EQ_INT((long) stacking_count(&s_desktop_a), 0,
            "stacking_count no longer attributes the removed" \
            " client to this desktop, since it cross-references" \
            " the very hash table just emptied of it");
    TAP_OK(stacking_bottom() == &client,
            "but the raw stacking list itself still physically" \
            " holds this exact client: removal from the table is" \
            " deliberately not removal from stacking");

    /* Left in stacking by design (see the assertion just above); this
     * function's own local 'client' outlives that leftover entry only
     * by explicitly cleaning it up here through the stacking policy
     * layer before returning */
    (void) stacking_remove(&client);

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Removing a client not actually in the hash table fails */
static void s_test_rem_missing_client_fails(void)
{
    client_td client;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");

    rc = desktop_action_client_rem(&s_desktop_a, &client);
    TAP_EQ_INT(rc, -1,
            "removing a client never added to this desktop's table" \
            " fails");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Moving to the same desktop is a no-op that reports success without
 * touching either table */
static void s_test_move_same_desktop_is_noop(void)
{
    client_td client;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");
    (void) desktop_action_client_add(&s_desktop_a, &client);
    client.desktop_id = s_desktop_a.id;

    rc = desktop_action_client_move(&s_desktop_a, &s_desktop_a, &client);

    TAP_EQ_INT(rc, 0, "moving a client to its own current desktop" \
            " succeeds trivially");
    TAP_EQ_INT((long) stacking_count(&s_desktop_a), 1,
            "and its single stacking entry is neither duplicated" \
            " nor removed");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A successful move removes the client from the source desktop's
 * table and adds it to the destination's, updating desktop_id */
static void s_test_move_success(void)
{
    client_td client;
    void *found;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");
    (void) desktop_action_client_add(&s_desktop_a, &client);
    client.desktop_id = s_desktop_a.id;

    rc = desktop_action_client_move(&s_desktop_a, &s_desktop_b, &client);

    TAP_EQ_INT(rc, 0, "moving to a different desktop succeeds");
    found = &client;
    TAP_EQ_INT(ohtbl_lookup(s_desktop_a.clients, &found), -1,
            "the client is no longer in the source desktop's table");
    found = &client;
    TAP_EQ_INT(ohtbl_lookup(s_desktop_b.clients, &found), 0,
            "and is now in the destination desktop's table");
    TAP_EQ_INT((long) client.desktop_id, (long) s_desktop_b.id,
            "the client's own desktop_id field follows the move");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A move whose destination insert fails (simulated by an ID already
 * occupied on the destination, with no live connection to clear it)
 * rolls back onto the original desktop rather than leaving the
 * client homeless */
static void s_test_move_failure_rolls_back(void)
{
    client_td moving;
    client_td blocker;
    void *found;
    int rc;

    s_reset();
    s_connection_is_live = false;
    s_make_client(&moving, 100u, "moving");
    s_make_client(&blocker, 100u, "blocker");
    (void) desktop_action_client_add(&s_desktop_a, &moving);
    moving.desktop_id = s_desktop_a.id;
    (void) desktop_action_client_add(&s_desktop_b, &blocker);

    rc = desktop_action_client_move(&s_desktop_a, &s_desktop_b,
            &moving);

    TAP_EQ_INT(rc, 1,
            "a move whose destination insert fails reports failure" \
            " (1), distinct from an invalid-argument failure (-1)");
    found = &moving;
    TAP_EQ_INT(ohtbl_lookup(s_desktop_a.clients, &found), 0,
            "the client is put back into the source desktop's" \
            " table after the failed move");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A NULL destination or client is rejected outright; moving from a
 * NULL source (a client with no current desktop yet) is a plain add */
static void s_test_move_null_guards(void)
{
    client_td client;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");

    rc = desktop_action_client_move(&s_desktop_a, NULL, &client);
    TAP_EQ_INT(rc, -1, "moving to a NULL destination fails");

    rc = desktop_action_client_move(&s_desktop_a, &s_desktop_b, NULL);
    TAP_EQ_INT(rc, -1, "moving a NULL client fails");

    rc = desktop_action_client_move(NULL, &s_desktop_b, &client);
    TAP_EQ_INT(rc, 0,
            "moving from a NULL source desktop onto a real" \
            " destination succeeds as a plain add");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* No urgent client on the desktop leaves is_urgent false and never
 * pops a notification dialog */
static void s_test_recompute_urgent_none_found(void)
{
    client_td client;

    s_reset();
    s_make_client(&client, 100u, "term");
    (void) desktop_action_client_add(&s_desktop_a, &client);
    s_desktop_a.is_urgent = false;

    desktop_action_recompute_urgent(&s_desktop_a);

    TAP_OK(!s_desktop_a.is_urgent,
            "with no urgent client, is_urgent stays false");
    TAP_EQ_INT(s_call_message_dialog_show, 0,
            "and no notification dialog is ever shown");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* The false-to-true urgency transition, on a desktop other than the
 * one currently shown, with notify_activity enabled and no dialog
 * already open, pops the activity-notification dialog */
static void s_test_recompute_urgent_transition_notifies(void)
{
    client_td client;

    s_reset();
    s_make_client(&client, 100u, "term");
    client.properties.flags |= CLIENT_FLAG_URGENT;
    (void) desktop_action_client_add(&s_desktop_a, &client);
    s_desktop_a.is_urgent = false;

    s_surface.desktop_cur = s_desktop_b.id; /* a different desktop is
                                                currently shown */
    s_config.base.urgency.notify_activity = true;
    s_wm_desktop_surface_answer = &s_surface;
    s_wm_config_answer = &s_config;
    s_message_dialog_is_open_answer = false;

    desktop_action_recompute_urgent(&s_desktop_a);

    TAP_OK(s_desktop_a.is_urgent,
            "a client with the urgency flag set makes is_urgent" \
            " become true");
    TAP_EQ_INT(s_call_message_dialog_show, 1,
            "the false-to-true transition, on a desktop other than" \
            " the one currently shown, with notifications enabled" \
            " and no dialog already open, shows exactly one" \
            " notification");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* On the very desktop being shown, but on a viewport page that is
 * not, the notice does appear: the titlebar blink that covers the
 * same-desktop case is off screen along with the window */
static void s_test_recompute_urgent_other_page_notifies(void)
{
    client_td client;

    s_reset();
    s_make_client(&client, 100u, "term");
    client.properties.flags |= CLIENT_FLAG_URGENT;
    (void) desktop_action_client_add(&s_desktop_a, &client);
    s_desktop_a.is_urgent = false;

    s_surface.desktop_cur = s_desktop_a.id;
    s_viewport_has_pages = true;
    s_client_page_col = 1u;
    s_shown_page_col = 0u;
    s_config.base.urgency.notify_activity = true;
    s_wm_desktop_surface_answer = &s_surface;
    s_wm_config_answer = &s_config;

    desktop_action_recompute_urgent(&s_desktop_a);

    TAP_EQ_INT(s_call_message_dialog_show, 1,
            "a client urgent on another page of the desktop being"
            " shown does raise the notice");
    TAP_EQ_INT((int) s_desktop_a.urgent_page.x, 1,
            "and the page it came from is recorded");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Urgency moving from one page to another is a fresh request even
 * though 'is_urgent' never went back to false in between */
static void s_test_recompute_urgent_page_change_notifies_again(void)
{
    client_td client;

    s_reset();
    s_make_client(&client, 100u, "term");
    client.properties.flags |= CLIENT_FLAG_URGENT;
    (void) desktop_action_client_add(&s_desktop_a, &client);
    s_desktop_a.is_urgent = false;
    s_desktop_a.has_urgent_page = false;

    s_surface.desktop_cur = s_desktop_a.id;
    s_viewport_has_pages = true;
    s_shown_page_col = 0u;
    s_config.base.urgency.notify_activity = true;
    s_wm_desktop_surface_answer = &s_surface;
    s_wm_config_answer = &s_config;

    s_client_page_col = 1u;
    desktop_action_recompute_urgent(&s_desktop_a);
    TAP_EQ_INT(s_call_message_dialog_show, 1,
            "the first request on page 1 is announced");

    desktop_action_recompute_urgent(&s_desktop_a);
    TAP_EQ_INT(s_call_message_dialog_show, 1,
            "recomputing with nothing changed announces nothing"
            " further");

    s_client_page_col = 2u;
    desktop_action_recompute_urgent(&s_desktop_a);
    TAP_EQ_INT(s_call_message_dialog_show, 2,
            "but the request moving to page 2 is announced again,"
            " which is_urgent alone could never tell apart");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* The same transition, but on the desktop currently shown on its own
 * surface, never pops a dialog: that case already gets its titlebar
 * blink elsewhere */
static void s_test_recompute_urgent_current_desktop_no_dialog(void)
{
    client_td client;

    s_reset();
    s_make_client(&client, 100u, "term");
    client.properties.flags |= CLIENT_FLAG_URGENT;
    (void) desktop_action_client_add(&s_desktop_a, &client);
    s_desktop_a.is_urgent = false;

    s_surface.desktop_cur = s_desktop_a.id; /* this desktop itself is
                                                the one shown */
    s_config.base.urgency.notify_activity = true;
    s_wm_desktop_surface_answer = &s_surface;
    s_wm_config_answer = &s_config;

    desktop_action_recompute_urgent(&s_desktop_a);

    TAP_OK(s_desktop_a.is_urgent, "is_urgent still becomes true");
    TAP_EQ_INT(s_call_message_dialog_show, 0,
            "but no dialog is shown for the desktop currently" \
            " visible on its own surface");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Already-urgent stays urgent on a later call with no new urgent
 * client added: not a false-to-true transition, so still no dialog */
static void s_test_recompute_urgent_already_true_no_dialog(void)
{
    client_td client;

    s_reset();
    s_make_client(&client, 100u, "term");
    client.properties.flags |= CLIENT_FLAG_URGENT;
    (void) desktop_action_client_add(&s_desktop_a, &client);
    s_desktop_a.is_urgent = true; /* already urgent before this call */

    s_surface.desktop_cur = s_desktop_b.id;
    s_config.base.urgency.notify_activity = true;
    s_wm_desktop_surface_answer = &s_surface;
    s_wm_config_answer = &s_config;

    desktop_action_recompute_urgent(&s_desktop_a);

    TAP_EQ_INT(s_call_message_dialog_show, 0,
            "no dialog is shown when the desktop was already" \
            " urgent before this call: only the false-to-true" \
            " transition notifies");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A NULL desktop, or one with no client table, is a safe no-op */
static void s_test_recompute_urgent_null_guards(void)
{
    desktop_td no_table;

    s_reset();
    memset(&no_table, 0, sizeof(no_table));
    no_table.clients = NULL;

    desktop_action_recompute_urgent(NULL);
    TAP_OK(true, "a NULL desktop is a safe no-op, no crash");

    desktop_action_recompute_urgent(&no_table);
    TAP_OK(true, "a desktop with no client table is a safe no-op");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Sending a client to the front raises it and enforces layers
 * afterward; with no transients registered, no extra raise happens */
static void s_test_send_front_raises_and_enforces_layers(void)
{
    client_td top;
    client_td bottom;
    int rc;

    s_reset();
    s_make_client(&bottom, 100u, "bottom");
    s_make_client(&top, 200u, "top");
    (void) desktop_action_client_add(&s_desktop_a, &bottom);
    (void) desktop_action_client_add(&s_desktop_a, &top);

    rc = desktop_action_client_send_front(&s_desktop_a, &bottom);

    TAP_EQ_INT(rc, 0, "sending the bottom client to the front" \
            " succeeds");
    TAP_EQ_INT(s_call_enforce_layers, 1,
            "layer enforcement runs exactly once for the move");
    TAP_OK(s_last_enforce_layers_desktop == &s_desktop_a,
            "enforcing layers on the same desktop the client" \
            " actually moved on");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Sending to front also raises every transient descendant of the
 * client, walking the real transients list */
static void s_test_send_front_raises_transients_too(void)
{
    client_td parent;
    client_td dialog;
    cdlist_td *transients;
    int rc;

    s_reset();
    s_make_client(&parent, 100u, "parent");
    s_make_client(&dialog, 200u, "dialog");
    (void) desktop_action_client_add(&s_desktop_a, &parent);
    (void) desktop_action_client_add(&s_desktop_a, &dialog);
    dialog.desktop_id = s_desktop_a.id;

    transients = cdlist_init(NULL);
    (void) cdlist_ins_next(transients, NULL, &dialog);
    parent.transients = transients;

    rc = desktop_action_client_send_front(&s_desktop_a, &parent);

    TAP_EQ_INT(rc, 0, "sending the parent to the front succeeds");
    TAP_EQ_INT(s_call_enforce_layers, 2,
            "layer enforcement runs once for the parent and once" \
            " more for its transient dialog");

    cdlist_destroy(transients);
    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Sending to the back calls the plain lower path, with no transient
 * walk at all */
static void s_test_send_back_lowers_only(void)
{
    client_td client;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");
    (void) desktop_action_client_add(&s_desktop_a, &client);

    rc = desktop_action_client_send_back(&s_desktop_a, &client);

    TAP_EQ_INT(rc, 0, "sending a client to the back succeeds");
    TAP_EQ_INT(s_call_enforce_layers, 1,
            "layer enforcement still runs exactly once for the" \
            " move, the same as sending to the front");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Sending to front or back with a client not actually on this
 * desktop's stacking order fails, without enforcing layers */
static void s_test_send_front_missing_client_fails(void)
{
    client_td client;
    int rc;

    s_reset();
    s_make_client(&client, 100u, "term");
    /* Deliberately never added to s_desktop_a's stacking order */

    rc = desktop_action_client_send_front(&s_desktop_a, &client);

    TAP_EQ_INT(rc, -1,
            "sending a client never placed on this desktop's" \
            " stacking order to the front fails");
    TAP_EQ_INT(s_call_enforce_layers, 0,
            "and layer enforcement never runs for a failed move");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Iconifying every client on a desktop reaches each one through the
 * real hash table, regardless of order */
static void s_test_iconify_all_reaches_every_client(void)
{
    client_td first;
    client_td second;
    int rc;

    s_reset();
    s_make_client(&first, 100u, "first");
    s_make_client(&second, 200u, "second");
    (void) desktop_action_client_add(&s_desktop_a, &first);
    (void) desktop_action_client_add(&s_desktop_a, &second);

    rc = desktop_action_clients_iconify_all(&s_desktop_a);

    TAP_EQ_INT(rc, 0, "iconifying all clients reports success");
    TAP_EQ_INT(s_call_enact_iconify, 2,
            "both clients on the desktop are individually iconified");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A NULL desktop is rejected for iconify-all */
static void s_test_iconify_all_null_guard(void)
{
    int rc;

    s_reset();

    rc = desktop_action_clients_iconify_all(NULL);
    TAP_EQ_INT(rc, -1, "iconifying all clients on a NULL desktop" \
            " fails");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Restoring only touches clients actually iconified, leaving every
 * other client's state untouched */
static void s_test_deiconify_all_restores_only_iconified(void)
{
    client_td iconified;
    client_td normal;
    int rc;

    s_reset();
    s_make_client(&iconified, 100u, "iconified");
    iconified.properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    s_make_client(&normal, 200u, "normal");
    (void) desktop_action_client_add(&s_desktop_a, &iconified);
    (void) desktop_action_client_add(&s_desktop_a, &normal);

    rc = desktop_action_clients_deiconify_all(&s_desktop_a);

    TAP_EQ_INT(rc, 0, "restoring all iconified clients reports" \
            " success");
    TAP_EQ_INT(s_call_enact_restore, 1,
            "only the one genuinely iconified client is restored");
    TAP_OK(s_last_enact_client == &iconified,
            "and it is specifically the iconified client, not the" \
            " normal one");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A NULL desktop is rejected for deiconify-all */
static void s_test_deiconify_all_null_guard(void)
{
    int rc;

    s_reset();

    rc = desktop_action_clients_deiconify_all(NULL);
    TAP_EQ_INT(rc, -1, "restoring all clients on a NULL desktop" \
            " fails");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* Launching a process with no live connection skips startup
 * notification entirely, and still spawns successfully */
static void s_test_launch_without_connection_skips_sn(void)
{
    int rc;

    s_reset();
    s_connection_is_live = false;
    s_config.base.startup_notification.is_enabled = true;
    s_spawn_command_answer = 0;

    rc = desktop_action_process_launch(&s_desktop_a, "xterm");

    TAP_EQ_INT(rc, 0, "launching with no live connection still" \
            " succeeds");
    TAP_EQ_INT(s_call_sn_begin, 0,
            "startup notification is never begun without a live" \
            " connection, regardless of the config setting");
    TAP_EQ_INT(s_call_spawn_command, 1,
            "the command is spawned exactly once");
    TAP_OK(s_last_spawn_opts.startup_id == NULL,
            "and is handed no startup ID, since none was begun");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* With a live connection and startup notification enabled, a
 * successful launch is handed the generated startup ID */
static void s_test_launch_with_sn_enabled_passes_startup_id(void)
{
    int rc;

    s_reset();
    s_connection_is_live = true;
    s_config.base.startup_notification.is_enabled = true;
    s_sn_begin_answer = true;
    s_spawn_command_answer = 0;

    rc = desktop_action_process_launch(&s_desktop_a, "xterm");

    TAP_EQ_INT(rc, 0, "launching with startup notification enabled" \
            " succeeds");
    TAP_EQ_INT(s_call_sn_begin, 1,
            "startup notification is begun exactly once");
    TAP_OK(s_last_spawn_opts.startup_id != NULL &&
            strcmp(s_last_spawn_opts.startup_id, s_sn_begin_id) == 0,
            "the spawned command is handed the generated startup ID");
    TAP_EQ_INT(s_call_sn_cancel, 0,
            "a successful spawn never cancels the sequence it just" \
            " began");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* When spawning itself fails after a startup-notification sequence
 * was begun, that sequence is canceled rather than left pending */
static void s_test_launch_spawn_failure_cancels_sn(void)
{
    int rc;

    s_reset();
    s_connection_is_live = true;
    s_config.base.startup_notification.is_enabled = true;
    s_sn_begin_answer = true;
    s_spawn_command_answer = -2;

    rc = desktop_action_process_launch(&s_desktop_a, "xterm");

    TAP_EQ_INT(rc, -2,
            "a failed spawn propagates spawn_command's own error" \
            " code");
    TAP_EQ_INT(s_call_sn_cancel, 1,
            "the startup-notification sequence begun just before" \
            " the failed spawn is canceled rather than left" \
            " pending forever");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* With startup notification disabled in config, no sequence is ever
 * begun even with a live connection */
static void s_test_launch_sn_disabled_never_begins(void)
{
    int rc;

    s_reset();
    s_connection_is_live = true;
    s_config.base.startup_notification.is_enabled = false;
    s_spawn_command_answer = 0;

    rc = desktop_action_process_launch(&s_desktop_a, "xterm");

    TAP_EQ_INT(rc, 0, "launching with startup notification" \
            " disabled still succeeds");
    TAP_EQ_INT(s_call_sn_begin, 0,
            "no sequence is begun when the config disables it,"
            " regardless of the connection being live");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* desktop_action_process_launch_with_class forwards a caller-supplied
 * class name to spawn_command's own options, and reports the spawned
 * PID back out */
static void s_test_launch_with_class_forwards_class_and_pid(void)
{
    pid_t out_pid = 0;
    int rc;

    s_reset();
    s_connection_is_live = false;
    s_spawn_command_answer = 0;
    s_spawn_command_pid_answer = 9999;

    rc = desktop_action_process_launch_with_class(&s_desktop_a,
            "xterm", "Xterm", &out_pid);

    TAP_EQ_INT(rc, 0, "launching with an explicit class succeeds");
    TAP_OK(s_last_spawn_opts.class_name != NULL &&
            strcmp(s_last_spawn_opts.class_name, "Xterm") == 0,
            "the caller-supplied class name is forwarded to" \
            " spawn_command unchanged");
    TAP_EQ_INT((long) out_pid, 9999,
            "the spawned PID is written back through out_pid on" \
            " success");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


/* A NULL or empty executable path, or a NULL desktop, is rejected
 * without ever spawning anything */
static void s_test_launch_null_and_empty_guards(void)
{
    int rc;

    s_reset();

    rc = desktop_action_process_launch(NULL, "xterm");
    TAP_EQ_INT(rc, -1, "launching on a NULL desktop fails");

    rc = desktop_action_process_launch(&s_desktop_a, NULL);
    TAP_EQ_INT(rc, -1, "launching a NULL executable path fails");

    rc = desktop_action_process_launch(&s_desktop_a, "");
    TAP_EQ_INT(rc, -1, "launching an empty executable path fails");

    TAP_EQ_INT(s_call_spawn_command, 0,
            "none of these three rejected calls ever reaches" \
            " spawn_command");

    s_destroy_desktop(&s_desktop_a);
    s_destroy_desktop(&s_desktop_b);
}


int main(void)
{
    TAP_PLAN(78);

    s_test_add_inserts_into_table_and_stacking();
    s_test_add_null_guards();
    s_test_add_existing_key_without_connection_keeps_entry();
    s_test_add_replaces_stale_ghost_entry();
    s_test_rem_removes_from_table_only();
    s_test_rem_missing_client_fails();
    s_test_move_same_desktop_is_noop();
    s_test_move_success();
    s_test_move_failure_rolls_back();
    s_test_move_null_guards();
    s_test_recompute_urgent_none_found();
    s_test_recompute_urgent_transition_notifies();
    s_test_recompute_urgent_other_page_notifies();
    s_test_recompute_urgent_page_change_notifies_again();
    s_test_recompute_urgent_current_desktop_no_dialog();
    s_test_recompute_urgent_already_true_no_dialog();
    s_test_recompute_urgent_null_guards();
    s_test_send_front_raises_and_enforces_layers();
    s_test_send_front_raises_transients_too();
    s_test_send_back_lowers_only();
    s_test_send_front_missing_client_fails();
    s_test_iconify_all_reaches_every_client();
    s_test_iconify_all_null_guard();
    s_test_deiconify_all_restores_only_iconified();
    s_test_deiconify_all_null_guard();
    s_test_launch_without_connection_skips_sn();
    s_test_launch_with_sn_enabled_passes_startup_id();
    s_test_launch_spawn_failure_cancels_sn();
    s_test_launch_sn_disabled_never_begins();
    s_test_launch_with_class_forwards_class_and_pid();
    s_test_launch_null_and_empty_guards();

    return TAP_DONE();
}
