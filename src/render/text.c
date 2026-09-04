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
#include <string.h>
#include <stdio.h>      /* snprintf, NULL */
#include <stdlib.h>     /* strtol, free */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>
#include <utils/xcb/connection.h>

/* Default initial values */
#include <defs/text.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <render/glyph.h>
#include <render/text.h>


/**
 * @brief Which backend an opened font is drawn through
 *
 * @c S_BACKEND_X11 renders through the X core font path this file
 * implements directly; @c S_BACKEND_GLYPH delegates every operation to
 * @c render/glyph.c instead, the xcb-render/FreeType2/fontconfig
 * fallback used for a font name that does not resolve to an X core font
 * (e.g., a TrueType/OpenType family name most cursor and icon themes
 * install but the X server's bitmap font set does not).
 */
enum s_text_backend_e {
    S_BACKEND_NONE = 0,
    S_BACKEND_X11,
    S_BACKEND_GLYPH
};


/**
 * @brief One opened font the renderer keeps ready for reuse
 *
 * The X core-font backend fills @p font and @p gc; the glyph backend
 * fills neither, since @c render/glyph.c holds those resources.
 * @p key is the name exactly as the caller gave it, which is what
 * a lookup matches on, since the XLFD pattern is derived from it and
 * fontconfig wants that original syntax anyway.
 */
typedef struct {
    char key[WM_TEXT_FONT_NAME_LENGTH];
    xcb_font_t font;
    xcb_gcontext_t gc;
    uint32_t last_used;
    enum s_text_backend_e backend;
    uint16_t char_width;
    int16_t ascent;
    int16_t descent;
    bool is_used;
} s_text_font_td;


/**
 * @brief Module state for the text renderer
 *
 * @p cache holds every font opened so far and @p current indexes
 * whichever one drawing goes through, or equals
 * @c WM_TEXT_FONT_CACHE_MAX when none is selected.  @p clock only
 * counts up, and the entry carrying the lowest @p last_used is the one
 * evicted when room is needed.
 */
static struct {
    s_text_font_td cache[WM_TEXT_FONT_CACHE_MAX];
    uint32_t clock;
    uint32_t current;
    bool is_initialized;

    /** Set once, for the life of the process, by
     *  @a text_renderer_disable_glyph_backend */
    bool is_glyph_backend_disabled;
} s_text = {
    .clock = 0u,
    .current = WM_TEXT_FONT_CACHE_MAX,
    .is_initialized = false,
    .is_glyph_backend_disabled = false
};


/**
 * @brief A codepoint no single-byte font can show, and what to put in
 *        its place
 */
struct s_ascii_fallback_s {
    const char *replacement;    /**< What "fixed" can show instead */
    uint32_t codepoint;         /**< What the text actually says */
};


/**
 * @brief Punctuation worth spelling out rather than dropping
 *
 * Only marks a window title is likely to carry and that have an obvious
 * ASCII reading.  A browser, an editor or a mail client puts these in
 * titles constantly, and every one of them reaches a single-byte font
 * as a question mark otherwise, which is where a title stops being
 * readable rather than merely imperfect.
 *
 * Deliberately short.  A replacement is worth making only where one is
 * evident: Greek, Cyrillic, CJK or mathematics have no ASCII reading at
 * all, and a question mark says as much about them as anything else
 * would.
 *
 * An empty replacement drops the codepoint, which is what the
 * zero-width marks want: they are invisible where they came from, and
 * a space in their place would be a change to the text rather than
 * a rendering of it.
 */
