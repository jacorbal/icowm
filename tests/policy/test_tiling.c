/**
 * @file tests/policy/test_tiling.c
 *
 * @brief Test battery for icon placement policy
 *
 * Both place_icon_apply and place_icon_avoid_systray_overlap are pure
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <client.h>
#include <policy/stacking.h>
#include <adt/ohtbl.h>
#include <desktop.h>
#include <config.h>
#include <harness/tap.h>
#include <policy/placement/icon.h>


/**
 * @brief Hash a client by its own address
 *
 * @param key Client to hash
 *
 * @return A hash of @p key
 *
 * @note Complexity: @e O(1)
 */
static size_t s_client_hash1(const void *key)
{
    return (size_t) (uintptr_t) key;
}


/**
 * @brief Second hash for the open-addressed table
 *
 * @param key Unused
 *
 * @return @c 1, probing every slot in turn
 *
 * @note Complexity: @e O(1)
 */
static size_t s_client_hash2(const void *key)
{
    (void) key;

    return 1u;
}


/**
 * @brief Whether two table entries are the same client
 *
 * @param key1 First client
 * @param key2 Second client
 *
 * @return @c true when they are the same
 *
 * @note Complexity: @e O(1)
 */
static bool s_client_match(const void *key1, const void *key2)
{
    return key1 == key2;
}


/* place_icon_apply's own guard clauses: any missing required
 * argument is a safe no-op that never touches out_pos.  The original
 * (pre-restructuring) API took separate out_x/out_y pointers, and
 * had its own test for "out_x specifically NULL, out_y still valid";
 * the current API bundles both into one out_pos pointer, so that
 * specific partial-NULL case no longer applies, replaced here by the
 * genuinely equivalent one: out_pos itself NULL, the third guard
 * condition the real function still checks. */
static void s_test_icon_guards(void)
{
    client_td client;
    config_td config;
    struct position_s out_pos = { 999, 999 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;

    place_icon_apply(NULL, NULL, CONFIG_ICON_PLACEMENT_BOTTOM,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);
    TAP_EQ_INT(out_pos.x, 999, "a NULL client leaves out_pos untouched");

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_BOTTOM,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, NULL);
    TAP_OK(true, "a NULL out_pos is a safe no-op, no crash");

    client.config = NULL;
    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_BOTTOM,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);
    TAP_EQ_INT(out_pos.x, 999, "a NULL config leaves out_pos untouched");
}


/* CONFIG_ICON_PLACEMENT_BOTTOM: on an empty desktop, the first icon
 * lands at the bottom-left corner (slot 0) */
static void s_test_icon_bottom_first_slot(void)
{
    client_td client;
    config_td config;
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_BOTTOM,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);

    /* ix = margin(8) + pri(0)*step_x = 8
     * iy = screen_h(600) - margin(8) - icon_h(32) - border(0) - 0 = 560 */
    TAP_EQ_INT(out_pos.x, 8,
            "BOTTOM policy: first slot's own x is at the margin");
    TAP_EQ_INT(out_pos.y, 560, "BOTTOM policy: first slot sits at the" \
            " bottom");
}


/* CONFIG_ICON_PLACEMENT_TOP: the first slot sits at the top-left
 * corner instead */
static void s_test_icon_top_first_slot(void)
{
    client_td client;
    config_td config;
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_TOP,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);

    TAP_EQ_INT(out_pos.x, 8,
            "TOP policy: first slot's own x is at the margin");
    TAP_EQ_INT(out_pos.y, 8, "TOP policy: first slot sits at the top" \
            " margin too, not the bottom");
}


/* CONFIG_ICON_PLACEMENT_RIGHT: the first slot is anchored to the
 * screen's own right edge, not the left */
static void s_test_icon_right_first_slot(void)
{
    client_td client;
    config_td config;
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_RIGHT,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);

    /* ix = screen_w(800) - icon_w(32) - margin(8) - border(0) - 0 = 760 */
    TAP_EQ_INT(out_pos.x, 760,
            "RIGHT policy: first slot is anchored to the right edge");
    TAP_EQ_INT(out_pos.y, 8,
            "RIGHT policy: first slot starts at the top margin");
}


/* A nonzero theme icon border width is folded into both the RIGHT/
 * BOTTOM edge offset and the overlap test, shrinking the usable
 * on-screen footprint accordingly */
