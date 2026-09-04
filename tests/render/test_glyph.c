/**
 * @file tests/render/test_glyph.c
 *
 * @brief Test battery for the UTF-8 decoder and the uninitialized-state
 *        fallbacks of the X Render glyph rendering backend
 *
 * @c render/glyph.c's own drawing/upload entry points
 * (@c glyph_renderer_init, @c glyph_draw_string, and the rest) round
 * trip through a real FreeType face, a real fontconfig match, and a
 * real X RENDER glyphset upload; none of that is exercised here.  What
 * this file covers is @c glyph_utf8_next, a plain UTF-8 decoder with
 * nothing glyph-rendering-specific about it, and the small built-in
 * fallback values @c glyph_font_ascent, @c glyph_font_descent, and
 * @c glyph_measure_string return before @c glyph_renderer_init has
 * ever run successfully; @c s_glyph starts zeroed (static storage), so
 * that state is simply this test binary's own starting point, not
 * something a fixture needs to construct.
 *
 * @c xcb_connection_get and @c logger_msg are link-only stand-ins
 * below for @c utils/xcb/connection.h and @c logger.h, matching the
 * pattern in @c tests/wm/test_lifecycle.c; neither is ever actually
 * reached by the functions this file calls, since none of them touch
 * the connection or log anything on this path, but @c render/glyph.c
 * as a whole references both, so the linker needs a definition
 * somewhere.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <render/glyph.h>


/** Link-only stand-in for xcb_connection_get (utils/xcb/connection.c):
 *  render/glyph.c as a whole references it, though nothing here ever
 *  reaches a code path that calls it */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}

/** Link-only stand-in for logger_msg (logger.c), behind the
 *  LOGGER_WARNING/LOGGER_ERROR macros render/glyph.c calls on its
 *  FreeType/fontconfig/X RENDER error paths; none of those paths are
 *  reached by this file's tests */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/* ==================================================================== *
 * glyph_utf8_next                                                       *
 * ==================================================================== */

/* An empty string decodes as codepoint 0 without advancing */
static void s_test_utf8_next_empty(void)
{
    size_t index = 0u;
    uint32_t codepoint = glyph_utf8_next("", &index);

    TAP_EQ_INT(codepoint, 0, "empty string decodes as codepoint 0");
    TAP_EQ_INT((long) index, 0, "...and index is left unchanged");
}

/* Plain ASCII passes through as-is, one byte per codepoint */
static void s_test_utf8_next_ascii(void)
{
    size_t index = 0u;
    uint32_t first = glyph_utf8_next("Ab", &index);
    uint32_t second;

    TAP_EQ_INT(first, (long) 'A', "ASCII byte decodes as its own value");
    TAP_EQ_INT((long) index, 1, "...advancing the index by one byte");
    second = glyph_utf8_next("Ab", &index);
    TAP_EQ_INT(second, (long) 'b',
            "the next ASCII byte decodes correctly in turn");
    TAP_EQ_INT((long) index, 2, "...advancing the index again");
}

/* A 2-byte sequence: U+00E9 (e acute), encoded as 0xC3 0xA9 */
static void s_test_utf8_next_two_byte(void)
{
    const char *text = "\xc3\xa9";
    size_t index = 0u;
    uint32_t codepoint = glyph_utf8_next(text, &index);

    TAP_EQ_INT(codepoint, 0x00e9,
            "a well-formed 2-byte sequence decodes to its codepoint");
    TAP_EQ_INT((long) index, 2, "...advancing past both its bytes");
}

/* A 3-byte sequence: U+20AC (euro sign), encoded as 0xE2 0x82 0xAC */
static void s_test_utf8_next_three_byte(void)
{
    const char *text = "\xe2\x82\xac";
    size_t index = 0u;
    uint32_t codepoint = glyph_utf8_next(text, &index);

    TAP_EQ_INT(codepoint, 0x20ac,
            "a well-formed 3-byte sequence decodes to its codepoint");
    TAP_EQ_INT((long) index, 3, "...advancing past all three bytes");
}

/* A 4-byte sequence: U+1F600 (grinning face), encoded as
 * 0xF0 0x9F 0x98 0x80 */
static void s_test_utf8_next_four_byte(void)
{
    const char *text = "\xf0\x9f\x98\x80";
    size_t index = 0u;
    uint32_t codepoint = glyph_utf8_next(text, &index);

    TAP_EQ_INT(codepoint, 0x1f600,
            "a well-formed 4-byte sequence decodes to its codepoint");
    TAP_EQ_INT((long) index, 4, "...advancing past all four bytes");
}

/* An invalid leading byte (a lone continuation byte, 0x80-0xBF, or a
 * byte no valid UTF-8 lead uses, 0xF8-0xFF) is returned permissively
 * as its own Latin-1 codepoint rather than rejecting the string */
