/**
 * @file tests/cmds/test_surface_viewport_pan.c
 *
 * @brief Test battery for the viewport-panning surface commands
 *        (cmds/surface.c)
 *
 * Exercises 'scmd_surface_viewport_pan_north' and its three siblings
 * ('_south', '_east', '_west'), plus the absolute-origin
 * 'scmd_surface_viewport_set', through their shared static helpers.
 * 'lookup_current_desktop' is a test-controlled stand-in, answering
 * whichever desktop pointer (or 'NULL') the currently running
 * scenario registered beforehand, so every branch of the panning
 * logic (no desktop, already at an edge, a real move) runs without a
 * live desktop list ever needing to exist.  'ccmd_target_win' and
 * 'ccmd_client_apply_geometry' are call-recording stand-ins, and
 * 'stacking_walk' is a test-controlled stand-in that calls the real
 * visitor function it is handed back against whichever fixture
 * clients the running scenario registered, letting this file assert
 * on the exact per-client translation 'cmds/surface.c' itself is
 * documented to perform without a real stacking list ever existing.
 * 'xcb_connection_get', 'notify_desktop_show', 'surface_desktop_select
 * ', its four directional siblings, and the three client-visibility
 * primitives are link-only stand-ins, unreachable from any panning
 * path exercised here, kept only so the rest of 'cmds/surface.c'
 * still links.  'scratchpad_notice_viewport_panned' is a
 * call-recording stand-in, letting scenarios assert that every real
 * pan reaches it with the right desktop, and that a clamped no-op
 * never does.
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

/* Type includes */
#include <types/direction.h>

/* Project includes */
#include <client.h>
#include <cmds/surface.h>
#include <config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <policy/stacking.h>
#include <scratchpad.h>
#include <surface.h>


/** Link-only stand-in for @a logger_msg (logger.c): every LOGGER_DEBUG
 *  call in cmds/surface.c reaches this, and this file asserts on
 *  nothing it would print
 *  @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    return 0;
}


/** Link-only stand-in for @a xcb_connection_get (utils/xcb/
 *  connection.c): unreachable from any panning path, kept only so
 *  'cmds/surface.c's desktop-switch half still links
 *  @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/** Link-only stand-in for @a notify_desktop_show (menu/notify/
 *  desktop.c): unreachable for the same reason as
 *  'xcb_connection_get' above
 *  @note Complexity: @e O(1)
 */
void notify_desktop_show(xcb_connection_t *connection,
        surface_td *surface, uint32_t desktop_idx,
        const char *desktop_name, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) desktop_idx;
    (void) desktop_name;
    (void) config;
}


/** Link-only stand-in for @a surface_desktop_select (surface.c):
 *  unreachable from any panning path exercised here
 *  @note Complexity: @e O(1)
 */
int surface_desktop_select(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;

    return 1;
}


/** Link-only stand-ins for @a surface_desktop_select_north and its
 *  three siblings (surface.c): unreachable from any panning path
 *  exercised here
 *  @note Complexity: @e O(1)
 */
int surface_desktop_select_north(surface_td *surface, bool cycle)
{
    (void) surface;
    (void) cycle;

    return 1;
}


int surface_desktop_select_south(surface_td *surface, bool cycle)
{
    (void) surface;
    (void) cycle;

    return 1;
}


int surface_desktop_select_east(surface_td *surface, bool cycle)
{
    (void) surface;
    (void) cycle;

    return 1;
}


int surface_desktop_select_west(surface_td *surface, bool cycle)
{
    (void) surface;
    (void) cycle;

    return 1;
}


/** Link-only stand-ins for the three client-visibility primitives
 *  (surface/actions/clients.c): unreachable from any panning path
 *  exercised here
 *  @note Complexity: @e O(1)
 */
void surface_clients_hide(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
}


void surface_clients_show(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
}


void surface_clients_pinned_transfer_all(surface_td *surface,
        uint32_t to_id)
{
    (void) surface;
    (void) to_id;
}


/** Desktop pointer 'lookup_current_desktop' should currently report,
 *  and how many times it has been called, both reset by 's_reset' */
static desktop_td *s_stub_desktop;
static int s_call_lookup;

