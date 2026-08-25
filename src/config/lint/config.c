/**
 * @file config/lint/config.c
 *
 * @brief 'config.json' schema
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
#include <config/lint/config.h>
#include <config/lint/common.h>


static const config_lint_key_td s_schema_screens[] = {
    {"count", NULL, 0u},
    {"desktops", NULL, 0u}
};

static const config_lint_key_td s_schema_topology[] = {
    {"screens", s_schema_screens,
        sizeof(s_schema_screens) / sizeof(s_schema_screens[0])}
};

static const config_lint_key_td s_schema_desktops[] = {
    {"show-overlay", NULL, 0u},
    {"notify-activity", NULL, 0u},
    {"warp-on-edge-drag", NULL, 0u},
    {"wrap-at-bounds", NULL, 0u},
    {"margins", s_schema_desktops_margins,
        sizeof(s_schema_desktops_margins) /
            sizeof(s_schema_desktops_margins[0])}
};

static const config_lint_key_td s_schema_scratchpad[] = {
    {"is-enabled", NULL, 0u},
    {"command", NULL, 0u},
    {"edge", NULL, 0u},
    {"width", NULL, 0u},
    {"height", NULL, 0u},
    {"ignore-margins", NULL, 0u}
};

static const config_lint_key_td s_schema_windows_edges_snap[] = {
    {"window", NULL, 0u},
    {"screen", NULL, 0u}
};

static const config_lint_key_td s_schema_windows_edges[] = {
    {"snap", s_schema_windows_edges_snap,
        sizeof(s_schema_windows_edges_snap) /
            sizeof(s_schema_windows_edges_snap[0])},
    {"resistance", NULL, 0u}
};

static const config_lint_key_td s_schema_placement_wrapper[] = {
    {"placement", NULL, 0u}
};

static const config_lint_key_td s_schema_windows[] = {
    {"move-step", NULL, 0u},
    {"resize-step", NULL, 0u},
    {"show-geom", NULL, 0u},
    {"solid-drag", NULL, 0u},
    {"gravity", NULL, 0u},
    {"edges", s_schema_windows_edges,
        sizeof(s_schema_windows_edges) / sizeof(s_schema_windows_edges[0])},
    {"focus", s_schema_windows_focus,
        sizeof(s_schema_windows_focus) / sizeof(s_schema_windows_focus[0])},
    {"placement", s_schema_windows_placement,
        sizeof(s_schema_windows_placement) /
            sizeof(s_schema_windows_placement[0])},
    /* Legacy location, see 'icons' below */
    {"icons", s_schema_placement_wrapper,
        sizeof(s_schema_placement_wrapper) /
            sizeof(s_schema_placement_wrapper[0])}
};

static const config_lint_key_td s_schema_icons[] = {
    {"show-geom", NULL, 0u},
    {"placement", NULL, 0u} /* object with 'policy', or a plain string;
                                either way accepted as a leaf here */
};

static const config_lint_key_td s_schema_startup_notification[] = {
    {"is-enabled", NULL, 0u},
    {"timeout-seconds", NULL, 0u}
};

static const config_lint_key_td s_schema_fortune[] = {
    {"is-enabled", NULL, 0u},
    {"command", NULL, 0u}
};

static const config_lint_key_td s_schema_menu_position[] = {
    {"position", NULL, 0u}
};

static const config_lint_key_td s_schema_menus[] = {
    {"root", s_schema_menu_position,
        sizeof(s_schema_menu_position) / sizeof(s_schema_menu_position[0])},
    {"windows", s_schema_menu_position,
        sizeof(s_schema_menu_position) / sizeof(s_schema_menu_position[0])}
};

const config_lint_key_td s_schema_config[] = {
    {"theme", NULL, 0u},
    {"topology", s_schema_topology,
        sizeof(s_schema_topology) / sizeof(s_schema_topology[0])},
    {"desktops", s_schema_desktops,
        sizeof(s_schema_desktops) / sizeof(s_schema_desktops[0])},
    {"programs", s_schema_programs,
        sizeof(s_schema_programs) / sizeof(s_schema_programs[0])},
    {"prompt", s_schema_prompt,
        sizeof(s_schema_prompt) / sizeof(s_schema_prompt[0])},
    {"windows", s_schema_windows,
        sizeof(s_schema_windows) / sizeof(s_schema_windows[0])},
    {"icons", s_schema_icons,
        sizeof(s_schema_icons) / sizeof(s_schema_icons[0])},
    {"startup-notification", s_schema_startup_notification,
        sizeof(s_schema_startup_notification) /
            sizeof(s_schema_startup_notification[0])},
    {"menus", s_schema_menus,
        sizeof(s_schema_menus) / sizeof(s_schema_menus[0])},
    {"systray", s_schema_systray,
        sizeof(s_schema_systray) / sizeof(s_schema_systray[0])},
    {"scratchpad", s_schema_scratchpad,
        sizeof(s_schema_scratchpad) / sizeof(s_schema_scratchpad[0])},
    {"shutdown", s_schema_shutdown,
        sizeof(s_schema_shutdown) / sizeof(s_schema_shutdown[0])},
    {"fortune", s_schema_fortune,
        sizeof(s_schema_fortune) / sizeof(s_schema_fortune[0])}
};


/* A key added to 's_schema_config' without its count in the header
 * following makes this declaration negative, and so the build fail
 * here rather than the linter read past the end of the table */
typedef char config_lint_config_size_check
    [(sizeof(s_schema_config) /
      sizeof(s_schema_config[0]) == CONFIG_LINT_CONFIG_KEYS)
     ? 1 : -1];
