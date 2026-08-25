/**
 * @file tests/client/test_state.c
 *
 * @brief Test battery for the client state bitmask
 *
 * Covers what EWMH asks of @c _NET_WM_STATE_MAXIMIZED_HORZ,
 * @c _NET_WM_STATE_MAXIMIZED_VERT and @c _NET_WM_STATE_FULLSCREEN:
 * that they are independent of one another, that a window may hold any
 * combination of them, and that adding one never silently discards
 * another the client did not ask to lose.
 *
 * The predicates are exercised against a bare @c client_td rather than
 * through the commands that set them, since those reach for an X
 * connection; what is under test here is the model itself, which is
 * where the states either are independent or are not.
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
#include <harness/tap.h>
#include <client.h>


/**
 * @brief Every bit is distinct, so no two share a value
 *
 * @note Complexity: @e O(1)
 */
static void s_test_bits_are_distinct(void)
{
    TAP_OK(CLIENT_STATE_NORMAL == 0u,
            "normal is the absence of every bit");
    TAP_OK((CLIENT_STATE_ICONIFIED & CLIENT_STATE_MAXIMIZED_HORZ) == 0u,
            "iconified and horizontal do not overlap");
    TAP_OK((CLIENT_STATE_MAXIMIZED_HORZ &
                CLIENT_STATE_MAXIMIZED_VERT) == 0u,
            "the two maximize axes do not overlap");
    TAP_OK((CLIENT_STATE_MAXIMIZED & CLIENT_STATE_FULLSCREEN) == 0u,
            "maximized and full screen do not overlap");
    TAP_OK(CLIENT_STATE_MAXIMIZED ==
                (CLIENT_STATE_MAXIMIZED_HORZ | CLIENT_STATE_MAXIMIZED_VERT),
            "maximized is exactly both axes at once");
}


/**
 * @brief One axis alone reads as that axis and not as the other
 *
 * @note Complexity: @e O(1)
 */
static void s_test_single_axis(void)
{
    client_td client;

    memset(&client, 0, sizeof(client));

    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    TAP_OK(client_is_maximized_horz(&client),
            "horizontal alone reads as horizontal");
    TAP_OK(!client_is_maximized_vert(&client),
            "horizontal alone does not read as vertical");
    TAP_OK(!client_is_maximized(&client),
            "horizontal alone is not maximized outright");
    TAP_OK(client_is_maximized_any(&client),
            "horizontal alone is maximized in some way");

    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED_VERT;
    TAP_OK(client_is_maximized_vert(&client),
            "vertical alone reads as vertical");
    TAP_OK(!client_is_maximized_horz(&client),
            "vertical alone does not read as horizontal");
}


/**
 * @brief Both axes read as maximized, and as each axis too
 *
 * @note Complexity: @e O(1)
 */
static void s_test_both_axes(void)
{
    client_td client;

    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED;

    TAP_OK(client_is_maximized(&client),
            "both axes read as maximized outright");
    TAP_OK(client_is_maximized_horz(&client),
            "both axes read as horizontally maximized too");
    TAP_OK(client_is_maximized_vert(&client),
            "both axes read as vertically maximized too");
}


/**
 * @brief Full screen sits alongside maximization rather than replacing
 *        it
 *
 * This is the combination EWMH explicitly allows and the one a client
 * produces by asking for full screen while already maximized, a
 * browser going full screen for a video being the ordinary case.
 *
 * @note Complexity: @e O(1)
 */
static void s_test_fullscreen_over_maximized(void)
{
    client_td client;

    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED;

    /* Adding full screen, the way 'ccmd_client_fullscreen' does */
    client.properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;

    TAP_OK(client_is_fullscreen(&client),
            "adding full screen reads as full screen");
    TAP_OK(client_is_maximized(&client),
            "adding full screen keeps the maximization");

    /* Removing it again, the way 'ccmd_client_unfullscreen' does */
    client.properties.state &= (uint16_t) ~CLIENT_STATE_FULLSCREEN;

    TAP_OK(!client_is_fullscreen(&client),
            "removing full screen clears full screen");
    TAP_OK(client_is_maximized(&client),
            "removing full screen leaves the maximization behind");
}


