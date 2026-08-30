/**
 * @file render/text.h
 *
 * @brief Basic XCB text rendering helpers
 *
 * @ingroup render
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_TEXT_H
#define RENDER_TEXT_H


/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* size_t */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Types includes */
#include <types/pair.h>


/* Public interface */
/**
 * @brief Permanently disable the glyph (@c xcb-render / FreeType2 /
 *        fontconfig) rendering backend for the life of the process
 *
 * @a text_renderer_use_font never attempts that backend afterward for
 * a font name that fails to resolve to an X core font, falling
 * straight through to the "fixed" fallback instead, the same as if
 * the attempt had simply failed.  Meant for restricted-memory mode,
 * called once at startup.  That mode already forces every theme font
 * to some variant of "fixed", see @c config/memguard.h, which always
 * resolves as an X core font, so the glyph backend is never needed
 * there.
 *
 * This closes the one remaining way it could still end up loaded
 * anyway, a font name that happens to resolve to an X core font under
 * a case-sensitive match but not under one that ignores case (e.g.,
 * a hipothetical Xft family literally named "Fixed Bold", capitalized,
 * distinct from the plain lowercase "fixed bold" restricted-memory
 * mode's font substitution rule intentionally treats as the X core
 * family instead) without relying on that font-name matching to be
 * perfect.
 *
 * @note Complexity: @e O(1)
 */
void text_renderer_disable_glyph_backend(void);

/**
 * @brief Initialize the text renderer
 *
 * Binds the renderer to a connection and empties the font cache.  No
 * font is open yet afterward; @a text_renderer_use_font opens the
 * first one.  Calling this again on an already-initialized renderer
 * closes every cached font first.
 *
 * A configuration reload does not need this.  The cache is keyed by
 * font name, so a theme that names the same font as before keeps
 * hitting the entry already open, and one that names a different font
 * simply misses and opens it, leaving the entry it replaced to age
 * out of the cache on its own.
 *
 * @param connection Pointer to the XCB connection
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 when @p connection is @c NULL
 *
 * @note The caller releases the renderer with
 *       @a text_renderer_destroy
 * @note Complexity: @e O(n), where @e n is the number of cached fonts
 *       closed
 */
int text_renderer_init(const xcb_connection_t *connection);

/**
 * @brief Make a font the one every later drawing call uses
 *
 * Looks the font up in the cache and, on a miss, opens it and stores
 * it.  A font already cached costs a lookup and nothing else, which
 * is what lets a caller alternate between two fonts without paying an
 * @c xcb_close_font and an @c xcb_open_font round trip each time.
 *
 * The X core-font backend is tried first, on the XLFD pattern derived
 * from @p font_name.  Should that fail, the glyph backend is tried on
 * @p font_name as fontconfig itself would read it, unless
 * @a text_renderer_disable_glyph_backend has been called.  Should
 * both fail, "fixed" is opened instead, so the renderer is never left
 * with no font at all.
 *
 * @param connection Pointer to the XCB connection
 * @param font_name  Font to use, or @c NULL for the default
 *
 * @return Status of the operation
 * @retval  0 The font is now the active one
 * @retval -1 Neither backend could open it, nor the "fixed" fallback
 *
 * @note If @p font_name is @c NULL or empty, "fixed" is used
 * @note Complexity: @e O(n) on a cache hit, where @e n is the number
 *       of cached fonts scanned; the cost of opening a font on a miss
 * @see @a text_renderer_init, which must have been called first
 */
int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name);

/**
 * @brief Destroy the global text renderer resources
 *
 * Releases the graphics context and font owned by the text renderer,
 * then resets the internal state to its defaults.
 *
 * @note Complexity: @e O(1)
 */
void text_renderer_destroy(void);

/**
 * @brief Update the foreground and background colors of the text GC
 *
 * Changes the @c XCB_GC_FOREGROUND and @c XCB_GC_BACKGROUND attributes
 * of the internal graphics context.  Call this after
 * @a text_renderer_use_font to select colors appropriate for the
 * context (e.g., theme active/inactive foreground colors for titlebars)
 * before invoking @a text_draw_string.
 *
 * @param fg Foreground pixel value (text color)
 * @param bg Background pixel value (used by some drawing operations)
 *
 * @note Has no effect when the renderer is not yet initialized
 * @note Complexity: @e O(1)
 */
void text_renderer_set_color(uint32_t fg, uint32_t bg);

/**
 * @brief Draw a string at the specified position
 *
 * Renders the supplied text on the given drawable using either the
 * provided graphics context or the internal text renderer GC when @p gc
 * is @c XCB_NONE.  The renderer is initialized on demand if needed.
 *
 * @param connection Pointer to the XCB connection
 * @param drawable   Target drawable where the text will be drawn
 * @param gc         Graphics context to use, or @c XCB_NONE for
 *                   the default one
 * @param pos        Position of the text baseline
 * @param text       Null-terminated string to draw
 *
 * @note Strings longer than 255 bytes are truncated to fit the XCB text
 *       drawing request limit
 * @note Complexity: @e O(n), where @e n is the length of the text
 */
void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text);

/**
 * @brief Measure the rendered width of a string
 *
 * Computes the approximate pixel width of the supplied string using the
 * cached character width stored in the text renderer state.
 *
 * @param text Null-terminated string to measure, or @c NULL
 *
 * @return Estimated string width in pixels, or @c UINT16_MAX on
 *         overflow
 *
 * @note Complexity: @e O(n), where @e n is the length of the text
 */
uint16_t text_string_measure(const char *text);

/**
 * @brief Copy @p text into @p buf, shortening it one character at a
 *        time from the end until it measures no wider than
 *        @p max_width
 *
 * @param buf       Destination buffer
 * @param buf_size  Size of @p buf in bytes
 * @param text      Source string to copy and, if needed, shorten
 * @param max_width Widest @p buf is allowed to measure afterward
 *
 * @note @p buf may end up empty if not even a single character of
 *       @p text fits within @p max_width
 * @note Complexity: @e O(n), where @e n is the length of @p text
 */
void text_truncate_to_width(char *buf, size_t buf_size,
        const char *text, uint16_t max_width);

/**
 * @brief Pixels the baseline sits below the top of a line, for
 *        whichever font @a text_renderer_use_font last selected
 *
 * Together with @a text_font_descent, lets a caller vertically center
 * or top/bottom-align a line of text against a known pixel height
 * without needing its hardcoded assumption about font metrics: the
 * Y coordinate @a text_draw_string expects is the baseline, so placing
 * text @p top pixels from the top of a box of height @p box_h means
 * passing @c top @c + @a text_font_ascent() as that Y coordinate.
 *
 * @return Font ascent in pixels; a small built-in default before the
 *         first successful @a text_renderer_use_font call
 *
 * @note Complexity: @e O(1)
 */
int16_t text_font_ascent(void);

/**
 * @brief Pixels the baseline sits above the bottom of a line, for
 *        whichever font @a text_renderer_use_font last selected
 *
 * @return Font descent in pixels; a small built-in default before the
 *         first successful @a text_renderer_use_font call
 *
 * @note Complexity: @e O(1)
 */
int16_t text_font_descent(void);


#endif  /* ! RENDER_TEXT_H */
