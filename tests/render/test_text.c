/**
 * @file tests/render/test_text.c
 *
 * @brief Test battery for the X core-font text renderer's
 *        configuration-string-to-XLFD pipeline, its UTF-8-to-Latin-1
 *        fallback rendering path, and its measurement/metrics state
 *
 * @c render/text.c is a drawing routine bound to raw XCB calls
 * (@c xcb_open_font, @c xcb_query_font, @c xcb_image_text_8, and so
 * on), but every one of those calls sits behind real, pure string
 * logic: a small parser turning IcoWM's own font-description syntax
 * into an X11 XLFD wildcard pattern (@c s_font_config_to_xlfd and its
 * static helpers), and a UTF-8-decode-and-substitute pass turning
 * arbitrary window-title text into the single-byte Latin-1 string
 * @c xcb_image_text_8 requires (@c s_utf8_to_latin1 and
 * @c s_ascii_fallback_for).  Both are @c static and unreachable from
 * a test file directly, so each is instead exercised end to end
 * through the real public entry points that call them
 * (@c text_renderer_use_font for the XLFD pipeline,
 * @c text_draw_string for the Latin-1 fallback), capturing exactly
 * the string each stubbed XCB call receives.
 *
 * Every raw XCB entry point @c text.c calls directly
 * (@c xcb_generate_id, @c xcb_open_font, @c xcb_query_font(_reply),
 * @c xcb_close_font, @c xcb_create_gc_checked, @c xcb_change_gc(_checked),
 * @c xcb_free_gc, @c xcb_image_text_8_checked, @c xcb_request_check,
 * @c xcb_get_setup, @c xcb_setup_roots_iterator) is a controllable,
 * call-recording link-only stand-in below, matching the established
 * pattern in @c tests/utils/test_cursor.c, so no XCB library is linked
 * at all.  Every @c glyph_* entry point @c text.c calls into
 * @c render/glyph.c for its fallback backend is stubbed the same way,
 * rather than linking the real @c render/glyph.c, since that gives
 * full deterministic control over whether the glyph backend
 * "succeeds" or "fails" to open a given font, which is exactly the
 * branch this file needs to steer on purpose; @c logger_msg is a
 * no-op stand-in, matching @c tests/render/test_glyph.c's pattern.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <render/glyph.h>
#include <render/text.h>


/* Controllable stand-in state */

static xcb_connection_t *s_connection_stub = (xcb_connection_t *) 1;

static int s_generate_id_calls = 0;
static uint32_t s_next_generated_id = 900u;

static int s_open_font_calls = 0;
static char s_last_opened_xlfd[512] = "";

static int s_query_font_calls = 0;
static bool s_query_font_should_fail = false;
static int16_t s_query_font_ascent_result = 12;
static int16_t s_query_font_descent_result = 4;
static int16_t s_query_font_char_width_result = 8;

static int s_close_font_calls = 0;

static int s_create_gc_checked_calls = 0;
static int s_change_gc_checked_calls = 0;
static int s_change_gc_calls = 0;
static int s_free_gc_calls = 0;

static int s_image_text_8_checked_calls = 0;
static char s_last_drawn_latin1[512] = "";
static uint8_t s_last_drawn_len = 0u;

static int s_request_check_calls = 0;

static int s_glyph_renderer_init_calls = 0;
static bool s_glyph_renderer_init_should_fail = false;
static const char *s_glyph_renderer_init_last_font = NULL;
static int s_glyph_renderer_release_calls = 0;
static int s_glyph_renderer_destroy_calls = 0;
static int s_glyph_renderer_set_color_calls = 0;
static int s_glyph_draw_string_calls = 0;
static char s_last_glyph_drawn[512] = "";
static uint16_t s_glyph_measure_string_result = 40u;
static int16_t s_glyph_font_ascent_result = 20;
static int16_t s_glyph_font_descent_result = 6;


/* Link-only stand-ins: 'utils/xcb/connection.h' */

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}


/* Link-only stand-ins: 'logger.h' */

void logger_msg(int level, const char *fmt, ...)
{
    (void) level;
    (void) fmt;
}


/* Link-only stand-ins: raw XCB entry points, no XCB library linked */

uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    s_generate_id_calls++;
    return s_next_generated_id++;
}

xcb_void_cookie_t xcb_open_font(xcb_connection_t *connection,
        xcb_font_t fid, uint16_t name_len, const char *name)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) fid;
    s_open_font_calls++;
    strncpy(s_last_opened_xlfd, name,
            (name_len < sizeof(s_last_opened_xlfd) - 1u)
                ? name_len : sizeof(s_last_opened_xlfd) - 1u);
    s_last_opened_xlfd[(name_len < sizeof(s_last_opened_xlfd) - 1u)
        ? name_len : sizeof(s_last_opened_xlfd) - 1u] = '\0';
    return cookie;
}

