/**
 * @file menu/dialog/quit.h
 *
 * @brief Quit-confirmation dialog (text content and wrapper)
 *
 * Defines only the visible text used by the quit-confirmation dialog:
 * the prompt format string, the cancel and exit button labels, and the
 * maximum prompt buffer size.  All layout and rendering logic lives in
 * the generic confirm-dialog infrastructure (@c menu/confirm.h).
 *
 * The thin wrapper function declared here is the public entry point
 * for opening the quit-confirmation dialog; it delegates directly to
 * the generic confirm-dialog API, which also handles every subsequent
 * interaction (repaint, click, selection, close) for it.
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

#ifndef MENU_DIALOG_QUIT_H
#define MENU_DIALOG_QUIT_H


/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/dialog.h>
#include <defs/uistr.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/* Thin wrapper interface */
/**
 * @brief Open the quit-confirmation dialog centered on the screen
 *
 * Builds the prompt from @c QUIT_DIALOG_PROMPT_FMT and the window
 * manager name, then delegates to @a menu_confirm_dialog_show.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void dialog_quit_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config);


#endif  /* ! MENU_DIALOG_QUIT_H */
