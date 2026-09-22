/**
 * @file tests/cmds/test_stage_viewport_pan.c
 *
 * @brief Test battery for the viewport-panning stage commands
 *        (cmds/stage.c)
 *
 * Exercises 'scmd_stage_viewport_pan_north' and its three siblings
 * ('_south', '_east', '_west'), plus the absolute-origin
 * 'scmd_stage_viewport_set', through their shared static helpers.
 * 'lookup_current_desktop' is a test-controlled stand-in, answering
 * whichever desktop pointer (or 'NULL') the currently running
 * scenario registered beforehand, so every branch of the panning
 * logic (no desktop, already at an edge, a real move) runs without a
 * live desktop list ever needing to exist.  'ccmd_target_win' and
 * 'ccmd_client_apply_geometry' are call-recording stand-ins, and
 * 'stacking_walk' is a test-controlled stand-in that calls the real
 * visitor function it is handed back against whichever fixture
 * clients the running scenario registered, letting this file assert
 * on the exact per-client translation 'cmds/stage.c' itself is
 * documented to perform without a real stacking list ever existing.
 * 'xcb_connection_get', 'notify_desktop_show', 'stage_desktop_select
 * ', its four directional siblings, and the three client-visibility
 * primitives are link-only stand-ins, unreachable from any panning
 * path exercised here, kept only so the rest of 'cmds/stage.c'
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
#include <cmds/stage.h>
#include <config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <policy/stacking.h>
#include <scratchpad.h>
#include <stage.h>


/** Link-only stand-in for @a logger_msg (logger.c): every LOGGER_DEBUG
 *  call in cmds/stage.c reaches this, and this file asserts on
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
 *  'cmds/stage.c's desktop-switch half still links
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
        stage_td *stage, uint32_t desktop_idx,
        const char *desktop_name, const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) desktop_idx;
    (void) desktop_name;
    (void) config;
}


/** Link-only stand-in for @a stage_desktop_select (stage.c):
 *  unreachable from any panning path exercised here
 *  @note Complexity: @e O(1)
 */
int stage_desktop_select(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    return 1;
}


/** Link-only stand-ins for @a stage_desktop_select_north and its
 *  three siblings (stage.c): unreachable from any panning path
 *  exercised here
 *  @note Complexity: @e O(1)
 */
int stage_desktop_select_north(stage_td *stage, bool cycle)
{
    (void) stage;
    (void) cycle;

    return 1;
}


int stage_desktop_select_south(stage_td *stage, bool cycle)
{
    (void) stage;
    (void) cycle;

    return 1;
}


int stage_desktop_select_east(stage_td *stage, bool cycle)
{
    (void) stage;
    (void) cycle;

    return 1;
}


int stage_desktop_select_west(stage_td *stage, bool cycle)
{
    (void) stage;
    (void) cycle;

    return 1;
}


/** Link-only stand-ins for the three client-visibility primitives
 *  (stage/actions/client.c): unreachable from any panning path
 *  exercised here
 *  @note Complexity: @e O(1)
 */
void stage_client_hide_all(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
}


void stage_client_show_all(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
}


void stage_client_pinned_transfer_all(stage_td *stage,
        uint32_t to_id)
{
    (void) stage;
    (void) to_id;
}


/** Desktop pointer 'lookup_current_desktop' should currently report,
 *  and how many times it has been called, both reset by 's_reset' */
static desktop_td *s_stub_desktop;
static int s_call_lookup;

/** Test-controlled stand-in for @a lookup_current_desktop (lookup.c)
 *  @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;

    s_call_lookup++;
    return s_stub_desktop;
}


/** Desktop the stand-in below reports a client as belonging to, so
 *  that a scenario can present one that is on another; left pointing
 *  at the current desktop by @a s_reset, which is what every scenario
 *  but that one wants */
static desktop_td *s_client_desktop;

