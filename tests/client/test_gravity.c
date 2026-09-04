/**
 * @file tests/client/test_gravity.c
 *
 * @brief Test battery for the gravity-anchor position adjustment
 *
 * Exercises 'client_gravity_adjust_pos' (client/state.c) directly:
 * the function has no external dependency of its own (no XCB call,
 * no allocation, no other project symbol), so this file links only
 * against the one source file under test, with nothing to stub.
 *
 * Every gravity value in 'enum client_gravity_e' is checked at least
 * once, since the function is a single, flat chain of independent
 * 'if'/'else if' branches keyed off that enumeration: each arm is
 * its own opportunity for an off-by-one or a transposed axis, and
 * none of them share enough structure for one passing case to imply
 * another passes too.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>

/* Local includes */
#include <client/state.h>
#include <harness/tap.h>


/* A frame growing from 100x100 to 200x200 anchored at its top-left
 * corner (the ICCCM default) never moves its position at all */
static void s_test_north_west_is_a_no_op(void)
{
    int32_t x = 10;
    int32_t y = 20;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 200u, 200u,
            (uint16_t) CLIENT_GRAVITY_NORTH_WEST);

    TAP_EQ_INT(x, 10, "north-west gravity leaves x untouched");
    TAP_EQ_INT(y, 20, "north-west gravity leaves y untouched");
}


/* 'CLIENT_GRAVITY_STATIC' anchors the client area itself rather than
 * the frame, which this function never adjusts for either */
static void s_test_static_is_a_no_op(void)
{
    int32_t x = 50;
    int32_t y = 60;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 50u, 50u,
            (uint16_t) CLIENT_GRAVITY_STATIC);

    TAP_EQ_INT(x, 50, "static gravity leaves x untouched");
    TAP_EQ_INT(y, 60, "static gravity leaves y untouched");
}


/* Anchoring the top-right corner keeps it fixed as the frame shrinks:
 * the left edge must move right by exactly the width lost */
static void s_test_north_east_shrink_moves_x_right(void)
{
    int32_t x = 0;
    int32_t y = 0;

    client_gravity_adjust_pos(&x, &y, 200u, 100u, 100u, 100u,
            (uint16_t) CLIENT_GRAVITY_NORTH_EAST);

    TAP_EQ_INT(x, 100, "north-east: x grows by the full width lost");
    TAP_EQ_INT(y, 0, "north-east: y is untouched (top edge fixed)");
}


/* The mirror case: growing a frame anchored at its top-right corner
 * pushes the left edge left by the width gained */
static void s_test_east_grow_moves_x_left(void)
{
    int32_t x = 100;
    int32_t y = 0;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 150u, 100u,
            (uint16_t) CLIENT_GRAVITY_EAST);

    TAP_EQ_INT(x, 50, "east: x shrinks by the full width gained");
    TAP_EQ_INT(y, 0, "east: y is untouched (vertical center fixed)");
}


/* 'CENTER' splits both axes' displacement in half, rounding toward
 * zero per plain integer division */
static void s_test_center_splits_both_axes(void)
{
    int32_t x = 0;
    int32_t y = 0;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 200u, 200u,
            (uint16_t) CLIENT_GRAVITY_CENTER);

    TAP_EQ_INT(x, -50, "center: x moves by half the width delta");
    TAP_EQ_INT(y, -50, "center: y moves by half the height delta");
}


/* An odd-sized delta under 'CENTER' truncates rather than rounding,
 * since the implementation divides signed integers by two */
static void s_test_center_odd_delta_truncates(void)
{
    int32_t x = 0;
    int32_t y = 0;

    client_gravity_adjust_pos(&x, &y, 101u, 101u, 100u, 100u,
            (uint16_t) CLIENT_GRAVITY_CENTER);

    TAP_EQ_INT(x, 0, "center: a one-pixel delta truncates to zero on x");
    TAP_EQ_INT(y, 0, "center: a one-pixel delta truncates to zero on y");
}


/* Anchoring the bottom-right corner moves both axes together on a
 * uniform shrink */
