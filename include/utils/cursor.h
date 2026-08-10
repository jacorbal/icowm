/**
 * @file utils/cursor.h
 *
 * @brief Theme-aware cursor loading, with a fallback to the X core
 *        cursor font
 *
 * Every cursor this window manager shows (the eight border-resize
 * cursors, the default pointer, the startup-notification busy cursor)
 * goes through this one helper, so the whole project draws cursors
 * from the user's actual cursor theme (the same one published via
 * @c xsettings.cursor-theme-name) via @c libxcb-cursor, rather than
 * the fixed, low-resolution glyphs built into the X server's own
 * "cursor" font.  The X core font is used only as an automatic
 * fallback, for a cursor name the active theme happens not to
 * provide.
 *
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_CURSOR_H
#define UTILS_CURSOR_H


/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Opaque cursor-loading context for one screen
 *
 * Wraps a @c libxcb-cursor theme lookup handle, plus (opened lazily,
 * only if ever actually needed) the X core "cursor" font used for the
 * fallback path.  Reusing one context to load several cursors avoids
 * repeating the theme lookup's own setup cost for each one.
 */
typedef struct util_cursor_ctx_s util_cursor_ctx_td;


/* Public interface */
/**
 * @brief Create a cursor-loading context for the given screen
 *
 * @param connection XCB connection
 * @param screen     Screen the context loads cursors for
 *
 * @return The new context, or @c NULL on failure (in which case
 *         @c util_cursor_load still works for every call, falling
 *         back to the X core font every time, just without the
 *         reused-lookup-handle savings)
 *
 * @note Complexity: @e O(1), aside from the underlying library's own
 *       one-time theme lookup setup
 */
util_cursor_ctx_td *util_cursor_ctx_new(xcb_connection_t *connection,
        xcb_screen_t *screen);

/**
 * @brief Load a cursor by its standard Xcursor name
 *
 * @param ctx            Context from @c util_cursor_ctx_new, or
 *                        @c NULL to always use the fallback font
 * @param name            Standard Xcursor name (e.g., @c "left_ptr",
 *                        @c "watch", @c "top_side")
 * @param fallback_glyph  X core cursor-font glyph to fall back to
 *                        (see @c defs/cursor.h) if the active cursor
 *                        theme does not provide @p name
 *
 * @return Identifier of the loaded cursor resource, or @c XCB_NONE if
 *         even the fallback could not be created
 *
 * @note The caller owns the returned cursor and must free it with
 *       @c xcb_free_cursor once no longer needed
 * @note Complexity: @e O(1)
 */
xcb_cursor_t util_cursor_load(util_cursor_ctx_td *ctx, const char *name,
        uint16_t fallback_glyph);

/**
 * @brief Destroy a cursor-loading context
 *
 * @param ctx Context to destroy, or @c NULL (a no-op)
 *
 * @note Complexity: @e O(1)
 */
void util_cursor_ctx_free(util_cursor_ctx_td *ctx);


#endif  /* ! UTILS_CURSOR_H */
