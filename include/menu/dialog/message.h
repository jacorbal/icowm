/**
 * @file menu/dialog/message.h
 *
 * @brief Read-only message modal dialog with a scrollable body and
 *        a single dismiss button
 *
 * Shows a message with an alert level (info, warning, or error) and
 * a single "OK" button.  Also backs the keyboard-shortcuts list (cfr.
 * @c menu/dialog/shortcuts.h) and the @c fortune easter egg (cfr.
 * @c menu/dialog/fortune.h), both of which pass @c MENU_MSG_LEVEL_NONE
 * for content that is not itself an alert.
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

#ifndef MENU_DIALOG_MESSAGE_H
#define MENU_DIALOG_MESSAGE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>

/* Default initial values */
#include <defs/dialog.h>


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
                                 output of the @c fortune easter egg */
    MENU_MSG_LEVEL_INFO,    /**< Informational message */
    MENU_MSG_LEVEL_WARNING, /**< Non-critical warning */
    MENU_MSG_LEVEL_ERROR    /**< Error or critical condition */
} menu_msg_level_e;


/**
 * @brief One row of a two-column message dialog
 *
 * Which of the two strings is present decides what the row is, so no
 * separate kind is carried alongside them:
 *
 * - both given: an aligned pair, @p label at the left margin and
 *   @p value at a column shared by every pair in the dialog
 * - @p label alone: a heading or a line of prose, drawn at the left
 *   margin across the full width
 * - neither: a blank line, for spacing between groups
 *
 * @note A @p value too long for its column is wrapped onto further
 *       lines that begin at that same column, so a row reads as one
 *       entry however many lines it takes
 */
struct dialog_pair_s {
    const char *label;      /**< Left column, or the whole line */
    const char *value;      /**< Right column, or null */
};


/**
 * @brief Append one blank line, for visual separation between groups,
 *        to a caller-built array of dialog pairs
 *
 * Shared by every dialog that gathers its own @c dialog_pair_s array
 * before handing it to @a menu_message_dialog_show_pairs, so a heading,
 * a group of rows, another heading and so on can be spaced apart the
 * same way wherever they are built.
 *
 * @param pairs Array being filled in
 * @param count Rows filled in so far; incremented by this call unless
 *              @p pairs is already full
 *
 * @note Silently does nothing once @p count reaches
 *       @c DIALOG_MSG_MAX_LINES, the same as every other row-adding
 *       helper built around this array
 * @note Complexity: @e O(1)
 */
void dialog_pair_append_blank(struct dialog_pair_s *pairs,
        uint8_t *count);

/**
 * @brief Open the message dialog centered on the screen
 *
 * Creates and maps a modal dialog window showing @p message with an
 * alert-level prefix.  A single "OK" button dismisses the dialog.  The
 * dialog is capped to a fraction of its target monitor's height and
 * scrolls when the message does not fit.
 *
 * @param connection XCB connection
 * @param stage      Stage on which to center the dialog
 * @param config     Active configuration (theme colors and font)
 * @param message    Null-terminated message text
 * @param level      Alert severity level
 *
 * @note Complexity: @e O(n), where @e n is the message text length
 *
 * @see @a menu_message_dialog_scroll
 */
void menu_message_dialog_show(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        const char *message, menu_msg_level_e level);

/**
 * @brief Show a message dialog whose content is a list of label and
 *        value pairs
 *
 * The same dialog @a menu_message_dialog_show opens, and with the same
 * scrolling, monitor cap and "OK" button, differing only in that the
 * values line up in a column of their own rather than following their
 * labels inline.
 *
 * That column sits half a line height past the widest label, measured
 * in pixels with the label font rather than counted in characters, so
 * it lands correctly whichever of the two font backends is in use and
 * whatever language the labels were translated into.  It is capped at
 * two fifths of the dialog's width, past which a label is truncated
 * rather than left to squeeze the values out of the dialog.
 *
 * @param connection XCB connection
 * @param stage      Stage to show the dialog on
 * @param config     Active configuration
 * @param pairs      Rows to show, in order
 * @param pair_count How many of them
 * @param level      Alert level, which decides the icon and the
 *                   initial button selection
 *
 * @note A row whose value wraps takes as many lines as it needs, and
 *       those count toward the scrolling the same as any other
 * @note Complexity: @e O(n * c), where @e n is @p pair_count and @e c
 *       the longest value's length, from measuring and wrapping each
 */
void menu_message_dialog_show_pairs(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        const struct dialog_pair_s *pairs, size_t pair_count,
        menu_msg_level_e level);

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
 * Selects the "OK" button and repaints when the pointer lands inside
 * it, then defers the actual close for shortly after.  So that newly
 * selected state is visible for a moment first, the same reasoning
 * @a menu_confirm_dialog_handle_click already applies to its two
 * buttons.  Clicks outside the button are not handled.
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint
 * @param x          Pointer X coordinate relative to the dialog
 * @param y          Pointer Y coordinate relative to the dialog
 *
 * @note Complexity: @e O(1)
 *
 * @see @a menu_dialog_defer_schedule in @c menu/dialog/defer.h
 */
void menu_message_dialog_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y);

/**
 * @brief Milliseconds remaining until a pending click-triggered close
 *        becomes due
 *
 * For the main loop to fold into its @c poll timeout computation,
 * the same way @a popup_ms_remaining and similar already are.
 *
 * @return Milliseconds remaining (never negative), or @c -1 if none is
 *         currently pending
 *
 * @note Complexity: @e O(1)
 */
int menu_message_dialog_ms_remaining(void);

/**
 * @brief Close the message dialog if a click-triggered close is pending
 *        and its deadline has arrived
 *
 * @param connection XCB connection
 *
 * @note A safe, cheap no-op when nothing is pending, including when no
 *       dialog is open at all
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_tick(xcb_connection_t *connection);

/**
 * @brief Query whether the currently visible message dialog requires
 *        the "OK" button to be explicitly selected before @c Enter or
 *        @c Space can activate it
 *
 * @c true for @c MENU_MSG_LEVEL_WARNING and @c MENU_MSG_LEVEL_ERROR;
 * @c false for every other level, and when no dialog is open at all.
 * See @a menu_message_dialog_show for why.
 *
 * @return @c true if the dialog currently open requires this
 *
 * @note Complexity: @e O(1)
 */
bool menu_message_dialog_requires_selection(void);

/**
 * @brief Query whether the "OK" button is currently selected
 *
 * Always @c true for a dialog that does not require selection at all.
 * Starts @c false for one that does, until
 * @a menu_message_dialog_select_ok is called.
 *
 * @return @c true if the button is currently selected
 *
 * @note Complexity: @e O(1)
 *
 * @see @a menu_message_dialog_requires_selection
 */
bool menu_message_dialog_ok_selected(void);

/**
 * @brief Select the "OK" button and repaint
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint
 *
 * @note A no-op if it is already selected, or if the dialog is not open
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_select_ok(xcb_connection_t *connection,
        const config_td *config);

/**
 * @brief Scroll the message dialog's text by a number of lines
 *
 * @param connection XCB connection
 * @param config     Active configuration, for the repaint
 * @param delta      Lines to scroll by; negative scrolls up (toward the
 *                   start), positive scrolls down (toward the end)
 *
 * @note A no-op when the whole message already fits without scrolling,
 *       when @p delta would not actually move the current scroll
 *       position (already at either end), or when the dialog is not
 *       open
 * @note Repaints immediately when there's motion
 * @note Parameter @p delta is lamped to the valid range, so passing an
 *       arbitrarily large magnitude is a safe way to scroll all the way
 *       to either end in one call
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
