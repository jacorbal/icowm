/**
 * @file tests/policy/test_tiling.c
 *
 * @brief Test battery for icon placement policy
 *
 * Both place_icon and icon_avoid_systray_overlap are pure
 * computation: neither calls into XCB or any other module at all,
 * so no stand-ins are needed here, only hand-computed expectations
 * against WM_ICON_GRID_MARGIN (8) and WM_ICON_SYSTRAY_GAP (8).
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Local includes */
#include <harness/tap.h>
#include <policy/placement.h>


/* place_icon's own guard clauses: any missing required argument is
 * a safe no-op that never touches out_x/out_y */
static void s_test_icon_guards(void)
{
    client_td client;
    struct config_theme_s theme;
    int16_t out_x = 999;
    int16_t out_y = 999;

    memset(&client, 0, sizeof(client));
    memset(&theme, 0, sizeof(theme));
    client.theme = &theme;

    place_icon(NULL, NULL, CONFIG_ICON_PLACEMENT_BOTTOM, 32u, 32u,
            800u, 600u, &out_x, &out_y);
    TAP_EQ_INT(out_x, 999, "a NULL client leaves out_x untouched");

    place_icon(&client, NULL, CONFIG_ICON_PLACEMENT_BOTTOM, 32u, 32u,
            800u, 600u, NULL, &out_y);
    TAP_OK(true, "a NULL out_x is a safe no-op, no crash");

    client.theme = NULL;
    place_icon(&client, NULL, CONFIG_ICON_PLACEMENT_BOTTOM, 32u, 32u,
            800u, 600u, &out_x, &out_y);
    TAP_EQ_INT(out_x, 999, "a NULL theme leaves out_x untouched");
}


/* CONFIG_ICON_PLACEMENT_BOTTOM: on an empty desktop, the first icon
 * lands at the bottom-left corner (slot 0) */
static void s_test_icon_bottom_first_slot(void)
{
    client_td client;
    struct config_theme_s theme;
    int16_t out_x = 0;
    int16_t out_y = 0;

    memset(&client, 0, sizeof(client));
    memset(&theme, 0, sizeof(theme));
    client.theme = &theme;

    place_icon(&client, NULL, CONFIG_ICON_PLACEMENT_BOTTOM, 32u, 32u,
            800u, 600u, &out_x, &out_y);

    /* ix = margin(8) + pri(0)*step_x = 8
     * iy = screen_h(600) - margin(8) - icon_h(32) - border(0) - 0 = 560 */
    TAP_EQ_INT(out_x, 8, "BOTTOM policy: first slot's own x is at the margin");
    TAP_EQ_INT(out_y, 560, "BOTTOM policy: first slot sits at the bottom");
}


/* CONFIG_ICON_PLACEMENT_TOP: the first slot sits at the top-left
 * corner instead */
static void s_test_icon_top_first_slot(void)
{
    client_td client;
    struct config_theme_s theme;
    int16_t out_x = 0;
    int16_t out_y = 0;

    memset(&client, 0, sizeof(client));
    memset(&theme, 0, sizeof(theme));
    client.theme = &theme;

    place_icon(&client, NULL, CONFIG_ICON_PLACEMENT_TOP, 32u, 32u,
            800u, 600u, &out_x, &out_y);

    TAP_EQ_INT(out_x, 8, "TOP policy: first slot's own x is at the margin");
    TAP_EQ_INT(out_y, 8, "TOP policy: first slot sits at the top margin" \
            " too, not the bottom");
}


/* CONFIG_ICON_PLACEMENT_RIGHT: the first slot is anchored to the
 * screen's own right edge, not the left */
static void s_test_icon_right_first_slot(void)
{
    client_td client;
    struct config_theme_s theme;
    int16_t out_x = 0;
    int16_t out_y = 0;

    memset(&client, 0, sizeof(client));
    memset(&theme, 0, sizeof(theme));
    client.theme = &theme;

    place_icon(&client, NULL, CONFIG_ICON_PLACEMENT_RIGHT, 32u, 32u,
            800u, 600u, &out_x, &out_y);

    /* ix = screen_w(800) - icon_w(32) - margin(8) - border(0) - 0 = 760 */
    TAP_EQ_INT(out_x, 760,
            "RIGHT policy: first slot is anchored to the right edge");
    TAP_EQ_INT(out_y, 8, "RIGHT policy: first slot starts at the top margin");
}