/** Test-controlled stand-in for @a lookup_current_desktop (lookup.c)
 *  @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(surface_td *surface)
{
    (void) surface;

    s_call_lookup++;
    return s_stub_desktop;
}


/** Fixed window every scenario's 'ccmd_target_win' stand-in answers
 *  with, and the call count and last-seen client pointer, all reset
 *  by 's_reset' */
static int s_call_target_win;
static client_td *s_last_target_win_client;

/** Call-recording stand-in for @a ccmd_target_win (cmds/client/
 *  screen.c)
 *  @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client)
{
    s_call_target_win++;
    s_last_target_win_client = client;
    return (xcb_window_t) 42;
}


/** Call count and last-seen arguments for 'ccmd_client_apply_geometry
 *  ', all reset by 's_reset' */
static int s_call_apply_geometry;
static xcb_window_t s_last_apply_target;
static uint16_t s_last_apply_mask;
static int32_t s_last_apply_x;
static int32_t s_last_apply_y;

/** Call-recording stand-in for @a ccmd_client_apply_geometry
 *  (cmds/client/move.c)
 *  @note Complexity: @e O(1)
 */
void ccmd_client_apply_geometry(const client_td *client,
        xcb_window_t target, uint16_t mask,
        int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    (void) client;
    (void) w;
    (void) h;
    (void) border_width;

    s_call_apply_geometry++;
    s_last_apply_target = target;
    s_last_apply_mask = mask;
    s_last_apply_x = x;
    s_last_apply_y = y;
}


/** Fixture client list 'stacking_walk' should currently visit, its
 *  length, and how many times 'stacking_walk' itself was called, all
 *  reset by 's_reset' */
static client_td **s_stub_clients;
static size_t s_stub_client_count;
static int s_call_stacking_walk;

/** Test-controlled stand-in for @a stacking_walk (policy/stacking.c):
 *  invokes @p visit against every fixture client the running scenario
 *  registered, exactly as the real function would against a live
 *  stacking list
 *  @note Complexity: @e O(1)
 */
void stacking_walk(const desktop_td *desktop, stacking_visitor_fn visit,
        void *data)
{
    size_t i;

    (void) desktop;

    s_call_stacking_walk++;
    if (visit == NULL) {
        return;
    }
    for (i = 0u; i < s_stub_client_count; i++) {
        visit(s_stub_clients[i], data);
    }
}


/** How many times @a scratchpad_notice_viewport_panned was called,
 *  and the desktop pointer it was last called with, both reset by
 *  's_reset' */
static int s_call_scratchpad_panned;
static const desktop_td *s_last_scratchpad_panned_desktop;

/** Call-recording stand-in for @a scratchpad_notice_viewport_panned
 *  (scratchpad.c)
 *  @note Complexity: @e O(1)
 */
void scratchpad_notice_viewport_panned(const desktop_td *desktop)
{
    s_call_scratchpad_panned++;
    s_last_scratchpad_panned_desktop = desktop;
}


static void s_reset(void)
{
    s_stub_desktop = NULL;
    s_call_lookup = 0;
    s_call_target_win = 0;
    s_last_target_win_client = NULL;
    s_call_apply_geometry = 0;
    s_last_apply_target = XCB_WINDOW_NONE;
    s_last_apply_mask = 0u;
    s_last_apply_x = 0;
    s_last_apply_y = 0;
    s_stub_clients = NULL;
    s_stub_client_count = 0u;
    s_call_stacking_walk = 0;
    s_call_scratchpad_panned = 0;
    s_last_scratchpad_panned_desktop = NULL;
}


static surface_td *s_make_surface(config_td *config, uint32_t id)
{
    surface_td *surface = calloc(1, sizeof(*surface));

    surface->id = id;
    surface->config = config;
    surface->is_outdated = false;
    return surface;
}


static desktop_td *s_make_desktop(uint32_t dim_w, uint32_t dim_h,
        int32_t origin_x, int32_t origin_y)
{
    desktop_td *desktop = calloc(1, sizeof(*desktop));

    desktop->geometry.dim.w = dim_w;
    desktop->geometry.dim.h = dim_h;
    desktop->viewport_origin.x = origin_x;
    desktop->viewport_origin.y = origin_y;
    return desktop;
}