static const struct s_ascii_fallback_s s_ascii_fallbacks[] = {
    /* Dashes and hyphens */
    { "-",   0x2010u },     /* hyphen */
    { "-",   0x2011u },     /* non-breaking hyphen */
    { "-",   0x2012u },     /* figure dash */
    { "-",   0x2013u },     /* en dash */
    { "--",  0x2014u },     /* em dash */
    { "--",  0x2015u },     /* horizontal bar */
    { "-",   0x2212u },     /* minus sign */

    /* Quotation marks, the apostrophe among them the commonest of
     * every mark here: any English title with a genitive carries it */
    { "'",   0x2018u },     /* left single quote */
    { "'",   0x2019u },     /* right single quote, and apostrophe */
    { "'",   0x201Au },     /* single low quote */
    { "'",   0x201Bu },     /* single high reversed quote */
    { "\"",  0x201Cu },     /* left double quote */
    { "\"",  0x201Du },     /* right double quote */
    { "\"",  0x201Eu },     /* double low quote */
    { "\"",  0x201Fu },     /* double high reversed quote */
    { "'",   0x2032u },     /* prime */
    { "\"",  0x2033u },     /* double prime */
    { "<",   0x2039u },     /* single left angle quote */
    { ">",   0x203Au },     /* single right angle quote */

    /* Marks a path or a breadcrumb trail is built from */
    { "*",   0x2022u },     /* bullet */
    { "...", 0x2026u },     /* ellipsis */
    { "/",   0x2044u },     /* fraction slash */
    { "/",   0x2215u },     /* division slash */

    /* Arrows */
    { "<-",  0x2190u },     /* leftwards arrow */
    { "->",  0x2192u },     /* rightwards arrow */
    { "<<-", 0x219Eu },     /* leftwards two haeaded arrow */
    { "<->", 0x2194u },     /* left right arrow */
    { "->>", 0x21A0u },     /* rightwards two headed arrow */
    { "<-<", 0x21A2u },     /* leftwards arrow with tail */
    { ">->", 0x21A3u },     /* rightwards arrow with tail */
    { "<-|", 0x21A4u },     /* leftwards arrow from bar */
    { "|->", 0x21A6u },     /* rightwards arrow from bar */
    { "<=",  0x21D0u },     /* leftwards double arrow */
    { "=>",  0x21D2u },     /* rightwards double arrow */
    { "<=>", 0x21D4u },     /* left right double arrow */
    { "|<-", 0x21E4u },     /* leftwards arrow to bar */
    { "->|", 0x21E5u },     /* rightwards arrow to bar */

    /* Comparisons and the arithmetic signs that keep them company.
     * A title carrying "x \u2265 3" is as unreadable with a question
     * mark in it as any other, and these have an unambiguous reading
     * that every programmer already writes by hand. */
    { "inf", 0x221Eu },     /* infinity */
    { "~=",  0x2248u },     /* almost equal to */
    { "!=",  0x2260u },     /* not equal to */
    { "==",  0x2261u },     /* identical to */
    { "<=",  0x2264u },     /* less-than or equal to */
    { ">=",  0x2265u },     /* greater-than or equal to */
    { "<=",  0x2266u },     /* less-than over equal to */
    { ">=",  0x2267u },     /* greater-than over equal to */
    { "<<",  0x226Au },     /* much less-than */
    { ">>",  0x226Bu },     /* much greater-than */
    { "<<<", 0x22D8u },     /* very much less-than */
    { ">>>", 0x22D9u },     /* very much greater-than */

    /* Spaces that are a space and nothing more.  U+00A0 is in the
     * Latin-1 range and would convert without complaint, but "fixed"
     * draws it as a blank box rather than a gap. */
    { " ",   0x00A0u },     /* no-break space */
    { " ",   0x2002u },     /* en space */
    { " ",   0x2003u },     /* em space */
    { " ",   0x2007u },     /* figure space */
    { " ",   0x2009u },     /* thin space */
    { " ",   0x202Fu },     /* narrow no-break space */
    { " ",   0x2028u },     /* line separator */
    { " ",   0x2029u },     /* paragraph separator */

    /* Invisible where they came from, and invisible here */
    { "",   0x00ADu },      /* soft hyphen */
    { "",   0x200Bu },      /* zero-width space */
    { "",   0x200Cu },      /* zero-width non-joiner */
    { "",   0x200Du },      /* zero-width joiner */
    { "",   0xFEFFu }       /* zero-width no-break space */
};


/**
 * @brief What to draw in place of a codepoint a single-byte font
 *        cannot show
 *
 * @param codepoint Codepoint to look up
 *
 * @return Its ASCII reading, or @c NULL when it has none
 *
 * @note Complexity: @e O(n), where @e n is the size of the table
 *       above, which is a fixed handful
 */