/* A nonzero theme icon border width is folded into both the RIGHT/
 * BOTTOM edge offset and the overlap test, shrinking the usable
 * on-screen footprint accordingly */
static void s_test_icon_border_width_shifts_position(void)
{
    client_td client;
    struct config_theme_s theme;
    int16_t out_x = 0;
    int16_t out_y = 0;

    memset(&client, 0, sizeof(client));
    memset(&theme, 0, sizeof(theme));
    theme.icon.active.border.width = 3u;
    client.theme = &theme;

    place_icon(&client, NULL, CONFIG_ICON_PLACEMENT_RIGHT, 32u, 32u,
            800u, 600u, &out_x, &out_y);

    /* border_twice = 3*2 = 6; ix = 800 - 32 - 8 - 6 - 0 = 754 */
    TAP_EQ_INT(out_x, 754,
            "a nonzero theme border width shifts the anchored edge in");
}


/* An icon occupying the first slot's exact footprint forces the next
 * icon to the next slot instead, rather than stacking directly on
 * top of it */
static void s_test_icon_avoids_occupied_slot(void)
{
    client_td client;
    client_td occupant;
    struct config_theme_s theme;
    desktop_td desktop;
    int16_t out_x = 0;
    int16_t out_y = 0;

    memset(&client, 0, sizeof(client));
    memset(&occupant, 0, sizeof(occupant));
    memset(&theme, 0, sizeof(theme));
    memset(&desktop, 0, sizeof(desktop));
    client.theme = &theme;

    /* Occupies exactly slot 0's own BOTTOM position (8, 560) */
    occupant.icon_window = 999u;
    occupant.is_icon_mapped = true;
    occupant.icon_x = 8;
    occupant.icon_y = 560;

    desktop.stacking = cdlist_init(NULL);
    cdlist_ins_next(desktop.stacking, NULL, &occupant);

    place_icon(&client, &desktop, CONFIG_ICON_PLACEMENT_BOTTOM, 32u, 32u,
            800u, 600u, &out_x, &out_y);

    TAP_OK(!(out_x == 8 && out_y == 560),
            "the occupied first slot is skipped for the next free one");

    cdlist_destroy(desktop.stacking);
}


/* CONFIG_ICON_PLACEMENT_SMART on an empty desktop still lands the
 * first icon at slot 0 (BOTTOM layout, zero cost, terminates
 * immediately) */
static void s_test_icon_smart_empty_desktop(void)
{
    client_td client;
    struct config_theme_s theme;
    int16_t out_x = 0;
    int16_t out_y = 0;

    memset(&client, 0, sizeof(client));
    memset(&theme, 0, sizeof(theme));
    client.theme = &theme;

    place_icon(&client, NULL, CONFIG_ICON_PLACEMENT_SMART, 32u, 32u,
            800u, 600u, &out_x, &out_y);

    TAP_EQ_INT(out_x, 8, "SMART on an empty desktop still picks slot 0's x");
    TAP_EQ_INT(out_y, 560, "SMART on an empty desktop still picks slot 0's" \
            " y (BOTTOM layout)");
}


/* CONFIG_ICON_PLACEMENT_SMART steers away from a visible window
 * sitting exactly on top of slot 0, picking a different, cheaper
 * slot instead */
static void s_test_icon_smart_avoids_visible_window(void)
{
    client_td client;
    client_td window;
    struct config_theme_s theme;
    desktop_td desktop;
    int16_t out_x = 0;
    int16_t out_y = 0;

    memset(&client, 0, sizeof(client));
    memset(&window, 0, sizeof(window));
    memset(&theme, 0, sizeof(theme));
    memset(&desktop, 0, sizeof(desktop));
    client.theme = &theme;

    /* A large visible window covering slot 0's own position */
    window.properties.state = (uint16_t) CLIENT_STATE_NORMAL;
    window.layout.geometry.cur.pos.x = 0;
    window.layout.geometry.cur.pos.y = 500;
    window.layout.geometry.cur.dim.w = 800u;
    window.layout.geometry.cur.dim.h = 100u;

    desktop.stacking = cdlist_init(NULL);
    cdlist_ins_next(desktop.stacking, NULL, &window);

    place_icon(&client, &desktop, CONFIG_ICON_PLACEMENT_SMART, 32u, 32u,
            800u, 600u, &out_x, &out_y);

    TAP_OK(!(out_x == 8 && out_y == 560),
            "a visible window covering slot 0 pushes SMART to a" \
            " different, cheaper slot");

    cdlist_destroy(desktop.stacking);
}