static client_td *s_make_client(int32_t x, int32_t y, bool sticky)
{
    client_td *client = calloc(1, sizeof(*client));

    client->layout.geometry.cur.pos.x = x;
    client->layout.geometry.cur.pos.y = y;
    client->layout.geometry.old.pos.x = x;
    client->layout.geometry.old.pos.y = y;
    if (sticky) {
        client->properties.flags |= (uint32_t) CLIENT_FLAG_STICKY;
    }
    return client;
}


/* A null surface is refused outright before ever looking up a
 * desktop, by every one of the four directions */
static void s_test_pan_null_surface(void)
{
    s_reset();

    scmd_surface_viewport_pan_north(NULL);
    scmd_surface_viewport_pan_south(NULL);
    scmd_surface_viewport_pan_east(NULL);
    scmd_surface_viewport_pan_west(NULL);
    TAP_EQ_INT(s_call_lookup, 0,
            "a null surface never reaches lookup_current_desktop");
}


/* No current desktop is a silent no-op: the walk never runs and the
 * surface is never marked outdated */
static void s_test_pan_no_desktop_is_noop(void)
{
    surface_td *surface = s_make_surface(NULL, 0u);

    s_reset();
    s_stub_desktop = NULL;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT(s_call_lookup, 1,
            "the current desktop is looked up exactly once");
    TAP_EQ_INT(s_call_stacking_walk, 0,
            "no desktop means the client walk never runs");
    TAP_OK(!surface->is_outdated,
            "no desktop means the surface is never marked outdated");

    free(surface);
}


/* With no config on the surface, the pannable area falls back to
 * exactly the physical screen (1x1), so panning in any direction from
 * the origin is already at that edge and is a no-op */
static void s_test_pan_no_config_falls_back_to_1x1(void)
{
    surface_td *surface = s_make_surface(NULL, 0u);
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT(s_call_stacking_walk, 0,
            "a 1x1 fallback pannable area is already at every edge, so"
            " panning east never walks any client");
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the viewport origin is left untouched");
    TAP_OK(!surface->is_outdated,
            "a no-op pan never marks the surface outdated");

    free(surface);
    free(desktop);
}


/* An 'id' past CONFIG_MAX_SCREENS falls back to the same 1x1 pannable
 * area as a null config */
static void s_test_pan_id_past_max_screens_falls_back_to_1x1(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 4u;
    config.base.screens[0].viewport.rows = 4u;
    surface = s_make_surface(&config, (uint32_t) CONFIG_MAX_SCREENS);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT(s_call_stacking_walk, 0,
            "an out-of-range screen id falls back to a 1x1 pannable"
            " area regardless of screen 0's own viewport settings");

    free(surface);
    free(desktop);
}


/* Panning east from the origin with a wider-than-one-screen viewport
 * moves the origin by exactly one screen width, translates every
 * non-sticky client the opposite way, and marks the surface
 * outdated */
static void s_test_pan_east_moves_and_translates_clients(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *sticky = s_make_client(30, 40, true);
    client_td *clients[2];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    clients[0] = moving;
    clients[1] = sticky;

    s_reset();
    s_stub_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 2u;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "the viewport origin moves one whole screen width east");
    TAP_EQ_INT(desktop->viewport_origin.y, 0,
            "panning east never touches the vertical origin");
    TAP_EQ_INT(s_call_stacking_walk, 1,
            "the client walk runs exactly once");
    TAP_EQ_INT(moving->layout.geometry.cur.pos.x, -790,
            "a non-sticky client's current position shifts by the"
            " same delta as the viewport, the opposite way");
    TAP_EQ_INT(moving->layout.geometry.old.pos.x, -790,
            "a non-sticky client's saved position shifts together"
            " with its current one");
    TAP_EQ_INT(sticky->layout.geometry.cur.pos.x, 30,
            "a sticky client is left exactly where it was");
    TAP_EQ_INT(s_call_target_win, 1,
            "ccmd_target_win is reached exactly once, for the"
            " non-sticky client alone");
    TAP_EQ_INT(s_call_apply_geometry, 1,
            "ccmd_client_apply_geometry is reached exactly once, for"
            " the non-sticky client alone");
    TAP_EQ_INT((int) s_last_apply_target, 42,
            "the target window applied is ccmd_target_win's own"
            " result");
    TAP_OK((s_last_apply_mask & (uint16_t) XCB_CONFIG_WINDOW_X) != 0u &&
            (s_last_apply_mask & (uint16_t) XCB_CONFIG_WINDOW_Y) != 0u,
            "only the X and Y bits are set in the applied mask");
    TAP_EQ_INT(s_last_apply_x, -790,
            "the X position applied matches the client's own shifted"
            " position");
    TAP_OK(surface->is_outdated,
            "a real pan marks the surface outdated");

    free(surface);
    free(desktop);
    free(moving);
    free(sticky);
}


