/**
 * @file tests/cmds/client/test_icon.c
 *
 * @brief Test battery for an iconified client's icon window creation,
 *        positioning, and slot-conflict detection
 *
 * Exercises 'ccmd_client_relocate_icon_if_taken' and
 * 'ccmd_client_ensure_icon_window' (cmds/client/icon.c) linked
 * against the real 'policy/stacking.c', 'adt/cdlist.c', and
 * 'adt/ohtbl.c', so the overlap search 'ccmd_client_relocate_icon_
 * if_taken' runs over a genuine stacking order actually walks real
 * client positions rather than a canned answer.  Everything else the
 * file under test reaches, monitor/screen resolution, the configured
 * placement policy, systray geometry, and the X server itself, is a
 * link-only or recording/test-controlled stand-in, so what is
 * actually exercised here is this file's own guard clauses, slot-
 * overlap detection, and create-versus-reposition branching, not the
 * placement policy or the systray's own logic (each already covered
 * on its own elsewhere).
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
#include <cmds/client/icon.h>
#include <config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <monitor.h>
#include <policy/stacking.h>
#include <systray.h>


/**
 * @brief Test-controlled stand-in for @a ccmd_client_monitor
 *
 * Answers @c false by default, sending every caller to
 * @a ccmd_screen_dim's fallback instead; a test opts into a resolved
 * monitor by setting @a s_monitor_available and @a s_monitor.
 *
 * @note Complexity: @e O(1)
 */
static bool s_monitor_available;
static monitor_td s_monitor;
static stage_td *s_monitor_stage;

bool ccmd_client_monitor(client_td *client, stage_td **out_stage,
        monitor_td *out_monitor)
{
    (void) client;

    if (!s_monitor_available) {
        return false;
    }
    if (out_stage != NULL) {
        *out_stage = s_monitor_stage;
    }
    if (out_monitor != NULL) {
        *out_monitor = s_monitor;
    }
    return true;
}


/**
 * @brief Test-controlled stand-in for @a ccmd_screen_dim
 *
 * Reached only when @a ccmd_client_monitor above answers @c false;
 * answers a fixed 1024x768 unless a test overrides it.
 *
 * @note Complexity: @e O(1)
 */
static bool s_screen_dim_available = true;
static uint16_t s_screen_w = 1024u;
static uint16_t s_screen_h = 768u;

bool ccmd_screen_dim(const client_td *client,
        uint16_t *restrict out_w, uint16_t *restrict out_h)
{
    (void) client;

    if (!s_screen_dim_available) {
        return false;
    }
    if (out_w != NULL) {
        *out_w = s_screen_w;
    }
    if (out_h != NULL) {
        *out_h = s_screen_h;
    }
    return true;
}


/**
 * @brief Link-only stand-in for @a systray_get_reserved_strut
 *
 * Answers @c NULL unconditionally, the same as a stage with no tray
 * reservation: none of these tests exercise the margin-shrink branch
 * that reads a real strut, only the plain monitor/screen sizing path.
 *
 * @note Complexity: @e O(1)
 */
const struct strut_partial_s *systray_get_reserved_strut(
        const stage_td *stage)
{
    (void) stage;
    return NULL;
}


/**
 * @brief Test-controlled stand-in for @a systray_get_geometry
 *
 * Answers @c false by default, matching a stage with no tray
 * mapped, which keeps @a s_icon_tray_avoid a no-op; a test opts into
 * a docked tray by setting @a s_tray_present and @a s_tray_geometry.
 *
 * @note Complexity: @e O(1)
 */
static bool s_tray_present;
static struct geometry_s s_tray_geometry;

bool systray_get_geometry(const stage_td *stage,
        struct geometry_s *restrict out_tray)
{
    (void) stage;

    if (!s_tray_present) {
        return false;
    }
    if (out_tray != NULL) {
        *out_tray = s_tray_geometry;
    }
    return true;
}


/**
 * @brief Recording, test-controlled stand-in for @a place_icon_apply
 *
 * Answers a fixed, test-controlled position regardless of the policy
 * or anchor handed in, and records how many times it ran: what is
 * under test in this file is whether icon.c's own guard clauses and
 * slot-overlap search call this at all and use its answer correctly,
 * not the placement grid @c policy/placement/icon.c computes on its
 * own (already covered there).
 *
 * @note Complexity: @e O(1)
 */