/**
 * @brief Test-controlled stand-in for @a wm_get_client_desktop
 *
 * @note Complexity: @e O(1)
 */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;

    return s_client_desktop;
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
void ccmd_client_apply_geometry(client_td *client,
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
    s_client_desktop = NULL;
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


static stage_td *s_make_stage(config_td *config, uint32_t id)
{
    stage_td *stage = calloc(1, sizeof(*stage));

    stage->id = id;
    stage->config = config;
    stage->is_outdated = false;
    return stage;
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


/* A fullscreen client covers the physical screen, so neither axis of
 * its current position follows the canvas, while its saved position,
 * which is where restoring it puts it back on that canvas, does */
static void s_test_pan_east_keeps_fullscreen_in_place(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *full = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);
    full->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;

    clients[0] = full;

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_stage_viewport_pan_east(stage);

    TAP_EQ_INT(full->layout.geometry.cur.pos.x, 10,
            "a fullscreen client keeps covering the screen it was"
            " sized to");
    TAP_EQ_INT(full->layout.geometry.old.pos.x, -790,
            "...while its saved position follows the canvas, so"
            " restoring it lands where the pan left that canvas");
    TAP_EQ_INT(s_call_target_win, 0,
            "...and its real window is never reconfigured");

    free(full);
    free(desktop);
    free(stage);
}


/* A null stage is refused outright before ever looking up a
 * desktop, by every one of the four directions */
static void s_test_pan_null_stage(void)
{
    s_reset();

    scmd_stage_viewport_pan_north(NULL);
    scmd_stage_viewport_pan_south(NULL);
    scmd_stage_viewport_pan_east(NULL);
    scmd_stage_viewport_pan_west(NULL);
    TAP_EQ_INT(s_call_lookup, 0,
            "a null stage never reaches lookup_current_desktop");
}


/* No current desktop is a silent no-op: the walk never runs and the
 * stage is never marked outdated */
static void s_test_pan_no_desktop_is_noop(void)
{
    stage_td *stage = s_make_stage(NULL, 0u);

    s_reset();
    s_stub_desktop = NULL;

    scmd_stage_viewport_pan_east(stage);
    TAP_EQ_INT(s_call_lookup, 1,
            "the current desktop is looked up exactly once");
    TAP_EQ_INT(s_call_stacking_walk, 0,
            "no desktop means the client walk never runs");
    TAP_OK(!stage->is_outdated,
            "no desktop means the stage is never marked outdated");

    free(stage);
}


/* With no config on the stage, the pannable area falls back to
 * exactly the physical screen (1x1), so panning in any direction from
 * the origin is already at that edge and is a no-op */
static void s_test_pan_no_config_falls_back_to_1x1(void)
{
    stage_td *stage = s_make_stage(NULL, 0u);
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_east(stage);
    TAP_EQ_INT(s_call_stacking_walk, 0,
            "a 1x1 fallback pannable area is already at every edge, so"
            " panning east never walks any client");
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the viewport origin is left untouched");
    TAP_OK(!stage->is_outdated,
            "a no-op pan never marks the stage outdated");

    free(stage);
    free(desktop);
}


/* An 'id' past CONFIG_MAX_SCREENS falls back to the same 1x1 pannable
 * area as a null config */
static void s_test_pan_id_past_max_screens_falls_back_to_1x1(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 4u;
    config.base.screens[0].viewport.rows = 4u;
    stage = s_make_stage(&config, (uint32_t) CONFIG_MAX_SCREENS);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_east(stage);
    TAP_EQ_INT(s_call_stacking_walk, 0,
            "an out-of-range screen id falls back to a 1x1 pannable"
            " area regardless of screen 0's own viewport settings");

    free(stage);
    free(desktop);
}


/* Panning east from the origin with a wider-than-one-screen viewport
 * moves the origin by exactly one screen width, translates every
 * non-sticky client the opposite way, and marks the stage
 * outdated */
static void s_test_pan_east_moves_and_translates_clients(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *sticky = s_make_client(30, 40, true);
    client_td *clients[2];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    clients[0] = moving;
    clients[1] = sticky;

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 2u;

    scmd_stage_viewport_pan_east(stage);
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
    TAP_OK(stage->is_outdated,
            "a real pan marks the stage outdated");

    free(stage);
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
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_east(stage);
    TAP_EQ_INT(s_call_scratchpad_panned, 1,
            "a real pan notifies the scratchpad exactly once");
    TAP_OK(s_last_scratchpad_panned_desktop == desktop,
            "the notified desktop is the one that actually panned");

    free(stage);
    free(desktop);
}


/* A client currently marked as the drag-excluded one (via
 * 'scmd_stage_viewport_drag_exclude') is skipped by the walk
 * exactly like a sticky client, since a pan mid-drag must never
 * reposition the real window out from under the drag that is already
 * tracking it by other means */
static void s_test_pan_east_skips_drag_excluded_client(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *dragged = s_make_client(30, 40, false);
    client_td *clients[2];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    clients[0] = moving;
    clients[1] = dragged;

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 2u;

    scmd_stage_viewport_drag_exclude(dragged);
    scmd_stage_viewport_pan_east(stage);
    scmd_stage_viewport_drag_exclude(NULL);

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

    free(stage);
    free(desktop);
    free(moving);
    free(dragged);
}


/* When 'viewport.pan-icons' is on and a client's icon is currently
 * mapped, panning shifts that icon's own saved position (and its
 * real window) by the exact same delta as the client itself, right
 * after the client's own window is moved */
static void s_test_pan_east_also_translates_mapped_icon(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    config.base.viewport.pan_icons = true;
    stage = s_make_stage(&config, 0u);

    moving->config = &config;
    moving->icon_window = (xcb_window_t) 7;
    moving->is_icon_mapped = true;
    moving->icon_pos.x = 50;
    moving->icon_pos.y = 60;

    clients[0] = moving;

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_stage_viewport_pan_east(stage);
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

    free(stage);
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
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 1u;
    config.base.viewport.pan_icons = true;
    stage = s_make_stage(&config, 0u);

    moving->config = &config;
    moving->icon_window = (xcb_window_t) 7;
    moving->is_icon_mapped = true;
    moving->icon_pos.x = 50;
    moving->icon_pos.y = 60;

    clients[0] = moving;

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_stage_viewport_pan_east(stage);
    TAP_EQ_INT((int) moving->icon_pos.x, 50 - 800,
            "the first pan shifts the icon, leaving its saved X"
            " negative");

    scmd_stage_viewport_pan_east(stage);
    TAP_EQ_INT((int) moving->icon_pos.x, 50 - 1600,
            "a second pan still shifts the icon by the same delta,"
            " even though its saved X was already negative");
    TAP_EQ_INT(s_call_apply_geometry, 4,
            "ccmd_client_apply_geometry runs twice per pan (window and"
            " icon), across both pans");

    free(stage);
    free(desktop);
    free(moving);
}


/* The same mapped icon is left untouched when 'viewport.pan-icons'
 * is off, the default, confirming the new behavior never engages
 * unless explicitly requested */
static void s_test_pan_east_leaves_icon_when_pan_icons_off(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    moving->config = &config;
    moving->icon_window = (xcb_window_t) 7;
    moving->is_icon_mapped = true;
    moving->icon_pos.x = 50;
    moving->icon_pos.y = 60;

    clients[0] = moving;

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_stage_viewport_pan_east(stage);
    TAP_EQ_INT(s_call_apply_geometry, 1,
            "ccmd_client_apply_geometry runs once, for the window"
            " alone");
    TAP_EQ_INT((int) moving->icon_pos.x, 50,
            "the icon's saved X is left untouched");

    free(stage);
    free(desktop);
    free(moving);
}


/* Panning east again once already at the rightmost edge is clamped
 * back to that same edge, recognized as no movement, and is a no-op */
static void s_test_pan_east_clamped_at_edge_is_noop(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 800, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_east(stage);
    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "the viewport origin is clamped back to the rightmost"
            " edge, unchanged");
    TAP_EQ_INT(s_call_stacking_walk, 0,
            "already being at the edge never walks any client");
    TAP_OK(!stage->is_outdated,
            "clamping back to the same edge never marks the stage"
            " outdated");
    TAP_EQ_INT(s_call_scratchpad_panned, 0,
            "a clamped no-op pan never notifies the scratchpad");

    free(stage);
    free(desktop);
}


