/**
 * @file config/memguard.c
 *
 * @brief Restricted-memory mode's own configuration profile
 *        (@c memguard.json) implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* NULL, size_t */
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* calloc */
#include <string.h>     /* strchr */
#include <strings.h>    /* strncasecmp */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/desktop.h>
#include <defs/memguard.h>

/* Project includes */
#include <logger.h>
#include <sn.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>
#include <config/memguard.h>


/**
 * @brief Whether @p font already names some variant of the @c "fixed"
 *        X core font family
 *
 * Recognizes both forms a theme's own font field can hold: a simple
 * alias, where the family is the leading word up to the first space
 * (e.g. @c "fixed", @c "fixed bold", @c "fixed-14"), and a full XLFD
 * pattern, where the family is the second @c '-'-delimited field
 * (e.g. @c "-misc-fixed-bold-r-normal--0-120-75-75-c-0-iso10646-1").
 * Either form lets a person still pick a specific size or encoding
 * while staying on the light X core rendering path, rather than the
 * plain literal strings @c "fixed" and @c "fixed bold" alone.
 *
 * @param font Font field to check, e.g. @c
 *             config->theme.window.active.font
 *
 * @return @c true if @p font's own family is @c "fixed"
 *         (case-insensitive)
 *
 * @note Complexity: @e O(n), where @e n is the length of @p font
 */
static bool s_memguard_is_fixed_variant(const char *font)
{
    const char *family_start;
    const char *family_end;
    size_t family_len;

    if (font == NULL || font[0] == '\0') {
        return false;
    }

    if (font[0] == '-') {
        const char *dash1 = strchr(font + 1, '-');

        if (dash1 == NULL) {
            return false;
        }
        family_start = dash1 + 1;
        family_end = strchr(family_start, '-');
    } else {
        family_start = font;
        family_end = strchr(font, ' ');
    }

    family_len = (family_end != NULL)
        ? (size_t) (family_end - family_start)
        : safe_strlen(family_start);

    return (family_len == 5u) &&
        (strncasecmp(family_start, "fixed", 5u) == 0);
}


/**
 * @brief Apply restricted-memory mode's own theme restrictions on top
 *        of whatever @p config->theme was just loaded from
 *
 * Every font field not already naming some variant of @c "fixed" (see
 * @a s_memguard_is_fixed_variant) is replaced outright with plain
 * @c MEMGUARD_FONT_NAME.  @c xsettings publishing, icon pixmaps, and
 * icon hint indicators are all forced off unconditionally.  Every
 * other theme field, colors, decoration, and @c is-captioned
 * included, is left exactly as the theme file specified: none of
 * those carry the ongoing memory cost the font backend and pixmap
 * compositing do.
 *
 * @param config Configuration structure whose already-loaded theme
 *               this restricts; must not be @c NULL
 *
 * @note Complexity: @e O(1), a fixed number of fields
 */
static void s_memguard_restrict_theme(config_td *config)
{
    char *const font_fields[] = {
        config->theme.dialog.button.selected.font,
        config->theme.dialog.button.unselected.font,
        config->theme.dialog.label.font,
        config->theme.icon.active.font,
        config->theme.icon.inactive.font,
        config->theme.menu.label.font,
        config->theme.menu.selected.font,
        config->theme.menu.unselected.font,
        config->theme.overlay.font,
        config->theme.systray.style.font,
        config->theme.window.active.font,
        config->theme.window.inactive.font,
    };

    for (size_t i = 0u; i < sizeof(font_fields) / sizeof(font_fields[0]);
            ++i) {
        if (!s_memguard_is_fixed_variant(font_fields[i])) {
            safe_strncpy(font_fields[i], MEMGUARD_FONT_NAME,
                    CONFIG_MAX_LENGTH_FONTNAME);
        }
    }

    config->theme.xsettings.is_enabled = false;
    config->theme.icon.show_pixmaps = false;
    config->theme.icon.show_hints = false;
}


/**
 * @brief Load @c memguard.json's own configurable fields into
 *        @p config
 *
 * Everything restricted-memory mode still lets a person configure:
 * the active theme's name, launched programs, desktop margins, the
 * systray block (via @a ci_config_load_systray, shared verbatim with
 * @c config.json's own identical @c "systray" object), and the
 * emergency shortcut.  A no-op, leaving every field at whatever
 * @a config_set_default_values_memguard already set, for any of these
 * not present in the file.
 *
 * @param filename Path to @c memguard.json
 * @param config   Configuration structure to update
 *
 * @return @c 0 on success, @c 1 if @p filename could not be loaded or
 *         parsed
 *
 * @note Complexity: @e O(n), where @e n is the size of @p filename
 */
