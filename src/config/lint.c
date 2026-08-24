/**
 * @file config/lint.c
 *
 * @brief Configuration file linter implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>      /* fprintf, snprintf */

/* Third-party includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Local includes */
#include <config/lint.h>


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

/**
 * @brief Shared state threaded through one file's recursive schema
 *        check
 *
 * Lets every finding for a file be grouped under that file's own name,
 * printed once as a header right before the first finding rather than
 * repeated on every line, which matters once more than one file has
 * something to report.
 */
typedef struct {
    const char *display_name; /**< Name to head the report with; may
                                   differ from the file's bare name on
                                   disk (see 'themes/' entries, headed
                                   by 'themes/<name>.json') */
    bool header_printed;
    int unknown_count;
} config_lint_report_td;


/* 'config.json' schema */

static const config_lint_key_td s_schema_screens[] = {
    {"count", NULL, 0u},
    {"desktops", NULL, 0u}
};

static const config_lint_key_td s_schema_topology[] = {
    {"screens", s_schema_screens,
        sizeof(s_schema_screens) / sizeof(s_schema_screens[0])}
};

static const config_lint_key_td s_schema_desktops_margins[] = {
    {"top", NULL, 0u},
    {"right", NULL, 0u},
    {"bottom", NULL, 0u},
    {"left", NULL, 0u}
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

static const config_lint_key_td s_schema_programs[] = {
    {"terminal", NULL, 0u},
    {"launcher", NULL, 0u},
    {"file-manager", NULL, 0u},
    {"web-browser", NULL, 0u},
    {"editor", NULL, 0u}
};

/* Accepted at config.json's own top level (built-in run-box) and
 * reused verbatim by memguard.json, same reasoning as 'programs' and
 * 'shutdown' above having one shared schema each */
static const config_lint_key_td s_schema_prompt[] = {
    {"is-enabled", NULL, 0u}
};

static const config_lint_key_td s_schema_scratchpad[] = {
    {"is-enabled", NULL, 0u},
    {"command", NULL, 0u},
    {"edge", NULL, 0u},
    {"width", NULL, 0u},
    {"height", NULL, 0u},
    {"ignore-margins", NULL, 0u}
};

static const config_lint_key_td s_schema_windows_focus[] = {
    {"policy", NULL, 0u},
    {"focus-new", NULL, 0u},
    {"raise", NULL, 0u}
};

static const config_lint_key_td s_schema_windows_placement[] = {
    {"policy", NULL, 0u},
    {"monitor", NULL, 0u},
    {"group-related", NULL, 0u}
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

static const config_lint_key_td s_schema_shutdown[] = {
    {"enable-emergency-shortcut", NULL, 0u},
    {"timeout-seconds", NULL, 0u}
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

static const config_lint_key_td s_schema_systray[] = {
    {"is-enabled", NULL, 0u},
    {"reserve-space", NULL, 0u},
    {"avoid-overlap", NULL, 0u},
    /* Reuses 'desktops.margins''s schema array: identical shape
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

static const config_lint_key_td s_schema_config[] = {
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


/* 'bindings.json schema' */

static const config_lint_key_td s_schema_wm_menus[] = {
    {"root", NULL, 0u},
    {"windows", NULL, 0u}
};

static const config_lint_key_td s_schema_go_to[] = {
    {"desktop0", NULL, 0u}, {"desktop1", NULL, 0u},
    {"desktop2", NULL, 0u}, {"desktop3", NULL, 0u},
    {"desktop4", NULL, 0u}, {"desktop5", NULL, 0u},
    {"desktop6", NULL, 0u}, {"desktop7", NULL, 0u},
    {"desktop8", NULL, 0u}, {"desktop9", NULL, 0u}
};

static const config_lint_key_td s_schema_kb_wm[] = {
    {"menus", s_schema_wm_menus,
        sizeof(s_schema_wm_menus) / sizeof(s_schema_wm_menus[0])},
    {"search", NULL, 0u},
    {"scratchpad", NULL, 0u},
    {"redraw", NULL, 0u},
    {"reload", NULL, 0u},
    {"quit", NULL, 0u},
    {"shortcuts", NULL, 0u},
    {"fortune", NULL, 0u},
    {"toggle-strutless-maximization", NULL, 0u}
};

static const config_lint_key_td s_schema_kb_desktop[] = {
    {"add", NULL, 0u},
    {"remove", NULL, 0u},
    {"show", NULL, 0u},
    {"go-to", s_schema_go_to,
        sizeof(s_schema_go_to) / sizeof(s_schema_go_to[0])}
};

static const config_lint_key_td s_schema_kb_launch[] = {
    {"terminal", NULL, 0u},
    {"launcher", NULL, 0u},
    {"file-manager", NULL, 0u},
    {"web-browser", NULL, 0u},
    {"editor", NULL, 0u}
};

static const config_lint_key_td s_schema_move_relative[] = {
    {"right", NULL, 0u}, {"left", NULL, 0u},
    {"up", NULL, 0u}, {"down", NULL, 0u}
};

static const config_lint_key_td s_schema_move_absolute[] = {
    {"center", NULL, 0u},
    {"top-left", NULL, 0u}, {"top-right", NULL, 0u},
    {"bottom-left", NULL, 0u}, {"bottom-right", NULL, 0u}
};

static const config_lint_key_td s_schema_window_move[] = {
    {"relative", s_schema_move_relative,
        sizeof(s_schema_move_relative) / sizeof(s_schema_move_relative[0])},
    {"absolute", s_schema_move_absolute,
        sizeof(s_schema_move_absolute) / sizeof(s_schema_move_absolute[0])}
};

static const config_lint_key_td s_schema_prev_next[] = {
    {"prev", NULL, 0u},
    {"next", NULL, 0u}
};

static const config_lint_key_td s_schema_compass[] = {
    {"north", NULL, 0u},
    {"south", NULL, 0u},
    {"east", NULL, 0u},
    {"west", NULL, 0u}
};

static const config_lint_key_td s_schema_window_send_to[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])},
    {"monitor", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])}
};

