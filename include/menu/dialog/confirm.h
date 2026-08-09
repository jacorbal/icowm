/**
 * @file menu/dialog/confirm.h
 *
 * @brief Two-button confirm/cancel modal dialog
 *
 * Presents a prompt and two choices (cancel and confirm).  The caller
 * supplies all visible text at show time; no compiled-in strings are
 * used.  An optional callback is invoked when the user activates the
 * confirm button.
 *
 * @note Only one instance may be visible at a time
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONFIRM_H
#define MENU_CONFIRM_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/**
 * @brief Open the confirm dialog centered on the screen
 *
 * Creates and maps a modal dialog window with @p prompt, a cancel
 * button labelled @p cancel_label, and a confirm button labelled
 * @p confirm_label.  The cancel button is selected by default.  Any
 * previously open confirm dialog is ignored (only one instance is
 * allowed at a time).
 *
 * @param connection    XCB connection
 * @param surface       Surface on which to center the dialog
 * @param config        Active configuration (theme colors and font)
 * @param prompt        Null-terminated prompt text
 * @param cancel_label  Null-terminated cancel button label
 * @param confirm_label Null-terminated confirm button label
 * @param on_confirm    Optional callback invoked when the confirm
 *                      button is activated; receives the XCB connection
 *                      (may be null)
 *
 * @note Complexity: @e O(n), where @e n is the total text length
 */
void menu_confirm_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *prompt,
        const char *cancel_label, const char *confirm_label,
        void (*on_confirm)(xcb_connection_t *));

/**
 * @brief Destroy the currently visible confirm dialog
 *
 * Closes the dialog window if it is open and resets all internal state.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_close(xcb_connection_t *connection);

/**
 * @brief Repaint the confirm dialog from current state
 *
 * Called from the expose handler.  Redraws the prompt and both buttons,
 * highlighting the currently selected one.
 *
 * @param connection XCB connection
 * @param config     Active configuration (theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_repaint(xcb_connection_t *connection,
        const config_td *config);

/**
 * @brief Handle a mouse click inside the confirm dialog
 *
 * Activates the clicked button when the pointer lands inside either
 * button rectangle.  Clicks outside both buttons return @c false.
 * Clicking the button that was not already selected moves the visual
 * selection there and repaints immediately, but the actual
 * close/accept is deferred a short moment (see
 * @c menu_confirm_dialog_tick) rather than happening on this same
 * call, so the new selection is visible before the dialog goes away
 * instead of the click reading as though it landed on the wrong spot.
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint of the
 *                    newly selected button; the click is still
 *                    handled, just without that repaint, if @c NULL
 * @param x          Pointer X coordinate relative to the dialog
 * @param y          Pointer Y coordinate relative to the dialog
 *
 * @return @c true if a button was activated, @c false otherwise
 *
 * @note Complexity: @e O(1)
 */
bool menu_confirm_dialog_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y);

/**
 * @brief Milliseconds until a pending click-triggered close/accept
 *        (see @c menu_confirm_dialog_handle_click) becomes due
 *
 * For the main loop to fold into its own @c poll timeout computation,
 * the same way @c popup_ms_remaining and similar already are.
 *
 * @return Milliseconds remaining (never negative), or -1 if no such
 *         action is currently pending
 *
 * @note Complexity: @e O(1)
 */
int menu_confirm_dialog_ms_remaining(void);

/**
 * @brief Perform the deferred click-triggered close/accept, if one is
 *        pending and due
 *
 * @param connection XCB connection
 *
 * @note No-op if nothing is pending, or pending but not yet due
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_tick(xcb_connection_t *connection);

/**
 * @brief Move selection to the next button (wraps around)
 *
 * Cycles the highlighted button from cancel to confirm and back.
 *
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_toggle_selection(void);

/**
 * @brief Activate the currently selected button
 *
 * Closes the dialog.  If the confirm button was selected, @p on_confirm
 * is called after closing, if non-null.
 *
 * @param connection XCB connection
 * @param on_confirm Optional callback invoked when the confirm button
 *                   is activated; receives the XCB connection
 *
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_accept(xcb_connection_t *connection,
        void (*on_confirm)(xcb_connection_t *));

/**
 * @brief Query whether the confirm dialog is currently visible
 *
 * @return @c true when the dialog window exists
 *
 * @note Complexity: @e O(1)
 */
bool menu_confirm_dialog_is_open(void);

/**
 * @brief Return the confirm dialog window identifier
 *
 * @return The dialog's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t menu_confirm_dialog_window(void);


#endif  /* ! MENU_CONFIRM_H */