static int s_place_icon_apply_calls;
static struct position_s s_place_icon_apply_result = { 100, 100 };

void place_icon_apply(const client_td *client, desktop_td *desktop,
        enum config_icon_placement_e policy,
        struct dimensions_s icon_dim,
        struct dimensions_s screen_dim,
        const struct position_s *anchor,
        struct position_s *restrict out_pos)
{
    (void) client;
    (void) desktop;
    (void) policy;
    (void) icon_dim;
    (void) screen_dim;
    (void) anchor;

    s_place_icon_apply_calls++;
    if (out_pos != NULL) {
        *out_pos = s_place_icon_apply_result;
    }
}


/**
 * @brief Recording stand-in for @a place_icon_avoid_systray_overlap
 *
 * Never actually pushes the position (answers @c false unconditionally,
 * leaving @p io_y untouched): every test here that cares about a
 * position's final value goes through a stage with no tray mapped, so
 * @c s_icon_tray_avoid never even reaches this once @c systray_get_
 * geometry above has already answered @c false first.  Kept as a
 * record-only stand-in regardless, for the one test that does map a
 * tray and only checks that this was reached, not what it computed.
 *
 * @note Complexity: @e O(1)
 */
static int s_avoid_systray_calls;

bool place_icon_avoid_systray_overlap(
        const int16_t *restrict io_x, int16_t *restrict io_y,
        struct dimensions_s icon_dim, struct geometry_s tray,
        const struct geometry_s *workarea)
{
    (void) io_x;
    (void) io_y;
    (void) icon_dim;
    (void) tray;
    (void) workarea;

    s_avoid_systray_calls++;
    return false;
}


/**
 * @brief Test-controlled stand-in for @a wm_get_client_desktop
 *
 * Answers a test-controlled desktop, or @c NULL by default: the
 * overlap search in @a s_icon_slot_is_taken (icon.c) treats a client
 * with no owning desktop as never colliding, and
 * @a s_icon_position_choose (icon.c) passes this straight through to
 * @a place_icon_apply, which this file's own stand-in above ignores
 * regardless.
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
 * @brief Recording stand-in for @a ccmd_client_apply_geometry
 * @note Complexity: @e O(1)
 */
static int s_apply_geometry_calls;
static int32_t s_apply_geometry_last_x;
static int32_t s_apply_geometry_last_y;
static xcb_window_t s_apply_geometry_last_target;

void ccmd_client_apply_geometry(client_td *client,
        xcb_window_t target, uint16_t mask,
        int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    (void) client;
    (void) mask;
    (void) w;
    (void) h;
    (void) border_width;

    s_apply_geometry_calls++;
    s_apply_geometry_last_target = target;
    s_apply_geometry_last_x = x;
    s_apply_geometry_last_y = y;
}


/**
 * @brief Test-controlled stand-in for @a xcb_connection_get
 *
 * Non-null by default, so the two functions under test take their
 * "connected" branch unless a test explicitly disconnects.
 *
 * @note Complexity: @e O(1)
 */
static xcb_connection_t *s_connection;

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection;
}


/**
 * @brief Recording stand-in for @a xcb_generate_id
 *
 * Hands out a fresh, always-nonzero identifier per call, the same
 * uniqueness guarantee the real allocator gives, without a live
 * connection to actually ask.
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_next_generated_id = 900u;

uint32_t xcb_generate_id(xcb_connection_t *c)
{
    (void) c;
    s_next_generated_id++;
    return s_next_generated_id;
}


/**
 * @brief Recording stand-in for @a xcb_create_window
 * @note Complexity: @e O(1)
 */
static int s_create_window_calls;
static xcb_window_t s_create_window_last_wid;
static int16_t s_create_window_last_x;
static int16_t s_create_window_last_y;
static uint16_t s_create_window_last_w;
static uint16_t s_create_window_last_h;

xcb_void_cookie_t xcb_create_window(xcb_connection_t *c, uint8_t depth,
        xcb_window_t wid, xcb_window_t parent, int16_t x, int16_t y,
        uint16_t width, uint16_t height, uint16_t border_width,
        uint16_t klass, xcb_visualid_t visual, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) depth;
    (void) parent;
    (void) border_width;
    (void) klass;
    (void) visual;
    (void) value_mask;
    (void) value_list;

    s_create_window_calls++;
    s_create_window_last_wid = wid;
    s_create_window_last_x = x;
    s_create_window_last_y = y;
    s_create_window_last_w = width;
    s_create_window_last_h = height;

    cookie.sequence = 0u;
    return cookie;
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
#define MAX_TEST_CLIENTS (16)
#define MAX_TEST_DESKTOPS (4)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;
static desktop_td *s_owned_desktops[MAX_TEST_DESKTOPS];
static int s_owned_desktops_used;
static config_td *s_owned_configs[MAX_TEST_CLIENTS];
static int s_owned_configs_used;


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