static const config_lint_key_td s_schema_kb_window[] = {
    {"arrange", NULL, 0u},
    {"close", NULL, 0u},
    {"decorate", NULL, 0u},
    {"deiconify-all", NULL, 0u},
    {"fullscreen", NULL, 0u},
    {"hide", NULL, 0u},
    {"iconify", NULL, 0u},
    {"iconify-all", NULL, 0u},
    {"info", NULL, 0u},
    {"kill", NULL, 0u},
    {"layer", NULL, 0u},
    {"maximize", NULL, 0u},
    {"pin", NULL, 0u},
    {"shade", NULL, 0u},
    {"move", s_schema_window_move,
        sizeof(s_schema_window_move) / sizeof(s_schema_window_move[0])},
    {"resize", s_schema_move_relative,
        sizeof(s_schema_move_relative) / sizeof(s_schema_move_relative[0])},
    {"send-to", s_schema_window_send_to,
        sizeof(s_schema_window_send_to) /
            sizeof(s_schema_window_send_to[0])}
};

static const config_lint_key_td s_schema_kb_cycle[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])},
    {"icon", s_schema_prev_next,
        sizeof(s_schema_prev_next) / sizeof(s_schema_prev_next[0])},
    {"window", s_schema_prev_next,
        sizeof(s_schema_prev_next) / sizeof(s_schema_prev_next[0])}
};

static const config_lint_key_td s_schema_keyboard[] = {
    {"wm", s_schema_kb_wm,
        sizeof(s_schema_kb_wm) / sizeof(s_schema_kb_wm[0])},
    {"desktop", s_schema_kb_desktop,
        sizeof(s_schema_kb_desktop) / sizeof(s_schema_kb_desktop[0])},
    {"launch", s_schema_kb_launch,
        sizeof(s_schema_kb_launch) / sizeof(s_schema_kb_launch[0])},
    {"window", s_schema_kb_window,
        sizeof(s_schema_kb_window) / sizeof(s_schema_kb_window[0])},
    {"cycle", s_schema_kb_cycle,
        sizeof(s_schema_kb_cycle) / sizeof(s_schema_kb_cycle[0])}
};

static const config_lint_key_td s_schema_mouse_window[] = {
    {"move", NULL, 0u},
    {"lower", NULL, 0u},
    {"resize", NULL, 0u}
};

