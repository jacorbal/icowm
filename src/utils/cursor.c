/**
 * @file utils/cursor.c
 *
 * @brief Theme-aware cursor loading implementation
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
#include <stdlib.h>     /* NULL, malloc, free */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <utils/cursor.h>


/**
 * @brief Cursor-loading context, one per screen
 */
struct util_cursor_ctx_s {
    xcb_connection_t *connection;
    xcb_cursor_context_t *theme_ctx; /**< @c NULL if the theme lookup
                                          itself could not be set up;
                                          every load then falls back
                                          to the X core font */
    xcb_font_t fallback_font;        /**< Opened lazily, on first
                                          actual fallback use */
    bool fallback_font_open;
};


/* Create a cursor-loading context for the given screen */
util_cursor_ctx_td *util_cursor_ctx_new(xcb_connection_t *connection,
        xcb_screen_t *screen)
{
    util_cursor_ctx_td *ctx;

    if (connection == NULL || screen == NULL) {
        return NULL;
    }

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->connection = connection;
    ctx->fallback_font = 0;
    ctx->fallback_font_open = false;

    if (xcb_cursor_context_new(connection, screen, &ctx->theme_ctx) < 0) {
        /* Theme lookup unavailable (no theme installed, or the library
         * itself could not initialize): every load will use the
         * fallback font instead, still through this same context so the
         * caller's code does not have to change either way. */
        ctx->theme_ctx = NULL;
    }

    return ctx;
}


/**
 * @brief Load @p fallback_glyph from the X core "cursor" font,
 *        opening that font on @p ctx the first time it is needed
 *
 * @param ctx            Context to open (and cache) the font on
 * @param fallback_glyph Source glyph; the mask glyph is
 *                       @p (fallback_glyph + 1)
 *
 * @return The loaded cursor, or @c XCB_NONE if @p ctx is @c NULL
 *
 * @note Complexity: @e O(1)
 */
static xcb_cursor_t s_load_fallback(util_cursor_ctx_td *ctx,
        uint16_t fallback_glyph)
{
    xcb_cursor_t cursor;

    if (ctx == NULL) {
        return XCB_NONE;
    }

    if (!ctx->fallback_font_open) {
        ctx->fallback_font = xcb_generate_id(ctx->connection);
        xcb_open_font(ctx->connection, ctx->fallback_font,
                (uint16_t) safe_strlen("cursor"), "cursor");
        ctx->fallback_font_open = true;
    }

    cursor = xcb_generate_id(ctx->connection);
    xcb_create_glyph_cursor(ctx->connection, cursor,
            ctx->fallback_font, ctx->fallback_font,
            fallback_glyph, (uint16_t) (fallback_glyph + 1u),
            0u, 0u, 0u, 0xffffu, 0xffffu, 0xffffu);

    return cursor;
}


/* Load a cursor by its standard Xcursor name */
xcb_cursor_t util_cursor_load(util_cursor_ctx_td *ctx, const char *name,
        uint16_t fallback_glyph)
{
    xcb_cursor_t cursor;

    if (ctx == NULL || name == NULL) {
        return s_load_fallback(ctx, fallback_glyph);
    }

    if (ctx->theme_ctx == NULL) {
        return s_load_fallback(ctx, fallback_glyph);
    }

    cursor = xcb_cursor_load_cursor(ctx->theme_ctx, name);
    if (cursor == XCB_NONE) {
        return s_load_fallback(ctx, fallback_glyph);
    }

    return cursor;
}


/* Destroy a cursor-loading context */
void util_cursor_ctx_free(util_cursor_ctx_td *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->theme_ctx != NULL) {
        xcb_cursor_context_free(ctx->theme_ctx);
    }
    if (ctx->fallback_font_open) {
        xcb_close_font(ctx->connection, ctx->fallback_font);
    }

    free(ctx);
}
