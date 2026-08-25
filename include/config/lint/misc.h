/**
 * @file config/lint/misc.h
 *
 * @brief Schemas for the files with a single level of their own
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

#ifndef CONFIG_LINT_MISC_H
#define CONFIG_LINT_MISC_H

/* Local includes */
#include <config/lint/internal.h>

/** How many keys @c s_schema_menu holds */
#define CONFIG_LINT_MENU_KEYS (1u)

extern const config_lint_key_td
    s_schema_menu[CONFIG_LINT_MENU_KEYS];

/** How many keys @c s_schema_randr holds */
#define CONFIG_LINT_RANDR_KEYS (2u)

extern const config_lint_key_td
    s_schema_randr[CONFIG_LINT_RANDR_KEYS];

/** How many keys @c s_schema_rules holds */
#define CONFIG_LINT_RULES_KEYS (1u)

extern const config_lint_key_td
    s_schema_rules[CONFIG_LINT_RULES_KEYS];

/** How many keys @c s_schema_session holds */
#define CONFIG_LINT_SESSION_KEYS (3u)

extern const config_lint_key_td
    s_schema_session[CONFIG_LINT_SESSION_KEYS];

#endif  /* ! CONFIG_LINT_MISC_H */
