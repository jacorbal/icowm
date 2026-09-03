/**
 * @file config/lint/common.c
 *
 * @brief Schemas both 'config.json' and 'memguard.json' use
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
#include <config/lint/common.h>


const config_lint_key_td s_schema_desktops_margins[] = {
    {"top", NULL, 0u},
    {"right", NULL, 0u},
    {"bottom", NULL, 0u},
    {"left", NULL, 0u}
};

const config_lint_key_td s_schema_programs[] = {
    {"terminal", NULL, 0u},
    {"launcher", NULL, 0u},
    {"file-manager", NULL, 0u},
    {"web-browser", NULL, 0u},
    {"editor", NULL, 0u}
};

/* Accepted at config.json's top level (built-in run-box) and
 * reused verbatim by memguard.json, same reasoning as 'programs' and
 * 'shutdown' above having one shared schema each */
const config_lint_key_td s_schema_prompt[] = {
    {"is-enabled", NULL, 0u}
};

const config_lint_key_td s_schema_windows_focus[] = {
    {"policy", NULL, 0u},
    {"focus-new", NULL, 0u},
    {"raise", NULL, 0u},
    {"delay-ms", NULL, 0u}
};

const config_lint_key_td s_schema_windows_placement[] = {
    {"policy", NULL, 0u},
    {"monitor", NULL, 0u},
    {"group-related", NULL, 0u}
};

const config_lint_key_td s_schema_shutdown[] = {
    {"enable-emergency-shortcut", NULL, 0u},
    {"timeout-seconds", NULL, 0u}
};

static const config_lint_key_td s_schema_battery_threshold[] = {
    {"charged", NULL, 0u},
    {"low", NULL, 0u},
    {"critical", NULL, 0u}
};

static const config_lint_key_td s_schema_battery_backend[] = {
    {"type", NULL, 0u},
    {"number", NULL, 0u}
};

static const config_lint_key_td s_schema_battery[] = {
    {"is-enabled", NULL, 0u},
    {"threshold", s_schema_battery_threshold,
        sizeof(s_schema_battery_threshold) /
            sizeof(s_schema_battery_threshold[0])},
    {"backend", s_schema_battery_backend,
        sizeof(s_schema_battery_backend) /
            sizeof(s_schema_battery_backend[0])},
    {"poll-seconds", NULL, 0u}
};

static const config_lint_key_td s_schema_clock[] = {
    {"is-enabled", NULL, 0u},
    {"format", NULL, 0u}
};

static const config_lint_key_td s_schema_systray_text[] = {
    {"order", NULL, 0u},
    {"position", NULL, 0u}
};

static const config_lint_key_td s_schema_systray_monitor[] = {
    {"anchor", NULL, 0u},
    {"index", NULL, 0u}
};

const config_lint_key_td s_schema_systray[] = {
    {"is-enabled", NULL, 0u},
    {"reserve-space", NULL, 0u},
    {"avoid-overlap", NULL, 0u},
    /* Reuses 'desktops.margins''s schema array.  Identical shape
     * (top/right/bottom/left), so no separate one is needed just for
     * this section. */
    {"margins", s_schema_desktops_margins,
        sizeof(s_schema_desktops_margins) /
            sizeof(s_schema_desktops_margins[0])},
    {"position", NULL, 0u},
    {"monitor", s_schema_systray_monitor,
        sizeof(s_schema_systray_monitor) /
            sizeof(s_schema_systray_monitor[0])},
    {"order", NULL, 0u},
    {"layer", NULL, 0u},
    {"clock", s_schema_clock,
        sizeof(s_schema_clock) / sizeof(s_schema_clock[0])},
    {"battery", s_schema_battery,
        sizeof(s_schema_battery) / sizeof(s_schema_battery[0])},
    {"text", s_schema_systray_text,
        sizeof(s_schema_systray_text) / sizeof(s_schema_systray_text[0])}
};


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_desktops_margins_size_check
    [(sizeof(s_schema_desktops_margins) /
      sizeof(s_schema_desktops_margins[0]) ==
          CONFIG_LINT_DESKTOPS_MARGINS_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_programs_size_check
    [(sizeof(s_schema_programs) /
      sizeof(s_schema_programs[0]) == CONFIG_LINT_PROGRAMS_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_prompt_size_check
    [(sizeof(s_schema_prompt) /
      sizeof(s_schema_prompt[0]) == CONFIG_LINT_PROMPT_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_shutdown_size_check
    [(sizeof(s_schema_shutdown) /
      sizeof(s_schema_shutdown[0]) == CONFIG_LINT_SHUTDOWN_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_systray_size_check
    [(sizeof(s_schema_systray) /
      sizeof(s_schema_systray[0]) == CONFIG_LINT_SYSTRAY_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_windows_focus_size_check
    [(sizeof(s_schema_windows_focus) /
      sizeof(s_schema_windows_focus[0]) == CONFIG_LINT_WINDOWS_FOCUS_KEYS)
     ? 1 : -1];


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_windows_placement_size_check
    [(sizeof(s_schema_windows_placement) /
      sizeof(s_schema_windows_placement[0]) ==
          CONFIG_LINT_WINDOWS_PLACEMENT_KEYS)
     ? 1 : -1];
