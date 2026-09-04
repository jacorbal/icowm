/**
 * @file tests/menu/test_draw.c
 *
 * @brief Test battery for the shared menu-window drawing primitives
 *
 * 'menu/draw.c' is a thin, XCB-facing layer over 'render/text.h' and
 * 'utils/safe/safestr.h', with almost no logic of its own beyond null
 * guards and forwarding arguments; the one real algorithm it owns is
 * 'menu_draw_truncate''s shrink-until-it-fits loop.  Every 'xcb_*'
 * call this file's real dependency makes ('xcb_generate_id',
 * 'xcb_create_gc', 'xcb_poly_fill_rectangle', 'xcb_free_gc') is a
 * link-only stand-in below, since no real X server or connection is
 * available here, following the same pattern
 * 'tests/systray/test_layout.c' already established for the raw XCB
 * entry points a drawing routine calls.  'text_draw_string' and
 * 'text_string_measure' (render/text.c) are recording/controllable
 * stand-ins too: what is under test is 'menu/draw.c''s own guard
 * clauses and its truncation loop, never the real glyph-measuring
 * logic those two already have their own coverage elsewhere.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/draw.h>


/** Fake, non-null XCB connection handle, standing in for a live one
 *  wherever draw.c merely forwards it onward to a stubbed function
 *  without ever dereferencing it directly itself */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

/** Call counters and last-seen arguments for every XCB stand-in below,
 *  reset by s_reset before each scenario */
static int s_call_xcb_generate_id;
static int s_call_xcb_create_gc;
static int s_call_xcb_poly_fill_rectangle;
static int s_call_xcb_free_gc;
static xcb_window_t s_last_gc_window;
static uint32_t s_last_gc_color;
static xcb_window_t s_last_rect_window;
static int16_t s_last_rect_x;
static int16_t s_last_rect_y;
static uint16_t s_last_rect_w;
static uint16_t s_last_rect_h;

/** Call counters for the text-renderer stand-ins */
static int s_call_text_draw_string;
static xcb_window_t s_last_draw_window;
static struct position_s s_last_draw_pos;
static char s_last_draw_text[256];

/** Width 'text_string_measure' answers next; test-controlled */
static uint16_t s_measure_reply;
/** Every text measured so far, for the truncation loop to check
 *  against */
static int s_call_text_string_measure;


/**
 * @brief Link-only stand-in for @a xcb_generate_id
 * @note Complexity: @e O(1)
 */
uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    s_call_xcb_generate_id++;
    return 42u;
}


/**
 * @brief Recording stand-in for @a xcb_create_gc
 *
 * Captures the target window and the foreground color requested, the
 * two things 'menu_draw_row_bg' hands it, so a test can confirm the
 * right color reached the right window without a live graphics
 * context ever being created.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_create_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    (void) value_mask;
    s_call_xcb_create_gc++;
    s_last_gc_window = drawable;
    s_last_gc_color = *(const uint32_t *) value_list;
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_poly_fill_rectangle
 *
 * Captures the single rectangle 'menu_draw_row_bg' always passes, so
 * a test can confirm the exact row geometry it computed.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t rects_len,
        const xcb_rectangle_t *rects)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    (void) rects_len;
    s_call_xcb_poly_fill_rectangle++;
    s_last_rect_window = drawable;
    s_last_rect_x = rects[0].x;
    s_last_rect_y = rects[0].y;
    s_last_rect_w = rects[0].width;
    s_last_rect_h = rects[0].height;
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_free_gc
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    s_call_xcb_free_gc++;
    return cookie;
}


/**
 * @brief Recording stand-in for @a text_draw_string
 *
 * Captures the drawable, position, and text 'menu_draw_label' forwards
 * to it, since the real implementation needs a live XCB font
 * connection that is not available here.
 *
 * @note Complexity: @e O(1)
 */
void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) gc;
    s_call_text_draw_string++;
    s_last_draw_window = drawable;
    s_last_draw_pos = pos;
    s_last_draw_text[0] = '\0';
    if (text != NULL) {
        (void) strncpy(s_last_draw_text, text,
                sizeof(s_last_draw_text) - 1u);
        s_last_draw_text[sizeof(s_last_draw_text) - 1u] = '\0';
    }
}