static const char *s_ascii_fallback_for(uint32_t codepoint)
{
    const size_t count =
        sizeof(s_ascii_fallbacks) / sizeof(s_ascii_fallbacks[0]);

    for (size_t i = 0u; i < count; ++i) {
        if (s_ascii_fallbacks[i].codepoint == codepoint) {
            return s_ascii_fallbacks[i].replacement;
        }
    }

    return NULL;
}


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
 *        its registry and encoding parts
 *
 * The last token is a charset spec when it contains a hyphen and is
 * not one of the style keywords ('bold'/'italic'/'oblique'); it is
 * then removed from @p tokens (via @p ntok) and split at its
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
        const char tokens[][WM_TEXT_FONT_TOKEN_LENGTH], size_t *ntok)
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
 * @code{.unparsed}
 *   [family] [bold] [italic|oblique] [size] [registry-encoding]
 * @endcode
 *
 * All fields except @p family are optional.  @p registry-encoding is
 * recognized as any whitespace-separated token that contains a hyphen
 * and is not a keyword; it is split at the last hyphen into the XLFD
 * @p charset_registry and @p charset_encoding fields.
 *
 * Examples:
 * @code{.unparsed}
 * "fixed"
 *     "fixed"
 * "fixed 13"
 *     "-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*"
 * "fixed bold 13"
 *     "-*-fixed-bold-r-*-*-13-*-*-*-*-*-*-*"
 * "fixed medium oblique"
 *     "-*-fixed-medium-o-*-*-*-*-*-*-*-*-*-*"
 * "fixed bold 13 iso8859-15"
 *     "-*-fixed-bold-r-*-*-13-*-*-*-*-*-iso8859-15"
 * "fixed bold iso8859-15"
 *     "-*-fixed-bold-r-*-*-*-*-*-*-*-*-iso8859-15"
 * @endcode
 *
 * If @p input already starts with @c '-' it is treated as a full XLFD
 * and copied verbatim into @p output.
 *
 * One static function per phase: tokenizing
 * (@a s_font_config_tokenize), charset extraction
 * (@a s_font_config_extract_charset), size extraction
 * (@a s_font_config_extract_size),
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
    /* Cast written out because C99 does not convert a pointer to an
     * array of 'char' into one to an array of 'const char' on its own,
     * unlike a plain object pointer; C23 does, this project does not
     * target it */
    size = s_font_config_extract_size(
            (const char (*)[WM_TEXT_FONT_TOKEN_LENGTH]) tokens, &ntok);
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
 * in that encoding.
 *
 * Past that range, a codepoint with an obvious ASCII reading is spelled
 * out with it (see @a s_ascii_fallbacks).  For example, a title
 * carrying an em-dash reads as "Page title -- Mozilla Firefox" rather
 * than "Page title ? Mozilla Firefox", which is where a title stops
 * being readable rather than merely imperfect.  Everything else
 * (Cyrillic, CJK, most everything else) becomes a literal '?', since
 * a bitmap X core font like "fixed" has no glyph for it regardless of
 * how faithfully the input text were decoded.
 *
 * @param text     Null-terminated UTF-8 string
 * @param out      Destination buffer
 * @param out_size Size of @p out, in bytes
 *
 * @return Length of the converted string in @p out, in bytes.
 *         A spelled-out replacement can be longer than the one byte its
 *         codepoint would have taken, but never longer than the UTF-8
 *         sequence it came from, so the result still fits wherever the
 *         original text did
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
        const char *replacement;
        size_t replacement_len;

        if (codepoint == 0u) {
            break;
        }

        replacement = s_ascii_fallback_for(codepoint);
        if (replacement == NULL) {
            out[out_len] = (codepoint <= 0xFFu)
                ? (char) codepoint : '?';
            out_len += 1u;
            continue;
        }

        /* Written whole or not at all: half of "--" reads as a hyphen
         * the text never had, which is worse than stopping here */
        replacement_len = strlen(replacement);
        if (out_len + replacement_len > out_size - 1u) {
            break;
        }
        memcpy(&out[out_len], replacement, replacement_len);
        out_len += replacement_len;
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
 * @param entry      Cache entry the font, its graphics context and
 *                   its metrics are stored in
 *
 * @return @c true if the font opened and its metrics could be read
 *
 * @note On failure, any font resource this call opened is closed again
 *       before returning, so the caller never has to clean up a partial
 *       X11 attempt itself
 * @note Complexity: @e O(1)
 */
