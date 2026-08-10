/**
 * @file menu/dialog/shortcuts.h
 *
 * @brief Keyboard-shortcuts list dialog
 *
 * A single entry point, @c dialog_shortcuts_show, that formats every
 * currently active keyboard binding into a grouped, human-readable
 * list and shows it through the generic message dialog (@c
 * menu/dialog/message.h), the same one @c dialog_fortune_show uses.
 * The text is read directly from @c config->bindings.keyboard, so it
 * always reflects whichever bindings actually took effect, config
 * file typos and all, rather than a separately maintained description
 * of what the defaults are supposed to be.
 *
 * Repetitive categories (the ten go-to-desktop bindings, the several
 * window move/resize directions, cycling) are collapsed to one line
 * each rather than listed individually, both for readability and to
 * keep the dialog within a height that fits a typical screen; see
 * @c DIALOG_MSG_MAX_LINES in menu/dialog/message.h.
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

#ifndef MENU_DIALOG_SHORTCUTS_H
#define MENU_DIALOG_SHORTCUTS_H


/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Open a message dialog listing every currently active
 *        keyboard shortcut
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (bindings, theme colors and
 *                   font)
 *
 * @note Complexity: @e O(1), the number of bindings is fixed at
 *       compile time
 */
void dialog_shortcuts_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);


#endif  /* ! MENU_DIALOG_SHORTCUTS_H */