/* A fresh config with every icon-relevant field left at its zero
 * default (uncaptioned, bottom-row placement policy) unless the
 * caller overrides one afterward */
static config_td *s_make_config(void)
{
    config_td *config = calloc(1, sizeof(*config));

    s_owned_configs[s_owned_configs_used] = config;
    s_owned_configs_used++;

    return config;
}


static client_td *s_make_client(uint32_t id, config_td *config)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    client->config = config;
    client->icon_pos.x = -1;
    client->icon_pos.y = -1;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


/* Put a client on a desktop's own table and the shared stacking
 * order */
static void s_desktop_add_client(desktop_td *desktop, client_td *client)
{
    ohtbl_insert(desktop->clients, client);
    (void) stacking_add(desktop, client);
}


static void s_reset(void)
{
    static int s_fake_connection_storage;

    s_monitor_available = false;
    memset(&s_monitor, 0, sizeof(s_monitor));
    s_monitor_stage = NULL;
    s_screen_dim_available = true;
    s_screen_w = 1024u;
    s_screen_h = 768u;
    s_tray_present = false;
    memset(&s_tray_geometry, 0, sizeof(s_tray_geometry));
    s_place_icon_apply_calls = 0;
    s_place_icon_apply_result.x = 100;
    s_place_icon_apply_result.y = 100;
    s_avoid_systray_calls = 0;
    s_owner_desktop = NULL;
    s_apply_geometry_calls = 0;
    s_apply_geometry_last_x = 0;
    s_apply_geometry_last_y = 0;
    s_apply_geometry_last_target = XCB_WINDOW_NONE;
    s_connection = (xcb_connection_t *) &s_fake_connection_storage;
    s_create_window_calls = 0;
    s_create_window_last_wid = XCB_WINDOW_NONE;
    s_create_window_last_x = 0;
    s_create_window_last_y = 0;
    s_create_window_last_w = 0;
    s_create_window_last_h = 0;
}


static void s_teardown(void)
{
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
    for (int i = 0; i < s_owned_configs_used; ++i) {
        free(s_owned_configs[i]);
    }
    s_owned_configs_used = 0;
}


/* A null client, or one with no config attached, is a silent no-op
 * for the relocate entry point */
static void s_test_relocate_null_or_unconfigured_is_a_no_op(void)
{
    client_td *unconfigured;

    s_reset();
    unconfigured = s_make_client(1u, NULL);
    unconfigured->icon_window = 7u;
    unconfigured->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;

    ccmd_client_relocate_icon_if_taken(NULL);
    ccmd_client_relocate_icon_if_taken(unconfigured);

    TAP_EQ_INT(s_apply_geometry_calls, 0,
            "neither a null client nor an unconfigured one ever"
            " reaches a geometry update");

    s_teardown();
}


/* A client with no icon window yet, or one that is not currently
 * iconified, is also a no-op: there is nothing mapped to relocate */
static void s_test_relocate_no_icon_or_not_iconified_is_a_no_op(void)
{
    config_td *config;
    client_td *no_icon_window;
    client_td *not_iconified;

    s_reset();
    config = s_make_config();
    no_icon_window = s_make_client(2u, config);
    no_icon_window->properties.state |=
        (uint16_t) CLIENT_STATE_ICONIFIED;
    not_iconified = s_make_client(3u, config);
    not_iconified->icon_window = 8u;

    ccmd_client_relocate_icon_if_taken(no_icon_window);
    ccmd_client_relocate_icon_if_taken(not_iconified);

    TAP_EQ_INT(s_apply_geometry_calls, 0,
            "neither an icon-less client nor a mapped-but-restored one"
            " reaches a geometry update");

    s_teardown();
}


/* An iconified client whose saved position overlaps nothing else on
 * its desktop keeps that exact spot: relocation never runs at all */
