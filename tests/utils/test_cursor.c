/**
 * @file tests/utils/test_cursor.c
 *
 * @brief Test battery for theme-aware cursor loading
 *
 * Exercises 'util_cursor_ctx_new', 'util_cursor_load', and
 * 'util_cursor_ctx_free' (utils/cursor.c) linked for real.  Every
 * 'xcb_cursor_*' and raw XCB entry point it calls is a controllable,
 * call-recording stand-in below, so neither '-lxcb' nor
 * '-lxcb-cursor' is required; 'xcb_connection_get' is stubbed the
 * same way 'tests/utils/test_spawn.c' stubs it.  'util_cursor_ctx_s'
 * is opaque outside 'cursor.c' itself, so every test here observes the
 * context only through its public API and this file's own stub call
 * counts, never through direct field access.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <string.h>     /* strcmp */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>

/* Local includes */
#include <harness/tap.h>
#include <utils/cursor.h>


/* Controllable stand-in state */

static xcb_connection_t *s_connection_stub = (xcb_connection_t *) 0x1;

static int s_context_new_calls = 0;
static int s_context_new_result = 0;

static int s_context_free_calls = 0;

static int s_load_cursor_calls = 0;
static xcb_cursor_t s_load_cursor_result = XCB_NONE;
static const char *s_load_cursor_last_name = NULL;

static int s_generate_id_calls = 0;
static xcb_cursor_t s_next_generated_id = 500u;

static int s_open_font_calls = 0;
static xcb_font_t s_open_font_last_font = 0u;

static int s_create_glyph_cursor_calls = 0;
static uint16_t s_create_glyph_cursor_last_glyph = 0u;
static xcb_font_t s_create_glyph_cursor_last_font = 0u;

static int s_close_font_calls = 0;


/* 'utils/xcb/connection.h' stand-in */

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}


/* 'xcb/xcb_cursor.h' stand-ins */

int xcb_cursor_context_new(xcb_connection_t *conn, xcb_screen_t *screen,
        xcb_cursor_context_t **ctx)
{
    (void) conn;
    (void) screen;
    s_context_new_calls++;
    /* A fixed, never-dereferenced non-NULL value: 'xcb_cursor_context_t'
     * is opaque outside libxcb-cursor itself, and every stand-in in
     * this file that receives it back only ever compares or forwards
     * the pointer, never reads through it */
    *ctx = (xcb_cursor_context_t *) 0x9999;
    return s_context_new_result;
}

xcb_cursor_t xcb_cursor_load_cursor(xcb_cursor_context_t *ctx,
        const char *name)
{
    (void) ctx;
    s_load_cursor_calls++;
    s_load_cursor_last_name = name;
    return s_load_cursor_result;
}

void xcb_cursor_context_free(xcb_cursor_context_t *ctx)
{
    (void) ctx;
    s_context_free_calls++;
}


/* Raw XCB stand-ins (not linking libxcb at all) */

uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    s_generate_id_calls++;
    return s_next_generated_id;
}

xcb_void_cookie_t xcb_open_font(xcb_connection_t *connection,
        xcb_font_t fid, uint16_t name_len, const char *name)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) name_len;
    (void) name;
    s_open_font_calls++;
    s_open_font_last_font = fid;
    return cookie;
}

xcb_void_cookie_t xcb_create_glyph_cursor(xcb_connection_t *connection,
        xcb_cursor_t cid, xcb_font_t source_font, xcb_font_t mask_font,
        uint16_t source_char, uint16_t mask_char,
        uint16_t fore_red, uint16_t fore_green, uint16_t fore_blue,
        uint16_t back_red, uint16_t back_green, uint16_t back_blue)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) cid;
    (void) mask_font;
    (void) mask_char;
    (void) fore_red;
    (void) fore_green;
    (void) fore_blue;
    (void) back_red;
    (void) back_green;
    (void) back_blue;
    s_create_glyph_cursor_calls++;
    s_create_glyph_cursor_last_font = source_font;
    s_create_glyph_cursor_last_glyph = source_char;
    return cookie;
}

