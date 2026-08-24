/**
 * @file render/text.c
 *
 * @brief Basic XCB text rendering helpers
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
#include <stdio.h>      /* snprintf, NULL */
#include <stdlib.h>     /* strtol, free */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/text.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <render/glyph.h>
#include <render/text.h>


/**
 * @brief Which backend @c text_renderer_init last successfully selected
 *
 * @c S_BACKEND_X11 renders through the X core font path this file
 * implements directly; @c S_BACKEND_GLYPH delegates every operation to
 * @c render/glyph.c instead, the xcb-render/FreeType2/fontconfig
 * fallback used for a font name that does not resolve to an X core font
 * (e.g., a TrueType/OpenType family name most cursor and icon themes
 * install but the X server's own bitmap font set does not).
 */
enum s_text_backend_e {
    S_BACKEND_NONE = 0,
    S_BACKEND_X11,
    S_BACKEND_GLYPH
};


/**
 * @brief Module state for the text renderer
 *
 * Stores the XCB core-font resources, the requested font name and its
 * XLFD form, cached font metrics, and the active text-rendering
 * backend.
 *
 * @note @p raw_font_name preserves the caller's original
 *       fontconfig-compatible pattern so the glyph backend can be
 *       selected as a fallback when the X core-font backend cannot load
 *       the converted XLFD name
 */
static struct {
    xcb_connection_t *connection;
    xcb_font_t font;
    xcb_gcontext_t gc;
    char font_name[256];

    /**
     * @brief Just @p font_name as the caller passed it, before the XLFD
     *        conversion below
     *
     * Kept so a fallback to the glyph backend can hand fontconfig its
     * own syntax instead of a mangled XLFD pattern it would not
     * understand.
     */
    char raw_font_name[256];

    uint16_t char_width;

    int16_t ascent;     /**< Pixels the baseline sits below the top of
                             a line of text, from the font's own metrics.
                             Used to vertically center or
                             top/bottom-align text against a known
                             pixel height (cfr. @a text_font_ascent) */
    int16_t descent;    /**< Pixels the baseline sits above the bottom of
                             a line of text.  (Cfr. @a text_font_descent) */

    enum s_text_backend_e backend;
    bool is_initialized;

    /** Set once, for the life of the process, by
     *  @a text_renderer_disable_glyph_backend */
    bool is_glyph_backend_disabled;
} s_text = {
    .connection = NULL,
    .font = XCB_NONE,
    .gc = XCB_NONE,
    .font_name = {'\0'},
    .raw_font_name = {'\0'},
    .char_width = 8,
    .ascent = 10,
    .descent = 3,
    .backend = S_BACKEND_NONE,
    .is_initialized = false,
    .is_glyph_backend_disabled = false
};


/**
 * @brief Split a font configuration string into whitespace-separated
 *        tokens
 *
 * @param input      Null-terminated font description string
 * @param tokens     Destination array of tokens
 * @param max_tokens Maximum number of tokens @p tokens can hold
 *
 * @return Number of tokens actually found, up to @p max_tokens
 *
 * @note Complexity: @e O(n), where @e n is the length of @p input
 */
static size_t s_font_config_tokenize(const char *restrict input,
        char tokens[][WM_TEXT_FONT_TOKEN_LENGTH], size_t max_tokens)
{
    const char *p;
    size_t ntok = 0u;

    p = input;
    while (*p != '\0' && ntok < max_tokens) {
        size_t tlen = 0u;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        while (*p != ' ' && *p != '\t' && *p != '\0' &&
                tlen < (size_t) (WM_TEXT_FONT_TOKEN_LENGTH - 1)) {
            tokens[ntok][tlen++] = *p++;
        }
        tokens[ntok][tlen] = '\0';
        ntok++;
    }

    return ntok;
}


