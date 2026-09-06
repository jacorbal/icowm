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


/* Both accepted shapes of 'topology.screens.desktops[]' entries are
 *      fixed once told apart; see 'config/base/desktops.c''s
 *      's_config_screens_uses_nested_layout', mirrored here by
 *      's_desktops_nested_discriminators' below */
static const config_lint_key_td s_schema_desktop_entry_flat[] = {
    {"name", NULL, 0u,
        0, NULL, 0u, NULL},
    {"background-color", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_desktop_layout[] = {
    {"orientation", NULL, 0u,
        0, NULL, 0u, NULL},
    {"corner", NULL, 0u,
        0, NULL, 0u, NULL},
    {"rows", NULL, 0u,
        0, NULL, 0u, NULL},
    {"columns", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_desktop_viewport[] = {
    {"columns", NULL, 0u,
        0, NULL, 0u, NULL},
    {"rows", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_desktop_entry_nested[] = {
    {"count", NULL, 0u,
        0, NULL, 0u, NULL},
    {"inaugural", NULL, 0u,
        0, NULL, 0u, NULL},
    {"layout", s_schema_desktop_layout,
        sizeof(s_schema_desktop_layout) /
            sizeof(s_schema_desktop_layout[0]),
        0, NULL, 0u, NULL},
    {"viewport", s_schema_desktop_viewport,
        sizeof(s_schema_desktop_viewport) /
            sizeof(s_schema_desktop_viewport[0]),
        0, NULL, 0u, NULL},
    {"settings", s_schema_desktop_entry_flat,
        sizeof(s_schema_desktop_entry_flat) /
            sizeof(s_schema_desktop_entry_flat[0]),
        CONFIG_LINT_ARRAY_UNIFORM, NULL, 0u, NULL}
};

/* Same exact key names 's_config_screens_uses_nested_layout' itself
 *      checks the first entry for, to decide the whole array is using
 *      the per-screen shape */
static const char *const s_desktops_nested_discriminators[] = {
    "settings", "count", "inaugural", NULL
};

static const config_lint_key_td s_schema_screens[] = {
    {"count", NULL, 0u,
        0, NULL, 0u, NULL},
    {"desktops", s_schema_desktop_entry_flat,
        sizeof(s_schema_desktop_entry_flat) /
            sizeof(s_schema_desktop_entry_flat[0]),
        CONFIG_LINT_ARRAY_POLYMORPHIC,
        s_schema_desktop_entry_nested,
        sizeof(s_schema_desktop_entry_nested) /
            sizeof(s_schema_desktop_entry_nested[0]),
        s_desktops_nested_discriminators}
};

static const config_lint_key_td s_schema_topology[] = {
    {"screens", s_schema_screens,
        sizeof(s_schema_screens) / sizeof(s_schema_screens[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_desktops[] = {
    {"notify-activity", NULL, 0u,
        0, NULL, 0u, NULL},
    {"warp-on-edge-drag", NULL, 0u,
        0, NULL, 0u, NULL},
    {"pan-on-edge-drag", NULL, 0u,
        0, NULL, 0u, NULL},
    {"pan-on-edge-hover", NULL, 0u,
        0, NULL, 0u, NULL},
    {"wrap-at-bounds", NULL, 0u,
        0, NULL, 0u, NULL},
    {"margins", s_schema_desktops_margins,
        sizeof(s_schema_desktops_margins) /
            sizeof(s_schema_desktops_margins[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_scratchpad[] = {
    {"is-enabled", NULL, 0u,
        0, NULL, 0u, NULL},
    {"command", NULL, 0u,
        0, NULL, 0u, NULL},
    {"edge", NULL, 0u,
        0, NULL, 0u, NULL},
    {"width", NULL, 0u,
        0, NULL, 0u, NULL},
    {"height", NULL, 0u,
        0, NULL, 0u, NULL},
    {"ignore-margins", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_windows_edges_snap[] = {
    {"window", NULL, 0u,
        0, NULL, 0u, NULL},
    {"screen", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_windows_edges[] = {
    {"snap", s_schema_windows_edges_snap,
        sizeof(s_schema_windows_edges_snap) /
            sizeof(s_schema_windows_edges_snap[0]),
        0, NULL, 0u, NULL},
    {"resistance", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_placement_wrapper[] = {
    {"placement", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_overlay[] = {
    {"on-desktop-switch", NULL, 0u,
        0, NULL, 0u, NULL},
    {"on-viewport-move", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_viewport_mesh_spacing[] = {
    {"horizontal", NULL, 0u,
        0, NULL, 0u, NULL},
    {"vertical", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_viewport_mesh[] = {
    {"is-enabled", NULL, 0u,
        0, NULL, 0u, NULL},
    {"spacing", s_schema_viewport_mesh_spacing,
        sizeof(s_schema_viewport_mesh_spacing) /
            sizeof(s_schema_viewport_mesh_spacing[0]),
        0, NULL, 0u, NULL},
    {"thickness", NULL, 0u,
        0, NULL, 0u, NULL},
    {"tone-shift", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_viewport[] = {
    {"move-step", NULL, 0u,
        0, NULL, 0u, NULL},
    {"mesh", s_schema_viewport_mesh,
        sizeof(s_schema_viewport_mesh) /
            sizeof(s_schema_viewport_mesh[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_windows[] = {
    {"move-step", NULL, 0u,
        0, NULL, 0u, NULL},
    {"resize-step", NULL, 0u,
        0, NULL, 0u, NULL},
    {"show-geom", NULL, 0u,
        0, NULL, 0u, NULL},
    {"solid-drag", NULL, 0u,
        0, NULL, 0u, NULL},
    {"gravity", NULL, 0u,
        0, NULL, 0u, NULL},
    {"edges", s_schema_windows_edges,
        sizeof(s_schema_windows_edges) / sizeof(s_schema_windows_edges[0]),
        0, NULL, 0u, NULL},
    {"focus", s_schema_windows_focus,
        sizeof(s_schema_windows_focus) / sizeof(s_schema_windows_focus[0]),
        0, NULL, 0u, NULL},
    {"placement", s_schema_windows_placement,
        sizeof(s_schema_windows_placement) /
            sizeof(s_schema_windows_placement[0]),
        0, NULL, 0u, NULL},
    /* Legacy location, see 'icons' below */
    {"icons", s_schema_placement_wrapper,
        sizeof(s_schema_placement_wrapper) /
            sizeof(s_schema_placement_wrapper[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_icons[] = {
    {"show-geom", NULL, 0u,
        0, NULL, 0u, NULL},
    {"follow-viewport", NULL, 0u,
        0, NULL, 0u, NULL},
    {"placement", NULL, 0u,
        0, NULL, 0u, NULL} /* object with 'policy', or a plain string;
                                either way accepted as a leaf here */
};

static const config_lint_key_td s_schema_startup_notification[] = {
    {"is-enabled", NULL, 0u,
        0, NULL, 0u, NULL},
    {"timeout-seconds", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_fortune[] = {
    {"is-enabled", NULL, 0u,
        0, NULL, 0u, NULL},
    {"command", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_menu_position[] = {
    {"position", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_menus[] = {
    {"root", s_schema_menu_position,
        sizeof(s_schema_menu_position) / sizeof(s_schema_menu_position[0]),
        0, NULL, 0u, NULL},
    {"windows", s_schema_menu_position,
        sizeof(s_schema_menu_position) / sizeof(s_schema_menu_position[0]),
        0, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_config[] = {
    {"theme", NULL, 0u,
        0, NULL, 0u, NULL},
    {"topology", s_schema_topology,
        sizeof(s_schema_topology) / sizeof(s_schema_topology[0]),
        0, NULL, 0u, NULL},
    {"desktops", s_schema_desktops,
        sizeof(s_schema_desktops) / sizeof(s_schema_desktops[0]),
        0, NULL, 0u, NULL},
    {"programs", s_schema_programs,
        sizeof(s_schema_programs) / sizeof(s_schema_programs[0]),
        0, NULL, 0u, NULL},
    {"prompt", s_schema_prompt,
        sizeof(s_schema_prompt) / sizeof(s_schema_prompt[0]),
        0, NULL, 0u, NULL},
    {"windows", s_schema_windows,
        sizeof(s_schema_windows) / sizeof(s_schema_windows[0]),
        0, NULL, 0u, NULL},
    {"overlay", s_schema_overlay,
        sizeof(s_schema_overlay) / sizeof(s_schema_overlay[0]),
        0, NULL, 0u, NULL},
    {"viewport", s_schema_viewport,
        sizeof(s_schema_viewport) / sizeof(s_schema_viewport[0]),
        0, NULL, 0u, NULL},
    {"icons", s_schema_icons,
        sizeof(s_schema_icons) / sizeof(s_schema_icons[0]),
        0, NULL, 0u, NULL},
    {"startup-notification", s_schema_startup_notification,
        sizeof(s_schema_startup_notification) /
            sizeof(s_schema_startup_notification[0]),
        0, NULL, 0u, NULL},
    {"menus", s_schema_menus,
        sizeof(s_schema_menus) / sizeof(s_schema_menus[0]),
        0, NULL, 0u, NULL},
    {"systray", s_schema_systray,
        sizeof(s_schema_systray) / sizeof(s_schema_systray[0]),
        0, NULL, 0u, NULL},
    {"scratchpad", s_schema_scratchpad,
        sizeof(s_schema_scratchpad) / sizeof(s_schema_scratchpad[0]),
        0, NULL, 0u, NULL},
    {"shutdown", s_schema_shutdown,
        sizeof(s_schema_shutdown) / sizeof(s_schema_shutdown[0]),
        0, NULL, 0u, NULL},
    {"fortune", s_schema_fortune,
        sizeof(s_schema_fortune) / sizeof(s_schema_fortune[0]),
        0, NULL, 0u, NULL}
};


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_config_size_check[(sizeof(s_schema_config) /
        sizeof(s_schema_config[0]) == CONFIG_LINT_CONFIG_KEYS) ? 1 : -1];
