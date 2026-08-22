/**
 * @file tests/surface/test_desktop_grid.c
 *
 * @brief Test battery for the desktop-grid row/column math and
 *        compass-direction navigation (surface/desktops.c)
 *
 * Builds minimal @c surface_td/@c desktop_td/@c config_td fixtures by
 * hand, the same way @c test_lookup.c already does, rather than going
 * through @c surface_init/@c desktop_init (both need a live XCB
 * connection to build one at all): only the fields @c surface_desktop_
 * north/@c south/@c east/@c west and @c surface_desktop_row_col
 * themselves actually read are populated.  Every grid shape exercised
 * here (orientation, corner, and the deliberate gap cases) was first
 * hand-verified in isolation before being written as a C assertion,
 * matching how this whole feature was originally designed; see the
 * per-test comments for what each one checks and why.
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

/* ADT includes */
#include <adt/cdlist.h>

/* Local includes */
#include <config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <surface.h>


static void s_destroy_desktop(void *data)
{
    free(data);
}


/** Link-only stand-in for desktop_destroy (desktop.c): surface/
 *  desktops.c as a whole references it (from surface_desktop_rem,
 *  which this file deliberately never calls -- every desktop here
 *  is torn down by 's_destroy_desktop' above instead, via
 *  'cdlist_destroy'), so the linker needs a definition for it
 *  somewhere even though nothing here ever calls it.  Pulling in the
 *  real desktop.c for this alone would drag in its own large,
 *  uncertain dependency chain just to satisfy the linker for a
 *  function this file never exercises. */
void desktop_destroy(desktop_td *desktop)
{
    (void) desktop;
}


/**
 * @brief Build a desktop_td with the given id, nothing else
 *        populated
 */
static desktop_td *s_make_desktop(uint32_t id)
{
    desktop_td *desktop = calloc(1, sizeof(desktop_td));

    desktop->id = id;
    return desktop;
}


/**
 * @brief Build a surface_td with 'desktop_count' desktops (ids
 *        0..desktop_count-1) and the given grid layout configured
 *        for screen 0
 *
 * @param desktop_count Number of desktops (and cdlist entries) to
 *                       build
 * @param orientation    'layout.orientation' to configure
 * @param corner         'layout.corner' to configure
 * @param rows           'layout.rows' to configure
 * @param columns        'layout.columns' to configure
 * @param wrap           'desktops.wrap_at_bounds' to configure
 */
static surface_td *s_make_surface(uint32_t desktop_count,
        enum config_desktop_orientation_e orientation,
        enum config_desktop_corner_e corner,
        uint32_t rows, uint32_t columns, bool wrap)
{
    surface_td *surface = calloc(1, sizeof(surface_td));
    config_td *config = calloc(1, sizeof(config_td));

    surface->id = 0u;
    surface->desktop_count = desktop_count;
    surface->desktop_cur = 0u;
    surface->desktops = cdlist_init(s_destroy_desktop);

    for (uint32_t i = 0u; i < desktop_count; ++i) {
        cdlist_ins_next(surface->desktops, cdlist_tail(surface->desktops),
                s_make_desktop(i));
    }

    config->base.screens[0].desktop_layout.orientation = orientation;
    config->base.screens[0].desktop_layout.corner = corner;
    config->base.screens[0].desktop_layout.rows = rows;
    config->base.screens[0].desktop_layout.columns = columns;
    config->desktops.wrap_at_bounds = wrap;
    surface->config = config;

    return surface;
}


static void s_destroy_surface(surface_td *surface)
{
    cdlist_destroy(surface->desktops);
    free(surface->config);
    free(surface);
}


/**
 * @brief Assert that a desktop pointer is non-NULL and has the
 *        expected id, in one call
 */
static void s_tap_desktop_id(const desktop_td *d, uint32_t expected,
        const char *desc)
{
    if (TAP_NOT_NULL(d, desc)) {
        TAP_EQ_INT((int) d->id, (int) expected, desc);
    } else {
        /* Keep the plan count exact even on the NULL branch: one
         * TAP_NOT_NULL above already counted this assertion, so a
         * second call here would count it twice against a single
         * logical check. */
        tap_ok_impl(0, desc, __FILE__, __LINE__);
    }
}