/* A real pan reaches 'scratchpad_notice_viewport_panned' exactly
 * once, with the desktop that actually panned, letting the
 * scratchpad hide itself before it can ever be seen drifted away
 * from its own configured edge */
static void s_test_pan_east_notifies_scratchpad_of_real_pan(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT(s_call_scratchpad_panned, 1,
            "a real pan notifies the scratchpad exactly once");
    TAP_OK(s_last_scratchpad_panned_desktop == desktop,
            "the notified desktop is the one that actually panned");

    free(surface);
    free(desktop);
}


/* A client currently marked as the drag-excluded one (via
 * 'scmd_surface_viewport_drag_exclude') is skipped by the walk
 * exactly like a sticky client, since a pan mid-drag must never
 * reposition the real window out from under the drag that is already
 * tracking it by other means */
static void s_test_pan_east_skips_drag_excluded_client(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *dragged = s_make_client(30, 40, false);
    client_td *clients[2];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    clients[0] = moving;
    clients[1] = dragged;

    s_reset();
    s_stub_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 2u;

    scmd_surface_viewport_drag_exclude(dragged);
    scmd_surface_viewport_pan_east(surface);
    scmd_surface_viewport_drag_exclude(NULL);

    TAP_EQ_INT(moving->layout.geometry.cur.pos.x, -790,
            "the un-excluded client is still translated as usual");
    TAP_EQ_INT(dragged->layout.geometry.cur.pos.x, 30,
            "the drag-excluded client is left exactly where it was");
    TAP_EQ_INT(s_call_target_win, 1,
            "ccmd_target_win is reached exactly once, for the"
            " un-excluded client alone");
    TAP_EQ_INT(s_call_apply_geometry, 1,
            "ccmd_client_apply_geometry is reached exactly once, for"
            " the un-excluded client alone");

    free(surface);
    free(desktop);
    free(moving);
    free(dragged);
}


/* When 'icons.follow-viewport' is on and a client's icon is currently
 * mapped, panning shifts that icon's own saved position (and its
 * real window) by the exact same delta as the client itself, right
 * after the client's own window is moved */
static void s_test_pan_east_also_translates_mapped_icon(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    config.base.icons.follow_viewport = true;
    surface = s_make_surface(&config, 0u);

    moving->config = &config;
    moving->icon_window = (xcb_window_t) 7;
    moving->is_icon_mapped = true;
    moving->icon_pos.x = 50;
    moving->icon_pos.y = 60;

    clients[0] = moving;

    s_reset();
    s_stub_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT(s_call_apply_geometry, 2,
            "ccmd_client_apply_geometry runs twice: once for the"
            " window, once for the mapped icon");
    TAP_EQ_INT((int) moving->icon_pos.x, 50 - 800,
            "the icon's saved X shifts by the same delta as the"
            " client's own window");
    TAP_EQ_INT((int) moving->icon_pos.y, 60,
            "panning east never touches the icon's saved Y");
    TAP_EQ_INT((int) s_last_apply_target, 7,
            "the icon window itself, not ccmd_target_win's result,"
            " is the last one reconfigured");
    TAP_EQ_INT(s_last_apply_x, 50 - 800,
            "the X position applied to the icon matches its own"
            " shifted position");

    free(surface);
    free(desktop);
    free(moving);
}


/* A second pan still translates the icon even once the first pan
 * already left its saved position negative, confirming the sentinel
 * that guards a genuinely never-iconified icon ('icon_pos' still at
 * (-1, -1)) is not mistaken for a legitimately off-screen one that a
 * pan produced along the way */
