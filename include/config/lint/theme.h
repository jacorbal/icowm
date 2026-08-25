/**
 * @file config/lint/theme.h
 *
 * @brief Theme schema, used by every file under 'themes/'
 *
 * One of the files @c config/lint/ is made of, each holding the
 * schema tables for one configuration file.  The linter's own
 * traversal lives in @c config/lint.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_LINT_THEME_H
#define CONFIG_LINT_THEME_H

/* Local includes */
#include <config/lint/internal.h>

/** How many keys @c s_schema_theme holds */
#define CONFIG_LINT_THEME_KEYS (13u)

extern const config_lint_key_td
    s_schema_theme[CONFIG_LINT_THEME_KEYS];

#endif  /* ! CONFIG_LINT_THEME_H */
