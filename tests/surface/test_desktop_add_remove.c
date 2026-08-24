/**
 * @file tests/surface/test_desktop_add_remove.c
 *
 * @brief Test battery for the desktop-grid growing on
 *        'surface_action_desktop_add' and shrinking on
 *        'surface_action_desktop_remove' (surface/switch.c)
 *
 * 'surface_action_desktop_add'/'_remove' pull in far more of the
 * project than the previous two test files in this same directory:
 * 'desktop_init' (needs a live XCB connection to build a real
 * desktop_td at all) and eight further functions client movement,
 * work-area refresh, and restricted-memory mode checking depend on.
 * Every one of them is stood in for here, link-only, the same way
 * 'test_lookup.c' already stands in for 'surface_desktop_get': none
 * of them are exercised for their own real behavior, since every
 * desktop this file ever builds stays empty of clients throughout
 * (client movement during removal is a no-op on an empty desktop
 * either way), only for the one effect this file actually checks,
 * the shape 's_surface_layout_grow_for'/'_shrink_after' (both
 * private to surface/switch.c) leave 'desktop_layout' in afterward.
 *
 * @note Unlike the previous two files in this directory, the
 *       precise signatures below could not be compile-verified in
 *       the environment this file was written in (missing XCB/RandR
 *       development headers); double-check each stand-in against its
 *       own real declaration if this file fails to build.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Local includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <memguard.h>
#include <surface.h>


/** Link-only stand-in for desktop_init (desktop.c): builds a
 *  minimal, usable desktop_td without the real XCB screen-setup
 *  work that function does, which this file has no live connection
 *  to perform.  Only 'id' matters to anything this file checks. */
desktop_td *desktop_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, uint32_t screen_id,
        uint32_t desktop_id, config_td *config)
{
    desktop_td *desktop = calloc(1, sizeof(desktop_td));

    (void) connection;
    (void) ewmh;
    (void) screen_id;
    (void) config;
    desktop->id = desktop_id;
    return desktop;
}


/** Link-only stand-in for desktop_destroy (desktop.c) */
void desktop_destroy(desktop_td *desktop)
{
    free(desktop);
}


/** Link-only stand-in for desktop_mark_outdated (desktop.c): this
 *  file never reads the flag it would set, only whether the call
 *  itself is safe against a hand-built desktop_td */
void desktop_mark_outdated(desktop_td *desktop)
{
    (void) desktop;
}


/** Link-only stand-in for desktop_action_client_move (desktop/
 *  dclient.c): moving a client between desktops is what the real one
 *  does through the client list this file's own hand-built
 *  desktop_td does not carry, so it only ever reports success */
int desktop_action_client_move(desktop_td *from, desktop_td *to,
        client_td *client)
{
    (void) from;
    (void) to;
    (void) client;
    return 0;
}


/** Link-only stand-in for memguard_max_clients (memguard.c): every
 *  test here runs as an ordinary, unrestricted session */
uint32_t memguard_max_clients(void)
{
    return 0u;
}


/** Link-only stand-in for desktop_action_client_add (desktop.c):
 *  never actually reached, since every desktop this file builds
 *  stays empty of clients throughout, but 's_surface_desktop_
 *  evacuate' (surface/switch.c) references it regardless */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}


/** Link-only stand-in for desktop_action_client_rem (desktop.c);
 *  see desktop_action_client_add's own comment just above */
int desktop_action_client_rem(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    return 0;
}


/** Link-only stand-in for ccmd_client_refill_maximized
 *  (cmds/client/geom.c); see desktop_action_client_add's own
 *  comment above for why this is never actually reached */
