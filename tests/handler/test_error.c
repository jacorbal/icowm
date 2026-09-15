/**
 * @file tests/handler/test_error.c
 *
 * @brief Test battery for handler/error.c: the connection-error
 *        string table and the response-type-zero protocol error
 *        classifier
 *
 * Both public entry points, handler_error_connection_str and
 * handler_error_protocol, are pure functions of their input: the
 * first is a plain switch over an int with no side effects at all,
 * and the second only ever reads the xcb_generic_error_t reinterpreted
 * from its xcb_generic_event_t pointer and calls logger_msg, which is
 * a link-only stand-in below recording level and the fields the real
 * function would have logged, so no live X connection, no XCB
 * library, and no other project module is needed at all.  Every
 * branch of the "routine target op" table and both outcomes of the
 * "vanished resource" classification are exercised directly.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <handler/error.h>
#include <harness/tap.h>


/* Recording state for the logger_msg stand-in below */
static int s_logger_calls;
static enum logger_level_e s_logger_last_level;

/** Link-only stand-in for logger_msg (logger.c): records the level it
 *  was called with and how many times, rather than formatting or
 *  printing anything */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    va_list args;

    (void) prefix;

    s_logger_calls++;
    s_logger_last_level = level;

    va_start(args, fmt);
    va_end(args);

    return 0;
}


/**
 * @brief Build a raw event carrying one 'xcb_generic_error_t' worth
 *        of fields, the same layout 'handler_error_protocol' itself
 *        reinterprets its argument as
 */
static xcb_generic_event_t *s_build_error_event(
        xcb_generic_event_t *storage, uint8_t error_code,
        uint8_t major_code, uint16_t minor_code, uint32_t resource_id)
{
    xcb_generic_error_t *const err = (xcb_generic_error_t *) storage;

    memset(storage, 0, sizeof(*storage));
    err->response_type = 0u;
    err->error_code = error_code;
    err->sequence = 42u;
    err->resource_id = resource_id;
    err->minor_code = minor_code;
    err->major_code = major_code;

    return storage;
}


int main(void)
{
    xcb_generic_event_t raw;

    TAP_PLAN(20);

    /* handler_error_connection_str: every named case plus the
     * default fallback */
    TAP_EQ_STR(handler_error_connection_str(XCB_CONN_ERROR),
            "XCB_CONN_ERROR (socket, pipe, or other stream error;" \
            " most likely the X server itself is gone)",
            "XCB_CONN_ERROR has its own description");
    TAP_EQ_STR(handler_error_connection_str(
                XCB_CONN_CLOSED_EXT_NOTSUPPORTED),
            "XCB_CONN_CLOSED_EXT_NOTSUPPORTED (a required X extension" \
            " is not supported)",
            "XCB_CONN_CLOSED_EXT_NOTSUPPORTED has its own description");
    TAP_EQ_STR(handler_error_connection_str(
                XCB_CONN_CLOSED_MEM_INSUFFICIENT),
            "XCB_CONN_CLOSED_MEM_INSUFFICIENT (out of memory)",
            "XCB_CONN_CLOSED_MEM_INSUFFICIENT has its own description");
    TAP_EQ_STR(handler_error_connection_str(
                XCB_CONN_CLOSED_REQ_LEN_EXCEED),
            "XCB_CONN_CLOSED_REQ_LEN_EXCEED (a request exceeded the" \
            " server's maximum length)",
            "XCB_CONN_CLOSED_REQ_LEN_EXCEED has its own description");
    TAP_EQ_STR(handler_error_connection_str(
                XCB_CONN_CLOSED_PARSE_ERR),
            "XCB_CONN_CLOSED_PARSE_ERR (error parsing the display name)",
            "XCB_CONN_CLOSED_PARSE_ERR has its own description");
    TAP_EQ_STR(handler_error_connection_str(
                XCB_CONN_CLOSED_INVALID_SCREEN),
            "XCB_CONN_CLOSED_INVALID_SCREEN (the server has no screen" \
            " matching the display)",
            "XCB_CONN_CLOSED_INVALID_SCREEN has its own description");
    TAP_EQ_STR(handler_error_connection_str(
                XCB_CONN_CLOSED_FDPASSING_FAILED),
            "XCB_CONN_CLOSED_FDPASSING_FAILED (file descriptor" \
            " passing failed)",
            "XCB_CONN_CLOSED_FDPASSING_FAILED has its own description");
    TAP_EQ_STR(handler_error_connection_str(999999),
            "unknown XCB connection error code",
            "an unrecognized code falls back to the default case");
    TAP_EQ_STR(handler_error_connection_str(0),
            "unknown XCB connection error code",
            "zero is not one of the named codes either");

    /* handler_error_protocol: a routine target op (X_MapWindow == 8)
     * with a BadWindow (3) is logged at DEBUG, since a client
     * destroying its own window between the request and the server
     * processing it is the expected outcome, not a bug */
    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 3u, 8u, 0u,
                0x1234u));
    TAP_EQ_INT(s_logger_calls, 1, "protocol_error logs exactly once" \
            " for a routine BadWindow");
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_DEBUG,
            "routine op + BadWindow logs at DEBUG");

    /* Same routine op, BadDrawable (9), still DEBUG */
    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 9u, 8u, 0u,
                0x1234u));
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_DEBUG,
            "routine op + BadDrawable logs at DEBUG");

    /* Same routine op, BadMatch (8), still DEBUG */
    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 8u, 8u, 0u,
                0x1234u));
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_DEBUG,
            "routine op + BadMatch logs at DEBUG");

    /* Routine op (X_ConfigureWindow == 12), but a different error
     * code (BadValue == 2): not a vanished-resource error, so WARNING */
    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 2u, 12u, 0u,
                0x5678u));
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_WARNING,
            "routine op + BadValue logs at WARNING, not DEBUG");

    /* A BadWindow (3) on a non-routine major code (e.g., 99, not in
     * the table at all): also WARNING, since the op itself is not one
     * of the routine per-window ones this window manager issues
     * constantly */
    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 3u, 99u, 0u,
                0x9999u));
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_WARNING,
            "BadWindow on a non-routine op logs at WARNING");

    /* Every remaining routine op in the table is reached, at least
     * one representative from each end of the list */
    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 3u, 2u, 0u, 0u));
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_DEBUG,
            "X_ChangeWindowAttributes (2) is a routine op");

    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 3u, 42u, 0u, 0u));
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_DEBUG,
            "X_SetInputFocus (42) is a routine op");

    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 3u, 20u, 0u, 0u));
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_DEBUG,
            "X_GetProperty (20) is a routine op");

    /* A completely unrelated error code (BadAlloc == 11) on a routine
     * op is still WARNING, since only BadWindow/BadDrawable/BadMatch
     * count as a vanished-resource echo */
    s_logger_calls = 0;
    handler_error_protocol(s_build_error_event(&raw, 11u, 4u, 0u, 0u));
    TAP_EQ_INT((int) s_logger_last_level, (int) LOG_WARNING,
            "BadAlloc on a routine op still logs at WARNING");

    /* Exactly one call is made per invocation, regardless of branch */
    TAP_EQ_INT(s_logger_calls, 1,
            "handler_error_protocol logs exactly once per call");

    return TAP_DONE();
}