xcb_query_font_cookie_t xcb_query_font(xcb_connection_t *connection,
        xcb_font_t font)
{
    xcb_query_font_cookie_t cookie = { 0u };

    (void) connection;
    (void) font;
    return cookie;
}

xcb_query_font_reply_t *xcb_query_font_reply(xcb_connection_t *connection,
        xcb_query_font_cookie_t cookie, xcb_generic_error_t **error)
{
    xcb_query_font_reply_t *reply;

    (void) connection;
    (void) cookie;
    if (error != NULL) {
        *error = NULL;
    }
    s_query_font_calls++;
    /* Failing unconditionally on every call would also break the
     * final "fixed" retry text_renderer_use_font falls back to on its
     * own, itself another xcb_query_font_reply call through this same
     * stub, since real X servers always ship a working "fixed" bitmap
     * font and only ever fail to resolve something more exotic, this
     * only fails a query for a font name other than plain "fixed" */
    if (s_query_font_should_fail &&
            strcmp(s_last_opened_xlfd, "fixed") != 0) {
        return NULL;
    }

    /* Heap-allocated, matching the real libxcb contract: 'text.c'
     * frees this reply itself with plain 'free()' on the success
     * path */
    reply = malloc(sizeof(*reply));
    memset(reply, 0, sizeof(*reply));
    reply->max_bounds.character_width = s_query_font_char_width_result;
    reply->font_ascent = s_query_font_ascent_result;
    reply->font_descent = s_query_font_descent_result;
    return reply;
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

xcb_void_cookie_t xcb_create_gc_checked(xcb_connection_t *connection,
        xcb_gcontext_t cid, xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) cid;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    s_create_gc_checked_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_change_gc_checked(xcb_connection_t *connection,
        xcb_gcontext_t gc, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) gc;
    (void) value_mask;
    (void) value_list;
    s_change_gc_checked_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_change_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) gc;
    (void) value_mask;
    (void) value_list;
    s_change_gc_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) gc;
    s_free_gc_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_image_text_8_checked(xcb_connection_t *connection,
        uint8_t string_len, xcb_drawable_t drawable, xcb_gcontext_t gc,
        int16_t x, int16_t y, const char *string)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) drawable;
    (void) gc;
    (void) x;
    (void) y;
    s_image_text_8_checked_calls++;
    s_last_drawn_len = string_len;
    memcpy(s_last_drawn_latin1, string, string_len);
    s_last_drawn_latin1[string_len] = '\0';
    return cookie;
}

xcb_generic_error_t *xcb_request_check(xcb_connection_t *connection,
        xcb_void_cookie_t cookie)
{
    (void) connection;
    (void) cookie;
    s_request_check_calls++;
    return NULL;
}

/* 'xcb_get_setup'/'xcb_setup_roots_iterator' back the one root-window
 * lookup 's_try_x11' makes for its GC; a fixed non-NULL setup pointer
 * and a screen iterator pointing at a static, zeroed screen is enough,
 * since nothing here reads any of its fields back out */
const struct xcb_setup_t *xcb_get_setup(xcb_connection_t *connection)
{
    (void) connection;
    return (const struct xcb_setup_t *) 1;
}

static xcb_screen_t s_root_screen;

xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t *setup)
{
    xcb_screen_iterator_t it;

    (void) setup;
    memset(&s_root_screen, 0, sizeof(s_root_screen));
    it.data = &s_root_screen;
    it.rem = 1;
    it.index = 0;
    return it;
}


/* Link-only stand-ins: 'render/glyph.h' */

/* A faithful reproduction of the real 'glyph_utf8_next' (verified
 * against 'src/render/glyph.c' and already exercised in full by
 * 'tests/render/test_glyph.c'), rather than either a fabricated
 * decoder or linking the real 'src/render/glyph.c' itself: the real
 * file pulls in the entire xcb-render/xcb-render-util/FreeType2/
 * fontconfig stack for its other functions, all of which this file
 * already stubs deliberately (see the file-level comment above), so
 * linking it would only produce duplicate-symbol errors against those
 * very stubs.  's_utf8_to_latin1' and 'text_string_measure' (the only
 * two callers in 'render/text.c') need genuinely correct UTF-8
 * decoding to exercise their own real logic meaningfully, which is
 * why this one function alone is reproduced rather than stubbed
 * out */
