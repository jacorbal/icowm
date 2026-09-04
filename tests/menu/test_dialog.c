/**
 * @file tests/menu/test_dialog.c
 *
 * @brief Test battery for the layout constants and drawing helpers
 *        shared by every modal dialog type
 *
 * 'src/menu/dialog.c' is linked for real in full, 'dlgutil_resolve_monitor'
 * included, since that is exactly where this file's own pointer-query
 * and monitor-resolution scenarios live.  Every libxcb entry point it
 * reaches (xcb_generate_id, xcb_create_gc, xcb_poly_rectangle,
 * xcb_free_gc, xcb_query_pointer, xcb_query_pointer_reply) is a
 * link-only stand-in below, the same 'tests/menu/dialog/test_confirm.c'
 * pattern of re-declaring libxcb's own entry points rather than linking
 * the real library, since there is no X server for this test binary to
 * actually talk to.  'surface_monitor_for_point' is likewise a
 * recording stand-in: it is a whole other module's own logic
 * ('src/surface/monitors.c'), not anything 'dialog.c' itself defines,
 * so this file only needs to control what it answers, not exercise it.
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
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/dialog.h>


/** Fake, non-null XCB connection handle, standing in for a live one
 *  wherever dialog.c merely forwards it onward to a stubbed libxcb
 *  call without ever dereferencing it directly itself */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

/** Fake, non-null XCB screen, so 'surface->screen != NULL' takes the
 *  pointer-query branch in 'dlgutil_resolve_monitor' */
static xcb_screen_t s_fake_screen;

/** Whether this file's own xcb_query_pointer_reply stand-in answers a
 *  reply or NULL, and what root-relative position it reports when it
 *  does */
static bool s_pointer_query_succeeds;
static int16_t s_pointer_root_x;
static int16_t s_pointer_root_y;

/** Monitor this file's own surface_monitor_for_point stand-in answers,
 *  and the last point it was asked to resolve */
static monitor_td s_stub_monitor;
static struct position_s s_last_resolved_point;
static int s_call_monitor_for_point;

static int s_call_generate_id;
static int s_call_create_gc;
static int s_call_poly_rectangle;
static int s_call_free_gc;
static xcb_window_t s_last_poly_rectangle_window;
static uint32_t s_last_poly_rectangle_count;
static xcb_rectangle_t s_last_rect;


/**
 * @brief Reset every stand-in's recorded call state between scenarios
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_pointer_query_succeeds = true;
    s_pointer_root_x = 0;
    s_pointer_root_y = 0;
    memset(&s_stub_monitor, 0, sizeof(s_stub_monitor));
    memset(&s_last_resolved_point, 0, sizeof(s_last_resolved_point));
    s_call_monitor_for_point = 0;
    s_call_generate_id = 0;
    s_call_create_gc = 0;
    s_call_poly_rectangle = 0;
    s_call_free_gc = 0;
    s_last_poly_rectangle_window = XCB_WINDOW_NONE;
    s_last_poly_rectangle_count = 0u;
    memset(&s_last_rect, 0, sizeof(s_last_rect));
}


/**
 * @brief Link-only stand-in for @a xcb_generate_id
 * @note Complexity: @e O(1)
 */
uint32_t xcb_generate_id(xcb_connection_t *c)
{
    (void) c;
    s_call_generate_id++;
    return 100u + (uint32_t) s_call_generate_id;
}


/**
 * @brief Link-only stand-in for @a xcb_create_gc
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_create_gc(xcb_connection_t *c, xcb_gcontext_t cid,
        xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) cid;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    s_call_create_gc++;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_poly_rectangle
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_poly_rectangle(xcb_connection_t *c,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t rectangles_len,
        const xcb_rectangle_t *rectangles)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) gc;
    s_call_poly_rectangle++;
    s_last_poly_rectangle_window = (xcb_window_t) drawable;
    s_last_poly_rectangle_count = rectangles_len;
    if (rectangles_len > 0u && rectangles != NULL) {
        s_last_rect = rectangles[0];
    }
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_free_gc
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_free_gc(xcb_connection_t *c, xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) gc;
    s_call_free_gc++;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_query_pointer
 * @note Complexity: @e O(1)
 */
