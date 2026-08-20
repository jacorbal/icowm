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


/* Public interface */
/**
 * @brief Permanently disable the glyph (@c xcb-render / FreeType2 /
 *        fontconfig) rendering backend for the life of the process
 *
 * @a text_renderer_init never even attempts that backend afterward for
 * any font name that fails to resolve to an X core font, falling
 * straight through to its own "fixed" fallback instead, the same as if
 * the attempt had simply failed.  Meant for restricted-memory mode
 * specifically, called once at startup.  That mode already forces every
 * theme font to some variant of "fixed" (@c config/memguard.h), which
 * always resolves as an X core font on its own, so the glyph backend is
 * never actually needed there.
 *
 * This closes the one remaining way it could still end up loaded
 * anyway, a font name that happens to resolve to an X core font under
 * a case-sensitive match but not under one that ignores case (e.g.,
 * a hipothetical Xft family literally named "Fixed Bold", capitalized,
 * distinct from the plain lowercase "fixed bold" restricted-memory
 * mode's own font substitution rule intentionally treats as the X core
 * family instead) without relying on that font-name matching to be
 * perfect.
 *
 * @note Complexity: @e O(1)
 */
void text_renderer_disable_glyph_backend(void);

/**
 * @brief Initialize the text renderer using the specified font
 *
 * Opens the requested X font, creates the graphics context used for
 * text rendering, and queries the font metrics to determine the
 * character width used by string measurement functions.
 *
 * @param connection Pointer to the XCB connection
 * @param font_name  Name of the font to load, or @c NULL to use default
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 on failure
 *
 * @note If @p font_name is @c NULL or empty, the default font "fixed"
 *       is used
 * @note Reinitialization with the same connection is skipped when the
 *       renderer is already initialized
 * @note Complexity: @e O(1)
 */
int text_renderer_init(xcb_connection_t *connection,
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
 * @a text_renderer_init to select colors appropriate for the drawing
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
 * @param gc         Graphics context to use, or @c XCB_NONE to use default
 * @param x          X coordinate of the text baseline
 * @param y          Y coordinate of the text baseline
 * @param text       Null-terminated string to draw
 *
 * @note Strings longer than 255 bytes are truncated to fit the XCB text
 *       drawing request limit
 * @note Complexity: @e O(n), where @e n is the length of the text
 */
void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        int16_t x, int16_t y, const char *text);

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
 *        whichever font @c text_renderer_init last selected
 *
 * Together with @a text_font_descent, lets a caller vertically center
 * or top/bottom-align a line of text against a known pixel height
 * without needing its own hardcoded assumption about font metrics: the
 * Y coordinate @a text_draw_string expects is the baseline, so placing
 * text @p top pixels from the top of a box of height @p box_h means
 * passing @c top @c + @a text_font_ascent() as that Y coordinate.
 *
 * @return Font ascent in pixels; a small built-in default before the
 *         first successful @a text_renderer_init call
 *
 * @note Complexity: @e O(1)
 */
int16_t text_font_ascent(void);

/**
 * @brief Pixels the baseline sits above the bottom of a line, for
 *        whichever font @a text_renderer_init last selected
 *
 * @return Font descent in pixels; a small built-in default before the
 *         first successful @a text_renderer_init call
 *
 * @note Complexity: @e O(1)
 */
int16_t text_font_descent(void);


#endif  /* ! RENDER_TEXT_H */