static int s_memguard_load_json(const char *filename, config_td *config)
{
    cJSON *json;
    cJSON *theme_item;
    cJSON *programs;
    cJSON *desktops_item;
    cJSON *margins;

    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    theme_item = json_get_item(json, "theme");
    if (theme_item != NULL && cJSON_IsString(theme_item)) {
        safe_strncpy(config->base.theme, theme_item->valuestring,
                CONFIG_MAX_LENGTH_NAME);
    }

    programs = cJSON_GetObjectItem(json, "programs");
    if (programs != NULL) {
        json_load_string(programs, "editor",
                config->base.programs.editor,
                sizeof(config->base.programs.editor));
        json_load_string(programs, "file-manager",
                config->base.programs.file_manager,
                sizeof(config->base.programs.file_manager));
        json_load_string(programs, "launcher",
                config->base.programs.launcher,
                sizeof(config->base.programs.launcher));
        json_load_string(programs, "terminal",
                config->base.programs.terminal,
                sizeof(config->base.programs.terminal));
        json_load_string(programs, "web-browser",
                config->base.programs.web_browser,
                sizeof(config->base.programs.web_browser));
    }

    desktops_item = cJSON_GetObjectItem(json, "desktops");
    if (desktops_item != NULL) {
        margins = cJSON_GetObjectItem(desktops_item, "margins");
        if (margins != NULL) {
            json_load_uint(margins, "top",
                    &config->desktops.margins.top);
            json_load_uint(margins, "right",
                    &config->desktops.margins.right);
            json_load_uint(margins, "bottom",
                    &config->desktops.margins.bottom);
            json_load_uint(margins, "left",
                    &config->desktops.margins.left);
        }
    }

    ci_config_load_systray(json, &config->base);

    json_load_bool(json, "enable-emergency-shortcut",
            &config->base.enable_emergency_shortcut);

    cJSON_Delete(json);
    return 0;
}


/* Allocate a new configuration structure, without populating it */
config_td *config_init_memguard(void)
{
    config_td *config;

    LOGGER_DEBUG("Initializing restricted-memory mode configuration" \
            " structure", L_NARG);

    config = calloc(1, sizeof(config_td));
    if (config == NULL) {
        LOGGER_ERROR("Failed to allocate memory for restricted-" \
                "memory mode configuration structure", L_NARG);
    }

    return config;
}


/* Populate a configuration structure with restricted-memory mode's
 * own fixed profile */
void config_set_default_values_memguard(config_td *config)
{
    if (config == NULL) {
        return;
    }

    config->base.theme[0] = '\0';
    config->base.screen_count = 1u;

    /* A single screen, a single desktop: neither warp nor cycle mean
     * anything with only one desktop to switch to. */
    config->desktops.warp = false;
    config->desktops.cycle = false;
    config->desktops.margins.top = 0u;
    config->desktops.margins.right = 0u;
    config->desktops.margins.bottom = 0u;
    config->desktops.margins.left = 0u;

    config->base.screens[0].desktop_count = 1u;
    config->base.screens[0].desktop_inaugural = 0u;
    safe_strncpy(config->base.screens[0].desktops[0].name, "Desktop 0",
            CONFIG_MAX_LENGTH_NAME);
    config->base.screens[0].desktops[0].settings.background.color =
        WM_DESKTOP_BG_COLOR_UNSET;

    /* Launched-program defaults, in case memguard.json does not
     * specify its own; identical to config_set_default_values's own
     * defaults, since restricted-memory mode has no particular reason
     * to prefer different programs. */
    safe_strcpy(config->base.programs.terminal, "xterm");
    safe_strcpy(config->base.programs.launcher, "gmrun");
    safe_strcpy(config->base.programs.file_manager, "pcmanfm");
    safe_strcpy(config->base.programs.editor, "gvim");
    safe_strcpy(config->base.programs.web_browser, "firefox");

    config->base.windows.move_step = 10u;
    config->base.windows.resize_step = 20u;
    config->base.windows.snap = 4u;
    config->base.windows.show_geom = false;
    config->base.windows.gravity = CONFIG_GRAVITY_NORTH_WEST;
    config->base.windows.focus_policy = CONFIG_FOCUS_POLICY_CLICK;
    config->base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_SMART;
    config->base.windows.monitor_policy = CONFIG_PLACEMENT_MONITOR_POINTER;
    config->base.windows.group_related = false;
    config->base.windows.focus.is_new_focused = true;
    config->base.windows.focus.is_raised_on_focus = false;

    config->base.icons.placement_policy = CONFIG_ICON_PLACEMENT_BOTTOM;
    config->base.icons.show_geom = false;

    config->base.enable_emergency_shortcut = false;
    config->base.enable_fortune_shortcut = false;

    config->base.startup_notification.is_enabled = false;
    config->base.startup_notification.timeout_seconds =
        (uint32_t) SN_TIMEOUT_SECONDS;

    config->base.show_desktop_overlay = false;

    config->base.menus.root.position = CONFIG_MENU_POSITION_UNDER_MOUSE;
    config->base.menus.windows.position = CONFIG_MENU_POSITION_UNDER_MOUSE;

    /* Systray defaults, in case memguard.json does not specify its
     * own; a lower 'battery.poll_seconds' than an ordinary session's
     * own default is the one deliberate difference here, both to
     * check less often and since a stale battery reading for a few
     * extra seconds matters little either way. */
    config->base.systray.is_enabled = true;
    /* Fixed false for this mode, deliberately not something
     * memguard.json is allowed to configure; see is_embedding_
     * enabled's own doc comment in config.h */
    config->base.systray.is_embedding_enabled = false;
    config->base.systray.reserve_space = false;
    config->base.systray.margins.top = 0u;
    config->base.systray.margins.right = 0u;
    config->base.systray.margins.bottom = 0u;
    config->base.systray.margins.left = 0u;
    config->base.systray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    config->base.systray.monitor.anchor = CONFIG_SYSTRAY_MONITOR_SURFACE;
    config->base.systray.monitor.index = 0u;
    config->base.systray.order = CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
    config->base.systray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    config->base.systray.clock.is_enabled = true;
    safe_strcpy(config->base.systray.clock.format, "%a %R");

    config->base.systray.battery.is_enabled = false;
    config->base.systray.battery.threshold.charged = 100u;
    config->base.systray.battery.threshold.low = 20u;
    config->base.systray.battery.threshold.critical = 5u;
    config->base.systray.battery.backend.type = CONFIG_BATTERY_BACKEND_ACPI;
    config->base.systray.battery.backend.number = 0u;
    config->base.systray.battery.poll_seconds = 30u;

    config->base.systray.text.order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    config->base.systray.text.order[1] = CONFIG_SYSTRAY_TEXT_BATTERY;
    config->base.systray.text.order_count = 2u;
    config->base.systray.text.position = CONFIG_SYSTRAY_TEXT_LEFT;

    /* RandR output-profile management: never consulted at all in
     * this mode, so this stays at its off/empty state regardless. */
    config->randr.is_enabled = false;
    config->randr.output_count = 0u;
}