static const config_lint_key_td s_schema_mouse_cycle[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])}
};

static const config_lint_key_td s_schema_mouse[] = {
    {"window", s_schema_mouse_window,
        sizeof(s_schema_mouse_window) / sizeof(s_schema_mouse_window[0])},
    {"cycle", s_schema_mouse_cycle,
        sizeof(s_schema_mouse_cycle) / sizeof(s_schema_mouse_cycle[0])}
};

static const config_lint_key_td s_schema_modifiers[] = {
    {"modc", NULL, 0u}, {"mods", NULL, 0u}, {"modl", NULL, 0u},
    {"mod1", NULL, 0u}, {"mod2", NULL, 0u}, {"mod3", NULL, 0u},
    {"mod4", NULL, 0u}, {"mod5", NULL, 0u}
};

static const config_lint_key_td s_schema_bindings[] = {
    {"modifiers", s_schema_modifiers,
        sizeof(s_schema_modifiers) / sizeof(s_schema_modifiers[0])},
    {"keyboard", s_schema_keyboard,
        sizeof(s_schema_keyboard) / sizeof(s_schema_keyboard[0])},
    {"mouse", s_schema_mouse,
        sizeof(s_schema_mouse) / sizeof(s_schema_mouse[0])}
};


/* `theme.json` schema: validated to the same full depth as every other
 *      fixed-shape file ('config.json', 'bindings.json', 'a11y.json').
 *      Unlike 'randr.json''s own "outputs" or 'rules.json''s own
 *      "rules", nothing under theme.json is genuinely polymorphic (see
 *      the opaque-subtree rule in 'config/lint.h' for what that means
 *      and why it does not apply here): every field's own shape is
 *      fixed and known ahead of time, so there is no risk of a false
 *      positive on a legitimate but less common shape the way there
 *      would be for those. */

/* The { font, color: {background, foreground}, border: {color,
 * width} } shape shared by every themeable surface's own row or
 * button style (menu.unselected/selected/label, dialog.button.
 * unselected/selected): never has its own 'opacity', since
 * '_NET_WM_WINDOW_OPACITY' is a per-window property that cannot vary
 * row by row or button by button; see 's_load_theme_colors''s own
 * doc comment, config/theme.c, for the full rationale. */
static const config_lint_key_td s_schema_theme_color[] = {
    {"background", NULL, 0u},
    {"foreground", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_border[] = {
    {"color", NULL, 0u},
    {"width", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_style[] = {
    {"font", NULL, 0u},
    {"color", s_schema_theme_color,
        sizeof(s_schema_theme_color) / sizeof(s_schema_theme_color[0])},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])}
};

/* The same shape, but for the six sites that stand for one real,
 * distinct window or window-state on their own and so do have their
 * own 'opacity' (window.active/inactive, icon.active/inactive,
 * systray, overlay); see 's_schema_theme_style' above for the
 * opacity-less variant and why the split exists at all. */
static const config_lint_key_td s_schema_theme_style_opacity[] = {
    {"font", NULL, 0u},
    {"color", s_schema_theme_color,
        sizeof(s_schema_theme_color) / sizeof(s_schema_theme_color[0])},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])},
    {"opacity", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_padding[] = {
    {"horizontal", NULL, 0u},
    {"vertical", NULL, 0u}
};

static const config_lint_key_td s_schema_titlebar_buttons_color[] = {
    {"on", NULL, 0u},
    {"off", NULL, 0u}
};

static const config_lint_key_td s_schema_titlebar_buttons[] = {
    {"color", s_schema_titlebar_buttons_color,
        sizeof(s_schema_titlebar_buttons_color) /
            sizeof(s_schema_titlebar_buttons_color[0])},
    {"left", NULL, 0u},
    {"right", NULL, 0u}
};

static const config_lint_key_td s_schema_titlebar[] = {
    {"height", NULL, 0u},
    {"alignment", NULL, 0u},
    {"padding", s_schema_theme_padding,
        sizeof(s_schema_theme_padding) / sizeof(s_schema_theme_padding[0])},
    {"buttons", s_schema_titlebar_buttons,
        sizeof(s_schema_titlebar_buttons) /
            sizeof(s_schema_titlebar_buttons[0])}
};

static const config_lint_key_td s_schema_theme_window[] = {
    {"is-decorated", NULL, 0u},
    {"titlebar", s_schema_titlebar,
        sizeof(s_schema_titlebar) / sizeof(s_schema_titlebar[0])},
    {"active", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0])},
    {"inactive", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0])}
};

