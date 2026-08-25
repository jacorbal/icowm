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
    {"window", NULL, 0u},
    {"screen", NULL, 0u}
};

static const config_lint_key_td s_schema_memguard_windows_edges[] = {
    {"snap", s_schema_memguard_windows_edges_snap,
        sizeof(s_schema_memguard_windows_edges_snap) /
            sizeof(s_schema_memguard_windows_edges_snap[0])},
    {"resistance", NULL, 0u}
};

static const config_lint_key_td s_schema_memguard_windows[] = {
    {"move-step", NULL, 0u},
    {"show-geom", NULL, 0u},
    {"edges", s_schema_memguard_windows_edges,
        sizeof(s_schema_memguard_windows_edges) /
            sizeof(s_schema_memguard_windows_edges[0])},
    {"gravity", NULL, 0u},
    /* Both 'focus' and 'placement' are now identical to config.json's
     * own identically-named objects (policy included in both:
     * memguard.json's own parser reads every field either accepts),
     * so shared verbatim rather than duplicated, same as 'programs'
     * and 'shutdown' above. */
    {"focus", s_schema_windows_focus,
        sizeof(s_schema_windows_focus) /
            sizeof(s_schema_windows_focus[0])},
    {"placement", s_schema_windows_placement,
        sizeof(s_schema_windows_placement) /
            sizeof(s_schema_windows_placement[0])}
};

static const config_lint_key_td s_schema_memguard_icons_placement[] = {
    {"policy", NULL, 0u}
};

static const config_lint_key_td s_schema_memguard_icons[] = {
    {"show-geom", NULL, 0u},
    {"placement", s_schema_memguard_icons_placement,
        sizeof(s_schema_memguard_icons_placement) /
            sizeof(s_schema_memguard_icons_placement[0])}
};

static const config_lint_key_td s_schema_memguard_desktops[] = {
    /* Reuses 'desktops.margins''s own schema array; see
     * 's_schema_systray''s own identical comment above for why. */
    {"margins", s_schema_desktops_margins,
        sizeof(s_schema_desktops_margins) /
            sizeof(s_schema_desktops_margins[0])}
};

const config_lint_key_td s_schema_memguard[] = {
    {"theme", NULL, 0u},
    /* 'programs', 'prompt', and 'shutdown' accept the exact same
     * fields as config.json's own identically-named sections, so
     * their schemas are shared verbatim rather than duplicated. */
    {"programs", s_schema_programs,
        sizeof(s_schema_programs) / sizeof(s_schema_programs[0])},
    {"prompt", s_schema_prompt,
        sizeof(s_schema_prompt) / sizeof(s_schema_prompt[0])},
    {"desktops", s_schema_memguard_desktops,
        sizeof(s_schema_memguard_desktops) /
            sizeof(s_schema_memguard_desktops[0])},
    {"windows", s_schema_memguard_windows,
        sizeof(s_schema_memguard_windows) /
            sizeof(s_schema_memguard_windows[0])},
    {"icons", s_schema_memguard_icons,
        sizeof(s_schema_memguard_icons) /
            sizeof(s_schema_memguard_icons[0])},
    /* 'systray' is loaded by the exact same 'ci_config_load_systray'
     * config.json itself uses, so every field it accepts there is
     * accepted here too, even the two ('text.position' and 'order')
     * that end up with no visible effect in this mode; see that
     * function's own call site in memguard.c for why. */
    {"systray", s_schema_systray,
        sizeof(s_schema_systray) / sizeof(s_schema_systray[0])},
    {"shutdown", s_schema_shutdown,
        sizeof(s_schema_shutdown) / sizeof(s_schema_shutdown[0])}
};


/* A key added to 's_schema_memguard' without its count in the header
 * following makes this declaration negative, and so the build fail
 * here rather than the linter read past the end of the table */
typedef char config_lint_memguard_size_check
    [(sizeof(s_schema_memguard) /
      sizeof(s_schema_memguard[0]) == CONFIG_LINT_MEMGUARD_KEYS)
     ? 1 : -1];