static void s_test_relocate_free_slot_is_untouched(void)
{
    desktop_td *desktop;
    client_td *client;

    s_reset();
    desktop = s_make_desktop(10u);
    client = s_make_client(20u, s_make_config());
    client->icon_window = 100u;
    client->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    client->icon_pos.x = 5;
    client->icon_pos.y = 5;
    s_desktop_add_client(desktop, client);
    s_owner_desktop = desktop;

    ccmd_client_relocate_icon_if_taken(client);

    TAP_EQ_INT(s_place_icon_apply_calls, 0,
            "a free slot never asks the placement policy for a fresh"
            " one");
    TAP_EQ_INT(s_apply_geometry_calls, 0,
            "and never reconfigures the icon window either");
    TAP_EQ_INT((long) client->icon_pos.x, 5,
            "the saved X position is left exactly where it was");

    s_teardown();
}


/* Two clients whose icons occupy the very same spot triggers
 * relocation of the second one examined, moving it to whatever
 * position the placement policy hands back */
static void s_test_relocate_overlapping_slot_is_moved(void)
{
    desktop_td *desktop;
    client_td *first;
    client_td *second;

    s_reset();
    desktop = s_make_desktop(11u);
    first = s_make_client(21u, s_make_config());
    first->icon_window = 101u;
    first->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    first->icon_pos.x = 5;
    first->icon_pos.y = 5;
    second = s_make_client(22u, s_make_config());
    second->icon_window = 102u;
    second->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    second->icon_pos.x = 5;
    second->icon_pos.y = 5;
    s_desktop_add_client(desktop, first);
    s_desktop_add_client(desktop, second);
    s_owner_desktop = desktop;
    s_place_icon_apply_result.x = 200;
    s_place_icon_apply_result.y = 220;

    ccmd_client_relocate_icon_if_taken(second);

    TAP_EQ_INT(s_place_icon_apply_calls, 1,
            "an overlapping slot asks the placement policy exactly"
            " once for a fresh position");
    TAP_EQ_INT((long) second->icon_pos.x, 200,
            "the client's saved X position is updated to the fresh"
            " one");
    TAP_EQ_INT((long) second->icon_pos.y, 220,
            "and so is its saved Y position");
    TAP_EQ_INT(s_apply_geometry_calls, 1,
            "the icon window is reconfigured exactly once");
    TAP_EQ_INT((long) s_apply_geometry_last_target,
            (long) second->icon_window,
            "targeting the relocated client's own icon window");
    TAP_EQ_INT((long) s_apply_geometry_last_x, 200,
            "carrying the fresh X coordinate");
    TAP_EQ_INT((long) s_apply_geometry_last_y, 220,
            "and the fresh Y coordinate");

    s_teardown();
}


/* A remembered position of exactly (-1, -1) is treated as unset,
 * never colliding with anything, so relocation is skipped even when
 * another client's icon genuinely does sit at the same coordinate */
static void s_test_relocate_unset_saved_position_is_never_taken(void)
{
    desktop_td *desktop;
    client_td *other;
    client_td *client;

    s_reset();
    desktop = s_make_desktop(12u);
    other = s_make_client(23u, s_make_config());
    other->icon_window = 103u;
    other->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    other->icon_pos.x = -1;
    other->icon_pos.y = -1;
    client = s_make_client(24u, s_make_config());
    client->icon_window = 104u;
    client->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    client->icon_pos.x = -1;
    client->icon_pos.y = -1;
    s_desktop_add_client(desktop, other);
    s_desktop_add_client(desktop, client);
    s_owner_desktop = desktop;

    ccmd_client_relocate_icon_if_taken(client);

    TAP_EQ_INT(s_place_icon_apply_calls, 0,
            "an unset saved position is never treated as taken");
    TAP_EQ_INT(s_apply_geometry_calls, 0,
            "so relocation never runs for it");

    s_teardown();
}


/* Relocating without a live X connection still updates the client's
 * own bookkeeping, but never touches the wire */