static void s_test_icon_border_width_shifts_position(void)
{
    client_td client;
    config_td config;
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    config.theme.icon.active.border.width = 3u;
    client.config = &config;

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_RIGHT,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);

    /* border_twice = 3*2 = 6; ix = 800 - 32 - 8 - 6 - 0 = 754 */
    TAP_EQ_INT(out_pos.x, 754,
            "a nonzero theme border width shifts the anchored edge in");
}


/* An icon occupying the first slot's exact footprint forces the next
 * icon to the next slot instead, rather than stacking directly on
 * top of it */
static void s_test_icon_avoids_occupied_slot(void)
{
    client_td client;
    client_td occupant;
    config_td config;
    desktop_td desktop;
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&occupant, 0, sizeof(occupant));
    memset(&config, 0, sizeof(config));
    memset(&desktop, 0, sizeof(desktop));
    client.config = &config;

    /* Occupies exactly slot 0's own BOTTOM position (8, 560) */
    occupant.icon_window = 999u;
    occupant.is_icon_mapped = true;
    occupant.properties.state = (uint16_t) CLIENT_STATE_ICONIFIED;
    occupant.icon_pos.x = 8;
    occupant.icon_pos.y = 560;

    /* The stacking order spans every managed client and filters by
     * asking the desktop's client table which of them it shows, so a
     * client has to be put in both to be seen by a walk */
    desktop.clients = ohtbl_init(8, 8, s_client_hash1, s_client_hash2,
            s_client_match, NULL);
    (void) stacking_create(&desktop);
    (void) ohtbl_insert(desktop.clients, &occupant);
    (void) stacking_add(&desktop, &occupant);

    place_icon_apply(&client, &desktop, CONFIG_ICON_PLACEMENT_BOTTOM,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);

    TAP_OK(!(out_pos.x == 8 && out_pos.y == 560),
            "the occupied first slot is skipped for the next free one");

    stacking_destroy(&desktop);
    ohtbl_destroy(desktop.clients);
}


/* CONFIG_ICON_PLACEMENT_SMART on an empty desktop still lands the
 * first icon at slot 0 (BOTTOM layout, zero cost, terminates
 * immediately) */
static void s_test_icon_smart_empty_desktop(void)
{
    client_td client;
    config_td config;
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_SMART,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);

    TAP_EQ_INT(out_pos.x, 8,
            "SMART on an empty desktop still picks slot 0's x");
    TAP_EQ_INT(out_pos.y, 560, "SMART on an empty desktop still picks" \
            " slot 0's y (BOTTOM layout)");
}


/* CONFIG_ICON_PLACEMENT_IN_PLACE puts the icon on the anchor it is
 * handed, which is where the window itself was, rather than on any of
 * the edge grids the other policies count from */
static void s_test_icon_in_place_uses_anchor(void)
{
    client_td client;
    config_td config;
    struct position_s anchor = { 300, 220 };
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_IN_PLACE,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, &anchor, &out_pos);

    TAP_EQ_INT(out_pos.x, 300,
            "IN_PLACE takes the anchor's x when nothing sits there");
    TAP_EQ_INT(out_pos.y, 220,
            "IN_PLACE takes the anchor's y when nothing sits there");
}


/* CONFIG_ICON_PLACEMENT_IN_PLACE keeps the icon wholly on screen: an
 * anchor past the right or bottom edge is pulled back rather than
 * placing the icon where it cannot be seen */
static void s_test_icon_in_place_clamps_anchor(void)
{
    client_td client;
    config_td config;
    struct position_s anchor = { 5000, 5000 };
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_IN_PLACE,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, &anchor, &out_pos);

    TAP_OK(out_pos.x >= 8 && out_pos.x <= 800 - 32 - 8,
            "IN_PLACE pulls an off-screen anchor's x back on screen");
    TAP_OK(out_pos.y >= 8 && out_pos.y <= 600 - 32 - 8,
            "IN_PLACE pulls an off-screen anchor's y back on screen");
}


/* CONFIG_ICON_PLACEMENT_IN_PLACE with no anchor to work from behaves
 * exactly as CONFIG_ICON_PLACEMENT_SMART, which is what it falls back
 * on */
static void s_test_icon_in_place_without_anchor_is_smart(void)
{
    client_td client;
    config_td config;
    struct position_s smart_pos = { 0, 0 };
    struct position_s in_place_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;

    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_SMART,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &smart_pos);
    place_icon_apply(&client, NULL, CONFIG_ICON_PLACEMENT_IN_PLACE,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &in_place_pos);

    TAP_EQ_INT(in_place_pos.x, smart_pos.x,
            "IN_PLACE without an anchor falls back on SMART's x");
    TAP_EQ_INT(in_place_pos.y, smart_pos.y,
            "IN_PLACE without an anchor falls back on SMART's y");
}