static bool s_try_x11(xcb_connection_t *connection, const char *xlfd,
        s_text_font_td *entry)
{
    uint32_t gc_values[2];
    xcb_query_font_reply_t *qf_reply;
    xcb_generic_error_t *gc_err;

    entry->font = xcb_generate_id(connection);
    xcb_open_font(connection, entry->font,
            (uint16_t) safe_strlen(xlfd), xlfd);

    qf_reply = xcb_query_font_reply(connection,
            xcb_query_font(connection, entry->font), NULL);
    if (qf_reply == NULL) {
        xcb_close_font(connection, entry->font);
        entry->font = XCB_NONE;
        return false;
    }

    if (qf_reply->max_bounds.character_width > 0) {
        entry->char_width =
            (uint16_t) qf_reply->max_bounds.character_width;
    }
    entry->ascent = qf_reply->font_ascent;
    entry->descent = qf_reply->font_descent;
    free(qf_reply);

    entry->gc = xcb_generate_id(connection);

    /* Neutral defaults: 'white-on-black'.
     * For themed titlebar, call 'text_renderer_set_color' afterwards to
     * use the foreground/background colors from theme */
    gc_values[0] = 0xFFFFFFu;   /* fg: white */
    gc_values[1] = 0x000000u;   /* bg: black */
    gc_err = xcb_request_check(connection,
            xcb_create_gc_checked(connection, entry->gc,
                xcb_setup_roots_iterator(
                    xcb_get_setup(connection)).data->root,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, gc_values));
    if (gc_err != NULL) {
        LOGGER_WARNING("xcb_create_gc failed for text renderer," \
                " error=%d", gc_err->error_code);
        free(gc_err);
    }
    gc_err = xcb_request_check(connection,
            xcb_change_gc_checked(connection, entry->gc, XCB_GC_FONT,
                (const uint32_t[]) {entry->font}));
    if (gc_err != NULL) {
        LOGGER_WARNING("xcb_change_gc failed for text renderer," \
                " error=%d", gc_err->error_code);
        free(gc_err);
    }

    return true;
}


/**
 * @brief Release whatever a cache entry holds and mark it free
 *
 * @param entry Entry to release
 *
 * @note Complexity: @e O(1)
 */
static void s_text_cache_release(s_text_font_td *entry)
{
    if (!entry->is_used) {
        return;
    }

    if (entry->backend == S_BACKEND_GLYPH) {
        glyph_renderer_release(entry->key);
    } else if (xcb_connection_get() != NULL) {
        if (entry->gc != XCB_NONE) {
            xcb_free_gc(xcb_connection_get(), entry->gc);
        }
        if (entry->font != XCB_NONE) {
            xcb_close_font(xcb_connection_get(), entry->font);
        }
    }

    entry->key[0] = '\0';
    entry->font = XCB_NONE;
    entry->gc = XCB_NONE;
    entry->last_used = 0u;
    entry->backend = S_BACKEND_NONE;
    entry->char_width = 8u;
    entry->ascent = 10;
    entry->descent = 3;
    entry->is_used = false;
}


/**
 * @brief Find a font already in the cache
 *
 * @param key Font name exactly as the caller gave it
 *
 * @return Index of the matching entry, or @c WM_TEXT_FONT_CACHE_MAX
 *         when the font is not cached
 *
 * @note Complexity: @e O(n), where @e n is
 *       @c WM_TEXT_FONT_CACHE_MAX
 */
static uint32_t s_text_cache_find(const char *key)
{
    for (uint32_t i = 0u; i < WM_TEXT_FONT_CACHE_MAX; ++i) {
        if (s_text.cache[i].is_used &&
                safe_strcmp(s_text.cache[i].key, key) == 0) {
            return i;
        }
    }

    return WM_TEXT_FONT_CACHE_MAX;
}


/**
 * @brief Reserve a cache slot for a font about to be opened
 *
 * A free slot is used when there is one.  Otherwise the least
 * recently used entry is released to make room, which is what bounds
 * the cache: it never grows, it only replaces.
 *
 * The glyph backend has a quota of its own,
 * @c WM_TEXT_FONT_CACHE_MAX_GLYPH, so a glyph font never crowds out
 * the cheap X core fonts.  Once that quota is met, the least recently
 * used glyph entry is the one released, whatever the global order
 * says.
 *
 * @param backend Backend the new font will use
 *
 * @return Index of a slot ready to be filled in
 *
 * @note Complexity: @e O(n), where @e n is
 *       @c WM_TEXT_FONT_CACHE_MAX
 */