xcb_query_pointer_cookie_t xcb_query_pointer(xcb_connection_t *c,
        xcb_window_t window)
{
    xcb_query_pointer_cookie_t cookie;

    (void) c;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Test-controlled stand-in for @a xcb_query_pointer_reply
 *
 * Answers a heap-allocated reply reporting the fixed
 * (s_pointer_root_x, s_pointer_root_y) position when
 * s_pointer_query_succeeds is set, or NULL otherwise, so each
 * scenario controls for itself whether 'dlgutil_resolve_monitor'
 * takes its pointer-driven branch or its monitor-less fallback.
 *
 * @note Complexity: @e O(1)
 */
xcb_query_pointer_reply_t *xcb_query_pointer_reply(xcb_connection_t *c,
        xcb_query_pointer_cookie_t cookie, xcb_generic_error_t **e)
{
    xcb_query_pointer_reply_t *reply;

    (void) c;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }
    if (!s_pointer_query_succeeds) {
        return NULL;
    }

    reply = malloc(sizeof(*reply));
    memset(reply, 0, sizeof(*reply));
    reply->root_x = s_pointer_root_x;
    reply->root_y = s_pointer_root_y;
    return reply;
}


/**
 * @brief Test-controlled stand-in for @a surface_monitor_for_point
 *
 * Records the point it was asked to resolve and answers whichever
 * fixed monitor the current scenario configured, so this file
 * controls 'dlgutil_resolve_monitor' 's own fallback-to-whole-surface
 * branch (an all-zero monitor) independently from whether the pointer
 * query itself succeeded.
 *
 * @note Complexity: @e O(1)
 */
monitor_td surface_monitor_for_point(const surface_td *surface,
        struct position_s point)
{
    (void) surface;
    s_call_monitor_for_point++;
    s_last_resolved_point = point;
    return s_stub_monitor;
}


/**
 * @brief Build a minimal, real surface fixture
 * @note Complexity: @e O(1)
 */
static void s_make_surface(surface_td *surface, bool with_screen)
{
    memset(surface, 0, sizeof(*surface));
    surface->screen = (with_screen) ? &s_fake_screen : NULL;
    surface->properties.dim.w = 1920u;
    surface->properties.dim.h = 1080u;
}


/* dlgutil_u16max answers the greater of its two arguments, in either
 * order, and either value when they are equal */
static void s_test_u16max(void)
{
    TAP_EQ_INT(dlgutil_u16max(3u, 7u), 7, "dlgutil_u16max: b greater");
    TAP_EQ_INT(dlgutil_u16max(7u, 3u), 7, "dlgutil_u16max: a greater");
    TAP_EQ_INT(dlgutil_u16max(5u, 5u), 5, "dlgutil_u16max: equal values");
    TAP_EQ_INT(dlgutil_u16max(0u, 0u), 0, "dlgutil_u16max: both zero");
}


/* dlgutil_button_border_draw is a no-op on every guarded invalid input */
static void s_test_button_border_draw_guards(void)
{
    struct geometry_s geom;

    geom.pos.x = 0;
    geom.pos.y = 0;
    geom.dim.w = 40u;
    geom.dim.h = 20u;

    s_reset();
    dlgutil_button_border_draw(NULL, 1u, 0xffffffu, 1u, geom);
    TAP_EQ_INT(s_call_create_gc, 0,
            "button_border_draw: NULL connection is a no-op");

    s_reset();
    dlgutil_button_border_draw(s_fake_connection, XCB_WINDOW_NONE,
            0xffffffu, 1u, geom);
    TAP_EQ_INT(s_call_create_gc, 0,
            "button_border_draw: XCB_WINDOW_NONE window is a no-op");

    s_reset();
    geom.dim.w = 0u;
    dlgutil_button_border_draw(s_fake_connection, 5u, 0xffffffu, 1u, geom);
    TAP_EQ_INT(s_call_create_gc, 0,
            "button_border_draw: zero width geometry is a no-op");

    s_reset();
    geom.dim.w = 40u;
    geom.dim.h = 0u;
    dlgutil_button_border_draw(s_fake_connection, 5u, 0xffffffu, 1u, geom);
    TAP_EQ_INT(s_call_create_gc, 0,
            "button_border_draw: zero height geometry is a no-op");

    s_reset();
    geom.dim.h = 20u;
    dlgutil_button_border_draw(s_fake_connection, 5u, 0xffffffu, 0u, geom);
    TAP_EQ_INT(s_call_create_gc, 0,
            "button_border_draw: zero border width is a no-op");
}


/* dlgutil_button_border_draw draws exactly one inset rectangle and
 * releases its graphics context when every argument is valid */
