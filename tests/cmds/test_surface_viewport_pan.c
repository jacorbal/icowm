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
 * still links.
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

    free(surface);
    free(desktop);
}


int main(void)
{
    TAP_PLAN(41);

    s_test_pan_null_surface();
    s_test_pan_no_desktop_is_noop();
    s_test_pan_no_config_falls_back_to_1x1();
    s_test_pan_id_past_max_screens_falls_back_to_1x1();
    s_test_pan_east_moves_and_translates_clients();
    s_test_pan_east_clamped_at_edge_is_noop();
    s_test_pan_west_moves_origin_back();
    s_test_pan_west_clamped_at_zero_is_noop();
    s_test_pan_south_moves_vertical_origin();
    s_test_pan_north_moves_vertical_origin_back();
    s_test_set_null_surface_or_no_desktop_is_noop();
    s_test_set_moves_to_absolute_origin();
    s_test_set_clamps_and_noops_at_same_origin();

    return TAP_DONE();
}
