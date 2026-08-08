/**
 * @file menu/dialog.h
 *
 * @brief Generic dialog infrastructure for confirm and message dialogs
 *
 * Provides two reusable modal dialog types:
 *
 * - **Confirm dialog:** presents a prompt and two choices (cancel and
 *   confirm).  The caller supplies all visible text at show time; no
 *   compiled-in strings are used.  An optional callback is invoked when
 *   the user activates the confirm button.
 *
 * - **Message dialog:** shows a read-only message with an alert level
 *   (info, warning, or error) and a single dismiss button.  Intended
 *   for future use; no callback is needed.
 *
 * Both dialog types share the centering helper and the same design.
 *
 * @note Only one instance of each type may be visible at a time
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_DIALOG_H
#define MENU_DIALOG_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/* Shared layout constants */
/** Bottom padding below buttons (pixels) */
#define DIALOG_PAD_BOTTOM (8u)

/** Minimum dialog width (pixels) */
#define DIALOG_MIN_W (220u)

/** Minimum dialog height (pixels) */
#define DIALOG_MIN_H (90u)

/** Minimum button width (pixels) */
#define DIALOG_BTN_MIN_W (60u)

/** Prompt baseline position from dialog top (pixels) */
#define DIALOG_PROMPT_BASELINE_Y (22u)

/** Vertical gap between prompt baseline and button top (pixels) */
#define DIALOG_PROMPT_TO_BTN_GAP (34u)

/** Maximum text length for dialogs (prompt + level prefix) */
#define DIALOG_TEXT_MAX_LEN (256u)

/**
 * @brief Maximum number of wrapped lines the message dialog will show
 *
 * A hard cap so a pathologically long message cannot grow the dialog
 * (and the fixed-size arrays backing it) without bound; text past
 * this many lines is simply not shown.  Raised well past what a
 * confirmation or alert message would ever need so the same dialog
 * can also serve the keyboard-shortcuts list (see
 * 'menu/dialog/shortcuts.h'), whose grouped categories run to a few
 * dozen lines.
 */
#define DIALOG_MSG_MAX_LINES (40u)

/** Maximum length of the raw message text before wrapping, prefix
 *  included; see 'DIALOG_MSG_MAX_LINES' for why this is larger than a
 *  short confirmation or alert message alone would need */
#define DIALOG_MSG_RAW_MAX_LEN (2048u)

/** Maximum length of a single already-wrapped line */
#define DIALOG_MSG_LINE_MAX_LEN (160u)

/**
 * @brief Pixel width the message text wraps at
 *
 * The dialog itself can still end up narrower than this: it is sized
 * to the widest line the wrap actually produces, not to this bound
 * directly, so a short one-line message stays compact.
 */
#define DIALOG_MSG_WRAP_WIDTH (480u)

/** Vertical gap between wrapped message lines, in pixels */
#define DIALOG_MSG_LINE_GAP (4u)

/** Label for the dismiss button in the message dialog */
#define DIALOG_MSG_LABEL_OK ("[ OK ]")

/** Prefix for info-level messages */
#define DIALOG_MSG_PREFIX_INFO ("[i] ")

/** Prefix for warning-level messages */
#define DIALOG_MSG_PREFIX_WARNING ("[!] ")

/** Prefix for error-level messages */
#define DIALOG_MSG_PREFIX_ERROR ("[X] ")

/** Maximum bytes read from the @c fortune command's output */
#define DIALOG_FORTUNE_MAX_LEN (1024u)

/** Shown instead when @c fortune is missing or produces no output;
 *  deliberately overwrought and archaic, per its whole point being a
 *  small joke rather than a plain error message */
#define DIALOG_FORTUNE_FALLBACK_MSG \
    "Alack! The oracle 'fortune' abideth not upon this machine, " \
    "wherefore no wisdom of the ancients may this day be divined. " \
    "Prithee, entreat thy package steward with an incantation " \
    "such as 'sudo apt install fortune-mod' (or whate'er charm " \
    "thy distribution demandeth), that the sages of yore might " \
    "once more speak through this humble dialog."


