/**
 * @file menu/dialog/message.h
 *
 * @brief Read-only message modal dialog with a scrollable body and a
 *        single dismiss button
 *
 * Shows a message with an alert level (info, warning, or error) and a
 * single "OK" button.  Also backs the keyboard-shortcuts list (see
 * @c menu/dialog/shortcuts.h) and the @c fortune easter egg (see
 * @c menu/dialog/fortune.h), both of which pass @c MENU_MSG_LEVEL_NONE
 * for content that is not itself an alert.
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

#ifndef MENU_DIALOG_MESSAGE_H
#define MENU_DIALOG_MESSAGE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/**
 * @brief Maximum number of wrapped lines the message dialog can ever
 *        hold, backing the fixed-size @c lines array
 *
 * A hard cap purely to bound the fixed-size arrays this backs, not a
 * display limit: unlike before scrolling existed, text past this many
 * lines is not simply cut off from view, it never enters @c lines at
 * all, so scrolling could not reach it regardless of how generous the
 * dialog's own monitor-height cap is (see @c menu/dialog/message.c's
 * @c s_message_compute_layout for that separate, on-screen limit).
 * Kept comfortably above what the keyboard-shortcuts list (see
 * @c menu/dialog/shortcuts.h) actually produces, plus headroom for it
 * growing over time, rather than tuned tightly to today's exact line
 * count.  Smaller under @c LOWMEM (see
 * @c defs/lowmem.h), though not as small as restricted-memory mode's
 * own shorter shortcuts list alone would allow: this build flag and
 * @c -M are meant to be combined but are not strictly coupled, so
 * this still comfortably covers the full, non-restricted shortcuts
 * list in case a low-memory build is ever run without @c -M too.
 */
#ifdef LOWMEM
#define DIALOG_MSG_MAX_LINES (64u)
#else
#define DIALOG_MSG_MAX_LINES (96u)
#endif

/** Maximum length of the raw message text before wrapping, prefix
 *  included; see @c DIALOG_MSG_MAX_LINES for the same headroom
 *  reasoning, low-memory build included */
#ifdef LOWMEM
#define DIALOG_MSG_RAW_MAX_LEN (2560u)
#else
#define DIALOG_MSG_RAW_MAX_LEN (4096u)
#endif

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


/**
 * @brief Alert level for the message dialog
 *
 * Controls the level prefix shown alongside the message (see
 * @c DIALOG_MSG_PREFIX_INFO and its siblings above) and the accent
 * color drawn from the active theme.
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


/**
 * @brief Open the message dialog centered on the screen
 *
 * Creates and maps a modal dialog window showing @p message with an
 * alert-level prefix.  A single "OK" button dismisses the dialog.
 * The dialog is capped to a fraction of its target monitor's height
 * and scrolls when the message does not fit; see
 * @c menu_message_dialog_scroll.
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
 * @brief Query whether the currently visible message dialog requires
 *        the "OK" button to be explicitly selected before Enter or
 *        Space can activate it
 *
 * @c true for @c MENU_MSG_LEVEL_WARNING and @c MENU_MSG_LEVEL_ERROR;
 * @c false for every other level, and when no dialog is open at all.
 * See @c menu_message_dialog_show for why.
 *
 * @return @c true if the dialog currently open requires this
 *
 * @note Complexity: @e O(1)
 */
bool menu_message_dialog_requires_selection(void);

/**
 * @brief Query whether the "OK" button is currently selected
 *
 * Always @c true for a dialog that does not require selection at all
 * (see @c menu_message_dialog_requires_selection); starts @c false
 * for one that does, until @c menu_message_dialog_select_ok is
 * called.
 *
 * @return @c true if the button is currently selected
 *
 * @note Complexity: @e O(1)
 */
bool menu_message_dialog_ok_selected(void);

/**
 * @brief Select the "OK" button and repaint
 *
 * A no-op if it is already selected, or if the dialog is not open.
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint
 *
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_select_ok(xcb_connection_t *connection,
        const config_td *config);

/**
 * @brief Scroll the message dialog's text by a number of lines
 *
 * A no-op when the whole message already fits without scrolling, when
 * @p delta would not actually move the current scroll position
 * (already at either end), or when the dialog is not open.  Repaints
 * immediately when it does move.
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint
 * @param delta      Lines to scroll by; negative scrolls up (toward
 *                   the start), positive scrolls down (toward the
 *                   end).  Clamped to the valid range, so passing an
 *                   arbitrarily large magnitude is a safe way to
 *                   scroll all the way to either end in one call
 *
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_scroll(xcb_connection_t *connection,
        const config_td *config, int32_t delta);

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


#endif  /* ! MENU_DIALOG_MESSAGE_H */
