/**
 * @file config/base/load.c
 *
 * @brief Base configuration loading entry point
 *
 * One of the files @c config/base/ is made of;
 * @a config_load_base is the top-level orchestrator, calling into
 * @c config/base/parse.c's enumeration parsers directly,
 * @c desktops.c's @a ci_config_screens_load and
 * @a ci_config_desktop_behavior_load, and @c systray.c's
 * @a ci_config_systray_load (all declared in @c config/internal.h),
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <inttypes.h>   /* PRIu32 */

/* Defs includes */
#include <defs/config.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <logger.h>

/* Utils includes */
#include <utils/config/json.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>


/**
 * @brief Correct one loaded value that falls outside its valid range
 *
 * Both bounds are inclusive, and a value crossing either is corrected
 * to the bound it crossed rather than to the field's default, which is
 * how @c topology.screens.count and @c topology.screens.desktops[].count
 * already behave.  A key left out of the file never reaches here at
 * all, and so keeps whatever @a config_set_default_base_values put
 * there.
 *
 * @param value       Value to check and, where needed, correct
 * @param minimum     Lowest value accepted
 * @param maximum     Highest value accepted
 * @param field_label Fully qualified key name, for the log message
 * @param filename    Path the value was loaded from, for the log
 *                    message only
 *
 * @note Complexity: @e O(1)
 */
static void s_config_clamp_range(uint32_t *value, uint32_t minimum,
        uint32_t maximum, const char *restrict field_label,
        const char *restrict filename)
{
    if (*value >= minimum && *value <= maximum) {
        return;
    }

    LOGGER_WARNING("'%s' in '%s' is %" PRIu32 ", outside the valid" \
            " range %" PRIu32 " to %" PRIu32 "; using %" PRIu32 \
            " instead",
            field_label, filename, *value, minimum, maximum,
            (*value < minimum) ? minimum : maximum);
    *value = (*value < minimum) ? minimum : maximum;
}


/**
 * @brief Load and validate the viewport mesh settings
 *
 * @p thickness is validated last on purpose: its upper bound is a
 * fraction of the smaller spacing, so both spacings have to have
 * settled on their final values before it can be checked against
 * them.
 *
 * @param viewport    @c viewport object the @c mesh object sits in
 * @param config_base Base configuration to load into
 * @param filename    Path the values were loaded from, for the log
 *                    messages only
 *
 * @note Complexity: @e O(1)
 */