/**
 * @brief Split off the trailing charset-spec token, if present, into
 *        its own registry and encoding parts
 *
 * The last token is a charset spec when it contains a hyphen and is
 * not one of the style keywords ('bold'/'italic'/'oblique'); it is
 * then removed from @p tokens (via @p ntok) and split at its own
 * last hyphen into @p registry and @p encoding.
 *
 * @param tokens         Tokens produced by @a s_font_config_tokenize
 * @param ntok           Token count; decremented if a charset spec
 *                        was found and removed
 * @param registry       Destination for the registry part
 * @param registry_size  Size of @p registry, in bytes
 * @param encoding       Destination for the encoding part
 * @param encoding_size  Size of @p encoding, in bytes
 *
 * @note @p registry and @p encoding are left empty if no charset
 *       spec was found
 * @note Complexity: @e O(k), where @e k is the length of the last
 *       token in @p tokens
 */
static void s_font_config_extract_charset(
        char tokens[][WM_TEXT_FONT_TOKEN_LENGTH], size_t *ntok,
        char *restrict registry, size_t registry_size,
        char *restrict encoding, size_t encoding_size)
{
    char charset_tok[WM_TEXT_FONT_TOKEN_LENGTH];
    size_t last_hyphen = 0u;
    bool has_hyphen = false;

    registry[0] = '\0';
    encoding[0] = '\0';

    for (size_t j = 0u; tokens[*ntok - 1u][j] != '\0'; ++j) {
        if (tokens[*ntok - 1u][j] == '-') {
            has_hyphen = true;
            break;
        }
    }
    if (!has_hyphen ||
            safe_strcmp(tokens[*ntok - 1u], "bold") == 0 ||
            safe_strcmp(tokens[*ntok - 1u], "italic") == 0 ||
            safe_strcmp(tokens[*ntok - 1u], "oblique") == 0) {
        return;
    }

    safe_strncpy(charset_tok, tokens[*ntok - 1u], sizeof(charset_tok));
    (*ntok)--;

    /* Find the last hyphen so we can split registry and encoding */
    for (size_t j = 0u; charset_tok[j] != '\0'; ++j) {
        if (charset_tok[j] == '-') {
            last_hyphen = j;
        }
    }

    /* 'safe_strncpy' with 'sz=last_hyphen+1' copies exactly
     * 'last_hyphen' chars */
    safe_strncpy(registry, charset_tok, last_hyphen + 1u);
    (void) registry_size;
    safe_strncpy(encoding, charset_tok + last_hyphen + 1u,
            encoding_size);
}


/**
 * @brief Split off the trailing pixel-size token, if present
 *
 * The last remaining token is a pixel size when every one of its
 * characters is a digit and it parses as an integer in
 * @e (0, 999].
 *
 * @param tokens Tokens remaining after
 *               @a s_font_config_extract_charset
 * @param ntok   Token count; decremented if a size token was found
 *               and removed
 *
 * @return The parsed pixel size, or @c 0 if the last token was not a
 *         valid one
 *
 * @note Complexity: @e O(k), where @e k is the length of the last
 *       token in @p tokens
 */
static int s_font_config_extract_size(
        char tokens[][WM_TEXT_FONT_TOKEN_LENGTH], size_t *ntok)
{
    char *endptr;
    long lval;
    int size = 0;
    bool is_num;

    is_num = (*ntok > 0u && tokens[*ntok - 1u][0] != '\0');
    for (size_t j = 0u; is_num && tokens[*ntok - 1u][j] != '\0'; ++j) {
        if (tokens[*ntok - 1u][j] < '0' ||
                tokens[*ntok - 1u][j] > '9') {
            is_num = false;
        }
    }
    if (!is_num) {
        return 0;
    }

    lval = strtol(tokens[*ntok - 1u], &endptr, 10);
    if (endptr != tokens[*ntok - 1u] && *endptr == '\0' &&
            lval > 0L && lval <= 999L) {
        size = (int) lval;
    }
    (*ntok)--;

    return size;
}


/**
 * @brief Scan tokens for the weight and slant style keywords
 *
 * @param tokens     Tokens remaining after
 *                   @a s_font_config_extract_size
 * @param ntok       Number of tokens in @p tokens
 * @param is_bold    Set to @c true if a 'bold' token was found
 * @param is_italic  Set to @c true if an 'italic' token was found
 * @param is_oblique Set to @c true if an 'oblique' token was found
 *
 * @note Complexity: @e O(n), where @e n is @p ntok
 */