static void s_test_pan_east_twice_keeps_translating_negative_icon(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 1u;
    config.base.icons.follow_viewport = true;
    surface = s_make_surface(&config, 0u);

    moving->config = &config;
    moving->icon_window = (xcb_window_t) 7;
    moving->is_icon_mapped = true;
    moving->icon_pos.x = 50;
    moving->icon_pos.y = 60;

    clients[0] = moving;

    s_reset();
    s_stub_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT((int) moving->icon_pos.x, 50 - 800,
            "the first pan shifts the icon, leaving its saved X"
            " negative");

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT((int) moving->icon_pos.x, 50 - 1600,
            "a second pan still shifts the icon by the same delta,"
            " even though its saved X was already negative");
    TAP_EQ_INT(s_call_apply_geometry, 4,
            "ccmd_client_apply_geometry runs twice per pan (window and"
            " icon), across both pans");

    free(surface);
    free(desktop);
    free(moving);
}


/* The same mapped icon is left untouched when 'icons.follow-viewport'
 * is off, the default, confirming the new behavior never engages
 * unless explicitly requested */
static void s_test_pan_east_leaves_icon_when_follow_viewport_off(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    moving->config = &config;
    moving->icon_window = (xcb_window_t) 7;
    moving->is_icon_mapped = true;
    moving->icon_pos.x = 50;
    moving->icon_pos.y = 60;

    clients[0] = moving;

    s_reset();
    s_stub_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT(s_call_apply_geometry, 1,
            "ccmd_client_apply_geometry runs once, for the window"
            " alone");
    TAP_EQ_INT((int) moving->icon_pos.x, 50,
            "the icon's saved X is left untouched");

    free(surface);
    free(desktop);
    free(moving);
}


/* Panning east again once already at the rightmost edge is clamped
 * back to that same edge, recognized as no movement, and is a no-op */
static void s_test_pan_east_clamped_at_edge_is_noop(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 800, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_east(surface);
    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "the viewport origin is clamped back to the rightmost"
            " edge, unchanged");
    TAP_EQ_INT(s_call_stacking_walk, 0,
            "already being at the edge never walks any client");
    TAP_OK(!surface->is_outdated,
            "clamping back to the same edge never marks the surface"
            " outdated");
    TAP_EQ_INT(s_call_scratchpad_panned, 0,
            "a clamped no-op pan never notifies the scratchpad");

    free(surface);
    free(desktop);
}


/* Panning west from the rightmost edge moves the origin back by one
 * whole screen width */
static void s_test_pan_west_moves_origin_back(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 800, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_west(surface);
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the viewport origin moves one whole screen width back"
            " west");
    TAP_OK(surface->is_outdated,
            "a real westward pan marks the surface outdated");

    free(surface);
    free(desktop);
}


/* Panning west from the leftmost edge is clamped back to zero and
 * recognized as no movement */
static void s_test_pan_west_clamped_at_zero_is_noop(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_west(surface);
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the viewport origin is clamped back to zero, unchanged");
    TAP_OK(!surface->is_outdated,
            "clamping back to zero never marks the surface outdated");

    free(surface);
    free(desktop);
}


/* Panning south from the origin with a taller-than-one-screen
 * viewport moves the origin by exactly one screen height */
static void s_test_pan_south_moves_vertical_origin(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 1u;
    config.base.screens[0].viewport.rows = 2u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_south(surface);
    TAP_EQ_INT(desktop->viewport_origin.y, 600,
            "the viewport origin moves one whole screen height south");
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "panning south never touches the horizontal origin");
    TAP_OK(surface->is_outdated,
            "a real southward pan marks the surface outdated");

    free(surface);
    free(desktop);
}


/* Panning north from below the top edge moves the origin back by one
 * whole screen height */
static void s_test_pan_north_moves_vertical_origin_back(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 600);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 1u;
    config.base.screens[0].viewport.rows = 2u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_north(surface);
    TAP_EQ_INT(desktop->viewport_origin.y, 0,
            "the viewport origin moves one whole screen height back"
            " north");
    TAP_OK(surface->is_outdated,
            "a real northward pan marks the surface outdated");

    free(surface);
    free(desktop);
}


/* Unlike its whole-screen siblings, the keyboard step pan moves the
 * origin by 'viewport.move-step' pixels alone */
static void s_test_pan_step_east_moves_by_move_step_pixels(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    config.base.viewport.move_step = 15u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_step(surface, COMPASS_EAST);
    TAP_EQ_INT(desktop->viewport_origin.x, 15,
            "the viewport origin moves 'viewport.move-step' pixels"
            " east, not a whole screen");
    TAP_OK(surface->is_outdated,
            "a real step pan marks the surface outdated");

    free(surface);
    free(desktop);
}


