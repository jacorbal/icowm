/**
 * @file tests/utils/xcb/test_pixmap.c
 *
 * @brief Test battery for off-screen pixmap buffer creation
 *
 * Exercises 'xcb_offscreen_buffer_create' (utils/xcb/pixmap.c) linked
 * for real.  'xcb_generate_id' and 'xcb_create_pixmap' are the only
 * two XCB entry points it calls, both stubbed below as controllable,
 * call-recording stand-ins, so no '-lxcb' link is required and every
 * branch (the zero-width/zero-height guard, and the ordinary path
 * that actually asks the server for a new pixmap) can be driven and
 * checked without a real X server.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <utils/xcb/pixmap.h>


/* Controllable stand-in state */

static int s_generate_id_calls = 0;
static xcb_connection_t *s_generate_id_last_connection = NULL;

static int s_create_pixmap_calls = 0;
static uint8_t s_create_pixmap_last_depth = 0u;
static xcb_pixmap_t s_create_pixmap_last_buffer = 0u;
static xcb_drawable_t s_create_pixmap_last_reference = 0u;
static uint16_t s_create_pixmap_last_width = 0u;
static uint16_t s_create_pixmap_last_height = 0u;


/* Raw XCB stand-ins (not linking libxcb at all) */

uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    s_generate_id_calls++;
    s_generate_id_last_connection = connection;
    return 42u;
}

xcb_void_cookie_t xcb_create_pixmap(xcb_connection_t *connection,
        uint8_t depth, xcb_pixmap_t pid, xcb_drawable_t drawable,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    s_create_pixmap_calls++;
    s_create_pixmap_last_depth = depth;
    s_create_pixmap_last_buffer = pid;
    s_create_pixmap_last_reference = drawable;
    s_create_pixmap_last_width = width;
    s_create_pixmap_last_height = height;
    return cookie;
}


static void s_reset(void)
{
    s_generate_id_calls = 0;
    s_generate_id_last_connection = NULL;
    s_create_pixmap_calls = 0;
    s_create_pixmap_last_depth = 0u;
    s_create_pixmap_last_buffer = 0u;
    s_create_pixmap_last_reference = 0u;
    s_create_pixmap_last_width = 0u;
    s_create_pixmap_last_height = 0u;
}


/* A zero width refuses the request outright, without ever asking the
 * server for a new identifier */
static void s_test_zero_width_is_refused(void)
{
    xcb_pixmap_t result;

    s_reset();
    result = xcb_offscreen_buffer_create((xcb_connection_t *) 1, 24u,
            7u, 0u, 100u);

    TAP_EQ_INT((long) result, (long) XCB_NONE,
            "zero width returns XCB_NONE");
    TAP_EQ_INT(s_generate_id_calls, 0,
            "no identifier is generated for a zero-width request");
    TAP_EQ_INT(s_create_pixmap_calls, 0,
            "and no pixmap is created for it either");
}


/* A zero height is refused the same way as a zero width */
static void s_test_zero_height_is_refused(void)
{
    xcb_pixmap_t result;

    s_reset();
    result = xcb_offscreen_buffer_create((xcb_connection_t *) 1, 24u,
            7u, 100u, 0u);

    TAP_EQ_INT((long) result, (long) XCB_NONE,
            "zero height returns XCB_NONE");
    TAP_EQ_INT(s_create_pixmap_calls, 0,
            "no pixmap is created for a zero-height request");
}


/* A non-zero width and height generates one identifier and creates
 * exactly one pixmap with the requested depth, reference drawable,
 * and dimensions */
static void s_test_ordinary_request_creates_pixmap(void)
{
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;
    xcb_pixmap_t result;

    s_reset();
    result = xcb_offscreen_buffer_create(connection, 32u, 99u, 16u,
            20u);

    TAP_EQ_INT((long) result, 42,
            "the new pixmap is the identifier xcb_generate_id handed"
            " back");
    TAP_EQ_INT(s_generate_id_calls, 1,
            "exactly one identifier is generated");
    TAP_OK(s_generate_id_last_connection == connection,
            "the identifier is generated on the given connection");
    TAP_EQ_INT(s_create_pixmap_calls, 1,
            "exactly one pixmap is created");
    TAP_EQ_INT((long) s_create_pixmap_last_depth, 32,
            "at the requested depth");
    TAP_EQ_INT((long) s_create_pixmap_last_buffer, 42,
            "using the just-generated identifier");
    TAP_EQ_INT((long) s_create_pixmap_last_reference, 99,
            "against the given reference drawable");
    TAP_EQ_INT((long) s_create_pixmap_last_width, 16,
            "with the requested width");
    TAP_EQ_INT((long) s_create_pixmap_last_height, 20,
            "and the requested height");
}


int main(void)
{
    TAP_PLAN(14);

    s_test_zero_width_is_refused();
    s_test_zero_height_is_refused();
    s_test_ordinary_request_creates_pixmap();

    return TAP_DONE();
}