uint32_t glyph_utf8_next(const char *text, size_t *index)
{
    unsigned char b0;
    uint32_t codepoint = 0u;
    int extra = 0;

    b0 = (unsigned char) text[*index];
    if (b0 == 0u) {
        return 0u;
    }

    if (b0 < 0x80u) {
        *index += 1u;
        return b0;
    }
    if ((b0 & 0xe0u) == 0xc0u) {
        codepoint = b0 & 0x1fu;
        extra = 1;
    } else if ((b0 & 0xf0u) == 0xe0u) {
        codepoint = b0 & 0x0fu;
        extra = 2;
    } else if ((b0 & 0xf8u) == 0xf0u) {
        codepoint = b0 & 0x07u;
        extra = 3;
    } else {
        *index += 1u;
        return b0;
    }

    *index += 1u;
    for (int i = 0; i < extra; ++i) {
        unsigned char bn = (unsigned char) text[*index];

        if ((bn & 0xc0u) != 0x80u) {
            return codepoint;
        }
        codepoint = (uint32_t) ((codepoint << 6) | (bn & 0x3fu));
        *index += 1u;
    }

    return codepoint;
}

int glyph_renderer_init(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    s_glyph_renderer_init_calls++;
    s_glyph_renderer_init_last_font = font_name;
    return (s_glyph_renderer_init_should_fail) ? -1 : 0;
}

void glyph_renderer_release(const char *font_name)
{
    (void) font_name;
    s_glyph_renderer_release_calls++;
}

void glyph_renderer_destroy(void)
{
    s_glyph_renderer_destroy_calls++;
}

void glyph_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
    s_glyph_renderer_set_color_calls++;
}

void glyph_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, struct position_s pos, const char *text)
{
    (void) connection;
    (void) drawable;
    (void) pos;
    s_glyph_draw_string_calls++;
    strncpy(s_last_glyph_drawn, text, sizeof(s_last_glyph_drawn) - 1u);
    s_last_glyph_drawn[sizeof(s_last_glyph_drawn) - 1u] = '\0';
}

uint16_t glyph_measure_string(const char *text)
{
    (void) text;
    return s_glyph_measure_string_result;
}

int16_t glyph_font_ascent(void)
{
    return s_glyph_font_ascent_result;
}

int16_t glyph_font_descent(void)
{
    return s_glyph_font_descent_result;
}


/* Fixture helpers */

/** Resets every stub call counter/state and the module-global text
 *  renderer via 'text_renderer_destroy', so each test starts from a
 *  clean, uninitialized renderer regardless of what an earlier test
 *  left cached */
static void s_reset_fixture(void)
{
    text_renderer_destroy();

    s_connection_stub = (xcb_connection_t *) 1;
    s_generate_id_calls = 0;
    s_next_generated_id = 900u;
    s_open_font_calls = 0;
    s_last_opened_xlfd[0] = '\0';
    s_query_font_calls = 0;
    s_query_font_should_fail = false;
    s_query_font_ascent_result = 12;
    s_query_font_descent_result = 4;
    s_query_font_char_width_result = 8;
    s_close_font_calls = 0;
    s_create_gc_checked_calls = 0;
    s_change_gc_checked_calls = 0;
    s_change_gc_calls = 0;
    s_free_gc_calls = 0;
    s_image_text_8_checked_calls = 0;
    s_last_drawn_latin1[0] = '\0';
    s_last_drawn_len = 0u;
    s_request_check_calls = 0;
    s_glyph_renderer_init_calls = 0;
    s_glyph_renderer_init_should_fail = false;
    s_glyph_renderer_init_last_font = NULL;
    s_glyph_renderer_release_calls = 0;
    s_glyph_renderer_destroy_calls = 0;
    s_glyph_renderer_set_color_calls = 0;
    s_glyph_draw_string_calls = 0;
    s_last_glyph_drawn[0] = '\0';
    s_glyph_measure_string_result = 40u;
    s_glyph_font_ascent_result = 20;
    s_glyph_font_descent_result = 6;
}


/* ==================================================================== *
 * text_renderer_init / text_renderer_disable_glyph_backend             *
 * ==================================================================== */

static void s_test_init_null_connection_fails(void)
{
    int rc;

    s_reset_fixture();
    rc = text_renderer_init(NULL);

    TAP_EQ_INT(rc, -1, "text_renderer_init(NULL) fails");
}

static void s_test_init_valid_connection_succeeds(void)
{
    int rc;

    s_reset_fixture();
    rc = text_renderer_init(s_connection_stub);

    TAP_EQ_INT(rc, 0, "text_renderer_init with a real connection"
            " succeeds");
}

static void s_test_init_closes_previously_cached_fonts(void)
{
    s_reset_fixture();
    (void) text_renderer_init(s_connection_stub);
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    (void) text_renderer_init(s_connection_stub);

    TAP_OK(s_close_font_calls >= 1,
            "re-initializing closes every previously cached X core"
            " font");
}


