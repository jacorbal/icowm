/**
 * @file config/lint/a11y.h
 *
 * @brief 'a11y.json' schema
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

#ifndef CONFIG_LINT_A11Y_H
#define CONFIG_LINT_A11Y_H


/* Local includes */
#include <config/lint/internal.h>

/** How many keys @c s_schema_a11y holds */
#define CONFIG_LINT_A11Y_KEYS (4u)

extern const config_lint_key_td
    s_schema_a11y[CONFIG_LINT_A11Y_KEYS];


#endif  /* ! CONFIG_LINT_A11Y_H */