/* Panning west from the rightmost edge moves the origin back by one
 * whole screen width */
static void s_test_pan_west_moves_origin_back(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 800, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_west(stage);
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the viewport origin moves one whole screen width back"
            " west");
    TAP_OK(stage->is_outdated,
            "a real westward pan marks the stage outdated");

    free(stage);
    free(desktop);
}


/* Panning west from the leftmost edge is clamped back to zero and
 * recognized as no movement */
static void s_test_pan_west_clamped_at_zero_is_noop(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_west(stage);
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the viewport origin is clamped back to zero, unchanged");
    TAP_OK(!stage->is_outdated,
            "clamping back to zero never marks the stage outdated");

    free(stage);
    free(desktop);
}


/* Panning south from the origin with a taller-than-one-screen
 * viewport moves the origin by exactly one screen height */
static void s_test_pan_south_moves_vertical_origin(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 1u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_south(stage);
    TAP_EQ_INT(desktop->viewport_origin.y, 600,
            "the viewport origin moves one whole screen height south");
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "panning south never touches the horizontal origin");
    TAP_OK(stage->is_outdated,
            "a real southward pan marks the stage outdated");

    free(stage);
    free(desktop);
}


/* Panning north from below the top edge moves the origin back by one
 * whole screen height */
static void s_test_pan_north_moves_vertical_origin_back(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 600);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 1u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_north(stage);
    TAP_EQ_INT(desktop->viewport_origin.y, 0,
            "the viewport origin moves one whole screen height back"
            " north");
    TAP_OK(stage->is_outdated,
            "a real northward pan marks the stage outdated");

    free(stage);
    free(desktop);
}


