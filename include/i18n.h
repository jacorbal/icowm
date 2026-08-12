/**
 * @file i18n.h
 *
 * @brief GUI text translation: the @c _() lookup macro, and one-time
 *        setup
 *
 * Covers dialog messages, buttons, and menu labels only (every @c
 * STR_* constant in @c defs/uistr.h): the command line (@c main.c,
 * @c icowm-msg) and @c LOGGER_* messages are deliberately never
 * translated, the same distinction @c defs/uistr.h itself already
 * draws, since both serve a diagnostic, not an end-user, audience.
 *
 * @c _(STR_FOO) is how a @c STR_* constant becomes a translated
 * string at the point it is actually used; @c defs/uistr.h itself
 * holds only the original, untranslated English text, wrapping
 * nothing on its own.
 *
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef I18N_H
#define I18N_H


/* System includes */
#include <libintl.h>


/**
 * @brief Look up one GUI string's own translation for the current
 *        locale
 *
 * A thin wrapper around @c gettext itself, named the short,
 * conventional way every gettext-using project names it, so a
 * translated string reads as a normal argument at its own call site
 * (@c _(STR_FOO)) rather than a visibly separate lookup step.
 *
 * @param string The original, untranslated string (in practice,
 *               always one of the @c STR_* constants @c defs/
 *               uistr.h defines)
 */
#define _(string) gettext(string)


/* Public interface */
/**
 * @brief Set up GUI text translation for the rest of this process's
 *        own lifetime
 *
 * Reads the locale the environment already names (@c setlocale with
 * an empty string, the standard way to defer to @c LANG/@c LC_ALL
 * rather than hardcoding one), then points this domain (@c
 * I18N_DOMAIN, "default") at the compiled @c .mo catalogs this
 * project ships under @c I18N_LOCALE_DIR.  Both are supplied by the
 * Makefile itself as build-time @c -D macros, the same way @c
 * PROJECT_VERSION and the rest of this project's own identity
 * constants already are, since @c I18N_LOCALE_DIR in particular is
 * an absolute path baked in from @c $(CURDIR) at compile time, not
 * something a header could name on its own.  Every @c _(STR_FOO)
 * call anywhere in the process looks a translation up through
 * exactly this setup; called once, at startup, well before the
 * first dialog or menu could possibly be shown.
 *
 * A missing catalog for the current locale, or no locale support
 * installed on the system at all, is never a startup failure: @c
 * gettext itself already falls back to the original English text
 * when it cannot find or load a translation, silently and
 * correctly, so this has nothing further to check or report.
 *
 * @note Complexity: @e O(1)
 */
void i18n_init(void);


#endif  /* ! I18N_H */
