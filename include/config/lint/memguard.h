/**
 * @file config/lint/memguard.h
 *
 * @brief 'memguard.json' schema
 *
 * One of the files @c config/lint/ is made of, each holding the
 * schema tables for one configuration file.  The linter's
 * traversal lives in @c config/lint.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_LINT_MEMGUARD_H
#define CONFIG_LINT_MEMGUARD_H

/* Local includes */
#include <config/lint/internal.h>
#include <config/lint/common.h>

/** How many keys @c s_schema_memguard holds */
#define CONFIG_LINT_MEMGUARD_KEYS (8u)

extern const config_lint_key_td
    s_schema_memguard[CONFIG_LINT_MEMGUARD_KEYS];

#endif  /* ! CONFIG_LINT_MEMGUARD_H */