/**
 * @brief Test-controlled stand-in for @a text_string_measure
 *
 * Answers 's_measure_reply' for every call while the buffer still has
 * more than 3 characters, and progressively smaller widths as
 * characters are trimmed, imitating a real font closely enough for
 * 'menu_draw_truncate''s shrink loop to actually terminate: each
 * character removed is worth 10 pixels, the same shape a real
 * fixed-width font's measurement would take.
 *
 * @note Complexity: @e O(1)
 */
uint16_t text_string_measure(const char *text)
{
    size_t len;

    s_call_text_string_measure++;
    if (text == NULL) {
        return 0u;
    }

    len = strlen(text);
    return (uint16_t) (len * 10u);
}


static void s_reset(void)
{
    s_call_xcb_generate_id = 0;
    s_call_xcb_create_gc = 0;
    s_call_xcb_poly_fill_rectangle = 0;
    s_call_xcb_free_gc = 0;
    s_last_gc_window = XCB_WINDOW_NONE;
    s_last_gc_color = 0u;
    s_last_rect_window = XCB_WINDOW_NONE;
    s_last_rect_x = 0;
    s_last_rect_y = 0;
    s_last_rect_w = 0;
    s_last_rect_h = 0;
    s_call_text_draw_string = 0;
    s_last_draw_window = XCB_WINDOW_NONE;
    s_last_draw_pos.x = 0;
    s_last_draw_pos.y = 0;
    s_last_draw_text[0] = '\0';
    s_measure_reply = 0u;
    s_call_text_string_measure = 0;
}


/* A null connection or window is a no-op: no GC created, no fill
 * request sent */
static void s_test_row_bg_null_guards(void)
{
    s_reset();

    menu_draw_row_bg(NULL, 7u, 0x112233u, 0, 20, 100);
    TAP_EQ_INT(s_call_xcb_create_gc, 0,
            "a null connection skips creating a GC entirely");

    menu_draw_row_bg(s_fake_connection, XCB_WINDOW_NONE, 0x112233u,
            0, 20, 100);
    TAP_EQ_INT(s_call_xcb_create_gc, 0,
            "XCB_WINDOW_NONE also skips creating a GC entirely");
}


/* A normal call creates exactly one GC with the requested color, fills
 * exactly one rectangle at the requested geometry, then frees the GC */
static void s_test_row_bg_fills_requested_rect(void)
{
    s_reset();

    menu_draw_row_bg(s_fake_connection, 55u, 0xff8800u, 12, 20, 140);

    TAP_EQ_INT(s_call_xcb_generate_id, 1,
            "one GC id is generated per call");
    TAP_EQ_INT(s_call_xcb_create_gc, 1,
            "exactly one GC is created");
    TAP_EQ_INT((long) s_last_gc_window, (long) 55u,
            "the GC is created against the row's own window");
    TAP_EQ_INT((long) s_last_gc_color, (long) 0xff8800u,
            "the GC's foreground is the requested color");
    TAP_EQ_INT(s_call_xcb_poly_fill_rectangle, 1,
            "exactly one rectangle is filled");
    TAP_EQ_INT((long) s_last_rect_window, (long) 55u,
            "the rectangle is filled on the row's own window");
    TAP_EQ_INT(s_last_rect_x, 0,
            "the rectangle always starts at x=0, spanning the full row");
    TAP_EQ_INT(s_last_rect_y, 12,
            "the rectangle's y matches the requested row_y");
    TAP_EQ_INT(s_last_rect_w, 140,
            "the rectangle's width matches the requested w");
    TAP_EQ_INT(s_last_rect_h, 20,
            "the rectangle's height matches the requested row_h");
    TAP_EQ_INT(s_call_xcb_free_gc, 1,
            "the GC is freed again before returning");
}


/* A null connection or null text is a no-op for the label draw */
static void s_test_label_null_guards(void)
{
    s_reset();

    menu_draw_label(NULL, 9u, (struct position_s) { 1, 2 }, "hello");
    TAP_EQ_INT(s_call_text_draw_string, 0,
            "a null connection skips drawing the label entirely");

    menu_draw_label(s_fake_connection, 9u, (struct position_s) { 1, 2 },
            NULL);
    TAP_EQ_INT(s_call_text_draw_string, 0,
            "null text also skips drawing the label entirely");
}