static void s_font_config_scan_style(
        char tokens[][WM_TEXT_FONT_TOKEN_LENGTH], size_t ntok,
        bool *is_bold, bool *is_italic, bool *is_oblique)
{
    *is_bold = false;
    *is_italic = false;
    *is_oblique = false;

    for (size_t i = 0u; i < ntok; ++i) {
        if (safe_strcmp(tokens[i], "bold") == 0) {
            *is_bold = true;
        } else if (safe_strcmp(tokens[i], "italic") == 0) {
            *is_italic = true;
        } else if (safe_strcmp(tokens[i], "oblique") == 0) {
            *is_oblique = true;
        }
    }
}


/**
 * @brief Join every non-keyword token, space-separated, into the
 *        font family string
 *
 * @param tokens      Tokens remaining after
 *                    @a s_font_config_extract_size
 * @param ntok        Number of tokens in @p tokens
 * @param family      Destination buffer for the family string
 * @param family_size Size of @p family, in bytes
 *
 * @note @p family is left empty if every token was a style keyword
 * @note Complexity: @e O(n), where @e n is the combined length of
 *       every token in @p tokens
 */
static void s_font_config_build_family(
        char tokens[][WM_TEXT_FONT_TOKEN_LENGTH], size_t ntok,
        char *restrict family, size_t family_size)
{
    size_t fi = 0u;

    family[0] = '\0';
    for (size_t i = 0u; i < ntok; ++i) {
        if (safe_strcmp(tokens[i], "bold") == 0 ||
                safe_strcmp(tokens[i], "italic") == 0 ||
                safe_strcmp(tokens[i], "oblique") == 0) {
            continue;
        }

        if (fi > 0u && fi < family_size - 1u) {
            family[fi++] = ' ';
        }

        for (size_t j = 0u;
                tokens[i][j] != '\0' && fi < family_size - 1u;
                ++j) {
            family[fi++] = tokens[i][j];
        }
    }
    family[fi] = '\0';
}


/**
 * @brief Build the final XLFD pattern from every already-extracted
 *        font field
 *
 * A bare family name with no size, weight, slant, or charset spec is
 * passed through as-is, itself a valid X font alias; every other
 * combination is rendered as an XLFD wildcard pattern, with @c '*'
 * standing in for whichever fields were not specified.
 *
 * @param family   Font family, from @a s_font_config_build_family
 * @param size     Pixel size, from @a s_font_config_extract_size
 *                 (@c 0 for unspecified)
 * @param is_bold  Whether the 'bold' keyword was found
 * @param is_italic Whether the 'italic' keyword was found
 * @param is_oblique Whether the 'oblique' keyword was found
 * @param registry Charset registry, from
 *                 @a s_font_config_extract_charset (empty if
 *                 unspecified)
 * @param encoding Charset encoding, from
 *                 @a s_font_config_extract_charset (empty if
 *                 unspecified)
 * @param output   Buffer for the resulting XLFD pattern
 * @param outsize  Size of @p output in bytes
 *
 * @note Complexity: @e O(1)
 */
static void s_font_config_build_xlfd_pattern(
        const char *restrict family, int size, bool is_bold,
        bool is_italic, bool is_oblique, const char *restrict registry,
        const char *restrict encoding, char *restrict output,
        size_t outsize)
{
    const char *weight_str;
    const char *slant_str;

    if (size == 0 && !is_bold && !is_italic && !is_oblique &&
            registry[0] == '\0') {
        safe_strncpy(output, family, outsize);
        return;
    }

    weight_str = (is_bold) ? "bold" : "medium";
    slant_str = (is_italic) ? "i" : ((is_oblique) ? "o" : "r");

    if (size > 0 && registry[0] != '\0') {
        (void) snprintf(output, outsize,
                "-*-%s-%s-%s-*-*-%d-*-*-*-*-*-%s-%s",
                family, weight_str, slant_str, size, registry,
                encoding);
    } else if (size > 0) {
        (void) snprintf(output, outsize,
                "-*-%s-%s-%s-*-*-%d-*-*-*-*-*-*-*",
                family, weight_str, slant_str, size);
    } else if (registry[0] != '\0') {
        (void) snprintf(output, outsize,
                "-*-%s-%s-%s-*-*-*-*-*-*-*-*-%s-%s",
                family, weight_str, slant_str, registry, encoding);
    } else {
        (void) snprintf(output, outsize,
                "-*-%s-%s-%s-*-*-*-*-*-*-*-*-*-*",
                family, weight_str, slant_str);
    }
}