xcb_void_cookie_t xcb_close_font(xcb_connection_t *connection,
        xcb_font_t font)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) font;
    s_close_font_calls++;
    return cookie;
}


static void s_reset(void)
{
    s_connection_stub = (xcb_connection_t *) 0x1;
    s_context_new_calls = 0;
    s_context_new_result = 0;
    s_context_free_calls = 0;
    s_load_cursor_calls = 0;
    s_load_cursor_result = XCB_NONE;
    s_load_cursor_last_name = NULL;
    s_generate_id_calls = 0;
    s_next_generated_id = 500u;
    s_open_font_calls = 0;
    s_open_font_last_font = 0u;
    s_create_glyph_cursor_calls = 0;
    s_create_glyph_cursor_last_glyph = 0u;
    s_create_glyph_cursor_last_font = 0u;
    s_close_font_calls = 0;
}


/* A NULL connection or screen refuses to allocate a context at all */
static void s_test_ctx_new_refuses_null_arguments(void)
{
    util_cursor_ctx_td *ctx;

    s_reset();
    ctx = util_cursor_ctx_new(NULL, (xcb_screen_t *) 0x2);
    TAP_OK(ctx == NULL, "a NULL connection refuses to create a context");

    ctx = util_cursor_ctx_new(s_connection_stub, NULL);
    TAP_OK(ctx == NULL, "a NULL screen refuses to create a context");

    TAP_EQ_INT(s_context_new_calls, 0,
            "the theme lookup is never even attempted for either"
            " refusal");
}


/* A successful theme lookup keeps the returned theme context; loading
 * a name the theme provides never touches the fallback font path */
static void s_test_successful_theme_load_skips_fallback(void)
{
    util_cursor_ctx_td *ctx;
    xcb_cursor_t cursor;

    s_reset();
    s_context_new_result = 0;
    s_load_cursor_result = (xcb_cursor_t) 12345u;

    ctx = util_cursor_ctx_new(s_connection_stub, (xcb_screen_t *) 0x2);
    TAP_OK(ctx != NULL, "a successful theme lookup returns a context");
    TAP_EQ_INT(s_context_new_calls, 1,
            "the theme lookup is attempted exactly once");

    cursor = util_cursor_load(ctx, "left_ptr", 68u);
    TAP_EQ_INT((long) cursor, 12345,
            "the theme's own cursor is returned when it provides the"
            " name");
    TAP_EQ_INT(s_load_cursor_calls, 1,
            "the theme is asked for the cursor exactly once");
    TAP_OK(s_load_cursor_last_name != NULL &&
            strcmp(s_load_cursor_last_name, "left_ptr") == 0,
            "asked for the exact requested name");
    TAP_EQ_INT(s_generate_id_calls, 0,
            "no fallback glyph identifier is ever generated");
    TAP_EQ_INT(s_open_font_calls, 0,
            "the fallback font is never opened");

    util_cursor_ctx_free(ctx);
    TAP_EQ_INT(s_context_free_calls, 1,
            "freeing the context releases the underlying theme"
            " lookup exactly once");
    TAP_EQ_INT(s_close_font_calls, 0,
            "the fallback font, never opened, is never closed either");
}


/* A failed theme lookup still returns a usable context, whose every
 * load then goes straight to the fallback font */
static void s_test_failed_theme_lookup_uses_fallback_every_time(void)
{
    util_cursor_ctx_td *ctx;
    xcb_cursor_t cursor;

    s_reset();
    s_context_new_result = -1;
    s_next_generated_id = 700u;

    ctx = util_cursor_ctx_new(s_connection_stub, (xcb_screen_t *) 0x2);
    TAP_OK(ctx != NULL,
            "a failed theme lookup still returns a usable context");

    cursor = util_cursor_load(ctx, "left_ptr", 68u);
    TAP_EQ_INT((long) cursor, 700,
            "the fallback font's freshly generated identifier is"
            " returned");
    TAP_EQ_INT(s_load_cursor_calls, 0,
            "the (unavailable) theme is never actually asked for"
            " anything");
    TAP_EQ_INT(s_open_font_calls, 1,
            "the fallback font is opened exactly once");
    TAP_EQ_INT((long) s_create_glyph_cursor_last_glyph, 68,
            "built from the requested fallback glyph");

    util_cursor_ctx_free(ctx);
    TAP_EQ_INT(s_close_font_calls, 1,
            "freeing the context closes the fallback font that was"
            " actually opened");
}