/* icon_avoid_systray_overlap: NULL coordinate pointers are a safe
 * no-op */
static void s_test_avoid_systray_null_guards(void)
{
    TAP_OK(!icon_avoid_systray_overlap(NULL, NULL, 32u, 32u,
                0, 0, 100u, 20u, NULL),
            "NULL coordinate pointers: safe no-op, returns false");
}


/* No actual overlap with the tray: the icon's own position is left
 * entirely alone */
static void s_test_avoid_systray_no_overlap(void)
{
    int16_t x = 500;
    int16_t y = 500;
    bool moved;

    moved = icon_avoid_systray_overlap(&x, &y, 32u, 32u,
            0, 0, 100u, 20u, NULL);

    TAP_OK(!moved, "no real overlap with the tray: nothing to avoid");
    TAP_EQ_INT(x, 500, "x is left untouched");
    TAP_EQ_INT(y, 500, "y is left untouched");
}


/* An overlapping icon with the tray in the upper half of the
 * workarea is pushed down, below the tray */
static void s_test_avoid_systray_tray_in_upper_half_pushes_down(void)
{
    int16_t x = 10;
    int16_t y = 10;
    struct geometry_s workarea;
    bool moved;

    workarea.pos.x = 0;
    workarea.pos.y = 0;
    workarea.dim.w = 800u;
    workarea.dim.h = 600u;

    /* Icon and tray overlap at (10,10); tray sits in the upper half
     * of a 600-tall workarea (mid_y=15 < 300) */
    moved = icon_avoid_systray_overlap(&x, &y, 32u, 32u,
            0, 0, 100u, 20u, &workarea);

    TAP_OK(moved, "an overlapping icon is moved");
    /* new_y = tray_y(0) + tray_h(20) + gap(8) = 28 */
    TAP_EQ_INT(y, 28,
            "with the tray in the upper half, the icon is pushed below it");
}


/* An overlapping icon with the tray in the lower half of the
 * workarea is pushed up, above the tray, instead */
static void s_test_avoid_systray_tray_in_lower_half_pushes_up(void)
{
    int16_t x = 10;
    int16_t y = 500;
    struct geometry_s workarea;
    bool moved;

    workarea.pos.x = 0;
    workarea.pos.y = 0;
    workarea.dim.w = 800u;
    workarea.dim.h = 600u;

    /* Tray near the bottom: mid_y = 500+10=510, well past 300 */
    moved = icon_avoid_systray_overlap(&x, &y, 32u, 32u,
            0, 500, 100u, 20u, &workarea);

    TAP_OK(moved, "an overlapping icon is moved");
    /* new_y = tray_y(500) - icon_h(32) - gap(8) = 460 */
    TAP_EQ_INT(y, 460,
            "with the tray in the lower half, the icon is pushed above it");
}


/* The corrected position is clamped to stay inside the workarea,
 * rather than pushing the icon out of it entirely */
static void s_test_avoid_systray_clamps_to_workarea(void)
{
    int16_t x = 10;
    int16_t y = 585;
    struct geometry_s workarea;
    bool moved;

    workarea.pos.x = 0;
    workarea.pos.y = 0;
    workarea.dim.w = 800u;
    workarea.dim.h = 600u;

    moved = icon_avoid_systray_overlap(&x, &y, 32u, 32u,
            0, 590, 100u, 20u, &workarea);

    TAP_OK(moved, "an overlapping icon near the workarea edge is moved");
    TAP_OK(y + 32 <= 600, "the clamped position keeps the icon fully" \
            " inside the workarea's own bottom edge");
    TAP_OK(y >= 0, "...and its own top edge");
}


int main(void)
{
    TAP_PLAN(25);

    s_test_icon_guards();
    s_test_icon_bottom_first_slot();
    s_test_icon_top_first_slot();
    s_test_icon_right_first_slot();
    s_test_icon_border_width_shifts_position();
    s_test_icon_avoids_occupied_slot();
    s_test_icon_smart_empty_desktop();
    s_test_icon_smart_avoids_visible_window();
    s_test_avoid_systray_null_guards();
    s_test_avoid_systray_no_overlap();
    s_test_avoid_systray_tray_in_upper_half_pushes_down();
    s_test_avoid_systray_tray_in_lower_half_pushes_up();
    s_test_avoid_systray_clamps_to_workarea();

    return TAP_DONE();
}
