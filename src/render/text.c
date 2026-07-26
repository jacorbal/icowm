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
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safestr.h>

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


/* Initialize the text renderer using the specified font */
int text_renderer_init(xcb_connection_t *connection,
        const char *font_name)
{
    const char *font = (font_name == NULL || font_name[0] == '\0')
        ? "fixed"
        : font_name;
    xcb_query_font_cookie_t qf_cookie;
    xcb_query_font_reply_t *qf_reply;
    uint32_t gc_values[2];

    if (connection == NULL) {
        return -1;
    }

    if (s_text.initialized &&
            s_text.connection == connection &&
            safe_strcmp(s_text.font_name, font) == 0) {
        return 0;
    }

    text_renderer_destroy();

    s_text.connection = connection;
    safe_strncpy(s_text.font_name, font, sizeof(s_text.font_name));
    s_text.font = xcb_generate_id(connection);
    xcb_open_font(connection, s_text.font,
            (uint16_t) safe_strlen(font), font);

    s_text.gc = xcb_generate_id(connection);

    /* Neutral defaults: white-on-black */
    /* NOTE: Callers that draw on a themed titlebar should call
     *       'text_renderer_set_color()' afterwards to use the
     *       foreground and background colors from theme */
    gc_values[0] = 0xFFFFFFu;   /* fg: white */
    gc_values[1] = 0x000000u;   /* bg: black */
    xcb_create_gc(connection, s_text.gc,
            xcb_setup_roots_iterator(xcb_get_setup(connection)).data->root,
            XCB_GC_FOREGROUND |
            XCB_GC_BACKGROUND,
            gc_values);
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
            XCB_GC_FOREGROUND |
            XCB_GC_BACKGROUND,
            gc_values);
}


/* Draw a string at the specified position */
void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        int16_t x, int16_t y, const char *text)
{
    size_t len;

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

    xcb_image_text_8(connection, (uint8_t) len, drawable,
            (gc == XCB_NONE) ? s_text.gc : gc, x, y, text);
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