static const config_lint_key_td s_schema_theme_icon[] = {
    {"is-captioned", NULL, 0u},
    {"show-pixmaps", NULL, 0u},
    {"show-hints", NULL, 0u},
    {"active", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0])},
    {"inactive", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0])}
};

static const config_lint_key_td s_schema_theme_systray_pixmap[] = {
    {"size", NULL, 0u},
    {"padding", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_systray_text[] = {
    {"gap", NULL, 0u},
    {"valign", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_systray[] = {
    {"font", NULL, 0u},
    {"color", s_schema_theme_color,
        sizeof(s_schema_theme_color) / sizeof(s_schema_theme_color[0])},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])},
    {"opacity", NULL, 0u},
    {"height", NULL, 0u},
    {"pixmap", s_schema_theme_systray_pixmap,
        sizeof(s_schema_theme_systray_pixmap) /
            sizeof(s_schema_theme_systray_pixmap[0])},
    {"text", s_schema_theme_systray_text,
        sizeof(s_schema_theme_systray_text) /
            sizeof(s_schema_theme_systray_text[0])}
};

static const config_lint_key_td s_schema_theme_desktop_color[] = {
    {"background", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_desktop[] = {
    {"color", s_schema_theme_desktop_color,
        sizeof(s_schema_theme_desktop_color) /
            sizeof(s_schema_theme_desktop_color[0])}
};

static const config_lint_key_td s_schema_theme_menu_disabled_color[] = {
    {"foreground", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_menu_disabled[] = {
    {"color", s_schema_theme_menu_disabled_color,
        sizeof(s_schema_theme_menu_disabled_color) /
            sizeof(s_schema_theme_menu_disabled_color[0])}
};

static const config_lint_key_td s_schema_theme_menu_separator[] = {
    {"color", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_menu[] = {
    {"unselected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"selected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"label", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"disabled", s_schema_theme_menu_disabled,
        sizeof(s_schema_theme_menu_disabled) /
            sizeof(s_schema_theme_menu_disabled[0])},
    {"separator", s_schema_theme_menu_separator,
        sizeof(s_schema_theme_menu_separator) /
            sizeof(s_schema_theme_menu_separator[0])},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])},
    {"opacity", NULL, 0u},
    {"padding", s_schema_theme_padding,
        sizeof(s_schema_theme_padding) / sizeof(s_schema_theme_padding[0])},
    {"show-pixmaps", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_search[] = {
    {"input", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"unselected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"selected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])}
};

static const config_lint_key_td s_schema_theme_prompt[] = {
    {"label", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"input", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])}
};

static const config_lint_key_td s_schema_theme_dialog_color[] = {
    {"background", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_dialog_label_color[] = {
    {"foreground", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_dialog_label[] = {
    {"font", NULL, 0u},
    {"color", s_schema_theme_dialog_label_color,
        sizeof(s_schema_theme_dialog_label_color) /
            sizeof(s_schema_theme_dialog_label_color[0])},
    {"padding", s_schema_theme_padding,
        sizeof(s_schema_theme_padding) / sizeof(s_schema_theme_padding[0])}
};

static const config_lint_key_td s_schema_theme_dialog_button[] = {
    {"unselected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"selected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0])},
    {"gap", NULL, 0u},
    {"padding", s_schema_theme_padding,
        sizeof(s_schema_theme_padding) / sizeof(s_schema_theme_padding[0])}
};

static const config_lint_key_td s_schema_theme_dialog[] = {
    {"color", s_schema_theme_dialog_color,
        sizeof(s_schema_theme_dialog_color) /
            sizeof(s_schema_theme_dialog_color[0])},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])},
    {"opacity", NULL, 0u},
    {"label", s_schema_theme_dialog_label,
        sizeof(s_schema_theme_dialog_label) /
            sizeof(s_schema_theme_dialog_label[0])},
    {"button", s_schema_theme_dialog_button,
        sizeof(s_schema_theme_dialog_button) /
            sizeof(s_schema_theme_dialog_button[0])}
};

static const config_lint_key_td s_schema_theme_xsettings_theme[] = {
    {"gtk-theme-name", NULL, 0u},
    {"icon-theme-name", NULL, 0u},
    {"cursor-theme-name", NULL, 0u},
    {"cursor-theme-size", NULL, 0u}
};

static const config_lint_key_td s_schema_theme_xsettings[] = {
    {"is-enabled", NULL, 0u},
    {"dpi", NULL, 0u},
    {"theme", s_schema_theme_xsettings_theme,
        sizeof(s_schema_theme_xsettings_theme) /
            sizeof(s_schema_theme_xsettings_theme[0])}
};

static const config_lint_key_td s_schema_theme_scratchpad[] = {
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])}
};

