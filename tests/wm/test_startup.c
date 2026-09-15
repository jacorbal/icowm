/**
 * @file tests/wm/test_startup.c
 *
 * @brief Test battery for wm/startup.c's extension-probe guard clauses
 *
 * wm_startup_randr_init and wm_startup_sync_init each have exactly one
 * meaningfully unit-testable behavior in isolation: the leading
 * 'if (wm == NULL || connection == NULL) return -1;' guard.  Every
 * line past that guard is a real XCB round trip against a live X
 * server, i.e., xcb_get_extension_data reading a real connection's
 * extension cache, xcb_randr_query_version/xcb_sync_initialize
 * sending real requests and blocking on real replies, and for
 * wm_startup_randr_init, further per-surface CRTC/output probing
 * (xcb_randr_get_screen_resources_current,
 * xcb_randr_get_crtc_info) that only makes sense against a real
 * X server's real display configuration.  None of that is
 * reachable, let alone meaningfully assertable, without one; this
 * file exercises only the guard, and documents the rest as skipped
 * for that reason.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <surface.h>
#include <wm/internal.h>
#include <wm/startup.h>


/**
 * @brief Link-only stand-in for logger_msg
 *
 * A silent no-op, matching the real logger's own behavior whenever
 * logger_start has never run (its first check is 'logger == NULL'),
 * the same reasoning tests/wm/test_lifecycle.c already documents for
 * never calling logger_start at all here either.
 *
 * @note Complexity: O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/**
 * @brief Link-only stand-in for surface_action_randr_apply_profiles
 *
 * Referenced unconditionally in wm_startup_randr_init's body, past
 * the guard clause this file actually exercises; a real call applies
 * a configured RandR output profile against a live surface's real
 * screen, which no guard-clause scenario here ever reaches.
 *
 * @param surface Unused
 * @param force   Unused
 *
 * @return false always, an arbitrary value never observed by any
 *         scenario in this file
 *
 * @note Complexity: O(1)
 */
bool surface_action_randr_apply_profiles(surface_td *surface, bool force)
{
    (void) surface;
    (void) force;
    return false;
}


/* A NULL wm makes wm_startup_randr_init fail its guard clause
 * immediately, without touching any extension state */
static void s_test_randr_init_null_wm(void)
{
    int result = wm_startup_randr_init(NULL);

    TAP_EQ_INT(result, -1,
            "a NULL wm makes wm_startup_randr_init return -1");
}


/* A non-NULL wm with no live connection also fails the guard clause,
 * since wm_connection(wm) resolves to NULL exactly as it would for a
 * NULL wm */
static void s_test_randr_init_null_connection(void)
{
    wm_td local_wm;
    int result;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.connection = NULL;

    result = wm_startup_randr_init(&local_wm);

    TAP_EQ_INT(result, -1,
            "a non-NULL wm with a NULL connection also returns -1,"
            " never reaching any real XRandR round trip");
}


/* A NULL wm makes wm_startup_sync_init fail its guard clause
 * immediately, without touching any extension state */
static void s_test_sync_init_null_wm(void)
{
    int result = wm_startup_sync_init(NULL);

    TAP_EQ_INT(result, -1,
            "a NULL wm makes wm_startup_sync_init return -1");
}


/* A non-NULL wm with no live connection also fails the guard clause,
 * since wm_connection(wm) resolves to NULL exactly as it would for a
 * NULL wm */
static void s_test_sync_init_null_connection(void)
{
    wm_td local_wm;
    int result;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.connection = NULL;

    result = wm_startup_sync_init(&local_wm);

    TAP_EQ_INT(result, -1,
            "a non-NULL wm with a NULL connection also returns -1,"
            " never reaching any real XSync round trip");
}


int main(void)
{
    TAP_PLAN(4);

    s_test_randr_init_null_wm();
    s_test_randr_init_null_connection();
    s_test_sync_init_null_wm();
    s_test_sync_init_null_connection();

    return TAP_DONE();
}