static void s_test_button_border_draw_draws_inset_rect(void)
{
    struct geometry_s geom;

    geom.pos.x = 10;
    geom.pos.y = 20;
    geom.dim.w = 50u;
    geom.dim.h = 30u;

    s_reset();
    dlgutil_button_border_draw(s_fake_connection, 42u, 0xff0000u, 4u, geom);

    TAP_EQ_INT(s_call_create_gc, 1,
            "button_border_draw: creates exactly one graphics context");
    TAP_EQ_INT(s_call_poly_rectangle, 1,
            "button_border_draw: draws exactly one rectangle outline");
    TAP_EQ_INT((int) s_last_poly_rectangle_count, 1,
            "button_border_draw: the poly-rectangle call lists one rect");
    TAP_EQ_INT((int) s_last_poly_rectangle_window, 42,
            "button_border_draw: draws on the given window");
    TAP_EQ_INT(s_call_free_gc, 1,
            "button_border_draw: frees the graphics context it created");
    TAP_EQ_INT(s_last_rect.x, 12,
            "button_border_draw: rect inset by half the border width (x)");
    TAP_EQ_INT(s_last_rect.y, 22,
            "button_border_draw: rect inset by half the border width (y)");
    TAP_EQ_INT(s_last_rect.width, 46,
            "button_border_draw: rect width shrunk by the border width");
    TAP_EQ_INT(s_last_rect.height, 26,
            "button_border_draw: rect height shrunk by the border width");
}


/* dlgutil_resolve_monitor answers an all-zero monitor without touching
 * XCB or the surface's dimensions when surface is NULL */
static void s_test_resolve_monitor_null_surface(void)
{
    monitor_td monitor;

    s_reset();
    monitor = dlgutil_resolve_monitor(s_fake_connection, NULL);

    TAP_EQ_INT((int) monitor.w, 0,
            "resolve_monitor: NULL surface answers zero width");
    TAP_EQ_INT((int) monitor.h, 0,
            "resolve_monitor: NULL surface answers zero height");
    TAP_EQ_INT(s_call_monitor_for_point, 0,
            "resolve_monitor: NULL surface never queries a monitor");
}


/* A successful pointer query resolves the monitor under the pointer's
 * reported root position */
static void s_test_resolve_monitor_uses_pointer_position(void)
{
    surface_td surface;
    monitor_td monitor;

    s_make_surface(&surface, true);
    s_reset();
    s_pointer_query_succeeds = true;
    s_pointer_root_x = 640;
    s_pointer_root_y = 480;
    s_stub_monitor.x = 0;
    s_stub_monitor.y = 0;
    s_stub_monitor.w = 800u;
    s_stub_monitor.h = 600u;

    monitor = dlgutil_resolve_monitor(s_fake_connection, &surface);

    TAP_EQ_INT(s_call_monitor_for_point, 1,
            "resolve_monitor: resolves through surface_monitor_for_point"
            " once");
    TAP_EQ_INT((int) s_last_resolved_point.x, 640,
            "resolve_monitor: resolves at the pointer's reported x");
    TAP_EQ_INT((int) s_last_resolved_point.y, 480,
            "resolve_monitor: resolves at the pointer's reported y");
    TAP_EQ_INT((int) monitor.w, 800,
            "resolve_monitor: answers the resolved monitor's width");
    TAP_EQ_INT((int) monitor.h, 600,
            "resolve_monitor: answers the resolved monitor's height");
}


/* A failed pointer query falls back to a monitor spanning the whole
 * surface */
static void s_test_resolve_monitor_falls_back_on_query_failure(void)
{
    surface_td surface;
    monitor_td monitor;

    s_make_surface(&surface, true);
    s_reset();
    s_pointer_query_succeeds = false;

    monitor = dlgutil_resolve_monitor(s_fake_connection, &surface);

    TAP_EQ_INT((int) monitor.x, 0,
            "resolve_monitor: query failure falls back to x = 0");
    TAP_EQ_INT((int) monitor.y, 0,
            "resolve_monitor: query failure falls back to y = 0");
    TAP_EQ_INT((int) monitor.w, 1920,
            "resolve_monitor: query failure falls back to surface width");
    TAP_EQ_INT((int) monitor.h, 1080,
            "resolve_monitor: query failure falls back to surface height");
}


/* A NULL connection skips the pointer query outright and falls back to
 * a monitor spanning the whole surface, the same as a failed query */
static void s_test_resolve_monitor_null_connection_falls_back(void)
{
    surface_td surface;
    monitor_td monitor;

    s_make_surface(&surface, true);
    s_reset();

    monitor = dlgutil_resolve_monitor(NULL, &surface);

    TAP_EQ_INT(s_call_monitor_for_point, 0,
            "resolve_monitor: NULL connection never queries a monitor");
    TAP_EQ_INT((int) monitor.w, 1920,
            "resolve_monitor: NULL connection falls back to surface"
            " width");
    TAP_EQ_INT((int) monitor.h, 1080,
            "resolve_monitor: NULL connection falls back to surface"
            " height");
}