static void s_config_load_viewport_mesh(cJSON *viewport,
        struct config_base_s *config_base,
        const char *restrict filename)
{
    struct config_viewport_mesh_s *const mesh =
        &config_base->viewport.mesh;
    cJSON *const mesh_item = cJSON_GetObjectItem(viewport, "mesh");
    cJSON *spacing;
    uint32_t thickness_max;

    if (mesh_item == NULL) {
        return;
    }

    json_load_bool(mesh_item, "is-enabled", &mesh->is_enabled);

    spacing = cJSON_GetObjectItem(mesh_item, "spacing");
    if (spacing != NULL) {
        json_load_uint(spacing, "horizontal",
                &mesh->spacing_horizontal);
        json_load_uint(spacing, "vertical", &mesh->spacing_vertical);
        s_config_clamp_range(&mesh->spacing_horizontal,
                CONFIG_VIEWPORT_MESH_SPACING_MIN,
                CONFIG_VIEWPORT_MESH_SPACING_MAX,
                "viewport.mesh.spacing.horizontal", filename);
        s_config_clamp_range(&mesh->spacing_vertical,
                CONFIG_VIEWPORT_MESH_SPACING_MIN,
                CONFIG_VIEWPORT_MESH_SPACING_MAX,
                "viewport.mesh.spacing.vertical", filename);
    }

    json_load_uint(mesh_item, "tone-shift", &mesh->tone_shift);
    s_config_clamp_range(&mesh->tone_shift,
            CONFIG_VIEWPORT_MESH_TONE_SHIFT_MIN,
            CONFIG_VIEWPORT_MESH_TONE_SHIFT_MAX,
            "viewport.mesh.tone-shift", filename);

    json_load_uint(mesh_item, "thickness", &mesh->thickness);
    thickness_max = ((mesh->spacing_horizontal < mesh->spacing_vertical)
            ? mesh->spacing_horizontal
            : mesh->spacing_vertical) /
        CONFIG_VIEWPORT_MESH_THICKNESS_DIVISOR;
    if (thickness_max < CONFIG_VIEWPORT_MESH_THICKNESS_MIN) {
        thickness_max = CONFIG_VIEWPORT_MESH_THICKNESS_MIN;
    }
    s_config_clamp_range(&mesh->thickness,
            CONFIG_VIEWPORT_MESH_THICKNESS_MIN, thickness_max,
            "viewport.mesh.thickness", filename);
}


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
    cJSON *urgency;
    cJSON *overlay;
    cJSON *viewport;
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

    ci_config_screens_load(json, config_base, filename);
    ci_config_desktop_behavior_load(json, config_desktop, filename);

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
        ci_config_scratchpad_size_parse(
                json_get_item(scratchpad, "width"),
                &config_base->scratchpad.width);
        ci_config_scratchpad_size_parse(
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
        cJSON *edges;
        cJSON *focus;
        cJSON *gravity;
        cJSON *placement;

        json_load_uint(windows, "move-step",
                &config_base->windows.move_step);

        /* The resize step is usually overwritten by the client's own
         * size hints and has no effect on the window in that case,
         * but this is loaded regardless, for the rare client that
         * declares none. */
        json_load_uint(windows, "resize-step",
                &config_base->windows.resize_step);

        json_load_bool(windows, "show-geom",
                &config_base->windows.show_geom);
        json_load_bool(windows, "solid-drag",
                &config_base->windows.solid_drag);
        edges = cJSON_GetObjectItem(windows, "edges");
        if (edges) {
            cJSON *snap_item;

            snap_item = cJSON_GetObjectItem(edges, "snap");
            if (snap_item) {
                json_load_uint(snap_item, "window",
                        &config_base->windows.edges.snap.window);
                json_load_uint(snap_item, "screen",
                        &config_base->windows.edges.snap.screen);
            }
            json_load_uint(edges, "resistance",
                    &config_base->windows.edges.resistance);
        }
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
            json_load_bool(focus, "group-fallback",
                    &config_base->windows.focus.use_group_fallback);
            json_load_uint(focus, "delay-ms",
                    &config_base->windows.focus.delay_ms);
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
            } else if (placement_monitor_item != NULL &&
                    cJSON_IsNumber(placement_monitor_item) &&
                    placement_monitor_item->valueint >= 0) {
                config_base->windows.monitor_policy =
                    CONFIG_PLACEMENT_MONITOR_INDEX;
                config_base->windows.monitor_index =
                    (uint32_t) placement_monitor_item->valueint;
            }
            json_load_bool(placement, "group-related",
                    &config_base->windows.group_related);
        }
    }

    /* Load urgency configuration */
    urgency = cJSON_GetObjectItem(json, "urgency");
    if (urgency) {
        json_load_bool(urgency, "notify-activity",
                &config_base->urgency.notify_activity);
    }

    /* Load overlay configuration */
    overlay = cJSON_GetObjectItem(json, "overlay");
    if (overlay) {
        json_load_bool(overlay, "on-desktop-switch",
                &config_base->overlay.on_desktop_switch);
        json_load_bool(overlay, "on-viewport-move",
                &config_base->overlay.on_viewport_move);
    }

    /* Load viewport base configuration */
    viewport = cJSON_GetObjectItem(json, "viewport");
    if (viewport) {
        json_load_uint(viewport, "pan-step",
                &config_base->viewport.pan_step);
        json_load_bool(viewport, "pan-icons",
                &config_base->viewport.pan_icons);
        json_load_bool(viewport, "pan-on-edge-drag",
                &config_base->viewport.pan_on_edge_drag);
        json_load_bool(viewport, "pan-on-edge-hover",
                &config_base->viewport.pan_on_edge_hover);
        s_config_load_viewport_mesh(viewport, config_base, filename);
    }

    /* Load icon policy configuration */
    icons = cJSON_GetObjectItem(json, "icons");
    if (icons) {
        cJSON *placement;

        json_load_bool(icons, "show-geom",
                &config_base->icons.show_geom);
                placement = cJSON_GetObjectItem(icons, "placement");
        if (placement && cJSON_IsObject(placement)) {
            cJSON *const icon_policy_item =
                json_get_item(placement, "policy");
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
        cJSON *const icons_item = cJSON_GetObjectItem(windows, "icons");
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
    ci_config_systray_load(json, config_base);

    /* Free memory */
    cJSON_Delete(json);
    return 0;
}
