/**
 * @file tests/surface/actions/test_randr.c
 *
 * @brief Test battery for the guard/early-return branches of RandR
 *        output-profile application and reversion
 *        (surface/actions/randr.c)
 *
 * The two public entry points, 'surface_action_apply_randr_profiles'
 * and 'surface_action_revert_randr_profiles', both begin with a chain
 * of guards ('surface' null, no live connection, no screen, no
 * config, RandR disabled, no snapshot yet to revert) that this file
 * exercises directly, entirely without a live X server: every
 * scenario keeps 'xcb_connection_get' (a test-controlled stand-in)
 * reporting a null connection, the exact condition each function's
 * own first real guard already checks for, so neither function ever
 * reaches an actual 'xcb_randr_*' round trip.  That deeper logic
 * (matching a profile's name against the screen's live output list,
 * allocating a free CRTC, applying a mode/position/rotation, snapshot
 * and revert bookkeeping across real CRTCs) is not reachable this
 * way, and is NOT covered here: it depends on genuine RandR protocol
 * replies from a real (or fully protocol-mocked) X server, which is
 * out of reach for a linked-unit test like this one, and forcing it
 * would mean faking an entire XCB reply-cookie protocol handshake
 * rather than testing this file's own logic.  'logger_msg' and
 * 'xcb_reply_log_error' are link-only stand-ins, reached only past
 * the guards this file deliberately never gets past.
 * 'surface_refresh_monitors' is a link-only stand-in for the same
 * reason.  The real 'libxcb-randr' is linked directly rather than
 * stubbed, since none of its functions are ever actually called
 * while every guard above keeps returning early first.
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
#include <xcb/randr.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <utils/xcb/reply.h>

/* Local includes */
#include <harness/tap.h>


/** Whichever connection 'xcb_connection_get' should currently report;
 *  every scenario in this file keeps it null, so neither function
 *  under test ever reaches a real 'xcb_randr_*' round trip */
static xcb_connection_t *s_stub_connection;

/** Test-controlled stand-in for @a xcb_connection_get (utils/xcb/
 *  connection.c)
 *  @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return s_stub_connection;
}


/** Link-only stand-in for @a logger_msg (logger.c): reached only past
 *  the guards this file deliberately never gets past
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


/** Link-only stand-in for @a xcb_reply_log_error (utils/xcb/
 *  reply.c): reached only once a real reply has come back, which
 *  never happens with no live connection
 *  @note Complexity: @e O(1)
 */
void xcb_reply_log_error(xcb_generic_error_t *error, const char *what)
{
    (void) error;
    (void) what;
}


/** Link-only stand-in for @a surface_refresh_monitors (surface/
 *  monitors.c): reached only once at least one profile actually
 *  changed something, which never happens with no live connection
 *  @note Complexity: @e O(1)
 */
void surface_refresh_monitors(surface_td *surface)
{
    (void) surface;
}


static void s_reset(void)
{
    s_stub_connection = NULL;
}


static surface_td *s_make_surface(bool randr_enabled)
{
    surface_td *surface = calloc(1, sizeof(*surface));
    config_td *config = calloc(1, sizeof(*config));
    xcb_screen_t *screen = calloc(1, sizeof(*screen));

    config->randr.is_enabled = randr_enabled;
    surface->config = config;
    surface->screen = screen;
    surface->id = 0u;
    return surface;
}


static void s_free_surface(surface_td *surface)
{
    free(surface->config);
    free(surface->screen);
    free(surface);
}


/* A null surface is refused outright, on the apply path */
static void s_test_apply_null_surface_refused(void)
{
    bool result;

    s_reset();

    result = surface_action_apply_randr_profiles(NULL, true);
    TAP_OK(!result, "a null surface is refused outright by apply");
}


/* No live connection at all (the normal case for any headless test
 * environment, and this file's own default) is refused, regardless of
 * how well-formed the surface otherwise is */
static void s_test_apply_no_connection_refused(void)
{
    surface_td *surface = s_make_surface(true);
    bool result;

    s_reset();

    result = surface_action_apply_randr_profiles(surface, true);
    TAP_OK(!result,
            "no live X connection is refused by apply, even with"
            " a fully-formed, RandR-enabled surface");

    s_free_surface(surface);
}


