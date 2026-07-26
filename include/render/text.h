/**
 * @file render/text.h
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


#ifndef RENDER_TEXT_H
#define RENDER_TEXT_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>


/* Public interface */
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
 * @return 0 on success, -1 on failure
 *
 * @note If @p font_name is @c NULL or empty, the default font
 *       @c "fixed" is used
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
 * of the internal graphics context.  To be called after
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
uint16_t text_measure_string(const char *text);


#endif  /* ! RENDER_TEXT_H */
