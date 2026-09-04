/**
 * @file tests/wm/startup/test_selection.c
 *
 * @brief Test battery for wm/startup/selection.c's guard clause
 *
 * wm_startup_acquire_selection has exactly one meaningfully
 * unit-testable behavior in isolation: its leading
 * 'if (wm == NULL || connection == NULL || surfaces == NULL) return
 * -1;' guard.  Every line past that guard interns the real 'MANAGER'
 * atom (a live XCB round trip through 'atom_intern'), then
 * unconditionally creates a real support window with
 * 'xcb_generate_id' and 'xcb_create_window', both of which need
 * a live connection's real setup data ('xcb_get_setup(connection)')
 * before the per-surface loop is ever reached.  The per-surface loop
 * body itself calls the static 's_acquire_one_screen', which opens
 * with 'xcb_get_selection_owner_reply', a blocking round trip with no
 * way to fabricate a plausible reply short of a real X server on the
 * other end, and, further in, 's_wait_for_relinquish', which polls
 * a real connection's file descriptor with 'xcb_wait_readable' and
 * 'xcb_poll_for_event' against a live 'CLOCK_MONOTONIC' deadline.
 * None of that is reachable, let alone meaningfully assertable,
 * without a live X server; both static helpers are file-local to
 * wm/startup/selection.c and not declared in any header, so they
 * cannot even be called directly from this file to probe them in
 * isolation.  This file exercises only the guard, and documents the
 * rest as skipped for that reason.
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
#include <string.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <utils/time/clock.h>
#include <utils/xcb/reply.h>
#include <utils/xcb/wait.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/window.h>
#include <wm/internal.h>
#include <wm/startup/selection.h>


/**
 * @brief Link-only stand-in for logger_msg
 *
 * A silent no-op; none of this file's scenarios get past the guard
 * clause, so the real logging call sites further into
 * wm/startup/selection.c are never actually reached, matching the
 * same reasoning tests/wm/test_startup.c already documents for this
 * exact stub.
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
 * @brief Link-only stand-in for atom_intern
 *
 * Never actually invoked by any scenario here, since every one of
 * them returns from wm_startup_acquire_selection's guard clause
 * before this call site is reached; exists only to satisfy the
 * linker.
 *
 * @note Complexity: O(1)
 */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;
    return XCB_ATOM_NONE;
}


/**
 * @brief Link-only stand-in for xcb_reply_log_error
 *
 * Never actually invoked by any scenario here; exists only to
 * satisfy the linker.
 *
 * @note Complexity: O(1)
 */
void xcb_reply_log_error(xcb_generic_error_t *error, const char *what)
{
    (void) error;
    (void) what;
}


/**
 * @brief Link-only stand-in for xcb_wait_readable
 *
 * Never actually invoked by any scenario here; exists only to
 * satisfy the linker.
 *
 * @note Complexity: O(1)
 */
bool xcb_wait_readable(xcb_connection_t *connection, int timeout_ms)
{
    (void) connection;
    (void) timeout_ms;
    return false;
}


/**
 * @brief Link-only stand-in for clock_add_ms
 *
 * Never actually invoked by any scenario here; exists only to
 * satisfy the linker.
 *
 * @note Complexity: O(1)
 */
void clock_add_ms(struct timespec *ts, unsigned int ms)
{
    (void) ts;
    (void) ms;
}


/**
 * @brief Link-only stand-in for clock_ms_until
 *
 * Never actually invoked by any scenario here; exists only to
 * satisfy the linker.
 *
 * @note Complexity: O(1)
 */
long clock_ms_until(const struct timespec *due)
{
    (void) due;
    return 0;
}


/**
 * @brief Link-only stand-in for xcb_window_destroy
 *
 * Never actually invoked by any scenario here; exists only to
 * satisfy the linker.
 *
 * @note Complexity: O(1)
 */
void xcb_window_destroy(xcb_window_t window)
{
    (void) window;
}


/* A NULL wm makes wm_startup_acquire_selection fail its guard clause
 * immediately, without touching any selection or window state */
static void s_test_acquire_null_wm(void)
{
    int result = wm_startup_acquire_selection(NULL, false);

    TAP_EQ_INT(result, -1,
            "a NULL wm makes wm_startup_acquire_selection return -1");
}


/* A non-NULL wm with no live connection also fails the guard clause,
 * since wm_connection(wm) resolves to NULL exactly as it would for
 * a NULL wm */
static void s_test_acquire_null_connection(void)
{
    wm_td local_wm;
    int result;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.connection = NULL;
    local_wm.surfaces = NULL;

    result = wm_startup_acquire_selection(&local_wm, false);

    TAP_EQ_INT(result, -1,
            "a non-NULL wm with a NULL connection also returns -1,"
            " never reaching any real XCB round trip");
}


/* A non-NULL wm with no surfaces list also fails the guard clause,
 * even with 'replace_requested' set, since wm_surfaces(wm) resolving
 * to NULL is checked independently of the connection */
static void s_test_acquire_null_surfaces(void)
{
    wm_td local_wm;
    int result;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.connection = NULL;
    local_wm.surfaces = NULL;

    result = wm_startup_acquire_selection(&local_wm, true);

    TAP_EQ_INT(result, -1,
            "a non-NULL wm with a NULL surfaces list also returns -1,"
            " regardless of replace_requested");
}


int main(void)
{
    TAP_PLAN(3);

    s_test_acquire_null_wm();
    s_test_acquire_null_connection();
    s_test_acquire_null_surfaces();

    return TAP_DONE();
}
