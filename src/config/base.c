/**
 * @file config/base.c
 *
 * @brief Base configuration loader implementation
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

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/**
 * @brief Parse focus policy text into configuration enumeration
 *
 * @param value Focus policy string from configuration
 *
 * @return Parsed focus policy enumeration value
 *
 * @note Supported values are @c click and @c follow-mouse
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_focus_policy_e
    s_config_parse_focus_policy(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_FOCUS_POLICY_CLICK;
    }

    if (safe_strcmp(value_norm, "follow-mouse") == 0) {
        return CONFIG_FOCUS_POLICY_FOLLOW_MOUSE;
    }

    return CONFIG_FOCUS_POLICY_CLICK;
}


/**
 * @brief Parse placement policy text into configuration enumeration
 *
 * @param value Placement policy string from configuration
 *
 * @return Parsed placement policy enumeration value
 *
 * @note Supported values are @c smart, @c cascade,
 *       @c centered, and @c under-mouse
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_placement_policy_e
    s_config_parse_placement_policy(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_PLACEMENT_POLICY_SMART;
    }

    if (safe_strcmp(value_norm, "cascade") == 0) {
        return CONFIG_PLACEMENT_POLICY_CASCADE;
    }
    if (safe_strcmp(value_norm, "centered") == 0) {
        return CONFIG_PLACEMENT_POLICY_CENTERED;
    }
    if (safe_strcmp(value_norm, "under-mouse") == 0) {
        return CONFIG_PLACEMENT_POLICY_UNDER_MOUSE;
    }

    return CONFIG_PLACEMENT_POLICY_SMART;
}


/**
 * @brief Parse default window gravity text into configuration
 *        enumeration
 *
 * @param value Gravity string from configuration
 *
 * @return Parsed gravity enumeration value
 *
 * @note Supported values are @c north-west, @c north, @c north-east,
 *       @c east, @c south-east, @c south, @c south-west, @c west,
 *       @c center, and @c static
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_gravity_e
    s_config_parse_gravity(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_GRAVITY_NORTH_WEST;
    }

    if (safe_strcmp(value_norm, "north") == 0) {
        return CONFIG_GRAVITY_NORTH;
    }
    if (safe_strcmp(value_norm, "north-east") == 0) {
        return CONFIG_GRAVITY_NORTH_EAST;
    }
    if (safe_strcmp(value_norm, "east") == 0) {
        return CONFIG_GRAVITY_EAST;
    }
    if (safe_strcmp(value_norm, "south-east") == 0) {
        return CONFIG_GRAVITY_SOUTH_EAST;
    }
    if (safe_strcmp(value_norm, "south") == 0) {
        return CONFIG_GRAVITY_SOUTH;
    }
    if (safe_strcmp(value_norm, "south-west") == 0) {
        return CONFIG_GRAVITY_SOUTH_WEST;
    }
    if (safe_strcmp(value_norm, "west") == 0) {
        return CONFIG_GRAVITY_WEST;
    }
    if (safe_strcmp(value_norm, "center") == 0) {
        return CONFIG_GRAVITY_CENTER;
    }
    if (safe_strcmp(value_norm, "static") == 0) {
        return CONFIG_GRAVITY_STATIC;
    }

    return CONFIG_GRAVITY_NORTH_WEST;
}


/**
 * @brief Parse icon placement policy text into configuration enumeration
 *
 * @param value Icon placement string from configuration
 *
 * @return Parsed icon placement policy enumeration value
 *
 * @note Supported values are @c bottom, @c top, @c left, @c right,
 *       and @c smart
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_icon_placement_e s_config_parse_icon_placement(
        const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_ICON_PLACEMENT_BOTTOM;
    }

    if (safe_strcmp(value_norm, "top") == 0) {
        return CONFIG_ICON_PLACEMENT_TOP;
    }
    if (safe_strcmp(value_norm, "left") == 0) {
        return CONFIG_ICON_PLACEMENT_LEFT;
    }
    if (safe_strcmp(value_norm, "right") == 0) {
        return CONFIG_ICON_PLACEMENT_RIGHT;
    }
    if (safe_strcmp(value_norm, "smart") == 0) {
        return CONFIG_ICON_PLACEMENT_SMART;
    }

    return CONFIG_ICON_PLACEMENT_BOTTOM;
}


/**
 * @brief Load a desktop entry from a JSON object
 *
 * Loads the desktop name and its background color from a JSON object
 * into the provided output fields.
 *
 * @param desktop_json JSON object with desktop settings
 * @param name_out     Destination desktop name
 * @param settings_out Destination desktop settings
 *
 * @note Complexity: @e O(n + m), where @e n is the length of the field
 *       names processed and @e m is the length of the desktop name
 */
static void s_config_load_desktop_entry(cJSON *desktop_json,
        char *name_out, struct desktop_settings_s *settings_out)
{
    if (desktop_json == NULL || name_out == NULL ||
            settings_out == NULL) {
        return;
    }

    json_load_string(desktop_json, "name", name_out,
            CONFIG_MAX_LENGTH_NAME);

    if (json_load_color(desktop_json, "background-color",
                &settings_out->background.color) != 0) {
        LOGGER_WARNING("Failed to load JSON string:"
                " 'background-color'; desktop '%s' keeps its"
                " default background color", name_out);
    }
}