/* A normal label call forwards window, position, and text unchanged,
 * always with XCB_NONE as the graphics context */
static void s_test_label_forwards_arguments(void)
{
    s_reset();

    menu_draw_label(s_fake_connection, 77u,
            (struct position_s) { 10, 30 }, "some window title");

    TAP_EQ_INT(s_call_text_draw_string, 1,
            "text_draw_string is called exactly once");
    TAP_EQ_INT((long) s_last_draw_window, (long) 77u,
            "the label is drawn on the requested window");
    TAP_EQ_INT(s_last_draw_pos.x, 10, "the label's x is forwarded");
    TAP_EQ_INT(s_last_draw_pos.y, 30, "the label's y is forwarded");
    TAP_EQ_STR(s_last_draw_text, "some window title",
            "the label's text is forwarded unchanged");
}


/* menu_draw_measure forwards to text_string_measure, and returns 0 for
 * a null string without ever calling it */
static void s_test_measure_forwards_and_guards_null(void)
{
    uint16_t result;

    s_reset();

    result = menu_draw_measure(NULL);
    TAP_EQ_INT(result, 0, "measuring a null string returns 0");
    TAP_EQ_INT(s_call_text_string_measure, 0,
            "a null string never reaches text_string_measure at all");

    result = menu_draw_measure("abc");
    TAP_EQ_INT(result, 30,
            "a non-null string is measured via text_string_measure"
            " (3 chars * 10px stand-in)");
    TAP_EQ_INT(s_call_text_string_measure, 1,
            "text_string_measure is called exactly once");
}


/* A null buffer, an empty buffer, or one that already fits is left
 * completely untouched */
static void s_test_truncate_leaves_short_text_alone(void)
{
    char buf[32];

    s_reset();
    menu_draw_truncate(NULL, 100);
    TAP_EQ_INT(s_call_text_string_measure, 0,
            "a null buffer never even measures anything");

    s_reset();
    buf[0] = '\0';
    menu_draw_truncate(buf, 100);
    TAP_EQ_INT(s_call_text_string_measure, 0,
            "an empty buffer is caught by the buf[0] == '\\0' guard"
            " before ever measuring anything");
    TAP_EQ_STR(buf, "", "the empty buffer is still empty afterward");

    s_reset();
    (void) strcpy(buf, "hi");
    menu_draw_truncate(buf, 100);
    TAP_EQ_STR(buf, "hi",
            "a buffer that already measures within max_w is untouched");
}


/* A buffer wider than max_w is shortened, one character at a time,
 * until it measures no wider than max_w */
static void s_test_truncate_shrinks_to_fit(void)
{
    char buf[32];

    s_reset();
    /* Stand-in measures 10px per character, so "widewidewide" (12
     * chars, 120px) truncated to fit 55px must shrink to 5 chars
     * (50px), since 6 chars (60px) would still be over */
    (void) strcpy(buf, "widewidewide");
    menu_draw_truncate(buf, 55);
    TAP_EQ_INT((int) strlen(buf), 5,
            "the buffer shrinks one character at a time until it fits"
            " within max_w");
    TAP_OK(text_string_measure(buf) <= 55,
            "the final buffer actually measures no wider than max_w");
}


/* A buffer that cannot fit even a single character (max_w smaller than
 * one character's own width) is shrunk all the way down to empty,
 * never left in some intermediate, still-too-wide state */
static void s_test_truncate_can_empty_the_buffer(void)
{
    char buf[32];

    s_reset();
    (void) strcpy(buf, "x");
    menu_draw_truncate(buf, 0);
    TAP_EQ_STR(buf, "",
            "a max_w narrower than any single character empties the"
            " buffer rather than looping forever or leaving it"
            " too wide");
}


int main(void)
{
    TAP_PLAN(31);

    s_test_row_bg_null_guards();
    s_test_row_bg_fills_requested_rect();
    s_test_label_null_guards();
    s_test_label_forwards_arguments();
    s_test_measure_forwards_and_guards_null();
    s_test_truncate_leaves_short_text_alone();
    s_test_truncate_shrinks_to_fit();
    s_test_truncate_can_empty_the_buffer();

    return TAP_DONE();
}
