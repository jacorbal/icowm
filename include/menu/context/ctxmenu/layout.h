/**
 * @file menu/context/ctxmenu/layout.h
 *
 * @brief Row geometry and hit-testing for a context menu
 *
 * @note This header is private to @c menu/context/ctxmenu/ (and
 *       @c menu/context/ctxmenu.c, which orchestrates every ctxmenu/
 *       file) and must not be included outside of them
 *
 * @ingroup menu_context
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONTEXT_CTXMENU_LAYOUT_H
#define MENU_CONTEXT_CTXMENU_LAYOUT_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Compute the pixel Y of the top edge of an entry by index
 *
 * Reads @p state->entry_top_y when the cache was successfully allocated
 * by @c ctxmenu_show, giving @e O(1) lookup; falls back to an @e O(n)
 * walk over @p state->entries otherwise (e.g., @c calloc failed at
 * menu-open time).
 *
 * @param state Menu state
 * @param idx   Entry index (0-based)
 *
 * @return Y coordinate (pixels) relative to the menu window top
 *
 * @note Complexity: @e O(1) with the cache, @e O(n) without it, where
 *       @e n is @p idx
 */
int ctxmenu_entry_top_y(const ctxmenu_state_td *state, int idx);

/**
 * @brief Compute per-row Y offsets and the total menu height
 *
 * Walks the entries once, filling @p state->entry_top_y (when
 * allocated) with the top-Y pixel offset of each row, so that
 * @c ctxmenu_entry_top_y and @c ctxmenu_entry_at_y can look rows up
 * directly afterwards instead of re-walking the entry array on every
 * call.  Also returns the total height, sparing a separate pass over
 * the same entries.
 *
 * @param state Menu state; @p entries and @p entry_count must already
 *              be set
 *
 * @return Total menu height in pixels
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
uint16_t ctxmenu_layout_build(ctxmenu_state_td *state);

/**
 * @brief Compute the pixel width required to display all menu entries
 *
 * Iterates over all entries and measures each label, adding space for
 * the left padding, the submenu indicator, and, for an entry with an
 * associated @c icon_window, its application icon.
 *
 * @param connection  XCB connection
 * @param entries     Array of menu entries
 * @param entry_count Number of entries
 * @param config      Active configuration
 *
 * @return Required width in pixels, at least @c WM_CTXMENU_MIN_WIDTH
 *
 * @note Complexity: @e O(n), where @e n is @p entry_count
 */
uint16_t ctxmenu_width_compute(xcb_connection_t *connection,
        const ctxmenu_entry_td *entries, int entry_count,
        const config_td *config);

/**
 * @brief Return the row index at the given pixel Y, or -1 if none
 *
 * Binary-searches @p state->entry_top_y for the last row whose top
 * offset is @c <= @p y, then checks @p y still falls within that row's
 * height, giving @e O(log n) lookup when the cache is available.
 * Falls back to an @e O(n) linear walk over @p state->entries when it
 * is not (e.g., @c calloc failed at menu-open time).
 *
 * @param state Menu state
 * @param y     Pixel Y relative to menu window
 *
 * @return Entry index, or -1 if @p y is outside all rows
 *
 * @note Complexity: @e O(log n) with the cache, @e O(n) without it,
 *       where @e n is @p state->entry_count
 */
int ctxmenu_entry_at_y(const ctxmenu_state_td *state, int y);


#endif  /* ! MENU_CONTEXT_CTXMENU_LAYOUT_H */