static uint32_t s_text_cache_claim(enum s_text_backend_e backend)
{
    uint32_t victim = WM_TEXT_FONT_CACHE_MAX;
    uint32_t oldest = UINT32_MAX;

    if (backend == S_BACKEND_GLYPH) {
        uint32_t glyph_count = 0u;

        for (uint32_t i = 0u; i < WM_TEXT_FONT_CACHE_MAX; ++i) {
            if (s_text.cache[i].is_used &&
                    s_text.cache[i].backend == S_BACKEND_GLYPH) {
                glyph_count++;
                if (s_text.cache[i].last_used < oldest) {
                    oldest = s_text.cache[i].last_used;
                    victim = i;
                }
            }
        }

        if (glyph_count >= WM_TEXT_FONT_CACHE_MAX_GLYPH &&
                victim < WM_TEXT_FONT_CACHE_MAX) {
            s_text_cache_release(&s_text.cache[victim]);
            return victim;
        }
    }

    for (uint32_t i = 0u; i < WM_TEXT_FONT_CACHE_MAX; ++i) {
        if (!s_text.cache[i].is_used) {
            return i;
        }
    }

    victim = 0u;
    oldest = s_text.cache[0].last_used;
    for (uint32_t i = 1u; i < WM_TEXT_FONT_CACHE_MAX; ++i) {
        if (s_text.cache[i].last_used < oldest) {
            oldest = s_text.cache[i].last_used;
            victim = i;
        }
    }

    s_text_cache_release(&s_text.cache[victim]);

    return victim;
}


/**
 * @brief The entry drawing currently goes through
 *
 * @return Pointer to the active entry, or @c NULL when no font has been
 *         selected yet
 *
 * @note Complexity: @e O(1)
 */
static s_text_font_td *s_text_current(void)
{
    if (!s_text.is_initialized ||
            s_text.current >= WM_TEXT_FONT_CACHE_MAX) {
        return NULL;
    }

    return &s_text.cache[s_text.current];
}


/* Permanently disable the glyph ('xcb-render'/FreeType2/fontconfig)
 * backend for the life of the process */
void text_renderer_disable_glyph_backend(void)
{
    s_text.is_glyph_backend_disabled = true;
}


/* Initialize the text renderer */
int text_renderer_init(const xcb_connection_t *connection)
{
    if (connection == NULL) {
        return -1;
    }

    /* Closing first covers the reload case, where the new theme may
     * name entirely different fonts and every cached one is stale */
    text_renderer_destroy();

    s_text.clock = 0u;
    s_text.current = WM_TEXT_FONT_CACHE_MAX;
    s_text.is_initialized = true;

    return 0;
}


/* Make a font the one every later drawing call uses */
int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    char xlfd[WM_TEXT_FONT_NAME_LENGTH];
    const char *raw;
    uint32_t index;
    s_text_font_td *entry;

    if (connection == NULL) {
        return -1;
    }

    /* A caller that never called 'text_renderer_init' still gets a
     * working renderer, bound to the connection it just handed over */
    if (!s_text.is_initialized || xcb_connection_get() != connection) {
        if (text_renderer_init(connection) != 0) {
            return -1;
        }
    }

    raw = (font_name == NULL || font_name[0] == '\0')
        ? "fixed"
        : font_name;

    s_text.clock++;

    index = s_text_cache_find(raw);
    if (index < WM_TEXT_FONT_CACHE_MAX) {
        /* A glyph-backend font lives in 'render/glyph.c', which keeps
         * a selection of its own, so a hit here has to point that
         * selection at this font too.  Without it, drawing would go
         * through whichever font the glyph backend happened to have
         * selected last, which is another one entirely as soon as
         * a theme names more than one. */
        if (s_text.cache[index].backend == S_BACKEND_GLYPH &&
                glyph_renderer_init(connection, raw) != 0) {
            s_text_cache_release(&s_text.cache[index]);
            return -1;
        }

        s_text.cache[index].last_used = s_text.clock;
        s_text.current = index;
        return 0;
    }

    /* Convert the config-style font description (e.g., "fixed bold 13")
     * to an XLFD wildcard pattern that 'xcb_open_font' can resolve */
    s_font_config_to_xlfd(raw, xlfd, sizeof(xlfd));

    index = s_text_cache_claim(S_BACKEND_X11);
    entry = &s_text.cache[index];

    if (s_try_x11(connection, xlfd, entry)) {
        safe_strncpy(entry->key, raw, sizeof(entry->key));
        entry->backend = S_BACKEND_X11;
        entry->last_used = s_text.clock;
        entry->is_used = true;
        s_text.current = index;
        return 0;
    }

    /* 'xlfd' did not resolve to any X core font (e.g., a TrueType or
     * OpenType family name most systems have via fontconfig but whose
     * bitmap X font set does not include).  Fall back to rendering it
     * through xcb-render/FreeType2/fontconfig instead, handing
     * fontconfig the caller's original string rather than the XLFD
     * pattern just built for X11, since fontconfig has its,
     * different pattern syntax.
     *
     * Never even attempted at all once
     * 'text_renderer_disable_glyph_backend' has been called: falls
     * straight through to the "fixed" fallback below instead, the same
     * as if this attempt had failed. */
    if (!s_text.is_glyph_backend_disabled) {
        index = s_text_cache_claim(S_BACKEND_GLYPH);
        entry = &s_text.cache[index];

        if (glyph_renderer_init(connection, raw) == 0) {
            safe_strncpy(entry->key, raw, sizeof(entry->key));
            entry->backend = S_BACKEND_GLYPH;
            entry->ascent = glyph_font_ascent();
            entry->descent = glyph_font_descent();
            entry->last_used = s_text.clock;
            entry->is_used = true;
            s_text.current = index;
            return 0;
        }
    }

    /* Both backends failed for this specific font description: fall
     * back to "fixed", which every X server ships and is guaranteed to
     * open, so the renderer is never left completely unusable. */
    if (safe_strcmp(raw, "fixed") != 0) {
        return text_renderer_use_font(connection, "fixed");
    }

    return -1;
}