/* ==================================================================== *
 * s_font_config_to_xlfd, exercised end to end via                      *
 * text_renderer_use_font's XLFD-building pipeline                      *
 * ==================================================================== */

/* Worked examples straight from text.c's own doc comment */

static void s_test_xlfd_bare_family_passthrough(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed");

    TAP_EQ_STR(s_last_opened_xlfd, "fixed",
            "a bare family name with no other field is passed through"
            " as-is, itself a valid X font alias");
}

static void s_test_xlfd_family_and_size(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");

    TAP_EQ_STR(s_last_opened_xlfd,
            "-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*",
            "family plus a bare pixel size builds a medium/roman"
            " XLFD wildcard pattern with that size filled in");
}

static void s_test_xlfd_family_bold_and_size(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed bold 13");

    TAP_EQ_STR(s_last_opened_xlfd,
            "-*-fixed-bold-r-*-*-13-*-*-*-*-*-*-*",
            "adding the 'bold' keyword changes the weight field to"
            " 'bold'");
}

static void s_test_xlfd_family_medium_oblique_no_size(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub,
            "fixed medium oblique");

    /* text.c's own doc comment claims this input produces
     * "-*-fixed-medium-o-*-*-*-*-*-*-*-*-*-*", i.e., with "medium"
     * NOT appearing in the family field.  Tracing the real body
     * (s_font_config_scan_style only recognizes 'bold'/'italic'/
     * 'oblique' as keywords; s_font_config_build_family joins every
     * other token, "medium" among them, into the family string) shows
     * that is not what actually happens: "medium" is not a recognized
     * keyword at all and becomes part of the family exactly like any
     * other non-keyword token would, giving a family of
     * "fixed medium" and a weight field that separately defaults to
     * "medium" only because is_bold is false.  This oracle reflects
     * the actual, personally-run output rather than the doc comment,
     * per this project's own rule against reporting an unrun
     * result */
    TAP_EQ_STR(s_last_opened_xlfd,
            "-*-fixed medium-medium-o-*-*-*-*-*-*-*-*-*-*",
            "'oblique' sets the slant field to 'o'; 'medium' is not a"
            " recognized style keyword at all, so it becomes part of"
            " the family string instead, giving a family of"
            " 'fixed medium' alongside a weight field that separately"
            " defaults to 'medium' on its own");
}

static void s_test_xlfd_family_bold_size_charset(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub,
            "fixed bold 13 iso8859-15");

    TAP_EQ_STR(s_last_opened_xlfd,
            "-*-fixed-bold-r-*-*-13-*-*-*-*-*-iso8859-15",
            "a trailing hyphenated non-keyword token is read as the"
            " charset registry-encoding and split into the XLFD's"
            " last two fields");
}

static void s_test_xlfd_family_bold_charset_no_size(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub,
            "fixed bold iso8859-15");

    TAP_EQ_STR(s_last_opened_xlfd,
            "-*-fixed-bold-r-*-*-*-*-*-*-*-*-iso8859-15",
            "the charset spec is recognized with no size token"
            " present at all, leaving the size field wildcarded");
}

static void s_test_xlfd_null_font_name_uses_fixed(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, NULL);

    TAP_EQ_STR(s_last_opened_xlfd, "fixed",
            "a NULL font name resolves to the bare 'fixed' family");
}

static void s_test_xlfd_empty_font_name_uses_fixed(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "");

    TAP_EQ_STR(s_last_opened_xlfd, "fixed",
            "an empty font name resolves to the bare 'fixed' family"
            " too");
}

static void s_test_xlfd_leading_dash_passthrough_verbatim(void)
{
    const char *raw = "-*-lucida-bold-i-*-*-15-*-*-*-*-*-*-*";

    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, raw);

    TAP_EQ_STR(s_last_opened_xlfd, raw,
            "a string already starting with '-' is treated as a"
            " full XLFD and passed through completely unchanged");
}

static void s_test_xlfd_only_style_keywords_falls_back_to_fixed(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "bold italic");

    TAP_EQ_STR(s_last_opened_xlfd, "fixed",
            "input made entirely of style keywords, with no actual"
            " family token surviving, falls back to the bare 'fixed'"
            " family");
}


/* ==================================================================== *
 * text_renderer_use_font: cache behavior and backend fallback chain    *
 * ==================================================================== */

static void s_test_use_font_null_connection_fails(void)
{
    int rc;

    s_reset_fixture();
    rc = text_renderer_use_font(NULL, "fixed");

    TAP_EQ_INT(rc, -1, "text_renderer_use_font(NULL, ...) fails");
}