/* Load base configuration */
int config_load_base(const char *filename,
        struct config_base_s *config_base)
{
    cJSON *json;
    cJSON *programs;
    cJSON *windows;
    cJSON *screen_settings;
    cJSON *icons;

    LOGGER_TRACE("Preparing to parse base configuration from file" \
            " '%s'", filename);

    /* Load file, or exit */
    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    /* Theme name */
    if (json_load_string(json, "theme", config_base->theme,
                CONFIG_MAX_LENGTH_FILENAME) != 0) {
        LOGGER_WARNING("Invalid theme specified:" \
                " '%s'; default configuration will be used",
                config_base->theme);
        config_base->theme[0] = '\0';
    }

    screen_settings = cJSON_GetObjectItem(json, "screens");
    if (screen_settings == NULL) {
        LOGGER_WARNING("No 'screens' object found in '%s';" \
                " desktop settings, including background colors," \
                " will keep their default values", filename);
    } else {
        cJSON *settings;
        cJSON *desktops_array;

        /* Load total number of screen */
        json_load_uint(screen_settings, "count",
                &config_base->screen_count);

        /* Get 'desktop' array inside 'settings' */
        settings =
            cJSON_GetObjectItem(screen_settings, "settings");
        desktops_array =
            cJSON_GetObjectItem(settings, "desktops");

        /* NOTE: Whilst I recognize this maze of if statements could
         *       benefit from finesse, I am stuck with it for now.
         *       A sophisticated refactor will come, but deadlines have
         *       a way of complicating matters. */
        if (desktops_array && cJSON_IsArray(desktops_array)) {
            unsigned int desktop_count =
                (unsigned int) cJSON_GetArraySize(desktops_array);
            cJSON *first_desktop_item;
            bool uses_nested_screen_layout = false;

            desktop_count = (desktop_count > CONFIG_MAX_DESKTOPS)
                ? CONFIG_MAX_DESKTOPS
                : desktop_count;
            first_desktop_item = cJSON_GetArrayItem(desktops_array, 0);
            if (first_desktop_item &&
                    cJSON_IsObject(first_desktop_item)) {
                if (cJSON_GetObjectItem(first_desktop_item, "settings") ||
                        cJSON_GetObjectItem(first_desktop_item, "count") ||
                        cJSON_GetObjectItem(first_desktop_item,
                            "inaugural")) {
                    uses_nested_screen_layout = true;
                }
            }

            if (!uses_nested_screen_layout) {
                config_base->screens[0].desktop_count = desktop_count;

                for (unsigned int i = 0;
                        i < desktop_count && i < CONFIG_MAX_DESKTOPS;
                        ++i) {
                    cJSON *desktop_item;

                    desktop_item =
                        cJSON_GetArrayItem(desktops_array, (int) i);
                    if (desktop_item == NULL) {
                        continue;
                    }
                    s_config_load_desktop_entry(desktop_item,
                            config_base->screens[0].desktops[i].name,
                            &config_base->screens[0].desktops[i].settings);
                }
            } else {
                /* Each entry of 'desktops_array' represents a screen in
                 * this layout, so the bound must be
                 * 'CONFIG_MAX_SCREENS', not 'CONFIG_MAX_DESKTOPS';
                 * otherwise 'config_base->screens[i]' would be written
                 * out of bounds */
                for (unsigned int i = 0;
                        i < desktop_count && i < CONFIG_MAX_SCREENS;
                        ++i) {
                    cJSON *desktop_item;

                    desktop_item =
                        cJSON_GetArrayItem(desktops_array, (int) i);
                    if (desktop_item) {
                        cJSON *desktop_settings;
                        /* Load desktop 'count' and 'inaugural' */
                        json_load_uint(desktop_item, "count",
                                &config_base->screens[i].desktop_count);
                        json_load_uint(desktop_item, "inaugural",
                                &config_base->screens[i].desktop_inaugural);

                        /* Desktops, as screens, are zero-based indexed,
                         * so if the inaugural desktop is a number
                         * bigger than the desktop, it reverts to the
                         * first desktop of all: the 0th */
                        if (config_base->screens[i].desktop_inaugural >
                                config_base->screens[i].desktop_count - 1) {
                            config_base->screens[i].desktop_inaugural = 0;
                        }

                        /* Get 'settings' field for each desktop */
                        desktop_settings =
                            cJSON_GetObjectItem(desktop_item, "settings");
                        if (desktop_settings &&
                                cJSON_IsArray(desktop_settings)) {
                            unsigned int settings_count =
                                (unsigned int)
                                cJSON_GetArraySize(desktop_settings);
                            for (unsigned int j = 0;
                                    j < settings_count &&
                                    j < CONFIG_MAX_DESKTOPS;
                                    ++j) {
                                cJSON *setting_item;

                                setting_item =
                                    cJSON_GetArrayItem(desktop_settings,
                                            (int) j);
                                if (setting_item) {
                                    s_config_load_desktop_entry(setting_item,
                                            config_base->screens[i]
                                            .desktops[j].name,
                                            &config_base->screens[i]
                                            .desktops[j].settings);
                                } /* ! if (setting_item) */
                            } /* ! for(j in 0..settings_count) */
                        } /* ! if (desktop_settings) */
                    } /* ! if (desktop_item) */
                } /* ! for (i in 0..desktop_count) */
            } /* ! if (!uses_nested_screen_layout) */
        } else {
            LOGGER_WARNING("No 'desktops' array found under" \
                    " 'screens.settings' in '%s'; desktop" \
                    " settings, including background colors, will" \
                    " keep their default values", filename);
        } /* ! if (desktops_array) */
    } /* ! if (screen_settings) */

