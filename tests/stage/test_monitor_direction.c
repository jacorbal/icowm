/**
 * @file tests/stage/test_monitor_direction.c
 *
 * @brief Test battery for real-geometry monitor direction resolution
 *        (stage/monitors.c)
 *
 * @a stage_monitor_direction calls nothing external at all: it is
 * pure arithmetic over @c stage->monitors[]/@c monitor_count, no
 * RandR, no XCB connection, nothing to stand in for.  Every shape
 * exercised here was first hand-verified in isolation (a 2x2 grid, a
 * row of 3, and a mixed-size arrangement where the nearest monitor
 * by center, not the largest or the first found, must win) before
 * being written as a C assertion.
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
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <monitor.h>
#include <stage.h>
#include <stage/monitor.h>
#include <types/direction.h>


/** Link-only stand-in for atom_name (utils/xcb/atom.c): stage/
 *  monitors.c as a whole references it (from stage_refresh_
 *  monitors, which this file deliberately never calls -- every
 *  monitor here is populated by hand, no live X connection or RandR
 *  round trip involved), so the linker needs a definition for it
 *  somewhere even though nothing here ever calls it.  The real
 *  xcb_randr_* functions monitors.c also references are instead
 *  linked from the genuine libxcb-randr this test's own build recipe
 *  pulls in (see tests/Makefile.mk): unlike this one small utility
 *  function, their own reply structs are XCB-protocol-generated and
 *  not something safe to reconstruct a stand-in for by hand. */
bool atom_name(xcb_connection_t *connection, xcb_atom_t atom,
        char *out_name, size_t out_name_size)
{
    (void) connection;
    (void) atom;
    if (out_name != NULL && out_name_size > 0u) {
        out_name[0] = '\0';
    }
    return false;
}


/**
 * @brief Build a stage_td with the given monitors populated,
 *        nothing else set
 */
static stage_td s_make_stage(const monitor_td *monitors,
        uint32_t count)
{
    stage_td stage;

    memset(&stage, 0, sizeof(stage));
    stage.monitor_count = count;
    for (uint32_t i = 0u; i < count; ++i) {
        stage.monitors[i] = monitors[i];
    }
    return stage;
}


/**
 * @brief Assert that a monitor's own @p x/@p y match the expected
 *        top-left corner, in one call
 */
static void s_tap_monitor_at(monitor_td m, int32_t x, int32_t y,
        const char *desc)
{
    TAP_OK(m.x == x && m.y == y, desc);
}


/* [top-left    ][top-right   ]
 * [bottom-left ][bottom-right]
 * Four equal 1920x1080 monitors in a 2x2 grid */
static void s_test_2x2_grid(void)
{
    monitor_td monitors[4] = {
        {0, 0, 1920u, 1080u},       /* top-left */
        {1920, 0, 1920u, 1080u},    /* top-right */
        {0, 1080, 1920u, 1080u},    /* bottom-left */
        {1920, 1080, 1920u, 1080u}, /* bottom-right */
    };
    stage_td stage = s_make_stage(monitors, 4u);

    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[0],
                COMPASS_EAST),
            1920, 0, "2x2 grid: east from top-left reaches top-right");
    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[0],
                COMPASS_SOUTH),
            0, 1080,
            "2x2 grid: south from top-left reaches bottom-left");
    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[3],
                COMPASS_NORTH),
            1920, 0,
            "2x2 grid: north from bottom-right reaches top-right");
    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[3],
                COMPASS_WEST),
            0, 1080,
            "2x2 grid: west from bottom-right reaches bottom-left");
    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[0],
                COMPASS_NORTH),
            0, 0,
            "2x2 grid: north from top-left finds nothing, stays put"
            " (never wraps)");
}


/* [monitor 0][monitor 1][monitor 2]
 * Three equal monitors side by side */
static void s_test_row_of_three(void)
{
    monitor_td monitors[3] = {
        {0, 0, 1920u, 1080u},
        {1920, 0, 1920u, 1080u},
        {3840, 0, 1920u, 1080u},
    };
    stage_td stage = s_make_stage(monitors, 3u);

    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[1],
                COMPASS_EAST),
            3840, 0, "row of 3: east from the middle reaches the last");
    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[1],
                COMPASS_WEST),
            0, 0, "row of 3: west from the middle reaches the first");
    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[0],
                COMPASS_WEST),
            0, 0, "row of 3: west from the first stays put, nothing"
            " further west");
    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[0],
                COMPASS_NORTH),
            0, 0, "row of 3: north from any of them stays put,"
            " nothing above a single row");
}


/* One large monitor, plus two small ones stacked to its east: the
 * one closer by center, not simply the first found, must win */
static void s_test_closest_by_center_with_mixed_sizes(void)
{
    monitor_td monitors[3] = {
        {0, 0, 1920u, 1080u},          /* large, on the west */
        {1920, 200, 800u, 600u},       /* small, upper-east */
        {1920, 900, 800u, 600u},       /* small, lower-east */
    };
    stage_td stage = s_make_stage(monitors, 3u);
    monitor_td result;

    /* Large monitor's own center: (960, 540).  Upper-east center:
     * (2320, 500), dy = -40.  Lower-east center: (2320, 1200),
     * dy = 660.  The upper one is closer by that measure and must
     * win, even though both are found scanning the same array. */
    result = stage_monitor_direction(&stage, monitors[0],
            COMPASS_EAST);
    TAP_OK(result.x == 1920 && result.y == 200,
            "mixed sizes: east from the large monitor picks the"
            " nearer-by-center small one (upper), not the farther"
            " one (lower)");
}


/* A single monitor: every direction is a no-op, staying exactly
 * where it started */
static void s_test_single_monitor_never_moves(void)
{
    monitor_td monitors[1] = {
        {0, 0, 1920u, 1080u},
    };
    stage_td stage = s_make_stage(monitors, 1u);

    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[0],
                COMPASS_EAST),
            0, 0, "single monitor: east stays put");
    s_tap_monitor_at(
            stage_monitor_direction(&stage, monitors[0],
                COMPASS_NORTH),
            0, 0, "single monitor: north stays put");
}


int main(void)
{
    TAP_PLAN(12);

    s_test_2x2_grid();
    s_test_row_of_three();
    s_test_closest_by_center_with_mixed_sizes();
    s_test_single_monitor_never_moves();

    return TAP_DONE();
}
