/**
 * @file menu/draw.h
 *
 * @brief Shared drawing primitives for window manager menu windows
 *
 * Low-level helpers used by the cycle menu and the info popup to
 * paint row backgrounds and text inside an XCB window.  This module
 * has no knowledge of menu state; it only performs raw drawing
 * operations.
 *
 * @ingroup menu
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_DRAW_H
#define MENU_DRAW_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>


/* Public interface */
/**
 * @brief Fill a horizontal row rectangle with a solid color
 *
 * Draws a filled rectangle covering the full width @p w of the menu
 * window at the vertical position @p row_y with height @p row_h.
 *
 * @param connection XCB connection
 * @param window     Target drawable window
 * @param color      Background fill color (pixel value)
 * @param row_y      Top-left Y of the row
 * @param row_h      Height of the row in pixels
 * @param w          Width of the row in pixels
 *
 * @note Complexity: @e O(1)
 */
void menu_draw_row_bg(xcb_connection_t *connection,
        xcb_window_t window,
        uint32_t color, int16_t row_y, uint16_t row_h, uint16_t w);

/**
 * @brief Draw a text label at the given position
 *
 * Renders @p text at (@p x, @p y) inside @p window using the
 * previously configured text renderer colors.  The caller must have
 * called @c text_renderer_init and @c text_renderer_set_color before
 * invoking this function.
 *
 * @param connection XCB connection
 * @param window     Target drawable window
 * @param x          Left margin in pixels
 * @param y          Baseline Y position in pixels
 * @param text       Null-terminated text to render
 *
 * @note Complexity: @e O(n), where @e n is the number of glyphs in
 *       @p text
 */
void menu_draw_label(xcb_connection_t *connection,
        xcb_window_t window, int16_t x, int16_t y, const char *text);

/**
 * @brief Measure the pixel width of a text string
 *
 * Wraps @c text_measure_string to provide a menu-module-local entry
 * point without requiring menu modules to include @c render/text.h
 * directly.
 *
 * @param text Null-terminated text to measure
 *
 * @return Width in pixels
 *
 * @note Complexity: @e O(n), where @e n is the number of glyphs
 */
uint16_t menu_draw_measure(const char *text);

/**
 * @brief Truncate a buffer in place, one character at a time, until
 *        it measures no wider than @p max_w
 *
 * A cheap linear shrink rather than a binary search, appropriate for
 * the short labels menus deal with (window titles, desktop names):
 * the difference is not measurable at that length.  Left untouched
 * if it already fits, or if it is empty.
 *
 * @param buf   Null-terminated buffer to truncate in place
 * @param max_w Maximum width in pixels the text may measure
 *
 * @note Complexity: @e O(n), where @e n is the length of @p buf
 */
void menu_draw_truncate(char *buf, uint16_t max_w);


#endif  /* ! MENU_DRAW_H */
