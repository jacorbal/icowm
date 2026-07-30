/**
 * @file menu/confirm.h
 *
 * @brief Quit-confirmation dialog interface
 *
 * Declares the functions for opening, closing, repainting, and
 * interacting with the modal confirmation dialog that is shown before
 * the window manager exits.  The dialog presents an exit-confirmation
 * message using the configured WM name and two choices: "Cancel"
 * (default, confirmed with 'Enter') and "Exit" (triggers a clean
 * shutdown).  Pressing 'Escape' or activating "Cancel" closes the
 * dialog without exiting.  All dialog state is private to the
 * implementation.
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


/* Confirm dialog values */
/** Quit prompt rendered in the confirmation dialog */
#define CONFIRM_PROMPT_FMT ("Are you sure you want to exit %s?")

/** Cancel button label text */
#define CONFIRM_LABEL_CANCEL ("[ Cancel ]")

/** Exit button label text */
#define CONFIRM_LABEL_EXIT ("[ Exit ]")

/** Maximum prompt buffer length */
#define CONFIRM_PROMPT_MAX_LEN (128u)

/** Horizontal dialog padding (pixels) */
#define CONFIRM_PAD_X (12u)

/** Bottom padding below buttons (pixels) */
#define CONFIRM_PAD_BOTTOM (8u)

/** Minimum dialog width (pixels) */
#define CONFIRM_MIN_W (300u)

/** Minimum dialog height (pixels) */
#define CONFIRM_MIN_H (90u)

/** Minimum button width (pixels) */
#define CONFIRM_BTN_MIN_W (80u)

/** Button height (pixels) */
#define CONFIRM_BTN_H (26u)

/** Gap between the two buttons (pixels) */
#define CONFIRM_BTN_GAP (8u)

/** Horizontal padding between button border and label (pixels) */
#define CONFIRM_BTN_LABEL_PAD_X (6u)

/** Prompt baseline position from top (pixels) */
#define CONFIRM_PROMPT_BASELINE_Y (22u)

/** Vertical gap between prompt baseline and button top (pixels) */
#define CONFIRM_PROMPT_TO_BTN_GAP (34u)

/** Baseline offset for button labels from button top (pixels) */
#define CONFIRM_BTN_LABEL_BASELINE_OFFSET (17u)


/* Public interface */
/**
 * @brief Open the quit-confirmation dialog centered on the screen
 *
 * Creates and maps a dialog window showing the exit prompt with
 * "Cancel" (selected by default) and "Exit" choices.  Any previously
 * open confirmation dialog is closed first.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param cfg        Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void confirm_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *cfg);

/**
 * @brief Destroy the currently visible confirmation dialog
 *
 * Closes the dialog window if it is open and resets all internal state.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void confirm_close(xcb_connection_t *connection);

/**
 * @brief Repaint the confirmation dialog from current state
 *
 * Called from the expose handler when the dialog receives an expose
 * event.  Redraws the prompt text and both buttons, highlighting the
 * currently selected one.
 *
 * @param connection XCB connection
 * @param cfg        Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void confirm_repaint(xcb_connection_t *connection,
        const config_td *cfg);

/**
 * @brief Move selection to the next button (wraps around)
 *
 * Cycles the highlighted button from "Cancel" to "Exit" and back.
 *
 * @note Complexity: @e O(1)
 */
void confirm_toggle_selection(void);

/**
 * @brief Activate the currently selected button
 *
 * If "Exit" is selected, requests a clean window manager shutdown and
 * closes the dialog.  If "Cancel" is selected, only closes the dialog.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void confirm_accept(xcb_connection_t *connection);

/**
 * @brief Query whether the confirmation dialog is currently visible
 *
 * @return @c true when the dialog window exists
 *
 * @note Complexity: @e O(1)
 */
bool confirm_is_open(void);

/**
 * @brief Return the confirmation dialog window identifier
 *
 * @return The dialog's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t confirm_window(void);


#endif  /* ! MENU_CONFIRM_H */
