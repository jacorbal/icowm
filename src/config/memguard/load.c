/**
 * @file config/memguard/load.c
 *
 * @brief Loading @c memguard.json's own configurable fields
 *        implementation
 *
 * Split out of @c config/memguard.c to keep that file focused on
 * orchestrating restricted-memory mode's own config loading, not on any
 * one loaded file's own contents.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/config.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>


/* Load memguard.json's own configurable fields into config */
int ci_memguard_load_json(const char *filename, config_td *config)
{
    cJSON *json;
    cJSON *theme_item;
    cJSON *programs;
    cJSON *desktops_item;
    cJSON *windows_item;
    cJSON *icons_item;
    cJSON *systray_item;
    cJSON *shutdown_item;

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
        cJSON *margins =
            cJSON_GetObjectItem(desktops_item, "margins");

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

    windows_item = cJSON_GetObjectItem(json, "windows");
    if (windows_item != NULL) {
        cJSON *placement_item;

        json_load_uint(windows_item, "move-step",
                &config->base.windows.move_step);

        placement_item = cJSON_GetObjectItem(windows_item, "placement");
        if (placement_item != NULL) {
            cJSON *policy_item = json_get_item(placement_item, "policy");

            if (policy_item != NULL && cJSON_IsString(policy_item)) {
                config->base.windows.placement_policy =
                    ci_config_parse_placement_policy(
                            policy_item->valuestring);
            }
        }
    }

    icons_item = cJSON_GetObjectItem(json, "icons");
    if (icons_item != NULL) {
        cJSON *placement_item = cJSON_GetObjectItem(icons_item,
                "placement");

        if (placement_item != NULL) {
            cJSON *policy_item = json_get_item(placement_item, "policy");

            if (policy_item != NULL && cJSON_IsString(policy_item)) {
                config->base.icons.placement_policy =
                    ci_config_parse_icon_placement(
                            policy_item->valuestring);
            }
        }
    }

    ci_config_load_systray(json, &config->base);

    /* 'text.position' and 'order' deliberately not something
     * memguard.json is allowed to configure, unlike an ordinary
     * session's own config.json: both only ever affect docked pixmap
     * icons (where the text block sits relative to them, and the order
     * newly docked ones are placed in), and this mode never docks any
     * (embedding is always off; see is_embedding_enabled's comment in
     * 'config.h'), so neither has any visible effect here at all.
     * 'ci_config_load_systray' just above still loads both (shared
     * verbatim with config.json's own identical "systray" object), so
     * this puts each back to its own fixed default afterward rather
     * than duplicating that whole function just to omit two fields. */
    systray_item = cJSON_GetObjectItem(json, "systray");
    if (systray_item != NULL) {
        cJSON *text_item = cJSON_GetObjectItem(systray_item, "text");

        if (text_item != NULL &&
                cJSON_GetObjectItem(text_item, "position") != NULL) {
            config->base.systray.text.position = CONFIG_SYSTRAY_TEXT_LEFT;
        }
        if (cJSON_GetObjectItem(systray_item, "order") != NULL) {
            config->base.systray.order =
                CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
        }
    }

    shutdown_item = cJSON_GetObjectItem(json, "shutdown");
    if (shutdown_item) {
        json_load_bool(shutdown_item, "enable-emergency-shortcut",
                &config->base.shutdown.enable_emergency_shortcut);
        json_load_uint(shutdown_item, "timeout-seconds",
                &config->base.shutdown.timeout_seconds);
    }

    cJSON_Delete(json);
    return 0;
}