void ccmd_client_refill_maximized(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for ccmd_client_relocate_icon_if_taken
 *  (cmds/client/basic.c); see desktop_action_client_add's own
 *  comment above for why this is never actually reached */
void ccmd_client_relocate_icon_if_taken(client_td *client)
{
    (void) client;
}


/** Link-only stand-in for surface_clients_hide (surface/actions/
 *  clients.c): only ever called on the desktop being switched away
 *  from when the desktop being removed was the current one, a case
 *  this file's own tests deliberately avoid (see each test's own
 *  comment on why 'desktop_cur' is kept away from the one removed) */
void surface_clients_hide(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
}


/** Link-only stand-in for surface_clients_show (surface/actions/
 *  clients.c); see surface_clients_hide's own comment just above */
void surface_clients_show(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
}


/** Link-only stand-in for surface_refresh_workareas (surface/
 *  workareas.c): this file never reads a desktop's own work area */
void surface_refresh_workareas(surface_td *surface)
{
    (void) surface;
}


/* The two stand-ins below exist purely because surface/switch.c
 * compiles as a single translation unit: surface_action_toggle_
 * strutless_maximize, a third public function in that file this
 * file never calls at all, references xcb_flush; s_surface_desktop_
 * evacuate (only reachable through surface_action_desktop_remove,
 * which this file does call) references xcb_change_property, but
 * only inside a branch guarded by a client actually being present
 * to evacuate, which never happens here since every desktop this
 * file builds stays empty throughout. */

/** Link-only stand-in for xcb_flush (libxcb) */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


/** Link-only stand-in for xcb_change_property (libxcb) */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *c, uint8_t mode,
        xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
        uint8_t format, uint32_t data_len, const void *data)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) mode;
    (void) window;
    (void) property;
    (void) type;
    (void) format;
    (void) data_len;
    (void) data;
    return cookie;
}


static void s_destroy_desktop(void *data)
{
    free(data);
}


/**
 * @brief Build a surface_td with 'desktop_count' desktops (ids
 *        0..desktop_count-1) and the given grid layout configured
 *        for screen 0, current desktop fixed to 0
 *
 * 'desktop_cur' is deliberately always 0, and every test in this
 * file only ever removes the highest-numbered desktop while
 * 'desktop_count' is well above 1: 0 is never the one being
 * removed, so 'surface_clients_hide'/'_show' (stood in for above)
 * are never actually reached, keeping every test here focused
 * purely on the resulting grid shape.
 */
static surface_td *s_make_surface(uint32_t desktop_count,
        enum config_desktop_orientation_e orientation,
        uint32_t rows, uint32_t columns)
{
    surface_td *surface = calloc(1, sizeof(surface_td));
    config_td *config = calloc(1, sizeof(config_td));
    desktop_td *desktop;

    surface->id = 0u;
    surface->desktop_count = desktop_count;
    surface->desktop_cur = 0u;
    surface->desktops = cdlist_init(s_destroy_desktop);

    for (uint32_t i = 0u; i < desktop_count; ++i) {
        desktop = calloc(1, sizeof(desktop_td));
        desktop->id = i;
        cdlist_ins_next(surface->desktops, cdlist_tail(surface->desktops),
                desktop);
    }

    config->base.screens[0].desktop_layout.orientation = orientation;
    config->base.screens[0].desktop_layout.corner =
        CONFIG_DESKTOP_CORNER_TOP_LEFT;
    config->base.screens[0].desktop_layout.rows = rows;
    config->base.screens[0].desktop_layout.columns = columns;
    surface->config = config;

    return surface;
}


static void s_destroy_surface(surface_td *surface)
{
    cdlist_destroy(surface->desktops);
    free(surface->config);
    free(surface);
}


/* Adding to a full 2x3 grid (6/6) opens a new row, never touching
 * 'columns': the exact scenario hand-verified earlier when this
 * feature was designed */
static void s_test_add_grows_rows_not_columns(void)
{
    surface_td *surface = s_make_surface(6,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL, 2u, 3u);

    TAP_EQ_INT(surface_action_desktop_add(surface), 0,
            "add to a full 2x3 grid succeeds");
    TAP_EQ_INT((int) surface->config->base.screens[0].desktop_layout.rows,
            3, "grows to 3 rows");
    TAP_EQ_INT(
            (int) surface->config->base.screens[0].desktop_layout.columns,
            3, "columns stays untouched at 3, not reshuffled");
    TAP_EQ_INT((int) surface->desktop_count, 7,
            "desktop_count is now 7");

    s_destroy_surface(surface);
}


/* Adding when a gap cell already exists (5 desktops in a 2x3 grid)
 * simply fills it; the grid does not grow at all */
static void s_test_add_fills_existing_gap_without_growing(void)
{
    surface_td *surface = s_make_surface(5,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL, 2u, 3u);

    TAP_EQ_INT(surface_action_desktop_add(surface), 0,
            "add into an existing gap succeeds");
    TAP_EQ_INT((int) surface->config->base.screens[0].desktop_layout.rows,
            2, "rows stays at 2: the gap absorbed the new desktop");
    TAP_EQ_INT(
            (int) surface->config->base.screens[0].desktop_layout.columns,
            3, "columns stays at 3 too");

    s_destroy_surface(surface);
}


/* Removing from a full 3x3 grid (9) down to 7 leaves the last row
 * with one other member still in it: the grid's own shape is
 * unaffected */