/* Unlike its whole-screen siblings, the keyboard step pan moves the
 * origin by 'viewport.pan-step' pixels alone */
static void s_test_pan_step_east_moves_by_pan_step_pixels(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    config.base.viewport.pan_step = 15u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_step(stage, COMPASS_EAST);
    TAP_EQ_INT(desktop->viewport_origin.x, 15,
            "the viewport origin moves 'viewport.pan-step' pixels"
            " east, not a whole screen");
    TAP_OK(stage->is_outdated,
            "a real step pan marks the stage outdated");

    free(stage);
    free(desktop);
}


/* The keyboard step pan clamps at the pannable area's edge exactly
 * like its whole-screen siblings, even when the configured step would
 * otherwise overshoot it */
static void s_test_pan_step_clamped_at_edge_is_noop(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 795, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    config.base.viewport.pan_step = 15u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_pan_step(stage, COMPASS_EAST);
    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "the origin clamps at the pannable area's east edge"
            " instead of overshooting by the full step");
    TAP_OK(stage->is_outdated,
            "a clamped step that still moves the origin marks the"
            " stage outdated");

    free(stage);
    free(desktop);
}


/* A null stage, or one with no resolvable current desktop, is
 * refused outright by the absolute-origin setter too */
static void s_test_set_null_stage_or_no_desktop_is_noop(void)
{
    config_td config;
    stage_td *stage;

    memset(&config, 0, sizeof(config));
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = NULL;

    scmd_stage_viewport_set(NULL, 100, 200);
    scmd_stage_viewport_set(stage, 100, 200);

    TAP_OK(!stage->is_outdated,
            "a null stage, or one with no resolvable current" \
            " desktop, never outdates anything");

    free(stage);
}


/* Setting an absolute origin within the pannable area moves the
 * viewport there directly and translates every non-sticky client by
 * the resulting delta, just like a directional pan does */
static void s_test_set_moves_to_absolute_origin(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *moving = s_make_client(10, 20, false);
    client_td *clients[1];

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    clients[0] = moving;

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_stage_viewport_set(stage, 800, 600);

    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "the viewport jumps straight to the requested X origin");
    TAP_EQ_INT(desktop->viewport_origin.y, 600,
            "the viewport jumps straight to the requested Y origin");
    TAP_EQ_INT(moving->layout.geometry.cur.pos.x, -790,
            "a non-sticky client's position shifts by the resulting" \
            " delta, the opposite way");
    TAP_OK(stage->is_outdated,
            "a real absolute-origin move marks the stage outdated");

    free(stage);
    free(desktop);
    free(moving);
}


/* A null stage, or one with no resolvable current desktop, is
 * refused outright by the page go-to command too */
static void s_test_goto_null_stage_or_no_desktop_is_noop(void)
{
    config_td config;
    stage_td *stage;

    memset(&config, 0, sizeof(config));
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = NULL;

    scmd_stage_viewport_goto(NULL, 0u);
    scmd_stage_viewport_goto(stage, 0u);

    TAP_OK(!stage->is_outdated,
            "a null stage, or one with no resolvable current" \
            " desktop, never outdates anything through" \
            " scmd_stage_viewport_goto");

    free(stage);
}