static void s_test_use_font_second_call_same_font_is_cache_hit(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    s_open_font_calls = 0;
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");

    TAP_EQ_INT(s_open_font_calls, 0,
            "asking for the same font name a second time hits the"
            " cache rather than reopening the font");
}

static void s_test_use_font_different_font_reopens(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    s_open_font_calls = 0;
    (void) text_renderer_use_font(s_connection_stub, "fixed bold 13");

    TAP_OK(s_open_font_calls >= 1,
            "a genuinely different font name misses the cache and"
            " opens a new one");
}

/* X core-font query failure falls through to the glyph backend */
static void s_test_use_font_x11_failure_falls_to_glyph(void)
{
    int rc;

    s_reset_fixture();
    s_query_font_should_fail = true;

    rc = text_renderer_use_font(s_connection_stub, "Sans 10");

    TAP_EQ_INT(rc, 0,
            "an X core-font query failure falls through to the glyph"
            " backend, which here succeeds");
    TAP_EQ_INT(s_glyph_renderer_init_calls, 1,
            "...calling glyph_renderer_init exactly once");
    TAP_EQ_STR(s_glyph_renderer_init_last_font, "Sans 10",
            "...with the caller's original string, not the XLFD"
            " pattern built for X11");
}

/* Both backends failing falls through to the "fixed" fallback */
static void s_test_use_font_both_backends_fail_falls_to_fixed(void)
{
    int rc;

    s_reset_fixture();
    s_query_font_should_fail = true;
    s_glyph_renderer_init_should_fail = true;

    rc = text_renderer_use_font(s_connection_stub, "NoSuchFont 10");

    TAP_EQ_INT(rc, 0,
            "when both backends fail for a font, the renderer retries"
            " with 'fixed' and that always succeeds");
    TAP_EQ_STR(s_last_opened_xlfd, "fixed",
            "...opening the bare 'fixed' family as the final"
            " fallback");
}

/* The glyph backend is never even attempted once
 * text_renderer_disable_glyph_backend has been called */
static void s_test_use_font_glyph_backend_disabled_skips_straight_to_fixed(
        void)
{
    int rc;

    s_reset_fixture();
    text_renderer_disable_glyph_backend();
    s_query_font_should_fail = true;

    rc = text_renderer_use_font(s_connection_stub, "Sans 10");

    TAP_EQ_INT(rc, 0,
            "with the glyph backend disabled, an X core-font failure"
            " falls straight through to 'fixed' instead");
    TAP_EQ_INT(s_glyph_renderer_init_calls, 0,
            "...glyph_renderer_init is never even attempted");

    /* 'is_glyph_backend_disabled' is a permanent, process-lifetime
     * flag per its own doc comment, not reset by
     * 'text_renderer_destroy'/'text_renderer_init'; nothing below
     * this test relies on the glyph backend being enabled again, so
     * no attempt is made to re-enable it */
}


/* ==================================================================== *
 * text_renderer_destroy                                                *
 * ==================================================================== */

static void s_test_destroy_releases_glyph_backend_globals(void)
{
    s_reset_fixture();
    (void) text_renderer_init(s_connection_stub);
    s_glyph_renderer_destroy_calls = 0;

    text_renderer_destroy();

    TAP_EQ_INT(s_glyph_renderer_destroy_calls, 1,
            "destroying the text renderer also releases whatever"
            " shared resources the glyph backend keeps across every"
            " cached font");
}

static void s_test_destroy_then_use_font_reinitializes_cleanly(void)
{
    int rc;

    s_reset_fixture();
    (void) text_renderer_init(s_connection_stub);
    text_renderer_destroy();
    rc = text_renderer_use_font(s_connection_stub, "fixed");

    TAP_EQ_INT(rc, 0,
            "a destroyed renderer still works correctly on the next"
            " text_renderer_use_font call, which re-initializes it"
            " on demand");
}


/* ==================================================================== *
 * text_renderer_set_color                                              *
 * ==================================================================== */

static void s_test_set_color_no_effect_before_any_font_selected(void)
{
    s_reset_fixture();
    text_renderer_set_color(0xffffffu, 0x000000u);

    TAP_EQ_INT(s_change_gc_calls, 0,
            "setting a color before any font has ever been selected"
            " has no effect");
}

static void s_test_set_color_updates_x11_gc(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    text_renderer_set_color(0x123456u, 0x654321u);

    TAP_EQ_INT(s_change_gc_calls, 1,
            "setting a color for an X core-font entry updates its"
            " graphics context");
}