/* Load restricted-memory mode's own configuration, entirely
 * independent of config_load's own config.json path */
int config_load_memguard(config_td *config, const char *config_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char config_memguard_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_bindings_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_theme_file[CONFIG_MAX_LENGTH_PATH_THEME];
    int result = 0;

    config_set_default_values_memguard(config);

    config_resolve_dir(config_prefix, config_dir);

    LOGGER_DEBUG("Loading restricted-memory mode configuration from" \
            " files on: '%s'", config_dir);

    snprintf(config_memguard_file, sizeof(config_memguard_file),
            "%s/%s", config_dir, CONFIG_FILENAME_MEMGUARD);
    if (s_memguard_load_json(config_memguard_file, config) != 0) {
        LOGGER_WARNING("Restricted-memory mode configuration could" \
                " not be loaded from '%s'; its own fixed defaults" \
                " will be used", config_memguard_file);
        result = 1;
    } else {
        LOGGER_DEBUG("Loaded restricted-memory mode configuration" \
                " from '%s'", config_memguard_file);
    }

    snprintf(config_bindings_file, sizeof(config_bindings_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BINDINGS);
    if (config_load_bindings(config_bindings_file,
                &(config->bindings)) != 0) {
        LOGGER_ERROR("Failed to load bindings from: '%s'; default" \
                " bindings will be used", config_bindings_file);
    } else {
        LOGGER_DEBUG("Loaded key/mouse bindings from '%s'",
                config_bindings_file);
    }

    snprintf(config_theme_file, sizeof(config_theme_file),
            "%s/%s/%s.json", config_dir, CONFIG_DIR_THEMES,
            config->base.theme);
    if (safe_strlen(config->base.theme) == 0u) {
        LOGGER_NOTICE("No theme specified in restricted-memory mode" \
                " configuration; default will be used", L_NARG);
    } else {
        LOGGER_DEBUG("Loading theme '%s' from '%s'",
                config->base.theme, config_theme_file);
        if (config_load_theme(config_theme_file, &(config->theme)) != 0) {
            LOGGER_WARNING("Failed to load theme from: '%s';" \
                    " default theme will be used", config_theme_file);
        } else {
            LOGGER_DEBUG("Loaded theme '%s' (\"%s\") from '%s'",
                    config->base.theme, config->theme.name,
                    config_theme_file);
        }
    }

    s_memguard_restrict_theme(config);

    return result;
}
