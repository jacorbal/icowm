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

const config_lint_key_td s_schema_theme[] = {
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


/* A key added to 's_schema_theme' without its count in the header
 * following makes this declaration negative, and so the build fail
 * here rather than the linter read past the end of the table */
typedef char config_lint_theme_size_check
    [(sizeof(s_schema_theme) /
      sizeof(s_schema_theme[0]) == CONFIG_LINT_THEME_KEYS)
     ? 1 : -1];