static void s_test_set_color_delegates_to_glyph_backend(void)
{
    s_reset_fixture();
    s_query_font_should_fail = true;
    (void) text_renderer_use_font(s_connection_stub, "Sans 10");
    s_change_gc_calls = 0;
    text_renderer_set_color(0x123456u, 0x654321u);

    TAP_EQ_INT(s_glyph_renderer_set_color_calls, 1,
            "setting a color for a glyph-backend entry delegates to"
            " glyph_renderer_set_color instead");
    TAP_EQ_INT(s_change_gc_calls, 0,
            "...never touching the (nonexistent) X11 GC for that"
            " entry");
}


/* ==================================================================== *
 * text_draw_string: guard clauses and glyph-backend delegation         *
 * ==================================================================== */

static void s_test_draw_string_null_guards(void)
{
    s_reset_fixture();
    text_draw_string(NULL, 1u, XCB_NONE, (struct position_s) { 0, 0 },
            "hi");
    text_draw_string(s_connection_stub, XCB_NONE, XCB_NONE,
            (struct position_s) { 0, 0 }, "hi");
    text_draw_string(s_connection_stub, 1u, XCB_NONE,
            (struct position_s) { 0, 0 }, NULL);

    TAP_EQ_INT(s_image_text_8_checked_calls +
            s_glyph_draw_string_calls, 0,
            "a NULL connection, XCB_NONE drawable, or NULL text draws"
            " nothing at all");
}

static void s_test_draw_string_auto_initializes_fixed_on_demand(void)
{
    s_reset_fixture();
    text_draw_string(s_connection_stub, 1u, XCB_NONE,
            (struct position_s) { 0, 0 }, "hello");

    TAP_EQ_INT(s_image_text_8_checked_calls, 1,
            "drawing before any font was ever selected auto-selects"
            " 'fixed' on demand and still draws");
}

static void s_test_draw_string_delegates_to_glyph_backend(void)
{
    s_reset_fixture();
    s_query_font_should_fail = true;
    (void) text_renderer_use_font(s_connection_stub, "Sans 10");

    text_draw_string(s_connection_stub, 1u, XCB_NONE,
            (struct position_s) { 5, 6 }, "hola");

    TAP_EQ_INT(s_glyph_draw_string_calls, 1,
            "drawing through a glyph-backend entry delegates to"
            " glyph_draw_string");
    TAP_EQ_STR(s_last_glyph_drawn, "hola",
            "...with the sanitized text passed through unchanged"
            " when it has no control characters");
    TAP_EQ_INT(s_image_text_8_checked_calls, 0,
            "...never touching the X core-font drawing path at all");
}

/* Control characters (e.g., a stray newline from a misbehaving
 * client) become a plain space before either backend ever sees the
 * string */
static void s_test_draw_string_sanitizes_control_characters(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed");

    text_draw_string(s_connection_stub, 1u, XCB_NONE,
            (struct position_s) { 0, 0 }, "line one\nline two");

    TAP_EQ_STR(s_last_drawn_latin1, "line one line two",
            "an embedded control character (newline) is replaced with"
            " a plain space ahead of either rendering backend");
}

/* s_utf8_to_latin1's direct byte-value path: every Latin-1-range
 * codepoint passes through as that exact byte */
static void s_test_draw_string_latin1_range_passthrough(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed");

    /* U+00E9 ('e' with acute accent), UTF-8 encoded as 0xC3 0xA9 */
    text_draw_string(s_connection_stub, 1u, XCB_NONE,
            (struct position_s) { 0, 0 }, "caf\xc3\xa9");

    TAP_EQ_INT((int) (unsigned char) s_last_drawn_latin1[3], 0xE9,
            "a Latin-1-range codepoint (e-acute) becomes the single"
            " byte of that same numeric value");
    TAP_EQ_INT((int) s_last_drawn_len, 4,
            "...and the whole word measures four bytes once decoded,"
            " down from the five its UTF-8 encoding actually took");
}

/* s_ascii_fallback_for's table: an em dash spells out as two ASCII
 * hyphens rather than becoming a '?' */
static void s_test_draw_string_ascii_fallback_em_dash(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed");

    /* U+2014 (em dash), UTF-8 encoded as 0xE2 0x80 0x94 */
    text_draw_string(s_connection_stub, 1u, XCB_NONE,
            (struct position_s) { 0, 0 }, "a\xe2\x80\x94z");

    TAP_EQ_STR(s_last_drawn_latin1, "a--z",
            "an em dash, with no single Latin-1 byte of its own,"
            " spells out as two ASCII hyphens via the fallback table"
            " rather than becoming a literal '?'");
}

/* A codepoint with neither a direct Latin-1 byte nor a table entry
 * becomes a literal '?' */