    /* Load default programs */
    programs = cJSON_GetObjectItem(json, "programs");
    if (programs) {
        json_load_string(programs, "terminal",
                config_base->programs.terminal,
                CONFIG_MAX_LENGTH_COMMAND);
        json_load_string(programs, "launcher",
                config_base->programs.launcher,
                CONFIG_MAX_LENGTH_COMMAND);
        json_load_string(programs, "file-manager",
                config_base->programs.file_manager,
                CONFIG_MAX_LENGTH_COMMAND);
        json_load_string(programs, "web-browser",
                config_base->programs.web_browser,
                CONFIG_MAX_LENGTH_COMMAND);
        json_load_string(programs, "editor",
                config_base->programs.editor,
                CONFIG_MAX_LENGTH_COMMAND);
    }

    /* Load window base configuration */
    windows = cJSON_GetObjectItem(json, "windows");
    if (windows) {
        cJSON *focus;
        cJSON *gravity;
        cJSON *placement;

        json_load_uint(windows, "snap", &config_base->windows.snap);
        json_load_uint(windows, "move-step",
                &config_base->windows.move_step);
        json_load_uint(windows, "resize-step",
                &config_base->windows.resize_step);
        json_load_bool(windows, "has-grips",
                &config_base->windows.has_grips);
        json_load_bool(windows, "show-geom",
                &config_base->windows.show_geom);
        gravity = json_get_item(windows, "gravity");
        if (gravity != NULL && cJSON_IsString(gravity)) {
            config_base->windows.gravity =
                s_config_parse_gravity(gravity->valuestring);
        }
        focus = cJSON_GetObjectItem(windows, "focus");
        if (focus) {
            cJSON *focus_policy_item;

            json_load_bool(focus, "is-new-focused",
                    &config_base->windows.focus.is_new_focused);
            json_load_bool(focus, "is-raised-on-focus",
                    &config_base->windows.focus.is_raised_on_focus);
            focus_policy_item = json_get_item(focus,
                    "policy");
            if (focus_policy_item != NULL &&
                    cJSON_IsString(focus_policy_item)) {
                config_base->windows.focus_policy =
                    s_config_parse_focus_policy(
                            focus_policy_item->valuestring);
            }
        }
        placement = cJSON_GetObjectItem(windows, "placement");
        if (placement) {
            cJSON *placement_policy_item;

            placement_policy_item = json_get_item(
                    placement, "policy");
            if (placement_policy_item != NULL &&
                    cJSON_IsString(placement_policy_item)) {
                config_base->windows.placement_policy =
                    s_config_parse_placement_policy(
                            placement_policy_item->valuestring);
            }
        }

    }

    /* Load icon policy configuration */
    icons = cJSON_GetObjectItem(json, "icons");
    if (icons) {
        cJSON *placement;

        json_load_bool(icons, "show-geom",
                &config_base->icons.show_geom);
        placement = cJSON_GetObjectItem(icons, "placement");
        if (placement && cJSON_IsObject(placement)) {
            cJSON *icon_policy_item = json_get_item(placement, "policy");
            if (icon_policy_item != NULL &&
                    cJSON_IsString(icon_policy_item)) {
                config_base->icons.placement_policy =
                    s_config_parse_icon_placement(
                            icon_policy_item->valuestring);
            }
        } else {
            cJSON *ip = json_get_item(icons, "placement");
            if (ip != NULL && cJSON_IsString(ip)) {
                config_base->icons.placement_policy =
                    s_config_parse_icon_placement(ip->valuestring);
            }
        }
    } else if (windows) {
        /* Backward compatibility: legacy location in 'windows.icons' */
        cJSON *icons_item = cJSON_GetObjectItem(windows, "icons");
        if (icons_item) {
            cJSON *ip = json_get_item(icons_item, "placement");
            if (ip != NULL && cJSON_IsString(ip)) {
                config_base->icons.placement_policy =
                    s_config_parse_icon_placement(ip->valuestring);
            }
        }
    }

    json_load_bool(json, "enable-emergency-shortcut",
            &config_base->enable_emergency_shortcut);
    json_load_bool(json, "show-desktop-notify",
            &config_base->show_desktop_notify);

    /* Free memory */
    cJSON_Delete(json);
    return 0;
}
