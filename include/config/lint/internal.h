/**
 * @file config/lint/internal.h
 *
 * @brief Types the linter's schema tables are built from
 *
 * Shared by every file under @c config/lint/, each of which holds the
 * schema for one configuration file.  Nothing outside the linter
 * needs these; @c config/lint.h is the public face.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_LINT_INTERNAL_H
#define CONFIG_LINT_INTERNAL_H

/* System includes */
#include <stdbool.h>
#include <stddef.h>


/** One key a schema recognizes at a given nesting level */
typedef struct config_lint_key_s {
    const char *name;
    const struct config_lint_key_s *children; /**< 'NULL' for a leaf,
                                                   or for a subtree
                                                   deliberately left
                                                   opaque (see
                                                   config/lint.h) */
    size_t children_count;
} config_lint_key_td;

/** One configuration file this linter knows how to check */
typedef struct {
    const char *filename;
    const config_lint_key_td *schema;
    size_t schema_count;
    bool required;
} config_lint_file_spec_td;

#endif  /* ! CONFIG_LINT_INTERNAL_H */
