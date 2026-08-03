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
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* atoi, free */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <render/text.h>


static struct {
    xcb_connection_t *connection;
    xcb_font_t font;
    xcb_gcontext_t gc;
    char font_name[256];
    uint16_t char_width;
    bool initialized;
} s_text = {
    .connection = NULL,
    .font = XCB_NONE,
    .gc = XCB_NONE,
    .char_width = 8,
    .initialized = false
};


/**
 * @brief Convert a font configuration string to an X11 XLFD pattern
 *
 * IcoWM uses a simple font description syntax in its theme files:
 *
 * @code
 *   [family] [bold] [italic|oblique] [size]
 * @endcode
 *
 * Examples:
 * @code
 *   "fixed"              -> "fixed"            (simple alias, pass through)
 *   "fixed 9"            -> "-*-fixed-medium-r-*-*-9-*-*-*-*-*-*-*"
 *   "fixed bold 9"       -> "-*-fixed-bold-r-*-*-9-*-*-*-*-*-*-*"
 *   "fixed bold oblique" -> "-*-fixed-bold-o-*-*-*-*-*-*-*-*-*-*"
 * @endcode
 *
 * If @p input already starts with @c '-' it is treated as a full XLFD
 * and copied verbatim into @p output.
 *
 * @param input   Null-terminated font description string
 * @param output  Buffer for the resulting XLFD pattern
 * @param outsize Size of @p output in bytes
 */
static void font_config_to_xlfd(const char *input, char *output,
        size_t outsize)
{
    char tokens[8][64];     /* Maximum tokens we ever need:
                             *  family + bold + italic/oblique + size */
    size_t ntok = 0u;
    const char *p;
    int size = 0;
    bool is_bold = false;
    bool is_italic = false;
    bool is_oblique = false;
    char family[128];
    size_t fi = 0u;
    const char *last;
    bool is_num;
    const char *weight_str;
    const char *slant_str;

    if (input == NULL || input[0] == '\0') {
        safe_strncpy(output, "fixed", outsize);
        return;
    }

    /* Pass XLFD strings (starting with '-') through unchanged */
    if (input[0] == '-') {
        safe_strncpy(output, input, outsize);
        return;
    }

    /* Tokenize on whitespace */
    p = input;
    while (*p != '\0' && ntok < 8u) {
        size_t tlen = 0u;

        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        /* Read one token */
        while (*p != ' ' && *p != '\t' && *p != '\0' && tlen < 63u) {
            tokens[ntok][tlen++] = *p++;
        }
        tokens[ntok][tlen] = '\0';
        ntok++;
    }

    if (ntok == 0u) {
        safe_strncpy(output, "fixed", outsize);
        return;
    }

    /* If the last token is an all-digit string, treat it as the pixel
     * size */
    last = tokens[ntok - 1u];
    is_num = (last[0] != '\0');
    for (size_t j = 0u; last[j] != '\0'; ++j) {
        if (last[j] < '0' || last[j] > '9') {
            is_num = false;
            break;
        }
    }
    if (is_num) {
        size = atoi(last);
        ntok--;
    }

    /* Scan remaining tokens for weight and slant keywords */
    for (size_t i = 0u; i < ntok; ++i) {
        if (safe_strcmp(tokens[i], "bold") == 0) {
            is_bold = true;
        } else if (safe_strcmp(tokens[i], "italic") == 0) {
            is_italic = true;
        } else if (safe_strcmp(tokens[i], "oblique") == 0) {
            is_oblique = true;
        }
    }

    /* Build family string from non-keyword tokens */
    family[0] = '\0';
    fi = 0u;
    for (size_t i = 0u; i < ntok; ++i) {
        if (safe_strcmp(tokens[i], "bold") == 0 ||
                safe_strcmp(tokens[i], "italic") == 0 ||
                safe_strcmp(tokens[i], "oblique") == 0) {
            continue;
        }

        if (fi > 0u && fi < sizeof(family) - 1u) {
            family[fi++] = ' ';
        }

        for (size_t j = 0u;
                tokens[i][j] != '\0' && fi < sizeof(family) - 1u;
                ++j) {
            family[fi++] = tokens[i][j];
        }
    }

    family[fi] = '\0';
    if (family[0] == '\0') {
        safe_strncpy(output, "fixed", outsize);
        return;
    }

    /* A bare family name with no size or weight modifiers is a valid
     * X font alias (e.g., "fixed"); pass it straight through */
    if (size == 0 && !is_bold && !is_italic && !is_oblique) {
        safe_strncpy(output, family, outsize);
        return;
    }

    /* Build an XLFD wildcard pattern */
    weight_str = (is_bold) ? "bold" : "medium";
    slant_str = (is_italic) ? "i" : (is_oblique ? "o" : "r");

    if (size > 0) {
        (void) snprintf(output, outsize,
                "-*-%s-%s-%s-*-*-%d-*-*-*-*-*-*-*",
                family, weight_str, slant_str, size);
    } else {
        (void) snprintf(output, outsize,
                "-*-%s-%s-%s-*-*-*-*-*-*-*-*-*-*",
                family, weight_str, slant_str);
    }
}