static const config_lint_key_td s_schema_theme_cycle[] = {
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0])}
};

static const config_lint_key_td s_schema_theme[] = {
    {"name", NULL, 0u},
    {"window", s_schema_theme_window,
        sizeof(s_schema_theme_window) / sizeof(s_schema_theme_window[0])},
    {"icon", s_schema_theme_icon,
        sizeof(s_schema_theme_icon) / sizeof(s_schema_theme_icon[0])},
    {"systray", s_schema_theme_systray,
        sizeof(s_schema_theme_systray) / sizeof(s_schema_theme_systray[0])},
    {"desktop", s_schema_theme_desktop,
        sizeof(s_schema_theme_desktop) / sizeof(s_schema_theme_desktop[0])},
    {"menu", s_schema_theme_menu,
        sizeof(s_schema_theme_menu) / sizeof(s_schema_theme_menu[0])},
    {"search", s_schema_theme_search,
        sizeof(s_schema_theme_search) / sizeof(s_schema_theme_search[0])},
    {"prompt", s_schema_theme_prompt,
        sizeof(s_schema_theme_prompt) / sizeof(s_schema_theme_prompt[0])},
    {"dialog", s_schema_theme_dialog,
        sizeof(s_schema_theme_dialog) / sizeof(s_schema_theme_dialog[0])},
    {"overlay", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0])},
    {"scratchpad", s_schema_theme_scratchpad,
        sizeof(s_schema_theme_scratchpad) /
            sizeof(s_schema_theme_scratchpad[0])},
    {"cycle", s_schema_theme_cycle,
        sizeof(s_schema_theme_cycle) /
            sizeof(s_schema_theme_cycle[0])},
    {"xsettings", s_schema_theme_xsettings,
        sizeof(s_schema_theme_xsettings) /
            sizeof(s_schema_theme_xsettings[0])}
};

static const config_lint_key_td s_schema_randr[] = {
    {"is-enabled", NULL, 0u},
    {"outputs", NULL, 0u}
};

static const config_lint_key_td s_schema_rules[] = {
    {"rules", NULL, 0u}
};

static const config_lint_key_td s_schema_session[] = {
    {"on-start", NULL, 0u},
    {"on-reload", NULL, 0u},
    {"on-exit", NULL, 0u}
};

static const config_lint_key_td s_schema_menu[] = {
    {"menu", NULL, 0u}
};

static const config_lint_key_td s_schema_a11y_interaction[] = {
    {"double-click-ms", NULL, 0u}
};

static const config_lint_key_td s_schema_a11y_focus_indicator[] = {
    {"min-border-width", NULL, 0u}
};

static const config_lint_key_td s_schema_a11y_urgency[] = {
    {"sound-bell", NULL, 0u},
    {"blink-interval-ms", NULL, 0u}
};

static const config_lint_key_td s_schema_a11y[] = {
    {"is-enabled", NULL, 0u},
    {"interaction", s_schema_a11y_interaction,
        sizeof(s_schema_a11y_interaction) /
            sizeof(s_schema_a11y_interaction[0])},
    {"focus-indicator", s_schema_a11y_focus_indicator,
        sizeof(s_schema_a11y_focus_indicator) /
            sizeof(s_schema_a11y_focus_indicator[0])},
    {"urgency", s_schema_a11y_urgency,
        sizeof(s_schema_a11y_urgency) /
            sizeof(s_schema_a11y_urgency[0])}
};