/* Destroy global text renderer resources */
void text_renderer_destroy(void)
{
    for (uint32_t i = 0u; i < WM_TEXT_FONT_CACHE_MAX; ++i) {
        s_text_cache_release(&s_text.cache[i]);
    }

    /* Releasing each cached font one by one closes its face and its
     * glyph set, but not what the glyph backend shares across all of
     * them.  The FreeType library and its solid-fill picture */
    glyph_renderer_destroy();

    s_text.clock = 0u;
    s_text.current = WM_TEXT_FONT_CACHE_MAX;
    s_text.is_initialized = false;
}


/* Update the text renderer GC's foreground and background colors */
void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    uint32_t gc_values[2];
    const s_text_font_td *const entry = s_text_current();

    if (entry == NULL) {
        return;
    }

    if (entry->backend == S_BACKEND_GLYPH) {
        glyph_renderer_set_color(fg, bg);
        return;
    }

    if (entry->gc == XCB_NONE || xcb_connection_get() == NULL) {
        return;
    }

    gc_values[0] = fg;
    gc_values[1] = bg;
    xcb_change_gc(xcb_connection_get(), entry->gc,
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

    if (s_text_current() == NULL ||
            xcb_connection_get() != connection) {
        if (text_renderer_use_font(connection, "fixed") != 0) {
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

    if (s_text_current()->backend == S_BACKEND_GLYPH) {
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
                    ? s_text_current()->gc
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
    const s_text_font_td *const entry = s_text_current();
    size_t char_count = 0u;
    size_t byte_index = 0u;

    if (entry == NULL) {
        return 0u;
    }
    if (entry->backend == S_BACKEND_GLYPH) {
        return glyph_measure_string(text);
    }
    if (text == NULL) {
        return 0u;
    }

    /* Counting decoded codepoints, not 'safe_strlen's UTF-8 byte
     * count: 's_utf8_to_latin1' always draws exactly one glyph per
     * codepoint (the Latin-1 byte itself, or a '?' substitute for
     * anything further out), so a multi-byte accented character
     * measures as the one character cell it actually occupies once
     * drawn, not the two UTF-8 bytes it takes on the wire. */
    while (glyph_utf8_next(text, &byte_index) != 0u) {
        char_count += 1u;
    }

    if (char_count > UINT16_MAX / entry->char_width) {
        return UINT16_MAX;
    }

    return (uint16_t) (char_count * entry->char_width);
}


/* Copy text into a buffer, shortening it a character at a time from the
 * end until it measures no wider than a given limit */
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
    const s_text_font_td *const entry = s_text_current();

    return (entry != NULL) ? entry->ascent : 10;
}


/* Pixels the baseline sits above the bottom of a line, for the current
 * font */
int16_t text_font_descent(void)
{
    const s_text_font_td *const entry = s_text_current();

    return (entry != NULL) ? entry->descent : 3;
}