/* The keyboard step pan clamps at the pannable area's edge exactly
 * like its whole-screen siblings, even when the configured step would
 * otherwise overshoot it */
static void s_test_pan_step_clamped_at_edge_is_noop(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 795, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    config.base.viewport.move_step = 15u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_pan_step(surface, COMPASS_EAST);
    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "the origin clamps at the pannable area's east edge"
            " instead of overshooting by the full step");
    TAP_OK(surface->is_outdated,
            "a clamped step that still moves the origin marks the"
            " surface outdated");

    free(surface);
    free(desktop);
}


/* A null surface, or one with no resolvable current desktop, is
 * refused outright by the absolute-origin setter too */
static void s_test_set_null_surface_or_no_desktop_is_noop(void)
{
    config_td config;
    surface_td *surface;

    memset(&config, 0, sizeof(config));
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = NULL;

    scmd_surface_viewport_set(NULL, 100, 200);
    scmd_surface_viewport_set(surface, 100, 200);

    TAP_OK(!surface->is_outdated,
            "a null surface, or one with no resolvable current" \
            " desktop, never outdates anything");

    free(surface);
}


/* Setting an absolute origin within the pannable area moves the
 * viewport there directly and translates every non-sticky client by
 * the resulting delta, just like a directional pan does */
static void s_test_set_moves_to_absolute_origin(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    surface = s_make_surface(&config, 0u);

    clients[0] = moving;

    s_reset();
    s_stub_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_surface_viewport_set(surface, 800, 600);

    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "the viewport jumps straight to the requested X origin");
    TAP_EQ_INT(desktop->viewport_origin.y, 600,
            "the viewport jumps straight to the requested Y origin");
    TAP_EQ_INT(moving->layout.geometry.cur.pos.x, -790,
            "a non-sticky client's position shifts by the resulting" \
            " delta, the opposite way");
    TAP_OK(surface->is_outdated,
            "a real absolute-origin move marks the surface outdated");

    free(surface);
    free(desktop);
    free(moving);
}


/* A null surface, or one with no resolvable current desktop, is
 * refused outright by the page go-to command too */
static void s_test_goto_null_surface_or_no_desktop_is_noop(void)
{
    config_td config;
    surface_td *surface;

    memset(&config, 0, sizeof(config));
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = NULL;

    scmd_surface_viewport_goto(NULL, 0u);
    scmd_surface_viewport_goto(surface, 0u);

    TAP_OK(!surface->is_outdated,
            "a null surface, or one with no resolvable current" \
            " desktop, never outdates anything through" \
            " scmd_surface_viewport_goto");

    free(surface);
}


/* A page index maps to its own origin in row-major order across the
 * configured grid, and lands there exactly the same way
 * 'scmd_surface_viewport_set' would given that pixel origin */
static void s_test_goto_moves_to_correct_page(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 2u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    /* Page 4 (zero-based), 3 columns wide: row 1, column 1 */
    scmd_surface_viewport_goto(surface, 4u);

    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "page 4 across a 3-column grid lands on column 1's" \
            " X origin");
    TAP_EQ_INT(desktop->viewport_origin.y, 600,
            "page 4 across a 3-column grid lands on row 1's" \
            " Y origin");
    TAP_OK(surface->is_outdated,
            "a real page jump marks the surface outdated");

    free(surface);
    free(desktop);
}


/* A page index at or past the configured grid's own page count is
 * refused outright, unlike a pixel origin which would instead be
 * clamped into range */
static void s_test_goto_out_of_range_page_is_noop(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_goto(surface, 4u);

    TAP_OK(s_call_stacking_walk == 0,
            "a page index past the grid's own count walks no client" \
            " at all");
    TAP_OK(!surface->is_outdated,
            "a page index past the grid's own count never outdates" \
            " the surface");

    free(surface);
    free(desktop);
}


/* A requested origin past the pannable area is clamped back to its
 * nearest edge, and requesting the origin already in effect is a
 * no-op */