static void s_test_remove_leaves_shape_when_row_not_yet_empty(void)
{
    surface_td *surface = s_make_surface(9,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL, 3u, 3u);

    TAP_EQ_INT(surface_action_desktop_remove(surface), 0,
            "first removal (9 -> 8) succeeds");
    TAP_EQ_INT(surface_action_desktop_remove(surface), 0,
            "second removal (8 -> 7) succeeds");
    TAP_EQ_INT((int) surface->config->base.screens[0].desktop_layout.rows,
            3, "row 2 still has desktop 6 left in it: shape unaffected");
    TAP_EQ_INT((int) surface->desktop_count, 7, "desktop_count is now 7");

    s_destroy_surface(surface);
}


/* Removing the very last member of the last row shrinks the grid
 * back down by one row: the exact inverse of the growth above */
static void s_test_remove_shrinks_when_row_becomes_empty(void)
{
    surface_td *surface = s_make_surface(9,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL, 3u, 3u);

    surface_action_desktop_remove(surface);   /* 9 -> 8 */
    surface_action_desktop_remove(surface);   /* 8 -> 7 */
    TAP_EQ_INT(surface_action_desktop_remove(surface), 0,
            "third removal (7 -> 6) succeeds");
    TAP_EQ_INT((int) surface->config->base.screens[0].desktop_layout.rows,
            2, "row 2 is now empty: shrinks back to 2 rows");
    TAP_EQ_INT(
            (int) surface->config->base.screens[0].desktop_layout.columns,
            3, "columns stays untouched at 3 throughout");
    TAP_EQ_INT((int) surface->desktop_count, 6, "desktop_count is now 6");

    s_destroy_surface(surface);
}


/* A full add-then-remove round trip returns the grid to exactly its
 * starting shape, with no drift */
static void s_test_add_then_remove_round_trip(void)
{
    surface_td *surface = s_make_surface(6,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL, 2u, 3u);

    surface_action_desktop_add(surface);      /* 6 -> 7, grows to 3x3 */
    surface_action_desktop_remove(surface);   /* 7 -> 6, shrinks back */
    TAP_EQ_INT((int) surface->config->base.screens[0].desktop_layout.rows,
            2, "round trip: back to 2 rows, matching the start");
    TAP_EQ_INT(
            (int) surface->config->base.screens[0].desktop_layout.columns,
            3, "round trip: columns unchanged at 3 throughout");
    TAP_EQ_INT((int) surface->desktop_count, 6,
            "round trip: desktop_count back to 6");

    s_destroy_surface(surface);
}


/* The grid never shrinks below 1 row: once down to a single row,
 * further removals just leave more of a gap in it */
static void s_test_never_shrinks_below_one_row(void)
{
    surface_td *surface = s_make_surface(3,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL, 1u, 3u);

    TAP_EQ_INT(surface_action_desktop_remove(surface), 0,
            "removal from an already single-row grid succeeds");
    TAP_EQ_INT((int) surface->config->base.screens[0].desktop_layout.rows,
            1, "rows floor stays at 1, never shrinks further");
    TAP_EQ_INT(
            (int) surface->config->base.screens[0].desktop_layout.columns,
            3, "columns unaffected, now with one gap cell in it");

    s_destroy_surface(surface);
}


/* Growing a vertical-orientation grid opens a new column instead,
 * never touching 'rows': the mirror image of the horizontal case */
static void s_test_add_grows_columns_for_vertical_orientation(void)
{
    surface_td *surface = s_make_surface(6,
            CONFIG_DESKTOP_ORIENTATION_VERTICAL, 3u, 2u);

    TAP_EQ_INT(surface_action_desktop_add(surface), 0,
            "add to a full vertical 3x2 grid succeeds");
    TAP_EQ_INT((int) surface->config->base.screens[0].desktop_layout.rows,
            3, "rows stays untouched at 3 for vertical orientation");
    TAP_EQ_INT(
            (int) surface->config->base.screens[0].desktop_layout.columns,
            3, "columns grows to 3 instead");

    s_destroy_surface(surface);
}


int main(void)
{
    TAP_PLAN(24);

    s_test_add_grows_rows_not_columns();
    s_test_add_fills_existing_gap_without_growing();
    s_test_remove_leaves_shape_when_row_not_yet_empty();
    s_test_remove_shrinks_when_row_becomes_empty();
    s_test_add_then_remove_round_trip();
    s_test_never_shrinks_below_one_row();
    s_test_add_grows_columns_for_vertical_orientation();

    return TAP_DONE();
}
