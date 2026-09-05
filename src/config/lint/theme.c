/**
 * @file config/lint/theme.c
 *
 * @brief Theme schema, used by every file under 'themes/'
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
#include <config/lint/theme.h>


/* The { font, color: {background, foreground}, border: {color,
 * width} } shape shared by every themeable surface's row or
 * button style (menu.unselected/selected/label, dialog.button.
 * unselected/selected): never has its 'opacity', since
 * '_NET_WM_WINDOW_OPACITY' is a per-window property that cannot vary
 * row by row or button by button; see 's_load_theme_colors''s
 * doc comment, config/theme.c, for the full rationale. */
static const config_lint_key_td s_schema_theme_color[] = {
    {"background", NULL, 0u,
        0, NULL, 0u, NULL},
    {"foreground", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_border[] = {
    {"color", NULL, 0u,
        0, NULL, 0u, NULL},
    {"width", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_style[] = {
    {"font", NULL, 0u,
        0, NULL, 0u, NULL},
    {"color", s_schema_theme_color,
        sizeof(s_schema_theme_color) / sizeof(s_schema_theme_color[0]),
        0, NULL, 0u, NULL},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL}
};

/* The same shape, but for the six sites that stand for one real,
 * distinct window or window-state on their own and so do have their
 * own 'opacity' (window.active/inactive, icon.active/inactive,
 * systray, overlay); see 's_schema_theme_style' above for the
 * opacity-less variant and why the split exists at all. */
static const config_lint_key_td s_schema_theme_style_opacity[] = {
    {"font", NULL, 0u,
        0, NULL, 0u, NULL},
    {"color", s_schema_theme_color,
        sizeof(s_schema_theme_color) / sizeof(s_schema_theme_color[0]),
        0, NULL, 0u, NULL},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL},
    {"opacity", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_padding[] = {
    {"horizontal", NULL, 0u,
        0, NULL, 0u, NULL},
    {"vertical", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_titlebar_buttons_color[] = {
    {"on", NULL, 0u,
        0, NULL, 0u, NULL},
    {"off", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_titlebar_buttons[] = {
    {"color", s_schema_titlebar_buttons_color,
        sizeof(s_schema_titlebar_buttons_color) /
            sizeof(s_schema_titlebar_buttons_color[0]),
        0, NULL, 0u, NULL},
    {"left", NULL, 0u,
        0, NULL, 0u, NULL},
    {"right", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_titlebar[] = {
    {"height", NULL, 0u,
        0, NULL, 0u, NULL},
    {"alignment", NULL, 0u,
        0, NULL, 0u, NULL},
    {"padding", s_schema_theme_padding,
        sizeof(s_schema_theme_padding) / sizeof(s_schema_theme_padding[0]),
        0, NULL, 0u, NULL},
    {"buttons", s_schema_titlebar_buttons,
        sizeof(s_schema_titlebar_buttons) /
            sizeof(s_schema_titlebar_buttons[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_window[] = {
    {"is-decorated", NULL, 0u,
        0, NULL, 0u, NULL},
    {"titlebar", s_schema_titlebar,
        sizeof(s_schema_titlebar) / sizeof(s_schema_titlebar[0]),
        0, NULL, 0u, NULL},
    {"active", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0]),
        0, NULL, 0u, NULL},
    {"inactive", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_icon[] = {
    {"is-captioned", NULL, 0u,
        0, NULL, 0u, NULL},
    {"show-pixmaps", NULL, 0u,
        0, NULL, 0u, NULL},
    {"show-hints", NULL, 0u,
        0, NULL, 0u, NULL},
    {"active", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0]),
        0, NULL, 0u, NULL},
    {"inactive", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_systray_pixmap[] = {
    {"size", NULL, 0u,
        0, NULL, 0u, NULL},
    {"padding", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_systray_text[] = {
    {"gap", NULL, 0u,
        0, NULL, 0u, NULL},
    {"valign", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_systray[] = {
    {"font", NULL, 0u,
        0, NULL, 0u, NULL},
    {"color", s_schema_theme_color,
        sizeof(s_schema_theme_color) / sizeof(s_schema_theme_color[0]),
        0, NULL, 0u, NULL},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL},
    {"opacity", NULL, 0u,
        0, NULL, 0u, NULL},
    {"height", NULL, 0u,
        0, NULL, 0u, NULL},
    {"pixmap", s_schema_theme_systray_pixmap,
        sizeof(s_schema_theme_systray_pixmap) /
            sizeof(s_schema_theme_systray_pixmap[0]),
        0, NULL, 0u, NULL},
    {"text", s_schema_theme_systray_text,
        sizeof(s_schema_theme_systray_text) /
            sizeof(s_schema_theme_systray_text[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_desktop_color[] = {
    {"background", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_desktop[] = {
    {"color", s_schema_theme_desktop_color,
        sizeof(s_schema_theme_desktop_color) /
            sizeof(s_schema_theme_desktop_color[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_menu_disabled_color[] = {
    {"foreground", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_menu_disabled[] = {
    {"color", s_schema_theme_menu_disabled_color,
        sizeof(s_schema_theme_menu_disabled_color) /
            sizeof(s_schema_theme_menu_disabled_color[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_menu_separator[] = {
    {"color", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_menu[] = {
    {"unselected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"selected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"label", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"disabled", s_schema_theme_menu_disabled,
        sizeof(s_schema_theme_menu_disabled) /
            sizeof(s_schema_theme_menu_disabled[0]),
        0, NULL, 0u, NULL},
    {"separator", s_schema_theme_menu_separator,
        sizeof(s_schema_theme_menu_separator) /
            sizeof(s_schema_theme_menu_separator[0]),
        0, NULL, 0u, NULL},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL},
    {"opacity", NULL, 0u,
        0, NULL, 0u, NULL},
    {"padding", s_schema_theme_padding,
        sizeof(s_schema_theme_padding) / sizeof(s_schema_theme_padding[0]),
        0, NULL, 0u, NULL},
    {"show-pixmaps", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_search[] = {
    {"input", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"unselected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"selected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_prompt[] = {
    {"label", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"input", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_dialog_color[] = {
    {"background", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_dialog_label_color[] = {
    {"foreground", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_dialog_label[] = {
    {"font", NULL, 0u,
        0, NULL, 0u, NULL},
    {"color", s_schema_theme_dialog_label_color,
        sizeof(s_schema_theme_dialog_label_color) /
            sizeof(s_schema_theme_dialog_label_color[0]),
        0, NULL, 0u, NULL},
    {"padding", s_schema_theme_padding,
        sizeof(s_schema_theme_padding) / sizeof(s_schema_theme_padding[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_dialog_button[] = {
    {"unselected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"selected", s_schema_theme_style,
        sizeof(s_schema_theme_style) / sizeof(s_schema_theme_style[0]),
        0, NULL, 0u, NULL},
    {"gap", NULL, 0u,
        0, NULL, 0u, NULL},
    {"padding", s_schema_theme_padding,
        sizeof(s_schema_theme_padding) / sizeof(s_schema_theme_padding[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_dialog[] = {
    {"color", s_schema_theme_dialog_color,
        sizeof(s_schema_theme_dialog_color) /
            sizeof(s_schema_theme_dialog_color[0]),
        0, NULL, 0u, NULL},
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL},
    {"opacity", NULL, 0u,
        0, NULL, 0u, NULL},
    {"label", s_schema_theme_dialog_label,
        sizeof(s_schema_theme_dialog_label) /
            sizeof(s_schema_theme_dialog_label[0]),
        0, NULL, 0u, NULL},
    {"button", s_schema_theme_dialog_button,
        sizeof(s_schema_theme_dialog_button) /
            sizeof(s_schema_theme_dialog_button[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_xsettings_theme[] = {
    {"gtk-theme-name", NULL, 0u,
        0, NULL, 0u, NULL},
    {"icon-theme-name", NULL, 0u,
        0, NULL, 0u, NULL},
    {"cursor-theme-name", NULL, 0u,
        0, NULL, 0u, NULL},
    {"cursor-theme-size", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_xsettings[] = {
    {"is-enabled", NULL, 0u,
        0, NULL, 0u, NULL},
    {"dpi", NULL, 0u,
        0, NULL, 0u, NULL},
    {"theme", s_schema_theme_xsettings_theme,
        sizeof(s_schema_theme_xsettings_theme) /
            sizeof(s_schema_theme_xsettings_theme[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_scratchpad[] = {
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_theme_cycle[] = {
    {"border", s_schema_theme_border,
        sizeof(s_schema_theme_border) / sizeof(s_schema_theme_border[0]),
        0, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_theme[] = {
    {"name", NULL, 0u,
        0, NULL, 0u, NULL},
    {"window", s_schema_theme_window,
        sizeof(s_schema_theme_window) / sizeof(s_schema_theme_window[0]),
        0, NULL, 0u, NULL},
    {"icon", s_schema_theme_icon,
        sizeof(s_schema_theme_icon) / sizeof(s_schema_theme_icon[0]),
        0, NULL, 0u, NULL},
    {"systray", s_schema_theme_systray,
        sizeof(s_schema_theme_systray) / sizeof(s_schema_theme_systray[0]),
        0, NULL, 0u, NULL},
    {"desktop", s_schema_theme_desktop,
        sizeof(s_schema_theme_desktop) / sizeof(s_schema_theme_desktop[0]),
        0, NULL, 0u, NULL},
    {"menu", s_schema_theme_menu,
        sizeof(s_schema_theme_menu) / sizeof(s_schema_theme_menu[0]),
        0, NULL, 0u, NULL},
    {"search", s_schema_theme_search,
        sizeof(s_schema_theme_search) / sizeof(s_schema_theme_search[0]),
        0, NULL, 0u, NULL},
    {"prompt", s_schema_theme_prompt,
        sizeof(s_schema_theme_prompt) / sizeof(s_schema_theme_prompt[0]),
        0, NULL, 0u, NULL},
    {"dialog", s_schema_theme_dialog,
        sizeof(s_schema_theme_dialog) / sizeof(s_schema_theme_dialog[0]),
        0, NULL, 0u, NULL},
    {"overlay", s_schema_theme_style_opacity,
        sizeof(s_schema_theme_style_opacity) /
            sizeof(s_schema_theme_style_opacity[0]),
        0, NULL, 0u, NULL},
    {"scratchpad", s_schema_theme_scratchpad,
        sizeof(s_schema_theme_scratchpad) /
            sizeof(s_schema_theme_scratchpad[0]),
        0, NULL, 0u, NULL},
    {"cycle", s_schema_theme_cycle,
        sizeof(s_schema_theme_cycle) /
            sizeof(s_schema_theme_cycle[0]),
        0, NULL, 0u, NULL},
    {"xsettings", s_schema_theme_xsettings,
        sizeof(s_schema_theme_xsettings) /
            sizeof(s_schema_theme_xsettings[0]),
        0, NULL, 0u, NULL}
};


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_theme_size_check
    [(sizeof(s_schema_theme) /
      sizeof(s_schema_theme[0]) == CONFIG_LINT_THEME_KEYS)
     ? 1 : -1];