/**
 * @brief Alert level for the message dialog
 *
 * Controls the level prefix shown alongside the message (see
 * @c DIALOG_MSG_PREFIX_INFO and its siblings in dialog.c) and the
 * accent color drawn from the active theme.
 */
typedef enum {
    MENU_MSG_LEVEL_NONE,    /**< No prefix at all; for content that is
                                  not itself an alert or a notice, such
                                  as the keyboard-shortcuts list or the
                                  output of the @c fortune easter egg
                                  (see @c dialog_shortcuts_show and
                                  @c dialog_fortune_show) */
    MENU_MSG_LEVEL_INFO,    /**< Informational message */
    MENU_MSG_LEVEL_WARNING, /**< Non-critical warning */
    MENU_MSG_LEVEL_ERROR    /**< Error or critical condition */
} menu_msg_level_e;


/* Centering helper */
/**
 * @brief Compute centered coordinates for a dialog on a surface
 *
 * @param surface Surface where the dialog will be shown
 * @param width   Dialog width in pixels
 * @param height  Dialog height in pixels
 * @param out_x   Receives centered X coordinate
 * @param out_y   Receives centered Y coordinate
 *
 * @note Complexity: @e O(1)
 */
void menu_dialog_center(const surface_td *surface,
        uint16_t width, uint16_t height, int16_t *out_x, int16_t *out_y);

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
 *
 * @param connection XCB connection
 * @param x          Pointer X coordinate relative to the dialog
 * @param y          Pointer Y coordinate relative to the dialog
 *
 * @return @c true if a button was activated, @c false otherwise
 *
 * @note Complexity: @e O(1)
 */
bool menu_confirm_dialog_handle_click(xcb_connection_t *connection,
        int x, int y);

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

/* Message dialog (informational, single dismiss button) */
/**
 * @brief Open the message dialog centered on the screen
 *
 * Creates and maps a modal dialog window showing @p message with an
 * alert-level prefix.  A single "OK" button dismisses the dialog.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (theme colors and font)
 * @param message    Null-terminated message text
 * @param level      Alert severity level
 *
 * @note Complexity: @e O(n), where @e n is the message text length
 */
void menu_message_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *message, menu_msg_level_e level);
/**
 * @brief Destroy the currently visible message dialog
 *
 * Closes the dialog window if it is open and resets all internal state.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_close(xcb_connection_t *connection);

/**
 * @brief Open a message dialog showing the output of the @c fortune
 *        command, or an invitation to install it if unavailable
 *
 * Runs @c fortune and shows its output through
 * @c menu_message_dialog_show, which already wraps arbitrarily long
 * or multi-line text across several lines, so a long fortune (or a
 * one-word-short one) is handled the same general way any other
 * message would be.  If @c fortune is not installed or exits without
 * producing output, shows a fixed, deliberately archaic message
 * inviting the person to install it instead of silently doing
 * nothing.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (theme colors and font)
 *
 * @note Complexity: @e O(n), where @e n is the length of the
 *       @c fortune output actually read (bounded, see
 *       @c DIALOG_FORTUNE_MAX_LEN in dialog.c)
 */
void dialog_fortune_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);

/**
 * @brief Repaint the message dialog from current state
 *
 * Called from the expose handler.  Redraws the message text and the
 * dismiss button.
 *
 * @param connection XCB connection
 * @param config     Active configuration (theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_repaint(xcb_connection_t *connection,
        const config_td *config);

/**
 * @brief Handle a mouse click inside the message dialog
 *
 * Closes the dialog when the pointer lands inside the "OK" button.
 * Clicks outside the button are not handled.
 *
 * @param connection XCB connection
 * @param x          Pointer X coordinate relative to the dialog
 * @param y          Pointer Y coordinate relative to the dialog
 *
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_handle_click(xcb_connection_t *connection,
        int x, int y);

/**
 * @brief Query whether the message dialog is currently visible
 *
 * @return @c true when the dialog window exists
 *
 * @note Complexity: @e O(1)
 */
bool menu_message_dialog_is_open(void);

/**
 * @brief Return the message dialog window identifier
 *
 * @return The dialog's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t menu_message_dialog_window(void);


#endif  /* ! MENU_DIALOG_H */
