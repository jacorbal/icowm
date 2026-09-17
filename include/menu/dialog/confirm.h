/**
 * @file menu/dialog/confirm.h
 *
 * @brief Two-button confirm/cancel modal dialog, with an optional
 *        countdown that automatically activates whichever button is
 *        currently selected once it elapses
 *
 * Presents a prompt and two choices (cancel & confirm).  The caller
 * supplies all visible text at show time; no compiled-in strings are
 * used.  Optional callbacks are invoked when the user (or, with
 * a countdown running, the dialog itself once it elapses) activates
 * either button; both are stored internally from
 * @a menu_confirm_dialog_show, so nothing else in this file needs to be
 * told which one is in play again.
 *
 * @note Only one instance may be visible at a time
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

#ifndef MENU_CONFIRM_H
#define MENU_CONFIRM_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/dialog.h>

/* Project includes */
#include <types/handles.h>


/**
 * @brief Open the confirm dialog centered on the screen
 *
 * Creates and maps a modal dialog window with @p prompt, a cancel
 * button labeled @p cancel_label, and a confirm button labeled
 * @p confirm_label.  The cancel button is selected by default.  Any
 * previously open confirm dialog is ignored (only one instance is
 * allowed at a time).
 *
 * @param connection    XCB connection
 * @param stage         Stage on which to center the dialog
 * @param config        Active configuration (theme colors and font)
 * @param prompt        Null-terminated prompt text
 * @param cancel_label  Null-terminated cancel button label
 * @param confirm_label Null-terminated confirm button label
 * @param on_confirm    Optional callback invoked when the confirm
 *                        button is activated; receives the XCB
 *                        connection (may be null)
 * @param on_cancel Optional callback invoked when the cancel
 *                        button is activated, either by a user
 *                        clicking it, selecting it and pressing
 *                        @c Enter / @c Space, pressing @c Escape (which
 *                        always acts as "cancel", regardless of which
 *                        button happens to be selected at the time), or
 *                        @p timeout_seconds elapsing while it is the
 *                        selected one; receives the XCB connection (may
 *                        be null)
 * @param timeout_seconds Seconds before the dialog automatically acts
 *                        as though its currently-selected button were
 *                        activated, showing a live countdown under the
 *                        prompt while it runs; @c 0 disables this
 *                        entirely, leaving the dialog open indefinitely
 *                        exactly as before this parameter existed
 *
 * @note Complexity: @e O(n), where @e n is the total text length
 */
void menu_confirm_dialog_show(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        const char *prompt,
        const char *cancel_label, const char *confirm_label,
        void (*on_confirm)(xcb_connection_t *),
        void (*on_cancel)(xcb_connection_t *),
        uint32_t timeout_seconds);

/**
 * @brief Repaint the confirm dialog from current state
 *
 * Called from the expose handler.  Redraws the prompt, both buttons,
 * and the countdown line if a timeout is running.
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
 * button rectangle.  Clicks outside both buttons return a confounded,
 * bloody @c false.  Clicking the button that was not already selected
 * moves the visual selection there and repaints immediately, but the
 * actual close/accept is deferred a short moment rather than happening
 * on this same call, so the new selection is visible before the dialog
 * goes away instead of the click reading as though it landed on the
 * wrong spot.
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint of the
 *                   newly selected button; the click is still
 *                   handled if null, just without that repaint
 * @param x Pointer X coordinate relative to the dialog
 * @param y Pointer Y coordinate relative to the dialog
 *
 * @return @c true if a button was activated
 *
 * @note Complexity: @e O(1)
 *
 * @see @a menu_confirm_dialog_tick
 */
bool menu_confirm_dialog_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y);

/**
 * @brief Milliseconds until the next thing this dialog needs the main
 *        loop to wake it up for becomes due
 *
 * Folds together three independent timers this dialog can have running
 * at once: a pending click-triggered close/accept, the once-a-second
 * countdown repaint while a timeout is running, and the timeout's
 * final expiry.  For the main loop to fold into its @p poll timeout
 * computation, the same way @a popup_ms_remaining and similar already
 * are.
 *
 * @return Milliseconds remaining until the soonest of these (never
 *         negative), or @c -1 if none is currently pending
 *
 * @note Complexity: @e O(1)
 *
 * @see @a menu_confirm_dialog_handle_click
 */
int menu_confirm_dialog_ms_remaining(void);

/**
 * @brief Service whichever timer belonging to this dialog is due
 *
 * Performs a pending click-triggered close/accept once its short delay
 * elapses; repaints to advance the visible countdown once a second
 * while a timeout is running.
 *
 * Calls @a menu_confirm_dialog_cancel once the timeout itself fully
 * elapses.  A safe, cheap no-op when nothing this dialog owns is due
 * yet, including when no dialog is open at all.
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the once-a-second
 *                   countdown repaint; that repaint is skipped (the
 *                   rest of this function still runs) if null
 *
 * @note Complexity: @e O(1)
 *
 * @see @a menu_confirm_dialog_ms_remaining
 */
void menu_confirm_dialog_tick(xcb_connection_t *connection,
        const config_td *config);

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
 * Closes the dialog, then calls whichever of @c on_confirm /
 * @c on_cancel (given to @a menu_confirm_dialog_show) matches the
 * button that was selected, if that one is non-null.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_accept(xcb_connection_t *connection);

/**
 * @brief Cancel the dialog regardless of which button is currently
 *        selected
 *
 * Closes the dialog and calls @c on_cancel (given to
 * @a menu_confirm_dialog_show), if non-null, the same as selecting the
 * cancel button and activating it would, without disturbing which
 * button was actually selected first (irrelevant, since this always
 * takes the cancel path).  Meant for @c Escape and for a countdown
 * timeout, both of which are "back out of this" regardless of whatever
 * a user may have tabbed the selection to in the meantime.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void menu_confirm_dialog_cancel(xcb_connection_t *connection);

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