/* [0][1][2]
 * [3][4][5]
 * Horizontal, top-left, 6 desktops, no wrap: the plain reading order
 * every screen without a configured layout already has */
static void s_test_horizontal_top_left_basic(void)
{
    surface_td *surface = s_make_surface(6,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL,
            CONFIG_DESKTOP_CORNER_TOP_LEFT, 2, 3, false);

    s_tap_desktop_id(surface_desktop_east(surface, 0, false), 1,
            "horizontal top-left: east from 0 reaches 1");
    s_tap_desktop_id(surface_desktop_south(surface, 0, false), 3,
            "horizontal top-left: south from 0 reaches 3");
    s_tap_desktop_id(surface_desktop_north(surface, 3, false), 0,
            "horizontal top-left: north from 3 reaches 0");
    s_tap_desktop_id(surface_desktop_west(surface, 5, false), 4,
            "horizontal top-left: west from 5 reaches 4");
    TAP_OK(surface_desktop_north(surface, 0, false) == NULL,
            "horizontal top-left: north from 0 (top row) finds"
            " nothing without wrap");
    TAP_OK(surface_desktop_east(surface, 5, false) == NULL,
            "horizontal top-left: east from 5 (grid corner) finds"
            " nothing without wrap");

    s_destroy_surface(surface);
}


/* [0][2][4]
 * [1][3][5]
 * Vertical orientation: each column fills before moving to the next
 * one */
static void s_test_vertical_orientation(void)
{
    surface_td *surface = s_make_surface(6,
            CONFIG_DESKTOP_ORIENTATION_VERTICAL,
            CONFIG_DESKTOP_CORNER_TOP_LEFT, 2, 3, false);

    s_tap_desktop_id(surface_desktop_south(surface, 0, false), 1,
            "vertical: south from 0 reaches 1 (same column)");
    s_tap_desktop_id(surface_desktop_east(surface, 0, false), 2,
            "vertical: east from 0 reaches 2 (next column)");
    s_tap_desktop_id(surface_desktop_east(surface, 1, false), 3,
            "vertical: east from 1 reaches 3 (next column, same row)");

    s_destroy_surface(surface);
}


/* [2][1][0]
 * [5][4][3]
 * Top-right corner: desktop 0 starts at the top right, IDs advance
 * leftward across each row */
static void s_test_top_right_corner(void)
{
    surface_td *surface = s_make_surface(6,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL,
            CONFIG_DESKTOP_CORNER_TOP_RIGHT, 2, 3, false);
    uint32_t row = 999u;
    uint32_t col = 999u;

    s_tap_desktop_id(surface_desktop_west(surface, 0, false), 1,
            "top-right corner: west from 0 reaches 1 (IDs advance"
            " leftward from the top-right start)");
    s_tap_desktop_id(surface_desktop_south(surface, 0, false), 3,
            "top-right corner: south from 0 reaches 3");
    TAP_OK(surface_desktop_row_col(surface, 0, &row, &col),
            "top-right corner: row_col succeeds for desktop 0");
    TAP_EQ_INT((int) row, 0, "top-right corner: desktop 0 is row 0");
    TAP_EQ_INT((int) col, 2,
            "top-right corner: desktop 0 is column 2 (rightmost),"
            " not column 0");

    s_destroy_surface(surface);
}


/* [0][1][2]
 * [3][4][ ]
 * 5 desktops in a 2x3 grid: the trailing cell is a desktop-less gap.
 * Navigation must step past it, never land on it */