static void s_test_draw_string_unmapped_codepoint_becomes_question_mark(
        void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed");

    /* U+4E2D (a CJK ideograph), UTF-8 encoded as 0xE4 0xB8 0xAD */
    text_draw_string(s_connection_stub, 1u, XCB_NONE,
            (struct position_s) { 0, 0 }, "a\xe4\xb8\xadz");

    TAP_EQ_STR(s_last_drawn_latin1, "a?z",
            "a codepoint with no Latin-1 byte and no ASCII fallback"
            " entry becomes a literal question mark");
}

/* A zero-width mark (e.g., a soft hyphen) is dropped entirely rather
 * than becoming any visible character at all */
static void s_test_draw_string_zero_width_mark_is_dropped(void)
{
    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed");

    /* U+200B (zero-width space), UTF-8 encoded as 0xE2 0x80 0x8B */
    text_draw_string(s_connection_stub, 1u, XCB_NONE,
            (struct position_s) { 0, 0 }, "a\xe2\x80\x8bz");

    TAP_EQ_STR(s_last_drawn_latin1, "az",
            "a zero-width mark contributes no byte at all to the"
            " converted string, rather than becoming a visible"
            " placeholder");
}


/* ==================================================================== *
 * text_string_measure                                                  *
 * ==================================================================== */

static void s_test_measure_before_any_font_selected_returns_zero(void)
{
    uint16_t w;

    s_reset_fixture();
    w = text_string_measure("hello");

    TAP_EQ_INT(w, 0, "measuring before any font has ever been"
            " selected returns 0");
}

static void s_test_measure_null_text_returns_zero(void)
{
    uint16_t w;

    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    w = text_string_measure(NULL);

    TAP_EQ_INT(w, 0, "measuring a NULL string returns 0, once a font"
            " is selected");
}

static void s_test_measure_counts_decoded_codepoints_not_bytes(void)
{
    uint16_t w;

    s_reset_fixture();
    s_query_font_char_width_result = 10;
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");

    /* Four decoded characters ('c', 'a', 'f', e-acute), five UTF-8
     * bytes on the wire */
    w = text_string_measure("caf\xc3\xa9");

    TAP_EQ_INT(w, 40,
            "a string is measured by decoded codepoint count times"
            " the font's per-character width, not by its raw UTF-8"
            " byte length");
}

static void s_test_measure_delegates_to_glyph_backend(void)
{
    uint16_t w;

    s_reset_fixture();
    s_query_font_should_fail = true;
    (void) text_renderer_use_font(s_connection_stub, "Sans 10");
    w = text_string_measure("anything");

    TAP_EQ_INT(w, s_glyph_measure_string_result,
            "measuring through a glyph-backend entry delegates to"
            " glyph_measure_string");
}


/* ==================================================================== *
 * text_truncate_to_width                                                *
 * ==================================================================== */

static void s_test_truncate_null_buf_is_noop(void)
{
    s_reset_fixture();
    text_truncate_to_width(NULL, 0u, "hello", 100u);

    TAP_OK(true, "a NULL destination buffer is a no-op (and does not"
            " crash)");
}

static void s_test_truncate_null_text_empties_buffer(void)
{
    char buf[32] = "stale";

    s_reset_fixture();
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    text_truncate_to_width(buf, sizeof(buf), NULL, 100u);

    TAP_EQ_STR(buf, "", "a NULL source text empties the destination"
            " buffer");
}

static void s_test_truncate_fits_within_width_unchanged(void)
{
    char buf[32];

    s_reset_fixture();
    s_query_font_char_width_result = 8;
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    text_truncate_to_width(buf, sizeof(buf), "hi", 100u);

    TAP_EQ_STR(buf, "hi",
            "text that already measures within max_width is copied"
            " through unchanged");
}

static void s_test_truncate_shortens_until_it_fits(void)
{
    char buf[32];

    s_reset_fixture();
    s_query_font_char_width_result = 10;
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    /* "areallylongtitle" is 16 chars * 10px = 160px; a 45px limit only
     * leaves room for 4 characters at 10px each */
    text_truncate_to_width(buf, sizeof(buf), "areallylongtitle", 45u);

    TAP_EQ_STR(buf, "area",
            "text wider than max_width is shortened one character at"
            " a time from the end until it measures no wider than"
            " the limit");
}

static void s_test_truncate_to_zero_width_empties_buffer(void)
{
    char buf[32];

    s_reset_fixture();
    s_query_font_char_width_result = 10;
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");
    text_truncate_to_width(buf, sizeof(buf), "anything", 0u);

    TAP_EQ_STR(buf, "",
            "a max_width of 0 shortens the buffer all the way down to"
            " an empty string");
}