/* Initialize the text renderer using the specified font */
int text_renderer_init(xcb_connection_t *connection,
        const char *font_name)
{
    char xlfd[256];
    xcb_query_font_cookie_t qf_cookie;
    xcb_query_font_reply_t *qf_reply;
    uint32_t gc_values[2];

    if (connection == NULL) {
        return -1;
    }


    /* Convert the config-style font description (e.g., "fixed bold 9")
     * to an XLFD wildcard pattern that 'xcb_open_font' can resolve */
    if (font_name == NULL || font_name[0] == '\0') {
        safe_strncpy(xlfd, "fixed", sizeof(xlfd));
    } else {
        font_config_to_xlfd(font_name, xlfd, sizeof(xlfd));
    }


    if (s_text.initialized &&
            s_text.connection == connection &&
            safe_strcmp(s_text.font_name, xlfd) == 0) {
        return 0;
    }

    text_renderer_destroy();

    s_text.connection = connection;
    safe_strncpy(s_text.font_name, xlfd, sizeof(s_text.font_name));
    s_text.font = xcb_generate_id(connection);
    xcb_open_font(connection, s_text.font,
            (uint16_t) safe_strlen(xlfd), xlfd);

    s_text.gc = xcb_generate_id(connection);

    /* Neutral defaults: white-on-black.  Callers that draw on a themed
     * titlebar should call 'text_renderer_set_color' afterwards to use
     * the foreground/background colors from their theme. */
    gc_values[0] = 0xFFFFFFu;  /* fg: white */
    gc_values[1] = 0x000000u;  /* bg: black */
    xcb_create_gc(connection, s_text.gc,
            xcb_setup_roots_iterator(xcb_get_setup(connection)).data->root,
            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, gc_values);
    xcb_change_gc(connection, s_text.gc, XCB_GC_FONT,
            (const uint32_t[]) {s_text.font});

    qf_cookie = xcb_query_font(connection, s_text.font);
    qf_reply = xcb_query_font_reply(connection, qf_cookie, NULL);
    if (qf_reply != NULL) {
        if (qf_reply->max_bounds.character_width > 0) {
            s_text.char_width =
                (uint16_t) qf_reply->max_bounds.character_width;
        }
        free(qf_reply);
    }

    s_text.initialized = true;
    return 0;
}


/* Destroy global text renderer resources */
void text_renderer_destroy(void)
{
    if (!s_text.initialized || s_text.connection == NULL) {
        return;
    }

    if (s_text.gc != XCB_NONE) {
        xcb_free_gc(s_text.connection, s_text.gc);
    }

    if (s_text.font != XCB_NONE) {
        xcb_close_font(s_text.connection, s_text.font);
    }

    s_text.connection = NULL;
    s_text.font = XCB_NONE;
    s_text.gc = XCB_NONE;
    s_text.font_name[0] = '\0';
    s_text.char_width = 8;
    s_text.initialized = false;
}


/* Update the foreground and background colors of the text renderer GC */
void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    uint32_t gc_values[2];

    if (!s_text.initialized || s_text.gc == XCB_NONE ||
            s_text.connection == NULL) {
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
        int16_t x, int16_t y, const char *text)
{
    size_t len;
    xcb_void_cookie_t draw_cookie;
    xcb_generic_error_t *draw_error;

    if (connection == NULL || drawable == XCB_NONE || text == NULL) {
        return;
    }
    if (!s_text.initialized || s_text.connection != connection) {
        if (text_renderer_init(connection, "fixed") != 0) {
            return;
        }
    }

    len = safe_strlen(text);
    if (len == 0) {
        return;
    }
    if (len > 255) {
        len = 255;
    }

    draw_cookie = xcb_image_text_8_checked(connection, (uint8_t) len,
            drawable, (gc == XCB_NONE) ? s_text.gc : gc, x, y, text);
    draw_error = xcb_request_check(connection, draw_cookie);
    if (draw_error != NULL) {
        LOGGER_WARNING("'xcb_image_text_8' failed on drawable %#x" \
                " (error=%u)",
                drawable, (unsigned) draw_error->error_code);
        free(draw_error);
    }
}


/* Measure the rendered width of a string */
uint16_t text_measure_string(const char *text)
{
    size_t len = safe_strlen(text);

    if (len > UINT16_MAX / s_text.char_width) {
        return UINT16_MAX;
    }

    return (uint16_t) (len * s_text.char_width);
}