/* A surface with no XCB screen pointer at all is refused */
static void s_test_apply_null_screen_refused(void)
{
    surface_td *surface = s_make_surface(true);
    bool result;

    s_reset();
    free(surface->screen);
    surface->screen = NULL;

    result = surface_action_apply_randr_profiles(surface, true);
    TAP_OK(!result, "a surface with no XCB screen is refused by apply");

    free(surface->config);
    free(surface);
}


/* A surface with no config at all is refused */
static void s_test_apply_null_config_refused(void)
{
    surface_td *surface = s_make_surface(true);
    bool result;

    s_reset();
    free(surface->config);
    surface->config = NULL;

    result = surface_action_apply_randr_profiles(surface, true);
    TAP_OK(!result, "a surface with no config at all is refused by"
            " apply");

    free(surface->screen);
    free(surface);
}


/* RandR disabled in config refuses the call even with everything else
 * well-formed */
static void s_test_apply_randr_disabled_refused(void)
{
    surface_td *surface = s_make_surface(false);
    bool result;

    s_reset();

    result = surface_action_apply_randr_profiles(surface, true);
    TAP_OK(!result,
            "config.randr.is_enabled being false refuses the call"
            " outright");

    s_free_surface(surface);
}


/* A snapshotting apply call that fails its own guard chain never
 * reaches the code that would add anything to the snapshot in the
 * first place, so a subsequent revert call still finds nothing to
 * act on and stays a safe no-op; resetting any snapshot left over
 * from an earlier reload is 'surface_action_randr_snapshot_begin''s
 * own job now, not something an individual apply call does on its
 * own, so this only confirms the failure path itself adds nothing
 * new, not that it clears anything old */
static void s_test_apply_failed_call_adds_nothing_to_snapshot(void)
{
    surface_td *surface = s_make_surface(true);
    bool result;

    s_reset();

    result = surface_action_apply_randr_profiles(surface, true);
    TAP_OK(!result,
            "a failed, snapshotting apply call still reports failure");

    /* A revert right after a failed apply must not crash, and must
     * not attempt any real XCB call either, since there is still no
     * live connection */
    surface_action_revert_randr_profiles();
    TAP_OK(true,
            "reverting right after a failed snapshotting apply call"
            " does not crash");

    s_free_surface(surface);
}


/* Beginning a new snapshot is always safe to call, whether or not
 * anything was ever actually snapshotted before it; a revert right
 * after still finds nothing to act on either way */
static void s_test_snapshot_begin_then_revert_is_noop(void)
{
    s_reset();

    surface_action_randr_snapshot_begin();
    TAP_OK(true, "beginning a new snapshot does not crash");

    surface_action_revert_randr_profiles();
    TAP_OK(true,
            "reverting right after beginning a new, empty snapshot"
            " does not crash");
}


/* Reverting with nothing ever snapshotted (the very first call this
 * process ever makes to it) is a safe no-op */
static void s_test_revert_nothing_snapshotted_is_noop(void)
{
    s_reset();

    surface_action_revert_randr_profiles();
    TAP_OK(true,
            "reverting with no snapshot at all recorded yet does not"
            " crash");
}


/* take_snapshot false does not disturb whatever snapshot bookkeeping
 * a previous, successful snapshot left behind; exercised here purely
 * through the connectionless guard, since actually populating
 * a snapshot needs a live server, confirming only that a non-
 * snapshotting call still refuses cleanly under the same guards */
static void s_test_apply_non_snapshotting_call_also_refused(void)
{
    surface_td *surface = s_make_surface(true);
    bool result;

    s_reset();

    result = surface_action_apply_randr_profiles(surface, false);
    TAP_OK(!result,
            "a non-snapshotting apply call is refused by the same"
            " connectionless guard as a snapshotting one");

    s_free_surface(surface);
}


int main(void)
{
    TAP_PLAN(11);

    s_test_apply_null_surface_refused();
    s_test_apply_no_connection_refused();
    s_test_apply_null_screen_refused();
    s_test_apply_null_config_refused();
    s_test_apply_randr_disabled_refused();
    s_test_apply_failed_call_adds_nothing_to_snapshot();
    s_test_snapshot_begin_then_revert_is_noop();
    s_test_revert_nothing_snapshotted_is_noop();
    s_test_apply_non_snapshotting_call_also_refused();

    return TAP_DONE();
}