static void s_test_relocate_with_no_connection_skips_wire_update(void)
{
    desktop_td *desktop;
    client_td *first;
    client_td *second;

    s_reset();
    s_connection = NULL;
    desktop = s_make_desktop(13u);
    first = s_make_client(25u, s_make_config());
    first->icon_window = 105u;
    first->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    first->icon_pos.x = 9;
    first->icon_pos.y = 9;
    second = s_make_client(26u, s_make_config());
    second->icon_window = 106u;
    second->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    second->icon_pos.x = 9;
    second->icon_pos.y = 9;
    s_desktop_add_client(desktop, first);
    s_desktop_add_client(desktop, second);
    s_owner_desktop = desktop;
    s_place_icon_apply_result.x = 300;
    s_place_icon_apply_result.y = 300;

    ccmd_client_relocate_icon_if_taken(second);

    TAP_EQ_INT((long) second->icon_pos.x, 300,
            "the client's own saved position still updates locally");
    TAP_EQ_INT(s_apply_geometry_calls, 0,
            "but no geometry request reaches the (absent) X server");

    s_teardown();
}


/* Creating a brand new icon window for a client with no remembered
 * position resolves one via the placement policy, and issues exactly
 * one 'xcb_create_window' call at that position */
static void s_test_ensure_icon_window_creates_fresh_window(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(30u, s_make_config());
    s_place_icon_apply_result.x = 40;
    s_place_icon_apply_result.y = 60;

    ccmd_client_ensure_icon_window(client, 48u);

    TAP_OK(client->icon_window != 0u,
            "the client is left holding a freshly generated icon"
            " window id");
    TAP_EQ_INT(s_create_window_calls, 1,
            "exactly one window is created for a client with none yet");
    TAP_EQ_INT((long) s_create_window_last_wid, (long) client->icon_window,
            "created under the very id just generated and stored");
    TAP_EQ_INT((long) s_create_window_last_x, 40,
            "placed at the X the placement policy chose");
    TAP_EQ_INT((long) s_create_window_last_y, 60,
            "and the Y it chose");
    TAP_EQ_INT((long) s_create_window_last_h, 48,
            "created at exactly the requested icon height");
    TAP_EQ_INT((long) client->icon_pos.x, 40,
            "the chosen X is also saved on the client itself");
    TAP_EQ_INT((long) client->icon_pos.y, 60,
            "as is the chosen Y");

    s_teardown();
}


/* A client already holding a saved, currently-free position reuses it
 * outright: the placement policy is never even asked */
static void s_test_ensure_icon_window_reuses_free_saved_position(void)
{
    desktop_td *desktop;
    client_td *client;

    s_reset();
    desktop = s_make_desktop(20u);
    client = s_make_client(31u, s_make_config());
    client->icon_pos.x = 15;
    client->icon_pos.y = 25;
    s_owner_desktop = desktop;

    ccmd_client_ensure_icon_window(client, 48u);

    TAP_EQ_INT(s_place_icon_apply_calls, 0,
            "a free saved position is reused without consulting the"
            " placement policy");
    TAP_EQ_INT((long) s_create_window_last_x, 15,
            "the window is created at the saved X");
    TAP_EQ_INT((long) s_create_window_last_y, 25,
            "and the saved Y");

    s_teardown();
}


/* A saved position with a negative component, the kind an
 * 'viewport.pan-icons' pan can legitimately leave behind, is still
 * reused rather than mistaken for the never-iconified sentinel of
 * (-1, -1) */
static void s_test_ensure_icon_window_reuses_negative_saved_position(void)
{
    desktop_td *desktop;
    client_td *client;

    s_reset();
    desktop = s_make_desktop(20u);
    client = s_make_client(31u, s_make_config());
    client->icon_pos.x = -750;
    client->icon_pos.y = 25;
    s_owner_desktop = desktop;

    ccmd_client_ensure_icon_window(client, 48u);

    TAP_EQ_INT(s_place_icon_apply_calls, 0,
            "a saved position with a negative X is still reused"
            " without consulting the placement policy");
    TAP_EQ_INT((long) s_create_window_last_x, -750,
            "the window is created at the saved, negative X");
    TAP_EQ_INT((long) s_create_window_last_y, 25,
            "and the saved Y");

    s_teardown();
}


/* A client with a saved position already claimed by another
 * iconified client's icon is sent through the placement policy
 * instead of reusing that stale spot */