/* A page index maps to its own origin in row-major order across the
 * configured grid, and lands there exactly the same way
 * 'scmd_stage_viewport_set' would given that pixel origin */
static void s_test_goto_moves_to_correct_page(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    /* Page 4 (zero-based), 3 columns wide: row 1, column 1 */
    scmd_stage_viewport_goto(stage, 4u);

    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "page 4 across a 3-column grid lands on column 1's" \
            " X origin");
    TAP_EQ_INT(desktop->viewport_origin.y, 600,
            "page 4 across a 3-column grid lands on row 1's" \
            " Y origin");
    TAP_OK(stage->is_outdated,
            "a real page jump marks the stage outdated");

    free(stage);
    free(desktop);
}


/* A page index at or past the configured grid's own page count is
 * refused outright, unlike a pixel origin which would instead be
 * clamped into range */
static void s_test_goto_out_of_range_page_is_noop(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_goto(stage, 4u);

    TAP_OK(s_call_stacking_walk == 0,
            "a page index past the grid's own count walks no client" \
            " at all");
    TAP_OK(!stage->is_outdated,
            "a page index past the grid's own count never outdates" \
            " the stage");

    free(stage);
    free(desktop);
}


/* A requested origin past the pannable area is clamped back to its
 * nearest edge, and requesting the origin already in effect is a
 * no-op */
static void s_test_set_clamps_and_noops_at_same_origin(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_set(stage, 5000, -5000);
    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "an out-of-range X origin is clamped to the rightmost" \
            " edge");
    TAP_EQ_INT(desktop->viewport_origin.y, 0,
            "an out-of-range negative Y origin is clamped to zero");

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;
    stage->is_outdated = false;

    scmd_stage_viewport_set(stage, 800, 0);
    TAP_OK(s_call_stacking_walk == 0,
            "requesting the origin already in effect walks no" \
            " client at all");
    TAP_OK(!stage->is_outdated,
            "requesting the origin already in effect never outdates" \
            " the stage");
    TAP_EQ_INT(s_call_scratchpad_panned, 0,
            "requesting the origin already in effect never notifies" \
            " the scratchpad either");

    free(stage);
    free(desktop);
}


/* A null stage or desktop is refused outright, walking no client */
static void s_test_reclamp_null_guards(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 800, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 1u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    scmd_stage_viewport_reclamp(NULL, desktop);
    scmd_stage_viewport_reclamp(stage, NULL);

    TAP_OK(s_call_stacking_walk == 0,
            "a null stage or desktop never walks any client");

    free(stage);
    free(desktop);
}


/* A desktop whose own origin is pulled back to a still-valid page
 * takes every non-sticky client with it by the resulting delta, the
 * same as an ordinary pan; a client that lands safely within the
 * new, smaller canvas from that translation alone is not moved
 * a second time by the individual per-client pass below */
static void s_test_reclamp_pulls_back_stranded_page(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 1600, 0);
    client_td *stranded = s_make_client(-500, 20, false);
    client_td *clients[1];

    /* The desktop's own origin sits at page index 2 (x = 1600, two
     * whole screens over), left there from before a reload shrank the
     * configured viewport down to 2 columns, whose highest valid page
     * is now index 1 (x = 800).  The client's canvas position (its
     * screen-relative x plus the origin it is being shown against,
     * -500 + 1600 = 1100) sits comfortably within the new 2-column,
     * 1600-wide canvas either way, so only the origin's own
     * correction ever needs to touch it. */
    stranded->layout.geometry.cur.dim.w = 100u;
    stranded->layout.geometry.cur.dim.h = 50u;
    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    clients[0] = stranded;

    s_reset();
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_stage_viewport_reclamp(stage, desktop);

    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "the origin is pulled back to the nearest page the" \
            " shrunk viewport still has");
    TAP_EQ_INT(desktop->viewport_origin.y, 0,
            "the vertical origin, already valid, is left alone");
    TAP_EQ_INT(stranded->layout.geometry.cur.pos.x, 300,
            "a client is translated by the resulting delta, coming" \
            " back into view along with the origin");
    TAP_EQ_INT(stranded->layout.geometry.cur.pos.y, 20,
            "its already-valid vertical position is left alone");
    TAP_OK(stage->is_outdated,
            "an origin that actually changes marks the stage" \
            " outdated");

    free(stage);
    free(desktop);
    free(stranded);
}


