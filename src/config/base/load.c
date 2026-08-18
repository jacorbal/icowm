/**
 * @file config/base/load.c
 *
 * @brief Base configuration loading entry point
 *
 * Split out of what used to be a single, flat @c config/base.c;
 * @c config_load_base is the top-level orchestrator, calling into
 * @c config/base/parse.c's own enumeration parsers directly,
 * @c desktops.c's @c ci_config_load_screens and
 * @c ci_config_load_desktop_behavior, and @c systray.c's
 * @c ci_config_load_systray (all declared in @c config/internal.h),
 * to assemble one fully-loaded @c config_td from @c config.json.
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

/* Project includes */
#include <logger.h>

/* Utils includes */
#include <utils/config/json.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>


/* Load base configuration settings from a JSON file */
int config_load_base(const char *filename,
        struct config_base_s *config_base,
        struct config_desktop_s *config_desktop)
{
    cJSON *json;
    cJSON *programs;
    cJSON *prompt;
    cJSON *scratchpad;
    cJSON *windows;
    cJSON *icons;
    cJSON *menus;
    cJSON *startup_notification_item;
    cJSON *fortune_item;
    cJSON *shutdown_item;

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

    ci_config_load_screens(json, config_base, filename);
    ci_config_load_desktop_behavior(json, config_desktop, filename);

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

    /* Load prompt (built-in run-box) configuration */
    prompt = cJSON_GetObjectItem(json, "prompt");
    if (prompt) {
        json_load_bool(prompt, "is-enabled",
                &config_base->prompt.is_enabled);
    }

    /* Load scratchpad configuration */
    scratchpad = cJSON_GetObjectItem(json, "scratchpad");
    if (scratchpad) {
        cJSON *edge_item;

        json_load_bool(scratchpad, "is-enabled",
                &config_base->scratchpad.is_enabled);
        json_load_string(scratchpad, "command",
                config_base->scratchpad.command,
                CONFIG_MAX_LENGTH_COMMAND);
        ci_config_parse_scratchpad_size(
                json_get_item(scratchpad, "width"),
                &config_base->scratchpad.width);
        ci_config_parse_scratchpad_size(
                json_get_item(scratchpad, "height"),
                &config_base->scratchpad.height);
        json_load_bool(scratchpad, "ignore-margins",
                &config_base->scratchpad.ignore_margins);

        edge_item = json_get_item(scratchpad, "edge");
        if (edge_item != NULL && cJSON_IsString(edge_item)) {
            config_base->scratchpad.edge =
                ci_config_parse_scratchpad_edge(edge_item->valuestring);
        }
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
/*
        // Resize step is usually overwritten by hints and it has no
        // effect on the window, so, just in case I decide to do
        // something with it, I leave it here, but commented, so it
        // raises no warning on the logger output.
        json_load_uint(windows, "resize-step",
                &config_base->windows.resize_step);
*/
        json_load_bool(windows, "show-geom",
                &config_base->windows.show_geom);
        json_load_bool(windows, "solid-drag",
                &config_base->windows.solid_drag);
        gravity = json_get_item(windows, "gravity");
        if (gravity != NULL && cJSON_IsString(gravity)) {
            config_base->windows.gravity =
                ci_config_parse_gravity(gravity->valuestring);
        }
        focus = cJSON_GetObjectItem(windows, "focus");
        if (focus) {
            cJSON *focus_policy_item;

            json_load_bool(focus, "focus-new",
                    &config_base->windows.focus.focus_new);
            json_load_bool(focus, "raise",
                    &config_base->windows.focus.raise);
            focus_policy_item = json_get_item(focus,
                    "policy");
            if (focus_policy_item != NULL &&
                    cJSON_IsString(focus_policy_item)) {
                config_base->windows.focus_policy =
                    ci_config_parse_focus_policy(
                            focus_policy_item->valuestring);
            }
        }
        placement = cJSON_GetObjectItem(windows, "placement");
        if (placement) {
            cJSON *placement_policy_item;
            cJSON *placement_monitor_item;

            placement_policy_item = json_get_item(
                    placement, "policy");
            if (placement_policy_item != NULL &&
                    cJSON_IsString(placement_policy_item)) {
                config_base->windows.placement_policy =
                    ci_config_parse_placement_policy(
                            placement_policy_item->valuestring);
            }
            placement_monitor_item = json_get_item(
                    placement, "monitor");
            if (placement_monitor_item != NULL &&
                    cJSON_IsString(placement_monitor_item)) {
                config_base->windows.monitor_policy =
                    ci_config_parse_placement_monitor(
                            placement_monitor_item->valuestring);
            }
            json_load_bool(placement, "group-related",
                    &config_base->windows.group_related);
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
                    ci_config_parse_icon_placement(
                            icon_policy_item->valuestring);
            }
        } else {
            cJSON *ip = json_get_item(icons, "placement");
            if (ip != NULL && cJSON_IsString(ip)) {
                config_base->icons.placement_policy =
                    ci_config_parse_icon_placement(ip->valuestring);
            }
        }
    } else if (windows) {
        /* Backward compatibility: legacy location in 'windows.icons' */
        cJSON *icons_item = cJSON_GetObjectItem(windows, "icons");
        if (icons_item) {
            cJSON *ip = json_get_item(icons_item, "placement");
            if (ip != NULL && cJSON_IsString(ip)) {
                config_base->icons.placement_policy =
                    ci_config_parse_icon_placement(ip->valuestring);
            }
        }
    }

    shutdown_item = cJSON_GetObjectItem(json, "shutdown");
    if (shutdown_item) {
        json_load_bool(shutdown_item, "enable-emergency-shortcut",
                &config_base->shutdown.enable_emergency_shortcut);
        json_load_uint(shutdown_item, "timeout-seconds",
                &config_base->shutdown.timeout_seconds);
    }

    fortune_item = cJSON_GetObjectItem(json, "fortune");
    if (fortune_item) {
        json_load_bool(fortune_item, "is-enabled",
                &config_base->fortune.is_enabled);
        json_load_string(fortune_item, "command",
                config_base->fortune.command,
                CONFIG_MAX_LENGTH_COMMAND);
    }

    startup_notification_item = cJSON_GetObjectItem(json,
            "startup-notification");
    if (startup_notification_item) {
        json_load_bool(startup_notification_item, "is-enabled",
                &config_base->startup_notification.is_enabled);
        json_load_uint(startup_notification_item, "timeout-seconds",
                &config_base->startup_notification.timeout_seconds);
    }

    /* Load per-menu-type context menu configuration */
    menus = cJSON_GetObjectItem(json, "menus");
    if (menus) {
        cJSON *root_menu;
        cJSON *windows_menu;
        cJSON *position_item;

        root_menu = cJSON_GetObjectItem(menus, "root");
        if (root_menu) {
            position_item = json_get_item(root_menu, "position");
            if (position_item != NULL && cJSON_IsString(position_item)) {
                config_base->menus.root.position =
                    ci_config_parse_menu_position(
                            position_item->valuestring);
            }
        }

        windows_menu = cJSON_GetObjectItem(menus, "windows");
        if (windows_menu) {
            position_item = json_get_item(windows_menu, "position");
            if (position_item != NULL && cJSON_IsString(position_item)) {
                config_base->menus.windows.position =
                    ci_config_parse_menu_position(
                            position_item->valuestring);
            }
        }
    }

    /* Load systray dock configuration */
    ci_config_load_systray(json, config_base);

    /* Free memory */
    cJSON_Delete(json);
    return 0;
}
