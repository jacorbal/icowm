/**
 * @file config/theme.c
 *
 * @brief Theme configuration loader implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/json.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/* Load theme configuration */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme)
{
    cJSON *json;
    cJSON *window;
    cJSON *icon;

    LOGGER_TRACE("Parsing theme configuration from file '%s'",
            filename);

    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    json_load_string(json, "name", config_theme->name,
            CONFIG_MAX_LENGTH_FONTNAME);

    window = cJSON_GetObjectItem(json, "window");
    if (window) {
        cJSON *general;
        cJSON *active;
        cJSON *inactive;

        general = cJSON_GetObjectItem(window, "general");
        if (general) {
            json_load_uint(general, "border-width",
                    &config_theme->window.general.border_width);
            json_load_bool(general, "is-decorated",
                    &config_theme->window.general.is_decorated);
        }

        active = cJSON_GetObjectItem(window, "active");
        if (active) {
            json_load_color(active, "background-color",
                    &config_theme->window.active.background_color);
            json_load_color(active, "foreground-color",
                    &config_theme->window.active.foreground_color);
            json_load_color(active, "border-color",
                    &config_theme->window.active.border_color);
            json_load_string(active, "font",
                    config_theme->window.active.font,
                    CONFIG_MAX_LENGTH_FONTNAME);
        }

        inactive = cJSON_GetObjectItem(window, "inactive");
        if (inactive) {
            json_load_color(inactive, "background-color",
                    &config_theme->window.inactive.background_color);
            json_load_color(inactive, "foreground-color",
                    &config_theme->window.inactive.foreground_color);
            json_load_color(inactive, "border-color",
                    &config_theme->window.inactive.border_color);
            json_load_string(inactive, "font",
                    config_theme->window.inactive.font,
                    CONFIG_MAX_LENGTH_FONTNAME);
        }
    }

    icon = cJSON_GetObjectItem(json, "icon");
    if (icon) {
        json_load_color(icon, "background-color",
                &config_theme->icon.background_color);
        json_load_color(icon, "foreground-color",
                &config_theme->icon.foreground_color);
        json_load_color(icon, "border-color",
                &config_theme->icon.border_color);
        json_load_uint(icon, "border-width",
                &config_theme->icon.border_width);
        json_load_bool(icon, "is-captioned",
                &config_theme->icon.is_captioned);
        json_load_string(icon, "font", config_theme->icon.font,
                CONFIG_MAX_LENGTH_FONTNAME);
    }

    cJSON_Delete(json);

    return 0;
}
