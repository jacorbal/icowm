/**
 * @file config/lint/memguard.c
 *
 * @brief 'memguard.json' schema
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
#include <config/lint/memguard.h>
#include <config/lint/common.h>


static const config_lint_key_td s_schema_memguard_windows_edges_snap[] = {
    {"window", NULL, 0u,
        0, NULL, 0u, NULL},
    {"screen", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_memguard_windows_edges[] = {
    {"snap", s_schema_memguard_windows_edges_snap,
        sizeof(s_schema_memguard_windows_edges_snap) /
            sizeof(s_schema_memguard_windows_edges_snap[0]),
        0, NULL, 0u, NULL},
    {"resistance", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_memguard_windows[] = {
    {"move-step", NULL, 0u,
        0, NULL, 0u, NULL},
    {"show-geom", NULL, 0u,
        0, NULL, 0u, NULL},
    {"edges", s_schema_memguard_windows_edges,
        sizeof(s_schema_memguard_windows_edges) /
            sizeof(s_schema_memguard_windows_edges[0]),
        0, NULL, 0u, NULL},
    {"gravity", NULL, 0u,
        0, NULL, 0u, NULL},
    /* Both 'focus' and 'placement' are now identical to config.json's
     * own identically-named objects (policy included in both:
     * memguard.json's parser reads every field either accepts),
     * so shared verbatim rather than duplicated, same as 'programs'
     * and 'shutdown' above. */
    {"focus", s_schema_windows_focus,
        sizeof(s_schema_windows_focus) /
            sizeof(s_schema_windows_focus[0]),
        0, NULL, 0u, NULL},
    {"placement", s_schema_windows_placement,
        sizeof(s_schema_windows_placement) /
            sizeof(s_schema_windows_placement[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_memguard_icons_placement[] = {
    {"policy", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_memguard_icons[] = {
    {"show-geom", NULL, 0u,
        0, NULL, 0u, NULL},
    {"placement", s_schema_memguard_icons_placement,
        sizeof(s_schema_memguard_icons_placement) /
            sizeof(s_schema_memguard_icons_placement[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_memguard_desktops[] = {
    /* Reuses 'desktops.margins''s schema array; see
     * 's_schema_systray''s identical comment above for why. */
    {"margins", s_schema_desktops_margins,
        sizeof(s_schema_desktops_margins) /
            sizeof(s_schema_desktops_margins[0]),
        0, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_memguard[] = {
    {"theme", NULL, 0u,
        0, NULL, 0u, NULL},
    /* 'programs', 'prompt', and 'shutdown' accept the exact same
     * fields as config.json's identically-named sections, so
     * their schemas are shared verbatim rather than duplicated. */
    {"programs", s_schema_programs,
        sizeof(s_schema_programs) / sizeof(s_schema_programs[0]),
        0, NULL, 0u, NULL},
    {"prompt", s_schema_prompt,
        sizeof(s_schema_prompt) / sizeof(s_schema_prompt[0]),
        0, NULL, 0u, NULL},
    {"desktops", s_schema_memguard_desktops,
        sizeof(s_schema_memguard_desktops) /
            sizeof(s_schema_memguard_desktops[0]),
        0, NULL, 0u, NULL},
    {"windows", s_schema_memguard_windows,
        sizeof(s_schema_memguard_windows) /
            sizeof(s_schema_memguard_windows[0]),
        0, NULL, 0u, NULL},
    {"icons", s_schema_memguard_icons,
        sizeof(s_schema_memguard_icons) /
            sizeof(s_schema_memguard_icons[0]),
        0, NULL, 0u, NULL},
    /* 'systray' is loaded by the exact same 'ci_config_load_systray'
     * config.json itself uses, so every field it accepts there is
     * accepted here too, even the two ('text.position' and 'order')
     * that end up with no visible effect in this mode; see that
     * function's call site in memguard.c for why. */
    {"systray", s_schema_systray,
        sizeof(s_schema_systray) / sizeof(s_schema_systray[0]),
        0, NULL, 0u, NULL},
    {"shutdown", s_schema_shutdown,
        sizeof(s_schema_shutdown) / sizeof(s_schema_shutdown[0]),
        0, NULL, 0u, NULL}
};


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_memguard_size_check
    [(sizeof(s_schema_memguard) /
      sizeof(s_schema_memguard[0]) == CONFIG_LINT_MEMGUARD_KEYS)
     ? 1 : -1];