/* A resolved monitor reporting zero width or height (as this file's
 * own stand-in can be told to do) is treated the same as a query
 * failure: dimensions fall back to the whole surface */
static void s_test_resolve_monitor_zero_size_falls_back(void)
{
    surface_td surface;
    monitor_td monitor;

    s_make_surface(&surface, true);
    s_reset();
    s_pointer_query_succeeds = true;
    s_stub_monitor.x = 5;
    s_stub_monitor.y = 5;
    s_stub_monitor.w = 0u;
    s_stub_monitor.h = 0u;

    monitor = dlgutil_resolve_monitor(s_fake_connection, &surface);

    TAP_EQ_INT((int) monitor.w, 1920,
            "resolve_monitor: zero-width resolved monitor falls back to"
            " surface width");
    TAP_EQ_INT((int) monitor.h, 1080,
            "resolve_monitor: zero-height resolved monitor falls back to"
            " surface height");
}


/* menu_dialog_center is a no-op, leaving out_x/out_y untouched, when
 * either output pointer is NULL */
static void s_test_center_null_output_guards(void)
{
    int16_t x;
    int16_t y;
    surface_td surface;

    s_make_surface(&surface, true);
    x = -1;
    y = -1;
    s_reset();
    menu_dialog_center(s_fake_connection, &surface, 100u, 50u, NULL, &y);
    TAP_EQ_INT((int) y, -1,
            "center: NULL out_x leaves out_y untouched (no-op)");

    x = -1;
    y = -1;
    s_reset();
    menu_dialog_center(s_fake_connection, &surface, 100u, 50u, &x, NULL);
    TAP_EQ_INT((int) x, -1,
            "center: NULL out_y leaves out_x untouched (no-op)");
}


/* menu_dialog_center reports (0, 0) for a NULL surface, without
 * touching XCB at all */
static void s_test_center_null_surface(void)
{
    int16_t x = -5;
    int16_t y = -5;

    s_reset();
    menu_dialog_center(s_fake_connection, NULL, 100u, 50u, &x, &y);

    TAP_EQ_INT((int) x, 0, "center: NULL surface reports x = 0");
    TAP_EQ_INT((int) y, 0, "center: NULL surface reports y = 0");
    TAP_EQ_INT(s_call_monitor_for_point, 0,
            "center: NULL surface never resolves a monitor");
}


/* menu_dialog_center centers a dialog smaller than the monitor exactly
 * in the middle of it */
static void s_test_center_centers_within_monitor(void)
{
    surface_td surface;
    int16_t x = 0;
    int16_t y = 0;

    s_make_surface(&surface, true);
    s_reset();
    s_pointer_query_succeeds = true;
    s_stub_monitor.x = 100;
    s_stub_monitor.y = 200;
    s_stub_monitor.w = 800u;
    s_stub_monitor.h = 600u;

    menu_dialog_center(s_fake_connection, &surface, 200u, 100u, &x, &y);

    TAP_EQ_INT((int) x, 100 + (800 - 200) / 2,
            "center: horizontally centered within the resolved monitor");
    TAP_EQ_INT((int) y, 200 + (600 - 100) / 2,
            "center: vertically centered within the resolved monitor");
}


/* A dialog as large as or larger than the monitor in one axis is
 * pinned flush to that axis's monitor origin instead of going negative */
static void s_test_center_clamps_oversized_dialog(void)
{
    surface_td surface;
    int16_t x = 0;
    int16_t y = 0;

    s_make_surface(&surface, true);
    s_reset();
    s_pointer_query_succeeds = true;
    s_stub_monitor.x = 50;
    s_stub_monitor.y = 60;
    s_stub_monitor.w = 300u;
    s_stub_monitor.h = 200u;

    menu_dialog_center(s_fake_connection, &surface, 300u, 400u, &x, &y);

    TAP_EQ_INT((int) x, 50,
            "center: dialog exactly as wide as the monitor is pinned to"
            " its left edge");
    TAP_EQ_INT((int) y, 60,
            "center: an over-tall dialog is pinned to the monitor's top"
            " edge");
}


int main(void)
{
    TAP_PLAN(44);

    s_test_u16max();
    s_test_button_border_draw_guards();
    s_test_button_border_draw_draws_inset_rect();
    s_test_resolve_monitor_null_surface();
    s_test_resolve_monitor_uses_pointer_position();
    s_test_resolve_monitor_falls_back_on_query_failure();
    s_test_resolve_monitor_null_connection_falls_back();
    s_test_resolve_monitor_zero_size_falls_back();
    s_test_center_null_output_guards();
    s_test_center_null_surface();
    s_test_center_centers_within_monitor();
    s_test_center_clamps_oversized_dialog();

    return TAP_DONE();
}
