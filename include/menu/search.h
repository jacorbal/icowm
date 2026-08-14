/**
 * @file menu/search.h
 *
 * @brief Fuzzy window-search widget interface
 *
 * Declares the public API for a floating, centered search bar that
 * lists every managed window across every desktop, live-filtered by
 * fuzzy subsequence matching as the user types.  All widget state is
 * private to the implementation.
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

#ifndef MENU_SEARCH_H
#define MENU_SEARCH_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Initialize the window-search widget
 *
 * Collects every focusable, non-skip-taskbar client across every
 * desktop of @p surface, creates the floating centered widget, and
 * clears any previously typed query.  Closes and reopens cleanly if
 * already open.
 *
 * @param surfaces   All managed surfaces (passed to focus_apply on
 *                   confirm)
 * @param connection XCB connection
 * @param surface    Surface on which to center the widget
 * @param cfg        Active configuration (for theme colors and the
 *                   @c show-pixmaps flag)
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 *       across every desktop of @p surface
 */
void search_init(list_td *surfaces, xcb_connection_t *connection,
        surface_td *surface, const config_td *cfg);

/**
 * @brief Destroy the window-search widget and restore previous focus
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void search_destroy(xcb_connection_t *connection);

/**
 * @brief Query whether the window-search widget is currently open
 *
 * @return @c true when the widget window exists
 *
 * @note Complexity: @e O(1)
 */
bool search_is_open(void);

/**
 * @brief Return the window-search widget's own X window identifier
 *
 * @return The widget's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t search_window(void);

/**
 * @brief Check whether a given window belongs to the search widget
 *
 * @param win Window to test
 *
 * @return @c true when @p win is the search widget's own window
 *
 * @note Complexity: @e O(1)
 */
bool search_owns_window(xcb_window_t win);

/**
 * @brief Handle a key press while the search widget is open
 *
 * A printable character appends to the query and re-filters the
 * match list; @c Backspace removes the last character; @c Up / @c
 * Down, or @c Tab / @c Shift+Tab, move the selection (@c Tab acting
 * like @c Down, @c Shift+Tab like @c Up, so the widget stays
 * navigable with one hand the same way a plain @c Tab-completion
 * list would); @c Return confirms the selected entry (switching to
 * its desktop, restoring it if iconified or shaded, and focusing
 * and raising it); @c Escape cancels and restores previous focus.
 * Every other key is ignored.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (passed to focus_apply on
 *                   confirm)
 * @param keysym     Key symbol of the pressed key
 * @param state      Modifier mask of the pressed key, with locking
 *                   bits already stripped, used only to tell a
 *                   plain @c Tab from @c Shift+Tab
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of collected
 *       clients (re-filtering happens on every character)
 */
void search_handle_keypress(xcb_connection_t *connection,
        list_td *surfaces, xcb_keysym_t keysym, uint16_t state,
        const config_td *cfg);

/**
 * @brief Handle a button-press event inside the search widget
 *
 * A click on a result row selects and confirms it, same as pressing
 * @c Return with that row selected; a click elsewhere in the widget
 * (the text-entry bar) does nothing but is still consumed.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (passed to focus_apply on
 *                   confirm)
 * @param x          Pointer X, relative to the widget window
 * @param y          Pointer Y, relative to the widget window
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(1)
 */
void search_handle_click(xcb_connection_t *connection,
        list_td *surfaces, int16_t x, int16_t y, const config_td *cfg);

/**
 * @brief Handle a pointer-motion event inside the search widget
 *
 * Moves the selection to whichever row the pointer is currently over
 * and repaints immediately, so hovering visibly previews a selection
 * the same way keyboard navigation does rather than only updating
 * internal state until some unrelated event happens to redraw next.
 *
 * @param x Pointer X, relative to the widget window
 * @param y Pointer Y, relative to the widget window
 *
 * @note Complexity: @e O(1)
 */
void search_handle_motion(int16_t x, int16_t y);

/**
 * @brief Repaint the search widget
 *
 * @param connection XCB connection
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of visible
 *       result rows
 */
void search_draw(xcb_connection_t *connection, const config_td *cfg);


#endif  /* ! MENU_SEARCH_H */
