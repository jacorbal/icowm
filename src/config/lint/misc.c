/**
 * @file config/lint/misc.c
 *
 * @brief Schemas for the files with a single level of their own
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* Local includes */
#include <config/lint/internal.h>
#include <config/lint/misc.h>


/* Every 'outputs[]' entry has one single fixed shape; see
 *      'config/randr.c''s 'config_load_randr' */
static const config_lint_key_td s_schema_randr_output_resolution[] = {
    {"w", NULL, 0u,
        0, NULL, 0u, NULL},
    {"h", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_randr_output_position[] = {
    {"x", NULL, 0u,
        0, NULL, 0u, NULL},
    {"y", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_randr_output[] = {
    {"name", NULL, 0u,
        0, NULL, 0u, NULL},
    {"is-enabled", NULL, 0u,
        0, NULL, 0u, NULL},
    {"is-primary", NULL, 0u,
        0, NULL, 0u, NULL},
    {"resolution", s_schema_randr_output_resolution,
        sizeof(s_schema_randr_output_resolution) /
            sizeof(s_schema_randr_output_resolution[0]),
        0, NULL, 0u, NULL},
    {"position", s_schema_randr_output_position,
        sizeof(s_schema_randr_output_position) /
            sizeof(s_schema_randr_output_position[0]),
        0, NULL, 0u, NULL},
    {"rotation", NULL, 0u,
        0, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_randr[] = {
    {"is-enabled", NULL, 0u,
        0, NULL, 0u, NULL},
    {"outputs", s_schema_randr_output,
        sizeof(s_schema_randr_output) / sizeof(s_schema_randr_output[0]),
        CONFIG_LINT_ARRAY_UNIFORM, NULL, 0u, NULL}
};

/* Every 'rules[]' entry has one single fixed shape ('when', 'match',
 *      'apply'); see 'rules.c''s 'rules_load'.  Several 'match' fields
 *      accept either a plain string or an array of strings, and two
 *      'apply' fields ('opacity', 'position') accept either a plain
 *      value or an object; that is value polymorphism, not key
 *      polymorphism, and 's_lint_object' already only recurses into
 *      one of those when it actually is an object, so it needs no
 *      special handling here */
static const config_lint_key_td s_schema_rules_match[] = {
    {"instance", NULL, 0u,
        0, NULL, 0u, NULL},
    {"class", NULL, 0u,
        0, NULL, 0u, NULL},
    {"role", NULL, 0u,
        0, NULL, 0u, NULL},
    {"title", NULL, 0u,
        0, NULL, 0u, NULL},
    {"type", NULL, 0u,
        0, NULL, 0u, NULL},
    {"transient", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_rules_apply_opacity[] = {
    {"active", NULL, 0u,
        0, NULL, 0u, NULL},
    {"inactive", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_rules_apply_position[] = {
    {"x", NULL, 0u,
        0, NULL, 0u, NULL},
    {"y", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_rules_apply_size[] = {
    {"width", NULL, 0u,
        0, NULL, 0u, NULL},
    {"height", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_rules_apply[] = {
    {"desktop", NULL, 0u,
        0, NULL, 0u, NULL},
    {"monitor", NULL, 0u,
        0, NULL, 0u, NULL},
    {"layer", NULL, 0u,
        0, NULL, 0u, NULL},
    {"focus", NULL, 0u,
        0, NULL, 0u, NULL},
    {"pinned", NULL, 0u,
        0, NULL, 0u, NULL},
    {"decorated", NULL, 0u,
        0, NULL, 0u, NULL},
    {"iconified", NULL, 0u,
        0, NULL, 0u, NULL},
    {"fullscreen", NULL, 0u,
        0, NULL, 0u, NULL},
    {"maximized", NULL, 0u,
        0, NULL, 0u, NULL},
    {"shaded", NULL, 0u,
        0, NULL, 0u, NULL},
    {"hidden", NULL, 0u,
        0, NULL, 0u, NULL},
    {"opacity", s_schema_rules_apply_opacity,
        sizeof(s_schema_rules_apply_opacity) /
            sizeof(s_schema_rules_apply_opacity[0]),
        0, NULL, 0u, NULL},
    {"position", s_schema_rules_apply_position,
        sizeof(s_schema_rules_apply_position) /
            sizeof(s_schema_rules_apply_position[0]),
        0, NULL, 0u, NULL},
    {"size", s_schema_rules_apply_size,
        sizeof(s_schema_rules_apply_size) /
            sizeof(s_schema_rules_apply_size[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_rules_entry[] = {
    {"when", NULL, 0u,
        0, NULL, 0u, NULL},
    {"match", s_schema_rules_match,
        sizeof(s_schema_rules_match) / sizeof(s_schema_rules_match[0]),
        0, NULL, 0u, NULL},
    {"apply", s_schema_rules_apply,
        sizeof(s_schema_rules_apply) / sizeof(s_schema_rules_apply[0]),
        0, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_rules[] = {
    {"rules", s_schema_rules_entry,
        sizeof(s_schema_rules_entry) / sizeof(s_schema_rules_entry[0]),
        CONFIG_LINT_ARRAY_UNIFORM, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_session[] = {
    {"on-start", NULL, 0u,
        0, NULL, 0u, NULL},
    {"on-reload", NULL, 0u,
        0, NULL, 0u, NULL},
    {"on-exit", NULL, 0u,
        0, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_menu[] = {
    {"menu", NULL, 0u,
        0, NULL, 0u, NULL}
};


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_menu_size_check
    [(sizeof(s_schema_menu) /
      sizeof(s_schema_menu[0]) == CONFIG_LINT_MENU_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_randr_size_check
    [(sizeof(s_schema_randr) /
      sizeof(s_schema_randr[0]) == CONFIG_LINT_RANDR_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_rules_size_check
    [(sizeof(s_schema_rules) /
      sizeof(s_schema_rules[0]) == CONFIG_LINT_RULES_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_session_size_check
    [(sizeof(s_schema_session) /
      sizeof(s_schema_session[0]) == CONFIG_LINT_SESSION_KEYS)
     ? 1 : -1];
