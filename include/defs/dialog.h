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
#define DIALOG_MSG_RAW_MAX_LENGTH (2560u)
#else
#define DIALOG_MSG_RAW_MAX_LENGTH (4096u)
#endif

/** Maximum length of a single already-wrapped line */
#define DIALOG_MSG_LINE_MAX_LENGTH (160u)

/**
 * @brief Byte count the message text wraps at
 *
 * A fixed byte count, independent of the active font, so wrapping
 * behaves the same regardless of which font the theme configures: 80
 * columns is the conventional plain-text wrap width.  Counts bytes,
 * not Unicode codepoints: a multi-byte UTF-8 character (e.g., an
 * accented letter) counts as more than one toward this limit, so text
 * containing them wraps at fewer than 80 visual characters.  Every
 * other length constant in this file (@c DIALOG_MSG_RAW_MAX_LENGTH,
 * @c DIALOG_MSG_LINE_MAX_LENGTH, @c DIALOG_QUIT_PROMPT_MAX_LENGTH,
 * @c DIALOG_FORTUNE_MAX_LENGTH) is a byte count for the same reason:
 * none of this project's text handling decodes UTF-8 sequences.  The
 * dialog itself can still end up narrower than this: it is sized to
 * the widest line the wrap actually produces (in pixels, via
 * @c menu_draw_measure), not to this bound directly, so a short
 * one-line message stays compact.
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
 * @brief Seconds the RandR output-profile confirm dialog (see
 *        @c menu/dialog/rrsafe.c) waits before automatically
 *        reverting a just-applied @c randr.json
 *
 * The cancel/revert button is the dialog's default selection (see
 * @c menu_confirm_dialog_show), so this timing out has the same
 * effect as a person pressing it themselves: long enough to actually
 * read the prompt and react even if the new profile left the screen
 * in an awkward state, short enough not to sit there indefinitely if
 * nobody is watching (e.g., a reload triggered from a script).
 */
#define DIALOG_RANDR_CONFIRM_TIMEOUT_SECONDS (10u)

/** Maximum bytes read from the @c fortune command's output */
#define DIALOG_FORTUNE_MAX_LENGTH (1024u)

/** Shown instead when @c fortune is missing or produces no output;
 *  deliberately overwrought and archaic, per its whole point being a
 *  small joke rather than a plain error message */
#define DIALOG_FORTUNE_FALLBACK_MSG \
    "Alack!  The oracle 'fortune' abideth not upon this machine, " \
    "wherefore no wisdom of the ancients may this day be divined.  " \
    "Prithee, entreat thy package steward with an incantation " \
    "such as 'sudo apt install fortune-mod' (or whate'er charm " \
    "thy distribution demandeth), that the sages of yore might " \
    "once more speak through this humble dialog."


#endif  /* ! DEFS_DIALOG_H */
