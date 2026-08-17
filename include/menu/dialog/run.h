/**
 * @file menu/dialog/run.h
 *
 * @brief Built-in run-box: a single centered text field for typing
 *        and launching a command directly, with no search, no
 *        listing, and no cache
 *
 * An alternative to @c KEYBIND_LAUNCH_LAUNCHER's own default of
 * spawning @p config_base_s.programs.launcher (an external program such
 * as, e.g., @c dmenu or @c gmrun), consulted only when
 * @p config_base_s.programs.use_builtin_launcher is set for where that
 * choice is made.  Deliberately does none of what a real launcher like
 * those does (no fuzzy matching over installed programs, no history, no
 * cache of any kind).
 *
 * Typing @c Return attempts to run whatever was typed exactly as given,
 * showing an informational dialog (never a blocking warning or error,
 * so a mistyped command never derails the person any further than
 * necessary) if it could not be found, or closing the box on success.
 *
 * Visually distinguished from the fuzzy window-search widget
 * (@c menu/search.h), which this shares its general shape with (a
 * single centered text field, the same theme colors), by its own "Run:"
 * prompt and its own, narrower fixed width.
 *
 * @note Only one instance may be visible at a time
 *
 * @see @a ik_handle_launch in @c input/kbd/interact.c
 * @see @c defs/run.h for both this and the widget's own dimensions
 *
 * @ingroup menu_dialog
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_DIALOG_RUN_H
#define MENU_DIALOG_RUN_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/**
 * @brief Open the run-box, centered on @p surface
 *
 * Any previously open run-box is replaced.  Grabs the keyboard for as
 * long as the box stays open, the same way the fuzzy window-search
 * widget's own @a search_init already does.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the box
 * @param cfg        Active configuration (theme colors and font)
 *
 * @note No-op if any pointer argument is null
 * @note Complexity: @e O(1)
 */
void run_init(xcb_connection_t *connection, surface_td *surface,
        const config_td *cfg);

/**
 * @brief Close the run-box and restore whichever window had input focus
 *        before it opened
 *
 * @param connection XCB connection
 *
 * @note A no-op if the run-box is not currently open
 * @note Complexity: @e O(1)
 */
void run_destroy(xcb_connection_t *connection);

/**
 * @brief Query whether the run-box is currently open
 *
 * @return @c true while a run-box window exists
 *
 * @note Complexity: @e O(1)
 */
bool run_is_open(void);

/**
 * @brief Query whether @p win is the run-box's own window
 *
 * @param win Window to test
 *
 * @return @c true if @p win is the currently open run-box
 *
 * @note Complexity: @e O(1)
 */
bool run_owns_window(xcb_window_t win);

/**
 * @brief Handle a key press while the run-box is open
 *
 * - @c Escape closes the box without running anything.
 * - @c Return attempts to run the typed command via
 *   @a desktop_action_process_launch (@c desktop.h), showing
 *   @c STR_RUN_COMMAND_NOT_FOUND_FMT (@c defs/uistr.h) as an
 *   informational dialog if it could not be found, and closing the box
 *   either way once launched successfully.
 *   @c Backspace deletes the last typed character.
 * - Any other printable character is appended.
 *
 * @param connection XCB connection
 * @param surface    Surface the box is centered on, needed to resolve
 *                   the desktop @a desktop_action_process_launch itself
 *                   requires
 * @param keysym     Key symbol of the pressed key
 * @param cfg        Active configuration
 *
 * @note A no-op if the run-box is not currently open
 * @note Complexity: @e O(1)
 */
void run_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym, const config_td *cfg);

/**
 * @brief Repaint the run-box
 *
 * @param connection XCB connection
 * @param cfg        Active configuration
 *
 * @note A no-op if the run-box is not currently open
 * @note Complexity: @e O(1)
 */
void run_draw(xcb_connection_t *connection, const config_td *cfg);


#endif  /* ! MENU_DIALOG_RUN_H */