static void s_test_set_clamps_and_noops_at_same_origin(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_set(surface, 5000, -5000);
    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "an out-of-range X origin is clamped to the rightmost" \
            " edge");
    TAP_EQ_INT(desktop->viewport_origin.y, 0,
            "an out-of-range negative Y origin is clamped to zero");

    s_reset();
    s_stub_desktop = desktop;
    surface->is_outdated = false;

    scmd_surface_viewport_set(surface, 800, 0);
    TAP_OK(s_call_stacking_walk == 0,
            "requesting the origin already in effect walks no" \
            " client at all");
    TAP_OK(!surface->is_outdated,
            "requesting the origin already in effect never outdates" \
            " the surface");
    TAP_EQ_INT(s_call_scratchpad_panned, 0,
            "requesting the origin already in effect never notifies" \
            " the scratchpad either");

    free(surface);
    free(desktop);
}


/* A client already sitting within the desktop's own canvas, but off
 * the currently panned-to page, is left exactly where it is by the
 * defensive clamp: only the viewport itself moves, to bring it into
 * view */
static void s_test_center_on_client_within_canvas_is_not_clamped(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *client = s_make_client(1600, 0, false);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_center_on_client(surface, client);
    TAP_EQ_INT(client->layout.geometry.cur.pos.x, 1600,
            "a client already inside the desktop's canvas is never" \
            " moved by the defensive clamp");
    TAP_EQ_INT(desktop->viewport_origin.x, 1200,
            "the viewport itself pans to bring the off-page client" \
            " into view instead, centered (client X 1600 minus half" \
            " the 800-wide screen)");

    free(surface);
    free(desktop);
    free(client);
}


/* The same, from a viewport already panned away from its origin: a
 * client's recorded position is relative to what is on screen, so the
 * defensive clamp has to convert to canvas coordinates before bounding
 * it.  Bounding the screen-relative value directly would drag every
 * client on an earlier page toward the current one, and then leave the
 * viewport where it was because the dragged client now overlaps the
 * screen */
static void s_test_center_on_client_from_panned_origin(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 800, 0);
    client_td *client = s_make_client(-800, 0, false);

    client->layout.geometry.cur.dim.w = 400u;
    client->layout.geometry.cur.dim.h = 300u;

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_center_on_client(surface, client);
    TAP_EQ_INT(client->layout.geometry.cur.pos.x, -800,
            "a client one page west of a panned viewport is left" \
            " exactly where it is, not dragged toward the current" \
            " page");
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the viewport pans west to the client's own page" \
            " instead");

    free(surface);
    free(desktop);
    free(client);
}


/* A decorated client pressed against a page's edge shows a sliver of
 * its frame on the neighboring page.  An overlap test would call that
 * "already visible" and refuse to pan; the page comparison that
 * replaced it does not, and the centering that follows accounts for
 * the whole frame rather than the content alone */
static void s_test_center_on_client_touching_page_edge(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 600);
    client_td *client = s_make_client(0, -300, false);

    /* Tall enough that its lower edge pokes twenty pixels into the
     * page now on screen, while its own corner, and so the page it
     * belongs to, is the one above.  The overlap test this replaced
     * saw those twenty pixels and refused to pan */
    client->layout.geometry.cur.dim.w = 400u;
    client->layout.geometry.cur.dim.h = 320u;
    client->layout.frame_extents.left = 1u;
    client->layout.frame_extents.right = 1u;
    client->layout.frame_extents.top = 20u;
    client->layout.frame_extents.bottom = 1u;

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 1u;
    config.base.screens[0].viewport.rows = 2u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_center_on_client(surface, client);
    /* 341 is the frame's own height, content plus titlebar plus
     * borders; centering the content alone would have landed on 160 */
    TAP_EQ_INT(desktop->viewport_origin.y, 170,
            "a client whose frame merely grazes the current page"
            " still pans toward the page the client is actually on,"
            " centering the whole frame rather than its content");
    TAP_EQ_INT(client->layout.geometry.cur.pos.y, -300,
            "and the client itself is left where it was: only the"
            " viewport moved");

    free(surface);
    free(desktop);
    free(client);
}


/* A desktop whose geometry has not been resolved yet reports page
 * zero rather than dividing by zero, which is undefined behavior and
 * not something a caller could recover from */
