/**
 * @file handler/error.c
 *
 * @brief X protocol error event handler and connection error strings
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
#include <stddef.h>     /* size_t */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <handler.h>
#include <handler/error.h>


/* Describe an 'xcb_connection_has_error' return value */
const char *handler_error_connection_str(int error_code)
{
    switch (error_code) {
        case XCB_CONN_ERROR:
            return "XCB_CONN_ERROR (socket, pipe, or other stream" \
                " error; most likely the X server itself is gone)";
        case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
            return "XCB_CONN_CLOSED_EXT_NOTSUPPORTED" \
                " (a required X extension is not supported)";
        case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
            return "XCB_CONN_CLOSED_MEM_INSUFFICIENT" \
                " (out of memory)";
        case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
            return "XCB_CONN_CLOSED_REQ_LEN_EXCEED" \
                " (a request exceeded the server's maximum length)";
        case XCB_CONN_CLOSED_PARSE_ERR:
            return "XCB_CONN_CLOSED_PARSE_ERR" \
                " (error parsing the display name)";
        case XCB_CONN_CLOSED_INVALID_SCREEN:
            return "XCB_CONN_CLOSED_INVALID_SCREEN" \
                " (the server has no screen matching the display)";
        case XCB_CONN_CLOSED_FDPASSING_FAILED:
            return "XCB_CONN_CLOSED_FDPASSING_FAILED" \
                " (file descriptor passing failed)";
        default:
            return "unknown XCB connection error code";
    }
}


/* Handle an X protocol error delivered as a response type 0 event */
void handler_error_protocol(const xcb_generic_event_t *event)
{
    static const uint8_t s_routine_target_ops[] = {
        2u,  /* X_ChangeWindowAttributes */
        3u,  /* X_GetWindowAttributes */
        4u,  /* X_DestroyWindow */
        8u,  /* X_MapWindow */
        10u, /* X_UnmapWindow */
        12u, /* X_ConfigureWindow */
        14u, /* X_GetGeometry */
        15u, /* X_QueryTree */
        18u, /* X_ChangeProperty */
        19u, /* X_DeleteProperty */
        20u, /* X_GetProperty */
        42u  /* X_SetInputFocus */
    };
    const xcb_generic_error_t *proto_error;
    bool is_routine_target_op = false;
    bool is_vanished_resource_error;

    proto_error = (const xcb_generic_error_t *) event;

    for (size_t oi = 0u; oi < sizeof(s_routine_target_ops) /
            sizeof(s_routine_target_ops[0]); ++oi) {
        if (proto_error->major_code == s_routine_target_ops[oi]) {
            is_routine_target_op = true;
            break;
        }
    }

    is_vanished_resource_error = is_routine_target_op &&
        (proto_error->error_code == 3u  /* BadWindow */ ||
         proto_error->error_code == 9u  /* BadDrawable */ ||
         proto_error->error_code == 8u  /* BadMatch */);

    if (is_vanished_resource_error) {
        LOGGER_DEBUG("X protocol error (code=%u," \
                " resource=0x%x, major=%u, minor=%u," \
                " sequence=%u)",
                proto_error->error_code,
                proto_error->resource_id,
                proto_error->major_code,
                proto_error->minor_code,
                proto_error->sequence);
    } else {
        LOGGER_WARNING("X protocol error (code=%u," \
                " resource=0x%x, major=%u, minor=%u," \
                " sequence=%u)",
                proto_error->error_code,
                proto_error->resource_id,
                proto_error->major_code,
                proto_error->minor_code,
                proto_error->sequence);
    }
}