/**
 * @brief Convert a font configuration string to an X11 XLFD pattern
 *
 * IcoWM uses a simple font description syntax in its theme files:
 *
 * @code
 *   [family] [bold] [italic|oblique] [size] [registry-encoding]
 * @endcode
 *
 * All fields except @p family are optional.  @p registry-encoding is
 * recognized as any whitespace-separated token that contains a hyphen
 * and is not a keyword; it is split at the last hyphen into the XLFD
 * @p charset_registry and @p charset_encoding fields.
 *
 * Examples:
 * @code
 * "fixed"                    -> "fixed"
 * "fixed 13"                 -> "-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*"
 * "fixed bold 13"            -> "-*-fixed-bold-r-*-*-13-*-*-*-*-*-*-*"
 * "fixed medium oblique"     -> "-*-fixed-medium-o-*-*-*-*-*-*-*-*-*-*"
 * "fixed bold 13 iso8859-15" -> "-*-fixed-bold-r-*-*-13-*-*-*-*-*-iso8859-15"
 * "fixed bold iso8859-15"    -> "-*-fixed-bold-r-*-*-*-*-*-*-*-*-iso8859-15"
 * @endcode
 *
 * If @p input already starts with @c '-' it is treated as a full XLFD
 * and copied verbatim into @p output.
 *
 * Split into one static function per phase: tokenizing (@a
 * s_font_config_tokenize), charset extraction (@a s_font_config_
 * extract_charset), size extraction (@a s_font_config_extract_size),
 * style keyword scanning (@a s_font_config_scan_style), family string
 * assembly (@a s_font_config_build_family), and the final pattern
 * assembly (@a s_font_config_build_xlfd_pattern), called here in that
 * order.
 *
 * @param input   Null-terminated font description string
 * @param output  Buffer for the resulting XLFD pattern
 * @param outsize Size of @p output in bytes
 */
static void s_font_config_to_xlfd(const char *restrict input,
        char *restrict output, size_t outsize)
{
    char tokens[WM_TEXT_FONT_MAX_TOKENS][WM_TEXT_FONT_TOKEN_LENGTH];
    char family[WM_TEXT_FONT_FAMILY_LENGTH];
    char registry[WM_TEXT_FONT_CHARSET_LENGTH];
    char encoding[WM_TEXT_FONT_CHARSET_LENGTH];
    size_t ntok;
    int size;
    bool is_bold;
    bool is_italic;
    bool is_oblique;

    if (input == NULL || input[0] == '\0') {
        safe_strncpy(output, "fixed", outsize);
        return;
    }

    /* Pass XLFD strings (starting with '-') through unchanged */
    if (input[0] == '-') {
        safe_strncpy(output, input, outsize);
        return;
    }

    ntok = s_font_config_tokenize(input, tokens,
            WM_TEXT_FONT_MAX_TOKENS);
    if (ntok == 0u) {
        safe_strncpy(output, "fixed", outsize);
        return;
    }

    s_font_config_extract_charset(tokens, &ntok, registry,
            sizeof(registry), encoding, sizeof(encoding));
    size = s_font_config_extract_size(tokens, &ntok);
    s_font_config_scan_style(tokens, ntok, &is_bold, &is_italic,
            &is_oblique);
    s_font_config_build_family(tokens, ntok, family, sizeof(family));

    if (family[0] == '\0') {
        safe_strncpy(output, "fixed", outsize);
        return;
    }

    s_font_config_build_xlfd_pattern(family, size, is_bold, is_italic,
            is_oblique, registry, encoding, output, outsize);
}


