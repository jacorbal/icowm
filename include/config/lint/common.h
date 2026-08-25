/**
 * @file config/lint/common.h
 *
 * @brief Schemas both 'config.json' and 'memguard.json' use
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

#ifndef CONFIG_LINT_COMMON_H
#define CONFIG_LINT_COMMON_H

/* Local includes */
#include <config/lint/internal.h>

/** How many keys @c s_schema_desktops_margins holds */
#define CONFIG_LINT_DESKTOPS_MARGINS_KEYS (4u)

extern const config_lint_key_td
    s_schema_desktops_margins[CONFIG_LINT_DESKTOPS_MARGINS_KEYS];

/** How many keys @c s_schema_programs holds */
#define CONFIG_LINT_PROGRAMS_KEYS (5u)

extern const config_lint_key_td
    s_schema_programs[CONFIG_LINT_PROGRAMS_KEYS];

/** How many keys @c s_schema_prompt holds */
#define CONFIG_LINT_PROMPT_KEYS (1u)

extern const config_lint_key_td
    s_schema_prompt[CONFIG_LINT_PROMPT_KEYS];

/** How many keys @c s_schema_shutdown holds */
#define CONFIG_LINT_SHUTDOWN_KEYS (2u)

extern const config_lint_key_td
    s_schema_shutdown[CONFIG_LINT_SHUTDOWN_KEYS];

/** How many keys @c s_schema_systray holds */
#define CONFIG_LINT_SYSTRAY_KEYS (11u)

extern const config_lint_key_td
    s_schema_systray[CONFIG_LINT_SYSTRAY_KEYS];

/** How many keys @c s_schema_windows_focus holds */
#define CONFIG_LINT_WINDOWS_FOCUS_KEYS (3u)

extern const config_lint_key_td
    s_schema_windows_focus[CONFIG_LINT_WINDOWS_FOCUS_KEYS];

/** How many keys @c s_schema_windows_placement holds */
#define CONFIG_LINT_WINDOWS_PLACEMENT_KEYS (3u)

extern const config_lint_key_td
    s_schema_windows_placement[CONFIG_LINT_WINDOWS_PLACEMENT_KEYS];

#endif  /* ! CONFIG_LINT_COMMON_H */
