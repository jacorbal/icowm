/**
 * @file defs/dialog.h
 *
 * @brief Sizing, wrapping, and label defaults for the message-based
 *        dialogs (@c menu/dialog/message.c, @c quit.c, @c fortune.c)
 *
 * @ingroup defs
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_DIALOG_H
#define DEFS_DIALOG_H


/**
 * @brief Maximum number of wrapped lines the message dialog can ever
 *        hold, backing the fixed-size @c lines array
 *
 * A hard cap purely to bound the fixed-size arrays this backs, not
 * a display limit: unlike before scrolling existed, text past this many
 * lines is not simply cut off from view, it never enters @p lines at
 * all, so scrolling could not reach it regardless of how generous the
 * dialog's monitor-height cap is (see @c menu/dialog/message.c's
 * @c s_message_compute_layout for that separate, on-screen limit).
 *
 * Kept comfortably above what the keyboard-shortcuts list (see
 * @c menu/dialog/shortcuts.h) actually produces, plus headroom for it
 * growing over time, rather than tuned tightly to today's exact line
 * count.  Smaller under @c COMPACT (see @c defs/compact.h), though not
 * as small as restricted-memory mode's shorter shortcuts list alone
 * would allow.
 *
 * This build flag and @c -M are meant to be combined but are not
 * strictly coupled, so this still comfortably covers the full,
 * non-restricted shortcuts list in case a compact build is ever run
 * without @c -M too.
 */
#ifdef COMPACT
#define DIALOG_MSG_MAX_LINES (64u)
#else
#define DIALOG_MSG_MAX_LINES (96u)
#endif

/**
 * @brief Maximum length of the raw message text before wrapping, prefix
 *        included
 *
 * @see @c DIALOG_MSG_MAX_LINES for the same headroom reasoning, compact
 *      build included
 */
#ifdef COMPACT
#define DIALOG_MSG_RAW_MAX_LENGTH (2560u)
#else
#define DIALOG_MSG_RAW_MAX_LENGTH (4096u)
#endif

/**
 * @brief Maximum length of a single already-wrapped line
 */
#define DIALOG_MSG_LINE_MAX_LENGTH (160u)

/**
 * @brief Byte count the message text wraps at
 *
 * A fixed byte count, independent of the active font, so wrapping
 * behaves the same regardless of which font the theme configures: 80
 * columns is the conventional plain-text wrap width.  Counts bytes, not
 * Unicode codepoints; a multi-byte UTF-8 character (e.g., an accented
 * letter) counts as more than one toward this limit, so text containing
 * them wraps at fewer than 80 visual characters.
 *
 * Every other length constant in this file
 * (@c DIALOG_MSG_RAW_MAX_LENGTH, @c DIALOG_MSG_LINE_MAX_LENGTH,
 * @c DIALOG_QUIT_PROMPT_MAX_LENGTH, @c DIALOG_FORTUNE_MAX_LENGTH) is
 * a byte count for the same reason: none of this project's text
 * handling decodes UTF-8 sequences.  The dialog itself can still end up
 * narrower than this, for it is sized to the widest line the wrap
 * actually produces (in pixels, via @a menu_draw_measure), not to this
 * bound directly, so a short one-line message stays compact.
 */
#define DIALOG_MSG_WRAP_LENGTH (80u)

/** Vertical gap between wrapped message lines, in pixels */
#define DIALOG_MSG_LINE_GAP (4u)

/** Prefix for info-level messages */
#define DIALOG_MSG_PREFIX_INFO "[i] "

/** Prefix for warning-level messages */
#define DIALOG_MSG_PREFIX_WARNING "[!] "

/** Prefix for error-level messages */
#define DIALOG_MSG_PREFIX_ERROR "[X] "

/** Maximum prompt buffer length for the quit-confirmation dialog */
#define DIALOG_QUIT_PROMPT_MAX_LENGTH (128u)

/**
 * @brief How long a dialog's newly clicked button stays visibly
 *        selected before a mouse click actually closes/accepts the
 *        dialog it belongs to
 *
 * Shared by every dialog that defers its click-triggered close this
 * way.  Closing on the very same repaint that shows the new selection
 * would not give a user any real chance to perceive it, since screen
 * updates and human perception both take a moment neither the repaint
 * nor the close itself can shortcut.
 *
 * @see @c menu/dialog/defer.h
 */
#define DIALOG_CLICK_FEEDBACK_DELAY_MS (150)

/**
 * @brief Seconds the RandR output-profile confirm dialog (see
 *        @c menu/dialog/rrsafe.c) waits before automatically reverting
 *        a just-applied @c randr.json
 *
 * The cancel/revert button is the dialog's default selection, so this
 * timing out has the same effect as a user pressing it themselves:
 * long enough to actually read the prompt and react even if the new
 * profile left the screen in an awkward state, short enough not to sit
 * there indefinitely if nobody is watching (e.g., a reload triggered
 * from a script).
 *
 * @see @a menu_config_dialog_show
 */
#define DIALOG_RANDR_CONFIRM_TIMEOUT_SECONDS (10u)

/** Maximum bytes read from the @c fortune command's output */
#define DIALOG_FORTUNE_MAX_LENGTH (1024u)


#endif  /* ! DEFS_DIALOG_H */