static void s_test_utf8_next_invalid_lead_byte(void)
{
    const char *text = "\xff";
    size_t index = 0u;
    uint32_t codepoint = glyph_utf8_next(text, &index);

    TAP_EQ_INT(codepoint, 0xff,
            "an invalid leading byte falls back to its Latin-1 value");
    TAP_EQ_INT((long) index, 1, "...advancing past that one byte only");
}

/* A malformed continuation byte truncates the sequence early, and
 * returns whatever partial codepoint had been decoded so far */
static void s_test_utf8_next_malformed_continuation(void)
{
    /* Leading byte announces a 3-byte sequence, but the second byte
     * is plain ASCII rather than a continuation byte (0x80-0xBF) */
    const char *text = "\xe2\x41\x41";
    size_t index = 0u;
    uint32_t codepoint = glyph_utf8_next(text, &index);

    TAP_EQ_INT(codepoint, 0x2,
            "a malformed continuation byte returns the partial"
            " codepoint decoded so far");
    TAP_EQ_INT((long) index, 1,
            "...advancing only past the leading byte, not the"
            " byte that broke the sequence");
}

/* A well-formed multi-byte sequence cut short by the string's own
 * null terminator: the one real continuation byte present is still
 * folded into the codepoint (it passes the 0x80-0xBF check), and only
 * the null terminator itself, read as the second continuation byte,
 * fails that check and stops the sequence early */
static void s_test_utf8_next_truncated_at_end_of_string(void)
{
    const char *text = "\xe2\x82";
    size_t index = 0u;
    uint32_t codepoint = glyph_utf8_next(text, &index);

    TAP_EQ_INT(codepoint, 0x82,
            "a sequence truncated by the string's own end returns"
            " the codepoint folded so far, one continuation byte"
            " short of the full 3-byte sequence");
    TAP_EQ_INT((long) index, 2,
            "...advancing past the leading byte and the one real"
            " continuation byte present, but not past the"
            " terminator itself");
}

/* Decoding a whole string byte-by-byte via repeated calls, mixing
 * ASCII and a multi-byte codepoint, matches walking it by hand */
static void s_test_utf8_next_full_string_walk(void)
{
    const char *text = "A\xc3\xa9Z";
    size_t index = 0u;
    uint32_t c1 = glyph_utf8_next(text, &index);
    uint32_t c2 = glyph_utf8_next(text, &index);
    uint32_t c3 = glyph_utf8_next(text, &index);
    uint32_t c4 = glyph_utf8_next(text, &index);

    TAP_EQ_INT(c1, (long) 'A', "walk: first codepoint is 'A'");
    TAP_EQ_INT(c2, 0x00e9, "walk: second codepoint is U+00E9");
    TAP_EQ_INT(c3, (long) 'Z', "walk: third codepoint is 'Z'");
    TAP_EQ_INT(c4, 0,
            "walk: the call past the null terminator returns 0");
    TAP_EQ_INT((long) index, 4,
            "...and the index never advances past the terminator");
}


/* ==================================================================== *
 * Uninitialized-state fallbacks                                         *
 * ==================================================================== */

/* Before any glyph_renderer_init call has ever succeeded, the ascent
 * and descent fall back to the small built-in defaults documented in
 * render/glyph.h, rather than reading uninitialized font state */
static void s_test_ascent_descent_fallback(void)
{
    TAP_EQ_INT(glyph_font_ascent(), 10,
            "glyph_font_ascent falls back to 10 before any font is"
            " open");
    TAP_EQ_INT(glyph_font_descent(), 3,
            "glyph_font_descent falls back to 3 before any font is"
            " open");
}

/* glyph_measure_string returns 0 for any text while uninitialized,
 * whatever the text actually is, since there is no font metrics table
 * yet to sum advances from */
static void s_test_measure_string_uninitialized(void)
{
    TAP_EQ_INT(glyph_measure_string("hello"), 0,
            "glyph_measure_string returns 0 while uninitialized");
    TAP_EQ_INT(glyph_measure_string(""), 0,
            "...even for an empty string");
}

/* A NULL text pointer returns 0 regardless of initialization state,
 * checked ahead of the initialized flag itself */
static void s_test_measure_string_null(void)
{
    TAP_EQ_INT(glyph_measure_string(NULL), 0,
            "glyph_measure_string returns 0 for a NULL string");
}


int main(void)
{
    TAP_PLAN(28);

    s_test_utf8_next_empty();
    s_test_utf8_next_ascii();
    s_test_utf8_next_two_byte();
    s_test_utf8_next_three_byte();
    s_test_utf8_next_four_byte();
    s_test_utf8_next_invalid_lead_byte();
    s_test_utf8_next_malformed_continuation();
    s_test_utf8_next_truncated_at_end_of_string();
    s_test_utf8_next_full_string_walk();

    s_test_ascent_descent_fallback();
    s_test_measure_string_uninitialized();
    s_test_measure_string_null();

    return TAP_DONE();
}
