/**
 * @file tests/wm/startup/test_subscribe.c
 *
 * @brief Test battery for wm/startup/subscribe.c's guard clauses and
 *        RandR-availability short-circuit
 *
 * wm_startup_subscribe_randr_events has two meaningfully
 * unit-testable behaviors in isolation: its leading
 * 'if (wm == NULL || surfaces == NULL || connection == NULL) return
 * -1;' guard, and, once past it, an unconditional
 * 'if (!wm_randr_available(wm)) return 0;' short-circuit that this
 * file can drive either way through a real 'wm_td' with
 * 'is_randr_available' set or clear, entirely without XCB, since
 * 'wm_randr_available' (wm/instance.c) is a plain field read.  Past
 * that short-circuit, the per-surface loop calls
 * 'xcb_randr_select_input_checked' followed by
 * 'xcb_request_check', both real blocking XCB round trips against
 * a live connection, so that loop body itself is out of reach here.
 *
 * The reverse case, 'wm_randr_available(wm)' true, is not exercised
 * even with an empty surfaces list: past that check the function
 * unconditionally calls 'xcb_flush(connection)' before returning,
 * regardless of whether the loop above it found anything to iterate,
 * and 'connection' has to be a real, live 'xcb_connection_t' for
 * that call to be safe rather than a dereference of whatever
 * placeholder non-NULL pointer a guard-clause scenario could supply.
 *
 * wm_startup_subscribe_root_events has only its own leading
 * 'if (wm == NULL || surfaces == NULL || connection == NULL) return
 * -1;' guard as a unit-testable behavior in isolation: every line
 * past it, starting with 'xcb_change_window_attributes_checked'
 * inside its own per-surface loop, is a real blocking XCB round trip,
 * and even the cursor-loading tail past that loop
 * ('util_cursor_ctx_new', 'util_cursor_load', 'xcb_free_cursor')
 * needs a live connection and a live X cursor theme or core font to
 * mean anything.  This file exercises only that guard for this
 * function, and documents the rest as skipped for that reason.
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
#include <utils/cursor.h>
#include <wm/internal.h>
#include <wm/startup/subscribe.h>


/**
 * @brief Link-only stand-in for logger_msg
 *
 * A silent no-op; none of this file's scenarios get past either
 * function's guard clause, or, for the RandR-unavailable scenario,
 * past its own early return, so the real logging call sites further
 * into wm/startup/subscribe.c are never actually reached, matching
 * the same reasoning tests/wm/test_startup.c already documents for
 * this exact stub.
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
 * @brief Link-only stand-in for util_cursor_ctx_new
 *
 * Never actually invoked by any scenario here, since none of them
 * reach wm_startup_subscribe_root_events past its own guard clause;
 * exists only to satisfy the linker.
 *
 * @note Complexity: O(1)
 */
util_cursor_ctx_td *util_cursor_ctx_new(xcb_connection_t *connection,
        xcb_screen_t *screen)
{
    (void) connection;
    (void) screen;
    return NULL;
}


/**
 * @brief Link-only stand-in for util_cursor_load
 *
 * Never actually invoked by any scenario here; exists only to
 * satisfy the linker.
 *
 * @note Complexity: O(1)
 */
xcb_cursor_t util_cursor_load(util_cursor_ctx_td *ctx, const char *name,
        uint16_t fallback_glyph)
{
    (void) ctx;
    (void) name;
    (void) fallback_glyph;
    return XCB_NONE;
}


/**
 * @brief Link-only stand-in for util_cursor_ctx_free
 *
 * Never actually invoked by any scenario here; exists only to
 * satisfy the linker.
 *
 * @note Complexity: O(1)
 */
void util_cursor_ctx_free(util_cursor_ctx_td *ctx)
{
    (void) ctx;
}


/* A NULL wm makes wm_startup_subscribe_randr_events fail its guard
 * clause immediately, without touching any RandR state */
static void s_test_randr_subscribe_null_wm(void)
{
    int result = wm_startup_subscribe_randr_events(NULL);

    TAP_EQ_INT(result, -1,
            "a NULL wm makes wm_startup_subscribe_randr_events"
            " return -1");
}


/* A non-NULL wm with no live connection also fails the guard clause,
 * since wm_connection(wm) resolves to NULL exactly as it would for
 * a NULL wm */
static void s_test_randr_subscribe_null_connection(void)
{
    wm_td local_wm;
    int result;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.connection = NULL;
    local_wm.surfaces = NULL;

    result = wm_startup_subscribe_randr_events(&local_wm);

    TAP_EQ_INT(result, -1,
            "a non-NULL wm with a NULL connection also returns -1,"
            " never reaching the RandR-availability check");
}


/* A non-NULL wm with no surfaces list also fails the guard clause,
 * even with the connection field non-NULL, since wm_surfaces(wm)
 * resolving to NULL is checked independently */
static void s_test_randr_subscribe_null_surfaces(void)
{
    wm_td local_wm;
    int result;
    xcb_connection_t *const fake_connection =
        (xcb_connection_t *) &local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.connection = fake_connection;
    local_wm.surfaces = NULL;

    result = wm_startup_subscribe_randr_events(&local_wm);

    TAP_EQ_INT(result, -1,
            "a non-NULL wm with a NULL surfaces list also returns -1,"
            " even with a non-NULL connection field");
}


/* Past the guard clause, an unavailable RandR extension makes
 * wm_startup_subscribe_randr_events return 0 immediately, before
 * ever dereferencing the connection field again; a placeholder,
 * non-NULL connection value is therefore safe to pass here, since
 * this scenario's whole point is that it is never actually used past
 * the guard */
static void s_test_randr_subscribe_unavailable_short_circuits(void)
{
    wm_td local_wm;
    struct list_s local_surfaces;
    int result;
    xcb_connection_t *const placeholder_connection =
        (xcb_connection_t *) &local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    memset(&local_surfaces, 0, sizeof(local_surfaces));
    local_wm.connection = placeholder_connection;
    local_wm.surfaces = &local_surfaces;
    local_wm.is_randr_available = false;

    result = wm_startup_subscribe_randr_events(&local_wm);

    TAP_EQ_INT(result, 0,
            "an unavailable RandR extension makes"
            " wm_startup_subscribe_randr_events return 0 without"
            " ever touching the connection again");
}


int main(void)
{
    TAP_PLAN(4);

    s_test_randr_subscribe_null_wm();
    s_test_randr_subscribe_null_connection();
    s_test_randr_subscribe_null_surfaces();
    s_test_randr_subscribe_unavailable_short_circuits();

    return TAP_DONE();
}
