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


const config_lint_key_td s_schema_randr[] = {
    {"is-enabled", NULL, 0u},
    {"outputs", NULL, 0u}
};

const config_lint_key_td s_schema_rules[] = {
    {"rules", NULL, 0u}
};

const config_lint_key_td s_schema_session[] = {
    {"on-start", NULL, 0u},
    {"on-reload", NULL, 0u},
    {"on-exit", NULL, 0u}
};

const config_lint_key_td s_schema_menu[] = {
    {"menu", NULL, 0u}
};


/* A key added to 's_schema_menu' without its count in the header
 * following makes this declaration negative, and so the build fail
 * here rather than the linter read past the end of the table */
typedef char config_lint_menu_size_check
    [(sizeof(s_schema_menu) /
      sizeof(s_schema_menu[0]) == CONFIG_LINT_MENU_KEYS)
     ? 1 : -1];


/* A key added to 's_schema_randr' without its count in the header
 * following makes this declaration negative, and so the build fail
 * here rather than the linter read past the end of the table */
typedef char config_lint_randr_size_check
    [(sizeof(s_schema_randr) /
      sizeof(s_schema_randr[0]) == CONFIG_LINT_RANDR_KEYS)
     ? 1 : -1];


/* A key added to 's_schema_rules' without its count in the header
 * following makes this declaration negative, and so the build fail
 * here rather than the linter read past the end of the table */
typedef char config_lint_rules_size_check
    [(sizeof(s_schema_rules) /
      sizeof(s_schema_rules[0]) == CONFIG_LINT_RULES_KEYS)
     ? 1 : -1];


/* A key added to 's_schema_session' without its count in the header
 * following makes this declaration negative, and so the build fail
 * here rather than the linter read past the end of the table */
typedef char config_lint_session_size_check
    [(sizeof(s_schema_session) /
      sizeof(s_schema_session[0]) == CONFIG_LINT_SESSION_KEYS)
     ? 1 : -1];