/* CONFIG_ICON_PLACEMENT_SMART steers away from a visible window
 * sitting exactly on top of slot 0, picking a different, cheaper
 * slot instead */
static void s_test_icon_smart_avoids_visible_window(void)
{
    client_td client;
    client_td window;
    config_td config;
    desktop_td desktop;
    struct position_s out_pos = { 0, 0 };

    memset(&client, 0, sizeof(client));
    memset(&window, 0, sizeof(window));
    memset(&config, 0, sizeof(config));
    memset(&desktop, 0, sizeof(desktop));
    client.config = &config;

    /* A large visible window covering slot 0's own position */
    window.properties.state = (uint16_t) CLIENT_STATE_NORMAL;
    window.layout.geometry.cur.pos.x = 0;
    window.layout.geometry.cur.pos.y = 500;
    window.layout.geometry.cur.dim.w = 800u;
    window.layout.geometry.cur.dim.h = 100u;

    desktop.clients = ohtbl_init(8, 8, s_client_hash1, s_client_hash2,
            s_client_match, NULL);
    (void) stacking_create(&desktop);
    (void) ohtbl_insert(desktop.clients, &window);
    (void) stacking_add(&desktop, &window);

    place_icon_apply(&client, &desktop, CONFIG_ICON_PLACEMENT_SMART,
            (struct dimensions_s) { 32u, 32u },
            (struct dimensions_s) { 800u, 600u }, NULL, &out_pos);

    TAP_OK(!(out_pos.x == 8 && out_pos.y == 560),
            "a visible window covering slot 0 pushes SMART to a" \
            " different, cheaper slot");

    stacking_destroy(&desktop);
    ohtbl_destroy(desktop.clients);
}


/* place_icon_avoid_systray_overlap: NULL coordinate pointers are a
 * safe no-op */
static void s_test_avoid_systray_null_guards(void)
{
    TAP_OK(!place_icon_avoid_systray_overlap(NULL, NULL,
                (struct dimensions_s) { 32u, 32u },
                (struct geometry_s) { { 0, 0 }, { 100u, 20u } }, NULL),
            "NULL coordinate pointers: safe no-op, returns false");
}


/* No actual overlap with the tray: the icon's own position is left
 * entirely alone */
static void s_test_avoid_systray_no_overlap(void)
{
    int16_t x = 500;
    int16_t y = 500;
    bool moved;

    moved = place_icon_avoid_systray_overlap(&x, &y,
            (struct dimensions_s) { 32u, 32u },
            (struct geometry_s) { { 0, 0 }, { 100u, 20u } }, NULL);

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
    moved = place_icon_avoid_systray_overlap(&x, &y,
            (struct dimensions_s) { 32u, 32u },
            (struct geometry_s) { { 0, 0 }, { 100u, 20u } }, &workarea);

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
    moved = place_icon_avoid_systray_overlap(&x, &y,
            (struct dimensions_s) { 32u, 32u },
            (struct geometry_s) { { 0, 500 }, { 100u, 20u } }, &workarea);

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

    moved = place_icon_avoid_systray_overlap(&x, &y,
            (struct dimensions_s) { 32u, 32u },
            (struct geometry_s) { { 0, 590 }, { 100u, 20u } }, &workarea);

    TAP_OK(moved, "an overlapping icon near the workarea edge is moved");
    TAP_OK(y + 32 <= 600, "the clamped position keeps the icon fully" \
            " inside the workarea's own bottom edge");
    TAP_OK(y >= 0, "...and its own top edge");
}


int main(void)
{
    TAP_PLAN(31);

    s_test_icon_guards();
    s_test_icon_bottom_first_slot();
    s_test_icon_top_first_slot();
    s_test_icon_right_first_slot();
    s_test_icon_border_width_shifts_position();
    s_test_icon_avoids_occupied_slot();
    s_test_icon_smart_empty_desktop();
    s_test_icon_in_place_uses_anchor();
    s_test_icon_in_place_clamps_anchor();
    s_test_icon_in_place_without_anchor_is_smart();
    s_test_icon_smart_avoids_visible_window();
    s_test_avoid_systray_null_guards();
    s_test_avoid_systray_no_overlap();
    s_test_avoid_systray_tray_in_upper_half_pushes_down();
    s_test_avoid_systray_tray_in_lower_half_pushes_up();
    s_test_avoid_systray_clamps_to_workarea();

    return TAP_DONE();
}