/* The fallback font is opened only once per context, even across
 * several fallback loads */
static void s_test_fallback_font_opens_only_once(void)
{
    util_cursor_ctx_td *ctx;

    s_reset();
    s_context_new_result = -1;

    ctx = util_cursor_ctx_new(s_connection_stub, (xcb_screen_t *) 0x2);
    (void) util_cursor_load(ctx, "left_ptr", 68u);
    (void) util_cursor_load(ctx, "watch", 150u);
    (void) util_cursor_load(ctx, "fleur", 52u);

    TAP_EQ_INT(s_open_font_calls, 1,
            "three fallback loads on the same context open the font"
            " exactly once");
    TAP_EQ_INT(s_create_glyph_cursor_calls, 3,
            "but each load still creates its own glyph cursor");

    util_cursor_ctx_free(ctx);
}


/* A theme that answers XCB_NONE for a given name (not provided by
 * that theme) falls back exactly the same way a missing theme does */
static void s_test_theme_miss_falls_back(void)
{
    util_cursor_ctx_td *ctx;
    xcb_cursor_t cursor;

    s_reset();
    s_context_new_result = 0;
    s_load_cursor_result = XCB_NONE;
    s_next_generated_id = 900u;

    ctx = util_cursor_ctx_new(s_connection_stub, (xcb_screen_t *) 0x2);
    cursor = util_cursor_load(ctx, "some-name-the-theme-lacks", 30u);

    TAP_EQ_INT(s_load_cursor_calls, 1,
            "the theme is still asked once before falling back");
    TAP_EQ_INT((long) cursor, 900,
            "a theme miss (XCB_NONE) still falls back to the X core"
            " font");

    util_cursor_ctx_free(ctx);
}


/* A NULL ctx, or a NULL name on a valid ctx, both go straight to the
 * fallback path without ever touching the theme lookup */
static void s_test_null_ctx_or_name_goes_straight_to_fallback(void)
{
    util_cursor_ctx_td *ctx;
    xcb_cursor_t cursor;

    s_reset();
    cursor = util_cursor_load(NULL, "left_ptr", 68u);
    TAP_EQ_INT((long) cursor, (long) XCB_NONE,
            "a NULL ctx with no fallback font of its own to open"
            " returns XCB_NONE");

    s_reset();
    s_context_new_result = 0;
    s_next_generated_id = 42u;
    ctx = util_cursor_ctx_new(s_connection_stub, (xcb_screen_t *) 0x2);
    cursor = util_cursor_load(ctx, NULL, 68u);
    TAP_EQ_INT((long) cursor, 42,
            "a NULL name on a valid context still falls back rather"
            " than asking the theme for anything");
    TAP_EQ_INT(s_load_cursor_calls, 0,
            "the theme is never asked for a NULL name");

    util_cursor_ctx_free(ctx);
}


/* 'util_cursor_ctx_free' on a NULL context is a documented no-op */
static void s_test_ctx_free_null_is_a_no_op(void)
{
    s_reset();
    util_cursor_ctx_free(NULL);

    TAP_EQ_INT(s_context_free_calls, 0,
            "freeing a NULL context never touches the theme lookup");
    TAP_EQ_INT(s_close_font_calls, 0,
            "nor the fallback font");
}


int main(void)
{
    TAP_PLAN(27);

    s_test_ctx_new_refuses_null_arguments();
    s_test_successful_theme_load_skips_fallback();
    s_test_failed_theme_lookup_uses_fallback_every_time();
    s_test_fallback_font_opens_only_once();
    s_test_theme_miss_falls_back();
    s_test_null_ctx_or_name_goes_straight_to_fallback();
    s_test_ctx_free_null_is_a_no_op();

    return TAP_DONE();
}