/* A client left parked on some other page the shrink also removed,
 * one the desktop was never actually showing and so whose own origin
 * never needed correcting at all, is still individually pulled back
 * onto the one page actually on screen, not merely left somewhere
 * else still technically inside a wider canvas */
static void s_test_reclamp_recovers_client_on_other_vanished_page(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *other_page = s_make_client(1650, 20, false);
    client_td *clients[1];

    /* The desktop's own origin already sits at page index 0, valid
     * before and after the shrink alike, so the origin correction
     * alone (see the test above) would do nothing at all here.  This
     * client's own screen-relative position (1650, unaffected by an
     * origin of zero) belongs to page index 2 of whatever wider
     * viewport used to be configured, past the single 800-wide page
     * now on screen entirely, and its whole 100-wide self, not
     * merely one pixel of it, is pulled back to fit flush against
     * that page's own right edge (700..799), rather than left mostly
     * hanging off it the way a bound of merely "some part of it
     * still inside the wider 1600-wide canvas" (0..1599) would. */
    other_page->layout.geometry.cur.dim.w = 100u;
    other_page->layout.geometry.cur.dim.h = 50u;
    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    clients[0] = other_page;

    s_reset();
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_stage_viewport_reclamp(stage, desktop);

    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the origin, already valid, is left exactly where it was");
    TAP_EQ_INT(other_page->layout.geometry.cur.pos.x, 700,
            "a client on a different, now-vanished page is pulled" \
            " back, whole, onto the one page actually on screen");
    TAP_EQ_INT(other_page->layout.geometry.cur.pos.y, 20,
            "its already-valid vertical position is left alone");

    free(stage);
    free(desktop);
    free(other_page);
}


/* A client wide enough that a mere one-pixel-still-inside tolerance
 * would leave nearly all of it hanging off the page's edge is pulled
 * back whole instead: this is the exact case a live reproduction
 * caught this fix missing on its first attempt, a window seemingly
 * "clamped" yet still invisible in practice */
static void s_test_reclamp_pulls_whole_client_not_one_pixel(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *wide = s_make_client(1000, 20, false);
    client_td *clients[1];

    /* A one-pixel-tolerant clamp would leave this 300-wide client at
     * x = 799 (page_w - 1), footprint 799..1099, all but a single
     * column of it past the 800-wide page's own right edge and
     * nowhere near actually visible.  Pulled back whole instead, it
     * lands flush against that edge: x = 500 (page_w - width),
     * footprint 500..800. */
    wide->layout.geometry.cur.dim.w = 300u;
    wide->layout.geometry.cur.dim.h = 100u;
    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 1u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    clients[0] = wide;

    s_reset();
    s_stub_clients = clients;
    s_stub_client_count = 1u;

    scmd_stage_viewport_reclamp(stage, desktop);

    TAP_EQ_INT(wide->layout.geometry.cur.pos.x, 500,
            "the client's whole width is pulled onto the page, not" \
            " just its leftmost pixel");
    TAP_OK(wide->layout.geometry.cur.pos.x +
                (int32_t) wide->layout.geometry.cur.dim.w <= 800,
            "its right edge actually lands within the page, not" \
            " past it");

    free(stage);
    free(desktop);
    free(wide);
}


/* A desktop whose origin still fits the currently configured viewport
 * (nothing shrank under it, or it never panned off the first page),
 * with no client of its own left outside the canvas either, is left
 * exactly as it is */
static void s_test_reclamp_still_valid_is_noop(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 800, 0);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    stage->is_outdated = false;

    scmd_stage_viewport_reclamp(stage, desktop);

    TAP_EQ_INT(desktop->viewport_origin.x, 800,
            "an origin still within the pannable area is left" \
            " untouched");
    TAP_OK(!stage->is_outdated,
            "a still-valid origin never marks the stage outdated");

    free(stage);
    free(desktop);
}