/**
 * @brief Iconifying keeps whatever else the window held
 *
 * EWMH: "a maximized window [may be] iconified and re-appear as
 * maximized upon de-iconification".
 *
 * @note Complexity: @e O(1)
 */
static void s_test_iconified_keeps_the_rest(void)
{
    client_td client;

    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;

    client.properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;
    TAP_OK(client_is_iconified(&client), "iconified reads as iconified");
    TAP_OK(client_is_maximized_horz(&client),
            "iconifying keeps the horizontal axis");

    client.properties.state &= (uint16_t) ~CLIENT_STATE_ICONIFIED;
    TAP_OK(!client_is_iconified(&client),
            "restoring clears the iconified bit");
    TAP_OK(client_is_maximized_horz(&client),
            "restoring leaves the horizontal axis behind");
}


/**
 * @brief Removing one axis leaves the other standing
 *
 * A client asking to drop @c _NET_WM_STATE_MAXIMIZED_HORZ said nothing
 * about the vertical axis, so the vertical axis stays.
 *
 * @note Complexity: @e O(1)
 */
static void s_test_removing_one_axis(void)
{
    client_td client;

    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED;

    client.properties.state &= (uint16_t) ~CLIENT_STATE_MAXIMIZED_HORZ;

    TAP_OK(!client_is_maximized_horz(&client),
            "removing the horizontal axis clears it");
    TAP_OK(client_is_maximized_vert(&client),
            "removing the horizontal axis keeps the vertical one");
    TAP_OK(!client_is_maximized(&client),
            "one axis left is not maximized outright");
}


/**
 * @brief Every state at once still reads correctly
 *
 * Nothing forbids this combination, and a property read back off the
 * window has to report all of it.
 *
 * @note Complexity: @e O(1)
 */
static void s_test_every_state_at_once(void)
{
    client_td client;

    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) (CLIENT_STATE_MAXIMIZED |
            CLIENT_STATE_FULLSCREEN | CLIENT_STATE_ICONIFIED);

    TAP_OK(client_is_maximized(&client), "all at once: still maximized");
    TAP_OK(client_is_fullscreen(&client),
            "all at once: still full screen");
    TAP_OK(client_is_iconified(&client), "all at once: still iconified");
}


/**
 * @brief Iconifying a full screen window keeps it full screen
 *
 * The window really does leave full screen on the way in, so that the
 * geometry remembered is its own rather than the whole screen; the bit
 * stays standing regardless, which is what brings it back full screen
 * on the way out.  Nothing about asking to be iconified says anything
 * about full screen.
 *
 * @note Complexity: @e O(1)
 */
static void s_test_iconified_keeps_fullscreen(void)
{
    client_td client;

    memset(&client, 0, sizeof(client));
    client.properties.state = (uint16_t) (CLIENT_STATE_MAXIMIZED |
            CLIENT_STATE_FULLSCREEN);

    /* What 'ccmd_client_iconify' does to the state: leave full screen
     * for its geometry, put the bit straight back, then hide */
    client.properties.state &= (uint16_t) ~CLIENT_STATE_FULLSCREEN;
    client.properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    client.properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;

    TAP_OK(client_is_iconified(&client), "iconified reads as iconified");
    TAP_OK(client_is_fullscreen(&client),
            "iconifying keeps the full screen bit");
    TAP_OK(client_is_maximized(&client),
            "iconifying keeps the maximization under it");

    /* And what 'ccmd_client_restore' does: clear the iconified bit,
     * then re-enter each state still standing */
    client.properties.state &= (uint16_t) ~CLIENT_STATE_ICONIFIED;

    TAP_OK(client_is_fullscreen(&client),
            "restoring comes back full screen");
    TAP_OK(client_is_maximized(&client),
            "restoring comes back maximized under it");
}


/**
 * @brief Run every case
 *
 * @return @c 0 when every assertion passed
 *
 * @note Complexity: @e O(1)
 */
int main(void)
{
    TAP_PLAN(33);

    s_test_bits_are_distinct();
    s_test_single_axis();
    s_test_both_axes();
    s_test_fullscreen_over_maximized();
    s_test_iconified_keeps_the_rest();
    s_test_removing_one_axis();
    s_test_iconified_keeps_fullscreen();
    s_test_every_state_at_once();

    return TAP_DONE();
}
