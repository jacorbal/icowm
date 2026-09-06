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


/**
 * @brief How an array-valued key's own elements get checked
 *
 * @c CONFIG_LINT_ARRAY_NONE is the default a plain @c {"name", NULL,
 * 0u} entry gets, unchanged from before this existed: an array value
 * is simply left alone, the same as any other leaf.  The other two
 * only apply to a key whose JSON value is itself an array of objects,
 * every element of which shares one fixed shape ('randr.json''s
 * @c outputs, 'rules.json''s @c rules): @c CONFIG_LINT_ARRAY_UNIFORM
 * checks every element against @c children.  @c
 * CONFIG_LINT_ARRAY_POLYMORPHIC is for the rarer case where a single
 * key accepts two genuinely different shapes
 * (@c topology.screens.desktops, see config/lint.h): the first
 * element is inspected for any of @c discriminator_keys, and every
 * element is then checked against @c children_alt if found, or
 * @c children otherwise, mirroring the same shape a loader itself
 * would pick.
 */
enum config_lint_array_kind_e {
    CONFIG_LINT_ARRAY_NONE = 0,
    CONFIG_LINT_ARRAY_UNIFORM,
    CONFIG_LINT_ARRAY_POLYMORPHIC
};


/**
 * @brief One key a schema recognizes at a given nesting level
 */
typedef struct config_lint_key_s {
    const char *name;
    const struct config_lint_key_s *children; /**< 'NULL' for a leaf,
                                                   for a subtree
                                                   deliberately left
                                                   opaque (see
                                                   config/lint.h), or
                                                   the array-element
                                                   schema used when
                                                   'discriminator_keys'
                                                   is not matched */
    size_t children_count;
    enum config_lint_array_kind_e array_kind; /**< 'CONFIG_LINT_ARRAY_NONE'
                                                   when omitted, so
                                                   every existing
                                                   three-field schema
                                                   entry keeps working
                                                   unchanged */

    /**
     * @brief Array-element schema used when @c discriminator_keys is
     *        matched
     *
     * @note Only meaningful under @c CONFIG_LINT_ARRAY_POLYMORPHIC
     */
    const struct config_lint_key_s *children_alt;

    size_t children_alt_count;

    /**
     * @brief Null-terminated list of key names whose presence in the
     *        first array element selects @c children_alt
     *
     * @note Only meaningful under @c CONFIG_LINT_ARRAY_POLYMORPHIC
     */
    const char *const *discriminator_keys;
} config_lint_key_td;


/**
 * @brief One configuration file this linter knows how to check
 */
typedef struct {
    const char *filename;
    const config_lint_key_td *schema;
    size_t schema_count;
    bool required;
} config_lint_file_spec_td;


/*
 * Each schema table has a key count declared beside it in a header, and
 * the two have to agree.  A key added to the table and not to the count
 * would leave the linter reading past the end of it.
 *
 * Each such declaration below is negative in that case, so the build
 * fails at the table rather than the linter running off it.  Every
 * schema table repeats this idiom under a name of its own; one line at
 * each site points back here rather than restating the reasoning
 * sixteen times.
 *
 * C99 has no @c static_assert, which is what this stands in for
 */


#endif  /* ! CONFIG_LINT_INTERNAL_H */