/* memguard.json schema: a stricter subset of config.json's own,
 *      since restricted-memory mode accepts fewer fields per section
 *      than an ordinary session does (see 'config.md' §10.1) */

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

static const config_lint_key_td s_schema_memguard[] = {
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


static const config_lint_file_spec_td s_files[] = {
    {"config.json", s_schema_config,
        sizeof(s_schema_config) / sizeof(s_schema_config[0]), true},
    {"bindings.json", s_schema_bindings,
        sizeof(s_schema_bindings) / sizeof(s_schema_bindings[0]), false},
    {"randr.json", s_schema_randr,
        sizeof(s_schema_randr) / sizeof(s_schema_randr[0]), false},
    {"rules.json", s_schema_rules,
        sizeof(s_schema_rules) / sizeof(s_schema_rules[0]), false},
    {"session.json", s_schema_session,
        sizeof(s_schema_session) / sizeof(s_schema_session[0]), false},
    {"menu.json", s_schema_menu,
        sizeof(s_schema_menu) / sizeof(s_schema_menu[0]), false},
    {"a11y.json", s_schema_a11y,
        sizeof(s_schema_a11y) / sizeof(s_schema_a11y[0]),
        false},
    {"memguard.json", s_schema_memguard,
        sizeof(s_schema_memguard) / sizeof(s_schema_memguard[0]),
        false}
};


/**
 * @brief Find the schema entry matching @p key, if any
 *
 * Matches the same way the real loaders do: case-insensitively, with
 * @c '-' and @c '_' treated as equivalent.
 *
 * @param key      Key name as it appears in the JSON file
 * @param schema   Schema entries to search
 * @param schema_count Number of entries in @p schema
 *
 * @return The matching entry, or @c NULL if none matches
 *
 * @note Complexity: @e O(n), where @e n is @p schema_count
 */
static const config_lint_key_td *s_find_key(const char *key,
        const config_lint_key_td *schema, size_t schema_count)
{
    char key_norm[JSON_FIELD_MAX];
    char entry_norm[JSON_FIELD_MAX];

    if (!json_field_normalize(key, key_norm, sizeof(key_norm))) {
        return NULL;
    }

    for (size_t i = 0u; i < schema_count; ++i) {
        if (!json_field_normalize(schema[i].name, entry_norm,
                    sizeof(entry_norm))) {
            continue;
        }
        if (safe_strcmp(key_norm, entry_norm) == 0) {
            return &schema[i];
        }
    }

    return NULL;
}


/**
 * @brief Recursively check one JSON object's keys against a schema
 *
 * @param obj          JSON object to check
 * @param schema       Schema entries valid at this level
 * @param schema_count Number of entries in @p schema
 * @param path         Dotted path to @p obj so far, for the report
 * @param report       This file's shared report state; the header is
 *                      printed here, lazily, on the first finding
 *
 * @note Complexity: @e O(n), where @e n is the number of keys in
 *       @p obj and everything nested under it
 */
static void s_lint_object(const cJSON *obj,
        const config_lint_key_td *schema, size_t schema_count,
        const char *path, config_lint_report_td *report)
{
    const cJSON *item;

    if (obj == NULL || !cJSON_IsObject((cJSON *) obj)) {
        return;
    }

    cJSON_ArrayForEach(item, obj) {
        char child_path[JSON_FIELD_MAX * 2u];
        const config_lint_key_td *match;

        if (item->string == NULL) {
            continue;
        }
        if (item->string[0] == '-' || item->string[0] == '_') {
            continue;
        }

        match = s_find_key(item->string, schema, schema_count);
        (void) snprintf(child_path, sizeof(child_path), "%s%s%s", path,
                (path[0] != '\0') ? "." : "", item->string);

        if (match == NULL) {
            if (!report->header_printed) {
                fprintf(stderr, "%s:\n", report->display_name);
                report->header_printed = true;
            }
            fprintf(stderr, "  %s: unknown key\n", child_path);
            report->unknown_count += 1;
            continue;
        }

        if (match->children != NULL && cJSON_IsObject((cJSON *) item)) {
            s_lint_object(item, match->children, match->children_count,
                    child_path, report);
        }
    }
}


/**
 * @brief Check one configuration file against its schema
 *
 * @param config_dir   Configuration directory the file lives under
 * @param spec         This file's name, schema, and whether it must
 *                      exist
 * @param display_name Name to head the report with if anything is
 *                      found; distinct from @c spec->filename so a
 *                      theme file can be headed by its path relative
 *                      to @p config_dir (e.g., @c "themes/default.json")
 *                      rather than its bare name alone
 * @param unknown_count Running count of unknown keys found; advanced
 *                      by this call
 *
 * @note Complexity: @e O(n), where @e n is the number of keys in the
 *       file
 */
static void s_lint_file(const char *restrict config_dir,
        const config_lint_file_spec_td *spec,
        const char *restrict display_name,
        int *unknown_count)
{
    char path[512];
    cJSON *json = NULL;
    config_lint_report_td report;

    (void) snprintf(path, sizeof(path), "%s/%s", config_dir,
            spec->filename);

    if (json_load_config(path, &json) != 0 || json == NULL) {
        if (spec->required) {
            fprintf(stderr, "%s:\n  file not found or unreadable" \
                    " ('%s')\n", display_name, path);
        }
        return;
    }

    report.display_name = display_name;
    report.header_printed = false;
    report.unknown_count = 0;

    if (cJSON_IsObject(json)) {
        s_lint_object(json, spec->schema, spec->schema_count, "",
                &report);
    }

    *unknown_count += report.unknown_count;

    cJSON_Delete(json);
}


/**
 * @brief Check every @c *.json file directly under
 *        @c <config_dir>/themes against the theme schema
 *
 * @param config_dir Configuration directory the @c themes
 *                    subdirectory lives under
 * @param unknown_count Running count of unknown keys found; advanced
 *                      by this call
 *
 * @note A missing @c themes subdirectory is not reported: unlike
 *       @c config.json, having no themes of one's own (using only
 *       whichever theme name @c config.json's own @c theme field
 *       names, which may resolve to a built-in default elsewhere) is
 *       an entirely ordinary setup, not an oversight
 * @note Complexity: @e O(n), where @e n is the total number of keys
 *       across every theme file found
 */
static void s_lint_themes(const char *config_dir, int *unknown_count)
{
    char themes_dir[512];
    DIR *dir;
    const struct dirent *entry;

    (void) snprintf(themes_dir, sizeof(themes_dir), "%s/themes",
            config_dir);

    dir = opendir(themes_dir);
    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        config_lint_file_spec_td spec;
        char display_name[512];
        size_t name_len = safe_strlen(entry->d_name);

        if (name_len < 6u ||
                safe_strcmp(entry->d_name + name_len - 5u, ".json") != 0) {
            continue;
        }

        spec.filename = entry->d_name;
        spec.schema = s_schema_theme;
        spec.schema_count =
            sizeof(s_schema_theme) / sizeof(s_schema_theme[0]);
        spec.required = false;

        (void) snprintf(display_name, sizeof(display_name),
                "themes/%s", entry->d_name);
        s_lint_file(themes_dir, &spec, display_name, unknown_count);
    }

    closedir(dir);
}


/* Check every known JSON configuration file for unknown keys */
int config_lint_run(const char *config_dir)
{
    DIR *dir;
    int unknown_count = 0;

    if (config_dir == NULL) {
        return -1;
    }

    dir = opendir(config_dir);
    if (dir == NULL) {
        /* Quoted so a stray leading character (e.g., an '=' from
         * typing '-c=<dir>', a GNU long-option convention getopt does
         * not apply to a short option like '-c') stands out rather
         * than blending into the surrounding text */
        fprintf(stderr, "'%s': cannot open configuration directory\n",
                config_dir);
        return -1;
    }
    closedir(dir);

    for (size_t i = 0u; i < sizeof(s_files) / sizeof(s_files[0]); ++i) {
        s_lint_file(config_dir, &s_files[i], s_files[i].filename,
                &unknown_count);
    }
    s_lint_themes(config_dir, &unknown_count);

    if (unknown_count == 0) {
        fprintf(stderr, "No unknown keys found.\n");
    } else {
        fprintf(stderr, "%d unknown key%s found.\n", unknown_count,
                (unknown_count == 1) ? "" : "s");
    }

    return unknown_count;
}