static void s_test_gap_is_skipped(void)
{
    surface_td *surface = s_make_surface(5,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL,
            CONFIG_DESKTOP_CORNER_TOP_LEFT, 2, 3, true);
    desktop_td *wrapped;

    s_tap_desktop_id(surface_desktop_east(surface, 4, true), 3,
            "gap: east from 4 wraps within its own row to 3,"
            " skipping the gap at (1,2)");
    TAP_OK(surface_desktop_east(surface, 4, false) == NULL,
            "gap: east from 4 without wrap finds nothing (the only"
            " thing further east is the gap itself)");
    wrapped = surface_desktop_south(surface, 2, true);
    TAP_OK(wrapped != NULL && wrapped->id == 2,
            "gap: south from 2, wrapping, lands back on 2 itself"
            " (column 2 has no other real desktop to reach)");

    s_destroy_surface(surface);
}


/* Linear degenerate case: rows == 1 means there is no "north" or
 * "south" at all without wrap, and east/west behave exactly like
 * the classic prev/next list navigation */
static void s_test_linear_rows_one_is_east_west_only(void)
{
    surface_td *surface = s_make_surface(4,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL,
            CONFIG_DESKTOP_CORNER_TOP_LEFT, 1, 4, false);

    TAP_OK(surface_desktop_north(surface, 2, false) == NULL,
            "linear (rows=1): north finds nothing without wrap");
    TAP_OK(surface_desktop_south(surface, 2, false) == NULL,
            "linear (rows=1): south finds nothing either, without wrap");
    s_tap_desktop_id(surface_desktop_east(surface, 3, true), 0,
            "linear (rows=1): east from the last one wraps to 0,"
            " exactly like the classic 'next' used to");
    s_tap_desktop_id(surface_desktop_west(surface, 0, true), 3,
            "linear (rows=1): west from the first one wraps to 3,"
            " exactly like the classic 'prev' used to");

    s_destroy_surface(surface);
}


/* With wrap-at-bounds enabled, north/south on a rows=1 grid does not
 * return NULL: with only one row, wrapping the row axis lands right
 * back where it started, the exact same "sparse column wraps to
 * itself" behavior a gap-adjacent column shows (see
 * s_test_gap_is_skipped above), not a bug */
static void s_test_linear_rows_one_wrap_returns_to_self(void)
{
    surface_td *surface = s_make_surface(4,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL,
            CONFIG_DESKTOP_CORNER_TOP_LEFT, 1, 4, true);
    desktop_td *wrapped_n;
    desktop_td *wrapped_s;

    wrapped_n = surface_desktop_north(surface, 2, true);
    TAP_OK(wrapped_n != NULL && wrapped_n->id == 2,
            "linear (rows=1), wrap enabled: north wraps back to the"
            " same desktop, since there is only one row to wrap"
            " within");
    wrapped_s = surface_desktop_south(surface, 2, true);
    TAP_OK(wrapped_s != NULL && wrapped_s->id == 2,
            "linear (rows=1), wrap enabled: south does the same");

    s_destroy_surface(surface);
}


/* Symmetric linear case: columns == 1 means purely north/south,
 * never east or west */
static void s_test_linear_columns_one_is_north_south_only(void)
{
    surface_td *surface = s_make_surface(3,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL,
            CONFIG_DESKTOP_CORNER_TOP_LEFT, 3, 1, false);

    TAP_OK(surface_desktop_east(surface, 1, false) == NULL,
            "linear (columns=1): east always finds nothing");
    TAP_OK(surface_desktop_west(surface, 1, false) == NULL,
            "linear (columns=1): west always finds nothing either");
    s_tap_desktop_id(surface_desktop_south(surface, 0, false), 1,
            "linear (columns=1): south from 0 reaches 1");
    s_tap_desktop_id(surface_desktop_south(surface, 1, false), 2,
            "linear (columns=1): south from 1 reaches 2");

    s_destroy_surface(surface);
}


int main(void)
{
    TAP_PLAN(41);

    s_test_horizontal_top_left_basic();
    s_test_vertical_orientation();
    s_test_top_right_corner();
    s_test_gap_is_skipped();
    s_test_linear_rows_one_is_east_west_only();
    s_test_linear_rows_one_wrap_returns_to_self();
    s_test_linear_columns_one_is_north_south_only();

    return TAP_DONE();
}