/* A client already sitting within the desktop's own canvas, but off
 * the currently panned-to page, is left exactly where it is by the
 * defensive clamp: only the viewport itself moves, to bring it into
 * view */
static void s_test_center_on_client_within_canvas_is_not_clamped(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *client = s_make_client(1600, 0, false);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_center_on_client(stage, client);
    TAP_EQ_INT(client->layout.geometry.cur.pos.x, 1600,
            "a client already inside the desktop's canvas is never" \
            " moved by the defensive clamp");
    TAP_EQ_INT(desktop->viewport_origin.x, 1200,
            "the viewport itself pans to bring the off-page client" \
            " into view instead, centered (client X 1600 minus half" \
            " the 800-wide screen)");

    free(stage);
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
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 800, 0);
    client_td *client = s_make_client(-800, 0, false);

    client->layout.geometry.cur.dim.w = 400u;
    client->layout.geometry.cur.dim.h = 300u;

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_center_on_client(stage, client);
    TAP_EQ_INT(client->layout.geometry.cur.pos.x, -800,
            "a client one page west of a panned viewport is left" \
            " exactly where it is, not dragged toward the current" \
            " page");
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "the viewport pans west to the client's own page" \
            " instead");

    free(stage);
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
    stage_td *stage;
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
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_center_on_client(stage, client);
    /* 341 is the frame's own height, content plus titlebar plus
     * borders; centering the content alone would have landed on 160 */
    TAP_EQ_INT(desktop->viewport_origin.y, 170,
            "a client whose frame merely grazes the current page"
            " still pans toward the page the client is actually on,"
            " centering the whole frame rather than its content");
    TAP_EQ_INT(client->layout.geometry.cur.pos.y, -300,
            "and the client itself is left where it was: only the"
            " viewport moved");

    free(stage);
    free(desktop);
    free(client);
}


/* A sticky client belongs to no page at all: it is excluded from the
 * pan translation, so its stored position is where it sits on screen
 * rather than a point on the canvas, and it is in view from every
 * origin.  Reporting the page its corner lands on would have callers
 * send the user somewhere for a window already in front of them */
static void s_test_sticky_client_belongs_to_no_page(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(1920u, 1080u, 1920, 0);
    client_td *sticky = s_make_client(100, 100, true);
    client_td *plain = s_make_client(100, 100, false);
    uint32_t col = 9u;
    uint32_t row = 9u;

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    TAP_OK(!scmd_stage_viewport_client_page(stage, desktop, sticky,
                &col, &row),
            "a sticky client is reported as belonging to no page");
    TAP_OK(col == 9u && row == 9u,
            "and neither output is touched");

    TAP_OK(scmd_stage_viewport_client_page(stage, desktop, plain,
                &col, &row),
            "while a plain client in the very same spot does report"
            " one");

    free(stage);
    free(desktop);
    free(sticky);
    free(plain);
}


/* And centring never pans for one, since there is nowhere it could be
 * brought into view from */
static void s_test_center_on_sticky_never_pans(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(1920u, 1080u, 1920, 0);
    client_td *sticky = s_make_client(100, 100, true);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_center_on_client(stage, sticky);

    TAP_EQ_INT((int) desktop->viewport_origin.x, 1920,
            "centring on a sticky client leaves the origin exactly"
            " where it was");

    free(stage);
    free(desktop);
    free(sticky);
}


/* Centring reads a client's position against the current desktop's
 * own viewport origin, which means nothing for a client belonging to
 * another desktop: the pan would land somewhere arbitrary and the
 * clamp would write a corrected position onto a window this desktop
 * has no business moving.  'focus_apply' reaches here with exactly
 * such a client from the window list and the '_NET_ACTIVE_WINDOW'
 * handler, neither of which switches desktops first */
static void s_test_center_on_client_of_another_desktop(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(1920u, 1080u, 1920, 0);
    desktop_td *elsewhere = s_make_desktop(1920u, 1080u, 0, 0);
    client_td *client = s_make_client(-4000, 100, false);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = elsewhere;

    scmd_stage_viewport_center_on_client(stage, client);

    TAP_EQ_INT((int) desktop->viewport_origin.x, 1920,
            "a client on another desktop never pans this one");
    TAP_EQ_INT((int) client->layout.geometry.cur.pos.x, -4000,
            "and its position is left untouched, clamp included");

    free(stage);
    free(desktop);
    free(elsewhere);
    free(client);
}


