/**
 * @file menu/dialog/quit.h
 *
 * @brief Quit-confirmation dialog (text content and wrappers)
 *
 * Defines only the visible text used by the quit-confirmation dialog:
 * the prompt format string, the cancel and exit button labels, and the
 * maximum prompt buffer size.  All layout and rendering logic lives in
 * the generic confirm-dialog infrastructure (@c menu/dialog.h).
 *
 * The thin wrapper functions declared here are the public entry points
 * for opening, interacting with, and closing the quit-confirmation
 * dialog; they delegate directly to the generic confirm-dialog API.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_DIALOG_QUIT_H
#define MENU_DIALOG_QUIT_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/uistr.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/** Maximum prompt buffer length */
#define DIALOG_QUIT_PROMPT_MAX_LEN (128u)


/* Thin wrapper interface */
/**
 * @brief Open the quit-confirmation dialog centered on the screen
 *
 * Builds the prompt from @c QUIT_DIALOG_PROMPT_FMT and the window
 * manager name, then delegates to @c menu_confirm_dialog_show.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void dialog_quit_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config);

/**
 * @brief Destroy the currently visible quit-confirmation dialog
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void dialog_quit_close(xcb_connection_t *connection);

/**
 * @brief Repaint the quit-confirmation dialog from current state
 *
 * @param connection XCB connection
 * @param config     Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void dialog_quit_repaint(xcb_connection_t *connection,
        const config_td *config);

/**
 * @brief Handle a mouse click inside the quit-confirmation dialog
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint of a newly
 *                    selected button; see @c
 *                    menu_confirm_dialog_handle_click
 * @param x          Pointer X coordinate relative to the dialog
 * @param y          Pointer Y coordinate relative to the dialog
 *
 * @return @c true if a button was activated, @c false otherwise
 *
 * @note Complexity: @e O(1)
 */
bool dialog_quit_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y);

/**
 * @brief Move selection to the next button (wraps around)
 *
 * @note Complexity: @e O(1)
 */
void dialog_quit_toggle_selection(void);

/**
 * @brief Activate the currently selected button
 *
 * If the exit button is selected, requests a clean window manager
 * shutdown.  Otherwise only closes the dialog.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void dialog_quit_accept(xcb_connection_t *connection);

/**
 * @brief Query whether the quit-confirmation dialog is currently
 *        visible
 *
 * @return @c true when the dialog window exists
 *
 * @note Complexity: @e O(1)
 */
bool dialog_quit_is_open(void);

/**
 * @brief Return the quit-confirmation dialog window identifier
 *
 * @return The dialog's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t dialog_quit_window(void);


#endif  /* ! MENU_DIALOG_QUIT_H */