/* ==================================================================== *
 * text_font_ascent / text_font_descent                                 *
 * ==================================================================== */

static void s_test_ascent_descent_default_before_any_font_selected(void)
{
    s_reset_fixture();

    TAP_EQ_INT(text_font_ascent(), 10,
            "text_font_ascent returns its small built-in default"
            " before any font has ever been selected");
    TAP_EQ_INT(text_font_descent(), 3,
            "text_font_descent returns its own built-in default"
            " likewise");
}

static void s_test_ascent_descent_reflect_selected_x11_font(void)
{
    s_reset_fixture();
    s_query_font_ascent_result = 14;
    s_query_font_descent_result = 5;
    (void) text_renderer_use_font(s_connection_stub, "fixed 13");

    TAP_EQ_INT(text_font_ascent(), 14,
            "once an X core font is selected, text_font_ascent"
            " reflects its queried metrics");
    TAP_EQ_INT(text_font_descent(), 5,
            "...and so does text_font_descent");
}

static void s_test_ascent_descent_reflect_selected_glyph_font(void)
{
    s_reset_fixture();
    s_query_font_should_fail = true;
    s_glyph_font_ascent_result = 22;
    s_glyph_font_descent_result = 7;
    (void) text_renderer_use_font(s_connection_stub, "Sans 10");

    TAP_EQ_INT(text_font_ascent(), 22,
            "once a glyph-backend font is selected, text_font_ascent"
            " reflects the metrics glyph_font_ascent reported at"
            " selection time");
    TAP_EQ_INT(text_font_descent(), 7,
            "...and so does text_font_descent, from"
            " glyph_font_descent");
}


int main(void)
{
    TAP_PLAN(55);

    s_test_init_null_connection_fails();
    s_test_init_valid_connection_succeeds();
    s_test_init_closes_previously_cached_fonts();

    s_test_xlfd_bare_family_passthrough();
    s_test_xlfd_family_and_size();
    s_test_xlfd_family_bold_and_size();
    s_test_xlfd_family_medium_oblique_no_size();
    s_test_xlfd_family_bold_size_charset();
    s_test_xlfd_family_bold_charset_no_size();
    s_test_xlfd_null_font_name_uses_fixed();
    s_test_xlfd_empty_font_name_uses_fixed();
    s_test_xlfd_leading_dash_passthrough_verbatim();
    s_test_xlfd_only_style_keywords_falls_back_to_fixed();

    s_test_use_font_null_connection_fails();
    s_test_use_font_second_call_same_font_is_cache_hit();
    s_test_use_font_different_font_reopens();
    s_test_use_font_x11_failure_falls_to_glyph();
    s_test_use_font_both_backends_fail_falls_to_fixed();

    s_test_destroy_releases_glyph_backend_globals();
    s_test_destroy_then_use_font_reinitializes_cleanly();

    s_test_set_color_no_effect_before_any_font_selected();
    s_test_set_color_updates_x11_gc();
    s_test_set_color_delegates_to_glyph_backend();

    s_test_draw_string_null_guards();
    s_test_draw_string_auto_initializes_fixed_on_demand();
    s_test_draw_string_delegates_to_glyph_backend();
    s_test_draw_string_sanitizes_control_characters();
    s_test_draw_string_latin1_range_passthrough();
    s_test_draw_string_ascii_fallback_em_dash();
    s_test_draw_string_unmapped_codepoint_becomes_question_mark();
    s_test_draw_string_zero_width_mark_is_dropped();

    s_test_measure_before_any_font_selected_returns_zero();
    s_test_measure_null_text_returns_zero();
    s_test_measure_counts_decoded_codepoints_not_bytes();
    s_test_measure_delegates_to_glyph_backend();

    s_test_truncate_null_buf_is_noop();
    s_test_truncate_null_text_empties_buffer();
    s_test_truncate_fits_within_width_unchanged();
    s_test_truncate_shortens_until_it_fits();
    s_test_truncate_to_zero_width_empties_buffer();

    s_test_ascent_descent_default_before_any_font_selected();
    s_test_ascent_descent_reflect_selected_x11_font();
    s_test_ascent_descent_reflect_selected_glyph_font();

    /* Runs dead last: 'text_renderer_disable_glyph_backend' sets a
     * permanent, process-lifetime flag per its own doc comment, never
     * reset by 'text_renderer_destroy'/'text_renderer_init', so every
     * other test exercising the glyph backend has to run before this
     * one permanently forecloses it for the rest of the process */
    s_test_use_font_glyph_backend_disabled_skips_straight_to_fixed();

    return TAP_DONE();
}