static void s_test_ensure_icon_window_skips_claimed_saved_position(void)
{
    desktop_td *desktop;
    client_td *taken;
    client_td *client;

    s_reset();
    desktop = s_make_desktop(21u);
    taken = s_make_client(40u, s_make_config());
    taken->icon_window = 200u;
    taken->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    taken->icon_pos.x = 70;
    taken->icon_pos.y = 70;
    client = s_make_client(41u, s_make_config());
    client->icon_pos.x = 70;
    client->icon_pos.y = 70;
    s_desktop_add_client(desktop, taken);
    s_desktop_add_client(desktop, client);
    s_owner_desktop = desktop;
    s_place_icon_apply_result.x = 500;
    s_place_icon_apply_result.y = 500;

    ccmd_client_ensure_icon_window(client, 48u);

    TAP_EQ_INT(s_place_icon_apply_calls, 1,
            "a claimed saved position sends this client through the"
            " placement policy instead");
    TAP_EQ_INT((long) s_create_window_last_x, 500,
            "the window is created at the freshly chosen X");
    TAP_EQ_INT((long) s_create_window_last_y, 500,
            "and the freshly chosen Y");

    s_teardown();
}


/* A client whose monitor lookup and tray query both decline (nothing
 * at all resolves beyond the plain screen fallback) still ends up
 * wherever the placement policy answers, exercising the pure
 * screen-fallback sizing path rather than the per-monitor one */
static void s_test_ensure_icon_window_falls_back_to_origin(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(50u, s_make_config());
    s_place_icon_apply_result.x = 0;
    s_place_icon_apply_result.y = 0;

    ccmd_client_ensure_icon_window(client, 48u);

    TAP_EQ_INT(s_create_window_calls, 1,
            "a window is still created with no monitor resolved");
    TAP_EQ_INT((long) s_create_window_last_x, 0,
            "falling back to X 0");
    TAP_EQ_INT((long) s_create_window_last_y, 0,
            "and Y 0");

    s_teardown();
}


/* An icon window that already exists is never recreated: the
 * existing-window branch only reconfigures it at its saved position */
static void s_test_ensure_icon_window_repositions_existing_window(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(60u, s_make_config());
    client->icon_window = 777u;
    client->icon_pos.x = 33;
    client->icon_pos.y = 44;

    ccmd_client_ensure_icon_window(client, 48u);

    TAP_EQ_INT(s_create_window_calls, 0,
            "an existing icon window is never recreated");
    TAP_EQ_INT(s_apply_geometry_calls, 1,
            "it is reconfigured exactly once instead");
    TAP_EQ_INT((long) s_apply_geometry_last_target, (long) 777,
            "targeting the already-existing icon window");
    TAP_EQ_INT((long) s_apply_geometry_last_x, 33,
            "at its saved X");
    TAP_EQ_INT((long) s_apply_geometry_last_y, 44,
            "and saved Y, wherever it may have been dragged to since");
    TAP_EQ_INT((long) client->icon_window, 777,
            "the client's icon window id itself never changes");

    s_teardown();
}


/* A monitor resolved for the client shifts the fresh position away
 * from the raw screen origin, offsetting it by that monitor's own
 * top-left corner */
static void s_test_ensure_icon_window_offsets_by_monitor_origin(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(70u, s_make_config());
    s_monitor_available = true;
    s_monitor.x = 1920;
    s_monitor.y = 0;
    s_monitor.w = 1280u;
    s_monitor.h = 1024u;
    s_place_icon_apply_result.x = 10;
    s_place_icon_apply_result.y = 20;

    ccmd_client_ensure_icon_window(client, 48u);

    TAP_EQ_INT((long) s_create_window_last_x, 1930,
            "the chosen position is shifted right by the resolved"
            " monitor's own X origin");
    TAP_EQ_INT((long) s_create_window_last_y, 20,
            "and down by its Y origin, zero here");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(44);

    s_test_relocate_null_or_unconfigured_is_a_no_op();
    s_test_relocate_no_icon_or_not_iconified_is_a_no_op();
    s_test_relocate_free_slot_is_untouched();
    s_test_relocate_overlapping_slot_is_moved();
    s_test_relocate_unset_saved_position_is_never_taken();
    s_test_relocate_with_no_connection_skips_wire_update();
    s_test_ensure_icon_window_creates_fresh_window();
    s_test_ensure_icon_window_reuses_free_saved_position();
    s_test_ensure_icon_window_reuses_negative_saved_position();
    s_test_ensure_icon_window_skips_claimed_saved_position();
    s_test_ensure_icon_window_falls_back_to_origin();
    s_test_ensure_icon_window_repositions_existing_window();
    s_test_ensure_icon_window_offsets_by_monitor_origin();

    return TAP_DONE();
}
