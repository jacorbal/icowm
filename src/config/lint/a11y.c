/**
 * @file config/lint/a11y.c
 *
 * @brief 'a11y.json' schema
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
#include <config/lint/a11y.h>


static const config_lint_key_td s_schema_a11y_interaction[] = {
    {"double-click-ms", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_a11y_focus_indicator[] = {
    {"min-border-width", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_a11y_urgency[] = {
    {"sound-bell", NULL, 0u,
        0, NULL, 0u, NULL},
    {"blink-interval-ms", NULL, 0u,
        0, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_a11y[] = {
    {"is-enabled", NULL, 0u,
        0, NULL, 0u, NULL},
    {"interaction", s_schema_a11y_interaction,
        sizeof(s_schema_a11y_interaction) /
            sizeof(s_schema_a11y_interaction[0]),
        0, NULL, 0u, NULL},
    {"focus-indicator", s_schema_a11y_focus_indicator,
        sizeof(s_schema_a11y_focus_indicator) /
            sizeof(s_schema_a11y_focus_indicator[0]),
        0, NULL, 0u, NULL},
    {"urgency", s_schema_a11y_urgency,
        sizeof(s_schema_a11y_urgency) /
            sizeof(s_schema_a11y_urgency[0]),
        0, NULL, 0u, NULL}
};


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_a11y_size_check[(sizeof(s_schema_a11y) /
        sizeof(s_schema_a11y[0]) == CONFIG_LINT_A11Y_KEYS) ? 1 : -1];