static void s_test_page_lookup_on_zero_geometry(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(0u, 0u, 0, 0);
    client_td *client = s_make_client(100, 100, false);
    uint32_t col = 9u;
    uint32_t row = 9u;

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    TAP_OK(scmd_surface_viewport_client_page(surface, desktop, client,
                &col, &row),
            "a zero-sized desktop still answers the client page"
            " query");
    TAP_OK(col == 0u && row == 0u,
            "and answers page zero rather than dividing by zero");

    free(surface);
    free(desktop);
    free(client);
}


/* A client whose recorded position ended up entirely outside the
 * desktop's own canvas (the kind of corruption a drag/pan/warp
 * calculation should never produce, but which this defensive backstop
 * does not rely on never happening) is clamped back inside the canvas
 * before the centering math runs, so it is guaranteed to land on some
 * real, reachable viewport page */
static void s_test_center_on_client_outside_canvas_is_clamped(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *client = s_make_client(50000, -50000, false);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_center_on_client(surface, client);
    TAP_OK(client->layout.geometry.cur.pos.x >= 0 &&
            client->layout.geometry.cur.pos.x < 1600,
            "the clamped X now sits inside the 2-column canvas" \
            " (0 <= x < 1600)");
    TAP_OK(client->layout.geometry.cur.pos.y >= 0 &&
            client->layout.geometry.cur.pos.y < 1200,
            "the clamped Y now sits inside the 2-row canvas" \
            " (0 <= y < 1200)");
    TAP_EQ_INT(client->layout.geometry.cur.pos.x,
            client->layout.geometry.old.pos.x,
            "'old.pos' is kept in sync with the clamp too, matching" \
            " every other part of the window manager that still" \
            " relies on it");
    TAP_OK(s_call_apply_geometry > 0,
            "the clamp applies the corrected position to the real" \
            " window, not just the stored geometry");
    TAP_OK(surface->is_outdated,
            "bringing an out-of-canvas client back in view marks the" \
            " surface outdated");

    free(surface);
    free(desktop);
    free(client);
}


/* A client already at least partly visible on the current page is
 * left alone entirely: neither the defensive clamp (already inside
 * the canvas by construction) nor the centering pan itself have
 * anything to do */
static void s_test_center_on_client_already_visible_is_noop(void)
{
    config_td config;
    surface_td *surface;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *client = s_make_client(100, 100, false);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    surface = s_make_surface(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;

    scmd_surface_viewport_center_on_client(surface, client);
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "a client already visible on the current page never" \
            " causes a pan");
    TAP_OK(!surface->is_outdated,
            "and never marks the surface outdated either");

    free(surface);
    free(desktop);
    free(client);
}


int main(void)
{
    TAP_PLAN(84);

    s_test_pan_null_surface();
    s_test_pan_no_desktop_is_noop();
    s_test_pan_no_config_falls_back_to_1x1();
    s_test_pan_id_past_max_screens_falls_back_to_1x1();
    s_test_pan_east_moves_and_translates_clients();
    s_test_pan_east_notifies_scratchpad_of_real_pan();
    s_test_pan_east_skips_drag_excluded_client();
    s_test_pan_east_also_translates_mapped_icon();
    s_test_pan_east_twice_keeps_translating_negative_icon();
    s_test_pan_east_leaves_icon_when_follow_viewport_off();
    s_test_pan_east_clamped_at_edge_is_noop();
    s_test_pan_west_moves_origin_back();
    s_test_pan_west_clamped_at_zero_is_noop();
    s_test_pan_south_moves_vertical_origin();
    s_test_pan_north_moves_vertical_origin_back();
    s_test_pan_step_east_moves_by_move_step_pixels();
    s_test_pan_step_clamped_at_edge_is_noop();
    s_test_set_null_surface_or_no_desktop_is_noop();
    s_test_set_moves_to_absolute_origin();
    s_test_set_clamps_and_noops_at_same_origin();
    s_test_goto_null_surface_or_no_desktop_is_noop();
    s_test_goto_moves_to_correct_page();
    s_test_goto_out_of_range_page_is_noop();
    s_test_center_on_client_within_canvas_is_not_clamped();
    s_test_center_on_client_from_panned_origin();
    s_test_center_on_client_touching_page_edge();
    s_test_page_lookup_on_zero_geometry();
    s_test_center_on_client_outside_canvas_is_clamped();
    s_test_center_on_client_already_visible_is_noop();

    return TAP_DONE();
}