/**
 * @brief Convert a UTF-8 string to single-byte Latin-1, for
 *        @c xcb_image_text_8, which has no multi-byte encoding support
 *        of its own at all
 *
 * Every codepoint in the Latin-1 range (U+0000-U+00FF, which is fixed's
 * own ISO 8859-1/8859-15 encoding, and covers every accented letter
 * Spanish, Galician, Catalan, French, Italian, German, and Portuguese
 * actually use) becomes the one byte that same numeric value already is
 * in that encoding.  Anything further out (Cyrillic, CJK, most
 * everything else) becomes a literal '?', since a bitmap X core font
 * like "fixed" has no glyph for it regardless of how faithfully the
 * input text were decoded.
 *
 * @param text     Null-terminated UTF-8 string
 * @param out      Destination buffer
 * @param out_size Size of @p out, in bytes
 *
 * @return Length of the converted string in @p out, in bytes (always at
 *         most one byte per decoded codepoint, so never longer than
 *         @p text's own UTF-8 byte length)
 *
 * @note Complexity: @e O(n), where @e n is the length of @p text
 */
static size_t s_utf8_to_latin1(const char *restrict text,
        char *restrict out, size_t out_size)
{
    size_t byte_index = 0u;
    size_t out_len = 0u;

    while (out_len < out_size - 1u) {
        uint32_t codepoint = glyph_utf8_next(text, &byte_index);

        if (codepoint == 0u) {
            break;
        }
        out[out_len] = (codepoint <= 0xFFu) ? (char) codepoint : '?';
        out_len += 1u;
    }
    out[out_len] = '\0';

    return out_len;
}


/**
 * @brief Try to open @p xlfd as an X core font and query its metrics
 *
 * @param connection Pointer to the XCB connection
 * @param xlfd       XLFD pattern to open
 *
 * @return @c true if the font opened and its metrics could be read
 *
 * @note On failure, any font resource this call opened is closed again
 *       before returning, so the caller never has to clean up a partial
 *       X11 attempt itself
 * @note Complexity: @e O(1)
 */
static bool s_try_x11(xcb_connection_t *connection, const char *xlfd)
{
    uint32_t gc_values[2];
    xcb_query_font_reply_t *qf_reply;

    s_text.font = xcb_generate_id(connection);
    xcb_open_font(connection, s_text.font,
            (uint16_t) safe_strlen(xlfd), xlfd);

    qf_reply = xcb_query_font_reply(connection,
            xcb_query_font(connection, s_text.font), NULL);
    if (qf_reply == NULL) {
        xcb_close_font(connection, s_text.font);
        s_text.font = XCB_NONE;
        return false;
    }

    if (qf_reply->max_bounds.character_width > 0) {
        s_text.char_width =
            (uint16_t) qf_reply->max_bounds.character_width;
    }
    s_text.ascent = qf_reply->font_ascent;
    s_text.descent = qf_reply->font_descent;
    free(qf_reply);

    s_text.gc = xcb_generate_id(connection);

    /* Neutral defaults: 'white-on-black'.
     * For themed titlebar, call 'text_renderer_set_color' afterwards to
     * use the foreground/background colors from theme */
    gc_values[0] = 0xFFFFFFu;   /* fg: white */
    gc_values[1] = 0x000000u;   /* bg: black */
    xcb_create_gc(connection, s_text.gc,
            xcb_setup_roots_iterator(xcb_get_setup(connection)).data->root,
            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, gc_values);
    xcb_change_gc(connection, s_text.gc, XCB_GC_FONT,
            (const uint32_t[]) {s_text.font});

    return true;
}


/* Permanently disable the glyph ('xcb-render'/FreeType2/fontconfig)
 * backend for the life of the process */
void text_renderer_disable_glyph_backend(void)
{
    s_text.is_glyph_backend_disabled = true;
}


