/**
 * @file render/glyph.h
 *
 * @brief TrueType/OpenType text rendering fallback, via xcb-render,
 *        FreeType2, and fontconfig
 *
 * Used by @c render/text.c as the fallback path for a font name that
 * does not resolve to an X core font: fontconfig picks a matching font
 * file, FreeType2 rasterizes each glyph on demand, and @c xcb-render
 * (through the @c xcb-render-util convenience library, which builds the
 * @c CompositeGlyphs wire format correctly, since hand-encoding it is
 * a well known trouble spot even among X developers) composites the
 * text onto the target drawable.
 *
 * This is not a general-purpose public API, as every drawing/measuring
 * entry point below exists purely as @c render/text.c's own fallback
 * path for a font name that does not resolve to an X core font, and is
 * not meant to be called directly from anywhere else in the project.
 * @c glyph_utf8_next is, maybe, the one exception: a plain UTF-8
 * decoder with nothing glyph-rendering-specific about it, reused by
 * @c render/text.c's own X core font path too, so a Latin-1-range
 * codepoint can be drawn correctly through @c xcb_image_text_8 (which,
 * unlike this file's own path, has no multi-byte text support of its
 * own at all).  It seems to work...
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

#ifndef RENDER_GLYPH_H
#define RENDER_GLYPH_H


/* System includes */
#include <stddef.h>     /* size_t */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Types includes */
#include <types/pair.h>


/* Public interface */
/**
 * @brief Decode the next UTF-8 codepoint from @p text
 *
 * A plain UTF-8-to-codepoint decoder, with nothing about it specific to
 * glyph rendering; lives here only because @c render/text.c's own
 * X core font path (@c xcb_image_text_8, single-byte only) needs the
 * exact same decoding this file's own glyph path already had, to turn
 * a Latin-1-range codepoint back into the one byte that font encoding
 * actually expects, rather than passing UTF-8's own multi-byte encoding
 * straight through.
 *
 * Malformed sequences are treated permissively: an invalid leading byte
 * is returned as its own Latin-1 codepoint rather than rejecting the
 * whole string, since this reads UI text, not untrusted input, and
 * a best-effort result reads better than nothing at all.
 *
 * @param text  Null-terminated UTF-8 string
 * @param index Byte offset to start decoding from; advanced past the
 *              consumed bytes on return
 *
 * @return The decoded codepoint, or 0 at the end of the string
 *
 * @note Complexity: @e O(1)
 */
uint32_t glyph_utf8_next(const char *text, size_t *index);

/**
 * @brief Try to initialize the glyph renderer for the given font
 *        description
 *
 * Resolves @p font_name through fontconfig (its own native pattern
 * syntax, e.g., "DejaVu Sans:bold:size=10", or a plain family name) and
 * loads the matched font file with FreeType2.
 *
 * @param connection Pointer to the XCB connection
 * @param font_name  Font description string
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 if no font matched or the render setup failed
 *
 * @note Reinitialization with the same connection and font name is
 *       skipped when the renderer is already initialized for both
 * @note Complexity: @e O(1)
 */
int glyph_renderer_init(xcb_connection_t *connection,
        const char *font_name);

/**
 * @brief Destroy the glyph renderer's resources
 *
 * Frees the glyph set, FreeType face and library, and every
 * @c xcb-render object owned by the renderer, then resets its internal
 * state.
 *
 * @note Complexity: @e O(1)
 */
void glyph_renderer_destroy(void);

/**
 * @brief Update the foreground and background colors used to draw text
 *
 * @param fg Foreground pixel value (text color), @c 0x00RRGGBB
 * @param bg Background pixel value, currently unused (glyphs are
 *           composited with an alpha mask, not opaque cells, so there
 *           is no background to paint)
 *
 * @note Has no effect when the renderer is not yet initialized
 * @note Complexity: @e O(1)
 */
void glyph_renderer_set_color(uint32_t fg, uint32_t bg);

/**
 * @brief Draw a UTF-8 string at the specified baseline position
 *
 * Glyphs for any codepoint not yet seen by this renderer are rasterized
 * and uploaded to the glyph set on demand, then cached for later calls.
 *
 * @param connection Pointer to the XCB connection
 * @param drawable   Target drawable where the text will be drawn
 * @param pos        Position of the text baseline
 * @param text       Null-terminated UTF-8 string to draw
 *
 * @note Complexity: @e O(n), where @e n is the length of the text
 */
void glyph_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, struct position_s pos, const char *text);

/**
 * @brief Measure the rendered width of a UTF-8 string
 *
 * Sums each codepoint's advance width from the current font's own
 * metrics, unlike the X core font path this is a fallback for, which
 * only approximates width from a single fixed character width;
 * a proportional TrueType/OpenType font needs the real per-glyph value.
 *
 * @param text Null-terminated UTF-8 string to measure, or @c NULL
 *
 * @return String width in pixels, or @c UINT16_MAX on overflow
 *
 * @note Complexity: @e O(n), where @e n is the length of the text
 */
uint16_t glyph_measure_string(const char *text);

/**
 * @brief Pixels the baseline sits below the top of a line
 *
 * @return Font ascent in pixels; a small built-in default before the
 *         first successful @a glyph_renderer_init call
 *
 * @note Complexity: @e O(1)
 */
int16_t glyph_font_ascent(void);

/**
 * @brief Pixels the baseline sits above the bottom of a line
 *
 * @return Font descent in pixels; a small built-in default before the
 *         first successful @a glyph_renderer_init call
 *
 * @note Complexity: @e O(1)
 */
int16_t glyph_font_descent(void);


#endif  /* ! RENDER_GLYPH_H */