/* A desktop whose geometry has not been resolved yet reports page
 * zero rather than dividing by zero, which is undefined behavior and
 * not something a caller could recover from */
static void s_test_page_lookup_on_zero_geometry(void)
{
    config_td config;
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(0u, 0u, 0, 0);
    client_td *client = s_make_client(100, 100, false);
    uint32_t col = 9u;
    uint32_t row = 9u;

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    TAP_OK(scmd_stage_viewport_client_page(stage, desktop, client,
                &col, &row),
            "a zero-sized desktop still answers the client page"
            " query");
    TAP_OK(col == 0u && row == 0u,
            "and answers page zero rather than dividing by zero");

    free(stage);
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
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *client = s_make_client(50000, -50000, false);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 2u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_center_on_client(stage, client);
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
    TAP_OK(stage->is_outdated,
            "bringing an out-of-canvas client back in view marks the" \
            " stage outdated");

    free(stage);
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
    stage_td *stage;
    desktop_td *desktop = s_make_desktop(800u, 600u, 0, 0);
    client_td *client = s_make_client(100, 100, false);

    memset(&config, 0, sizeof(config));
    config.base.screens[0].viewport.columns = 2u;
    config.base.screens[0].viewport.rows = 1u;
    stage = s_make_stage(&config, 0u);

    s_reset();
    s_stub_desktop = desktop;
    s_client_desktop = desktop;

    scmd_stage_viewport_center_on_client(stage, client);
    TAP_EQ_INT(desktop->viewport_origin.x, 0,
            "a client already visible on the current page never" \
            " causes a pan");
    TAP_OK(!stage->is_outdated,
            "and never marks the stage outdated either");

    free(stage);
    free(desktop);
    free(client);
}


int main(void)
{
    TAP_PLAN(106);

    s_test_pan_null_stage();
    s_test_pan_no_desktop_is_noop();
    s_test_pan_no_config_falls_back_to_1x1();
    s_test_pan_id_past_max_screens_falls_back_to_1x1();
    s_test_pan_east_moves_and_translates_clients();
    s_test_pan_east_keeps_fullscreen_in_place();
    s_test_pan_east_notifies_scratchpad_of_real_pan();
    s_test_pan_east_skips_drag_excluded_client();
    s_test_pan_east_also_translates_mapped_icon();
    s_test_pan_east_twice_keeps_translating_negative_icon();
    s_test_pan_east_leaves_icon_when_pan_icons_off();
    s_test_pan_east_clamped_at_edge_is_noop();
    s_test_pan_west_moves_origin_back();
    s_test_pan_west_clamped_at_zero_is_noop();
    s_test_pan_south_moves_vertical_origin();
    s_test_pan_north_moves_vertical_origin_back();
    s_test_pan_step_east_moves_by_pan_step_pixels();
    s_test_pan_step_clamped_at_edge_is_noop();
    s_test_set_null_stage_or_no_desktop_is_noop();
    s_test_set_moves_to_absolute_origin();
    s_test_set_clamps_and_noops_at_same_origin();
    s_test_reclamp_null_guards();
    s_test_reclamp_pulls_back_stranded_page();
    s_test_reclamp_recovers_client_on_other_vanished_page();
    s_test_reclamp_pulls_whole_client_not_one_pixel();
    s_test_reclamp_still_valid_is_noop();
    s_test_goto_null_stage_or_no_desktop_is_noop();
    s_test_goto_moves_to_correct_page();
    s_test_goto_out_of_range_page_is_noop();
    s_test_center_on_client_within_canvas_is_not_clamped();
    s_test_center_on_client_from_panned_origin();
    s_test_center_on_client_touching_page_edge();
    s_test_center_on_client_of_another_desktop();
    s_test_sticky_client_belongs_to_no_page();
    s_test_center_on_sticky_never_pans();
    s_test_page_lookup_on_zero_geometry();
    s_test_center_on_client_outside_canvas_is_clamped();
    s_test_center_on_client_already_visible_is_noop();

    return TAP_DONE();
}