/* Initialize the text renderer using the specified font */
int text_renderer_init(xcb_connection_t *connection,
        const char *font_name)
{
    char xlfd[256];
    const char *raw;

    if (connection == NULL) {
        return -1;
    }

    raw = (font_name == NULL || font_name[0] == '\0')
        ? "fixed"
        : font_name;

    if (s_text.is_initialized &&
            s_text.connection == connection &&
            safe_strcmp(s_text.raw_font_name, raw) == 0) {
        return 0;
    }

    text_renderer_destroy();

    /* Convert the config-style font description (e.g., "fixed bold 13")
     * to an XLFD wildcard pattern that 'xcb_open_font' can resolve */
    s_font_config_to_xlfd(raw, xlfd, sizeof(xlfd));

    s_text.connection = connection;
    safe_strncpy(s_text.raw_font_name, raw,
            sizeof(s_text.raw_font_name));

    if (s_try_x11(connection, xlfd)) {
        safe_strncpy(s_text.font_name, xlfd, sizeof(s_text.font_name));
        s_text.backend = S_BACKEND_X11;
        s_text.is_initialized = true;
        return 0;
    }

    /* 'xlfd' did not resolve to any X core font (e.g.,
     * a TrueType/OpenType family name most systems have via fontconfig
     * but whose bitmap X font set does not include).  Fall back to
     * rendering it through xcb-render/FreeType2/fontconfig instead,
     * handing fontconfig the caller's original string rather than the
     * XLFD pattern just built for X11, since fontconfig has its own,
     * different pattern syntax.
     *
     * Never even attempted at all once
     * 'text_renderer_disable_glyph_backend' has been called: falls
     * straight through to the "fixed" fallback below instead, the same
     * as if this attempt had failed. */
    if (!s_text.is_glyph_backend_disabled &&
            glyph_renderer_init(connection, raw) == 0) {
        s_text.ascent = glyph_font_ascent();
        s_text.descent = glyph_font_descent();
        s_text.backend = S_BACKEND_GLYPH;
        s_text.is_initialized = true;
        return 0;
    }

    /* Both backends failed for this specific font description: fall
     * back to "fixed", which every X server ships and is guaranteed to
     * open, so the renderer is never left completely unusable. */
    if (safe_strcmp(raw, "fixed") != 0 &&
            s_try_x11(connection, "fixed")) {
        safe_strncpy(s_text.font_name, "fixed",
                sizeof(s_text.font_name));
        s_text.backend = S_BACKEND_X11;
        s_text.is_initialized = true;
        return 0;
    }

    s_text.connection = NULL;
    return -1;
}


/* Destroy global text renderer resources */
void text_renderer_destroy(void)
{
    if (!s_text.is_initialized) {
        return;
    }

    if (s_text.backend == S_BACKEND_GLYPH) {
        glyph_renderer_destroy();
    } else if (s_text.connection != NULL) {
        if (s_text.gc != XCB_NONE) {
            xcb_free_gc(s_text.connection, s_text.gc);
        }
        if (s_text.font != XCB_NONE) {
            xcb_close_font(s_text.connection, s_text.font);
        }
    }

    s_text.connection = NULL;
    s_text.font = XCB_NONE;
    s_text.gc = XCB_NONE;
    s_text.font_name[0] = '\0';
    s_text.raw_font_name[0] = '\0';
    s_text.char_width = 8;
    s_text.backend = S_BACKEND_NONE;
    s_text.is_initialized = false;
}


/* Update the foreground and background colors of the text renderer GC */
void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    uint32_t gc_values[2];

    if (!s_text.is_initialized) {
        return;
    }

    if (s_text.backend == S_BACKEND_GLYPH) {
        glyph_renderer_set_color(fg, bg);
        return;
    }

    if (s_text.gc == XCB_NONE || s_text.connection == NULL) {
        return;
    }

    gc_values[0] = fg;
    gc_values[1] = bg;
    xcb_change_gc(s_text.connection, s_text.gc,
            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, gc_values);
}


