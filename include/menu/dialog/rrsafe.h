/**
 * @file menu/dialog/rrsafe.h
 *
 * @brief RandR output-profile confirm dialog (thin wrapper)
 *
 * Shown after @a wm_action_config_reload applies a changed
 * @c randr.json, offering a chance to revert it before it sticks.
 * A generic @a menu_confirm_dialog with a countdown, whose cancel
 * button (selected by default) undoes the change if picked or if the
 * countdown itself elapses; the confirm button just keeps it.  Unlike
 * @c menu/dialog/quit.h, only the show entry point is needed here:
 * keyboard, mouse, and repaint handling for the underlying dialog are
 * already generic (could be interesting to check as well all the
 * family, @a menu_confirm_dialog_is_open and its siblings, called
 * directly from @c input/kbd/event.c, @c input/mouse/event/press.c,
 * and
 * @c handler/expose.c), with nothing left that needs a dialog-specific
 * wrapper of its own.
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

#ifndef MENU_DIALOG_RRSAFE_H
#define MENU_DIALOG_RRSAFE_H


/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <types/handles.h>


/**
 * @brief Open the RandR output-profile confirm dialog centered on
 *        the screen
 *
 * Delegates to @a menu_confirm_dialog_show with a fixed prompt, the
 * cancel button reverting the just-applied profile if picked or left to
 * the countdown, and the confirm button doing nothing beyond closing
 * the dialog, since the profile is already live by the time this is
 * shown.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (theme colors and font)
 *
 * @note Complexity: @e O(1)
 *
 * @see @c STR_DIALOG_RANDR_CONFIRM_PROMPT and
 *      @a surface_action_revert_randr_profiles
 */
void dialog_rrsafe_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config);


#endif  /* ! MENU_DIALOG_RRSAFE_H */
