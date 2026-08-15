/**
 * @file tests/input/mouse/test_bounds.c
 *
 * @brief Test battery for shared resize-border bounds computation
 *
 * WM_RESIZE_GRAB_THRESHOLD (defs/input.h) is 0 on this build, which
 * makes the adaptive margin collapse to a plain max(border, 0); every
 * hand-computed expectation below already accounts for that.
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

/* Local includes */
#include <client.h>
#include <harness/tap.h>
#include <input/mouse/bounds.h>


/* A NULL client yields every field zeroed, no crash */
static void s_test_null_client(void)
{
    im_resize_bounds_td bounds = im_resize_bounds(NULL);

    TAP_EQ_INT(bounds.left, 0, "left is 0 for a NULL client");
    TAP_EQ_INT(bounds.right, 0, "right is 0 for a NULL client");
    TAP_OK(!bounds.has_titlebar_row,
            "has_titlebar_row is false for a NULL client");
}


/* A decorated client with a titlebar: bounds match its own geometry,
 * margins match its own frame extents, and the titlebar row is
 * carved out correctly above the resize border */
static void s_test_decorated_with_titlebar(void)
{
    client_td client;
    im_resize_bounds_td bounds;

    memset(&client, 0, sizeof(client));
    client.frame = 1;  /* decorated */
    client.layout.geometry.cur.pos.x = 100;
    client.layout.geometry.cur.pos.y = 50;
    client.layout.geometry.cur.dim.w = 800u;
    client.layout.geometry.cur.dim.h = 600u;
    client.layout.frame_extents.left = 5;
    client.layout.frame_extents.right = 5;
    client.layout.frame_extents.top = 30;
    client.layout.frame_extents.bottom = 5;
    client.title_height = 25u;

    bounds = im_resize_bounds(&client);

    TAP_EQ_INT(bounds.left, 100, "left matches the client's own x");
    TAP_EQ_INT(bounds.top, 50, "top matches the client's own y");
    TAP_EQ_INT(bounds.right, 900, "right = x + width");
    TAP_EQ_INT(bounds.bottom, 650, "bottom = y + height");

    TAP_EQ_INT(bounds.margin_left, 5, "left margin matches its border");
    TAP_EQ_INT(bounds.margin_right, 5, "right margin matches its border");
    /* border_top = frame_extents.top - title_height = 30 - 25 = 5 */
    TAP_EQ_INT(bounds.margin_top, 5,
            "top margin is the border strip above the titlebar only," \
            " not the full frame_extents.top");
    TAP_EQ_INT(bounds.margin_bottom, 5, "bottom margin matches its border");

    TAP_OK(bounds.has_titlebar_row,
            "a titlebar is present: has_titlebar_row is true");
    /* titlebar_row_top = top + margin_top = 50 + 5 = 55 */
    TAP_EQ_INT(bounds.titlebar_row_top, 55, "titlebar row starts right" \
            " after the top border strip");
    /* titlebar_row_bottom = top + frame_extents.top = 50 + 30 = 80 */
    TAP_EQ_INT(bounds.titlebar_row_bottom, 80,
            "titlebar row ends at the full frame_extents.top");
}


/* A decorated client with no titlebar (title_height == 0) has no
 * titlebar row at all */
static void s_test_decorated_without_titlebar(void)
{
    client_td client;
    im_resize_bounds_td bounds;

    memset(&client, 0, sizeof(client));
    client.frame = 1;
    client.layout.frame_extents.top = 5;
    client.title_height = 0u;

    bounds = im_resize_bounds(&client);

    TAP_OK(!bounds.has_titlebar_row,
            "no titlebar: has_titlebar_row is false");
}


/* An undecorated client (frame == 0) uses client_border_width for
 * all four edges equally, via border_override */
static void s_test_undecorated_uses_theme_border(void)
{
    client_td client;
    struct config_theme_s theme;
    im_resize_bounds_td bounds;

    memset(&client, 0, sizeof(client));
    memset(&theme, 0, sizeof(theme));
    client.frame = 0;
    client.theme = &theme;
    client.border_override.is_set = true;
    client.border_override.width = 3u;

    bounds = im_resize_bounds(&client);

    TAP_EQ_INT(bounds.margin_left, 3,
            "undecorated: left margin comes from the theme border");
    TAP_EQ_INT(bounds.margin_right, 3,
            "undecorated: right margin comes from the theme border");
    TAP_EQ_INT(bounds.margin_top, 3,
            "undecorated: top margin comes from the theme border");
    TAP_EQ_INT(bounds.margin_bottom, 3,
            "undecorated: bottom margin comes from the theme border");
}


/* A negative computed border (title_height taller than
 * frame_extents.top) clamps its own margin down to 0, not a
 * negative value */
static void s_test_negative_border_clamps_to_zero(void)
{
    client_td client;
    im_resize_bounds_td bounds;

    memset(&client, 0, sizeof(client));
    client.frame = 1;
    client.layout.frame_extents.top = 10;
    client.title_height = 25u;  /* taller than frame_extents.top */

    bounds = im_resize_bounds(&client);

    /* border_top = 10 - 25 = -15, clamped to 0 */
    TAP_EQ_INT(bounds.margin_top, 0,
            "a negative computed border clamps to a 0 margin");
}


int main(void)
{
    TAP_PLAN(20);

    s_test_null_client();
    s_test_decorated_with_titlebar();
    s_test_decorated_without_titlebar();
    s_test_undecorated_uses_theme_border();
    s_test_negative_border_clamps_to_zero();

    return TAP_DONE();
}