/* Draw a string at the specified position */
void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text)
{
    size_t len;
    xcb_generic_error_t *draw_error;
    char sanitized[512];
    char latin1[512];

    if (connection == NULL || drawable == XCB_NONE || text == NULL) {
        return;
    }

    if (!s_text.is_initialized || s_text.connection != connection) {
        if (text_renderer_init(connection, "fixed") != 0) {
            return;
        }
    }

    /* Replace any control character (a stray newline in a window title
     * set by a misbehaving client, most commonly) with a plain space
     * before drawing, rather than passing it through as-is.
     *
     * Xft/FreeType glyphs happen not to draw anything visible for most
     * control codes, which is what makes this go unnoticed with an Xft
     * font; a bitmap X core font like 'fixed' has an actual glyph at
     * nearly every code point in its table, including the control
     * range, so the same character shows up as a visible box there
     * instead.  Sanitizing once here, ahead of either backend, means
     * neither depends on that difference in font behavior to look
     * right. */
    len = safe_strlen(text);
    if (len >= sizeof(sanitized)) {
        len = sizeof(sanitized) - 1u;
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char) text[i];
        sanitized[i] = (c < 0x20u || c == 0x7Fu) ? ' ' : text[i];
    }
    sanitized[len] = '\0';

    if (s_text.backend == S_BACKEND_GLYPH) {
        glyph_draw_string(connection, drawable, pos, sanitized);
        return;
    }

    len = s_utf8_to_latin1(sanitized, latin1, sizeof(latin1));
    if (len == 0) {
        return;
    }
    if (len > 255) {
        len = 255;
    }

    draw_error = xcb_request_check(connection,
            xcb_image_text_8_checked(connection, (uint8_t) len,
                drawable, (gc == XCB_NONE)
                    ? s_text.gc
                    : gc, (int16_t) pos.x, (int16_t) pos.y, latin1));
    if (draw_error != NULL) {
        LOGGER_WARNING("'xcb_image_text_8' failed on drawable %#x" \
                " (error=%u)",
                drawable, (unsigned) draw_error->error_code);
        free(draw_error);
    }
}


/* Measure the rendered width of a string */
uint16_t text_string_measure(const char *text)
{
    size_t char_count = 0u;
    size_t byte_index = 0u;

    if (s_text.backend == S_BACKEND_GLYPH) {
        return glyph_measure_string(text);
    }
    if (text == NULL) {
        return 0u;
    }

    /* Counting decoded codepoints, not 'safe_strlen's own UTF-8 byte
     * count: 's_utf8_to_latin1' always draws exactly one glyph per
     * codepoint (the Latin-1 byte itself, or a '?' substitute for
     * anything further out), so a multi-byte character like 'á'
     * measures as the one character cell it actually occupies once
     * drawn, not the two UTF-8 bytes it happens to take on the wire. */
    while (glyph_utf8_next(text, &byte_index) != 0u) {
        char_count += 1u;
    }

    if (char_count > UINT16_MAX / s_text.char_width) {
        return UINT16_MAX;
    }

    return (uint16_t) (char_count * s_text.char_width);
}


/* Copy text into a buffer, shortening it a character at a time from
 * the end until it measures no wider than a given limit */
void text_truncate_to_width(char *buf, size_t buf_size,
        const char *text, uint16_t max_width)
{
    uint16_t w;

    if (buf == NULL || buf_size == 0u) {
        return;
    }
    if (text == NULL) {
        buf[0] = '\0';
        return;
    }

    safe_strncpy(buf, text, buf_size);
    w = text_string_measure(buf);

    if (w > max_width) {
        size_t len = safe_strlen(buf);

        while (len > 0u && w > max_width) {
            --len;
            buf[len] = '\0';
            w = text_string_measure(buf);
        }
    }
}


/* Pixels the baseline sits below the top of a line, for the current
 * font */
int16_t text_font_ascent(void)
{
    return s_text.ascent;
}


/* Pixels the baseline sits above the bottom of a line, for the current
 * font */
int16_t text_font_descent(void)
{
    return s_text.descent;
}