static void s_test_south_east_shrink_moves_both_axes(void)
{
    int32_t x = 10;
    int32_t y = 10;

    client_gravity_adjust_pos(&x, &y, 200u, 200u, 100u, 100u,
            (uint16_t) CLIENT_GRAVITY_SOUTH_EAST);

    TAP_EQ_INT(x, 110, "south-east: x grows by the width lost");
    TAP_EQ_INT(y, 110, "south-east: y grows by the height lost");
}


/* 'SOUTH' keeps the bottom edge fixed while leaving x alone: only the
 * height delta is applied, in full, to y */
static void s_test_south_moves_only_y(void)
{
    int32_t x = 5;
    int32_t y = 5;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 100u, 150u,
            (uint16_t) CLIENT_GRAVITY_SOUTH);

    TAP_EQ_INT(x, 5, "south: x is untouched");
    TAP_EQ_INT(y, -45, "south: y shrinks by the full height gained");
}


/* 'SOUTH_WEST' keeps the bottom-left corner fixed: x is untouched
 * (left edge already fixed) and y absorbs the full height delta */
static void s_test_south_west_moves_only_y(void)
{
    int32_t x = 5;
    int32_t y = 0;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 100u, 60u,
            (uint16_t) CLIENT_GRAVITY_SOUTH_WEST);

    TAP_EQ_INT(x, 5, "south-west: x is untouched");
    TAP_EQ_INT(y, 40, "south-west: y grows by the full height lost");
}


/* 'WEST' keeps the left edge fixed on x (a no-op there) while
 * centering the vertical displacement, exactly like 'CENTER' does
 * for y alone */
static void s_test_west_centers_y_only(void)
{
    int32_t x = 0;
    int32_t y = 0;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 100u, 200u,
            (uint16_t) CLIENT_GRAVITY_WEST);

    TAP_EQ_INT(x, 0, "west: x is untouched");
    TAP_EQ_INT(y, -50, "west: y moves by half the height delta");
}


/* 'NORTH' centers the horizontal displacement while leaving y fixed
 * (top edge already fixed) */
static void s_test_north_centers_x_only(void)
{
    int32_t x = 0;
    int32_t y = 0;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 300u, 100u,
            (uint16_t) CLIENT_GRAVITY_NORTH);

    TAP_EQ_INT(x, -100, "north: x moves by half the width delta");
    TAP_EQ_INT(y, 0, "north: y is untouched");
}


/* An unrecognized gravity value (outside 'enum client_gravity_e')
 * falls through every branch untouched, the safe degradation for a
 * malformed 'WM_NORMAL_HINTS.win_gravity' */
static void s_test_unknown_gravity_is_a_no_op(void)
{
    int32_t x = 42;
    int32_t y = 99;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 400u, 400u, 0u);

    TAP_EQ_INT(x, 42, "unknown gravity leaves x untouched");
    TAP_EQ_INT(y, 99, "unknown gravity leaves y untouched");
}


/* A frame that does not change size at all is a no-op regardless of
 * gravity, since every branch's delta is then zero */
static void s_test_unchanged_size_is_a_no_op(void)
{
    int32_t x = 7;
    int32_t y = 8;

    client_gravity_adjust_pos(&x, &y, 100u, 100u, 100u, 100u,
            (uint16_t) CLIENT_GRAVITY_SOUTH_EAST);

    TAP_EQ_INT(x, 7, "unchanged size: x untouched even for a corner"
            " anchor");
    TAP_EQ_INT(y, 8, "unchanged size: y untouched even for a corner"
            " anchor");
}


int main(void)
{
    TAP_PLAN(26);

    s_test_north_west_is_a_no_op();
    s_test_static_is_a_no_op();
    s_test_north_east_shrink_moves_x_right();
    s_test_east_grow_moves_x_left();
    s_test_center_splits_both_axes();
    s_test_center_odd_delta_truncates();
    s_test_south_east_shrink_moves_both_axes();
    s_test_south_moves_only_y();
    s_test_south_west_moves_only_y();
    s_test_west_centers_y_only();
    s_test_north_centers_x_only();
    s_test_unknown_gravity_is_a_no_op();
    s_test_unchanged_size_is_a_no_op();

    return TAP_DONE();
}
