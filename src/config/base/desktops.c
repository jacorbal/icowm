/**
 * @file config/base/desktops.c
 *
 * @brief Screen and desktop topology loading
 *
 * Split out of what used to be a single, flat @c config/base.c;
 * everything here loads @c topology.screens (screen count, and each
 * screen's desktop count/inaugural desktop/desktop entries, in
 * either the flat or nested on-disk shape) and @c desktops
 * (desktop-navigation and reserved-space behavior) from parsed
 * @c config.json. @c ci_config_load_screens and
 * @c ci_config_load_desktop_behavior are the only two entry points
 * @c config/base/load.c's own @c config_load_base calls from here;
 * everything else stays static to this file.
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

/* Default initial values */
#include <defs/desktop.h>

/* Project includes */
#include <logger.h>

/* Utils includes */
#include <utils/config/json.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>



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

    /* Reset to the sentinel before every attempt, not just the very
     * first one: a reload whose 'config.json' no longer names a
     * 'background-color' for this desktop must fall back to the
     * theme's own 'desktop.color.background' (see 'desktop_init',
     * desktop.c, and its own reload-time counterpart in wm/actions.c)
     * the same way a desktop that never had one does, rather than
     * keeping whatever color an earlier load happened to leave here.
     * 'json_load_color' below already logs its own DEBUG line when
     * the field is absent, so nothing further is logged here for
     * that, entirely ordinary, case. */
    settings_out->background.color = WM_DESKTOP_BG_COLOR_UNSET;
    (void) json_load_color(desktop_json, "background-color",
            &settings_out->background.color);
}


/**
 * @brief Enforce a minimum on a just-loaded configuration count field,
 *        logging and correcting it in place if it falls short
 *
 * A handful of configuration count fields (number of screens, number
 * of desktops on a screen) are meaningless below 1: a window manager
 * with 0 screens or 0 desktops has nowhere to put a single window.
 * Centralizes the "warn and default to the floor" behavior every one
 * of them needs, rather than repeating the same check at each call
 * site.
 *
 * @param value       Field to check and, if needed, correct in place
 * @param minimum     Smallest value considered valid; typically 1
 * @param field_label Human-readable name for the log message
 * @param filename    Path the value was loaded from, for the log
 *                     message only
 *
 * @note Complexity: @e O(1)
 */
static void s_config_enforce_min_count(uint32_t *value, uint32_t minimum,
        const char *restrict field_label, const char *restrict filename)
{
    if (*value >= minimum) {
        return;
    }

    LOGGER_WARNING("'%s' in '%s' is %u, below the minimum of %u;" \
            " defaulting to %u",
            field_label, filename, *value, minimum, minimum);
    *value = minimum;
}


/**
 * @brief Detect which of the two accepted @p topology.screens.desktops
 *        shapes a JSON array is using, looking at its first entry alone
 *
 * The flat, single-screen shape has plain desktop entries
 * (name/background color and the like).  The per-screen shape instead
 * has each entry carrying its own @p settings / @p count / @p inaugural
 * fields describing a whole screen.
 *
 * @param desktops_array The @p topology.screens.desktops array itself
 *
 * @return @c true if the per-screen (nested) shape is in use
 *
 * @note Complexity: @e O(1)
 */
static bool s_config_screens_uses_nested_layout(cJSON *desktops_array)
{
    cJSON *first_desktop_item;

    first_desktop_item = cJSON_GetArrayItem(desktops_array, 0);
    if (first_desktop_item == NULL ||
            !cJSON_IsObject(first_desktop_item)) {
        return false;
    }

    return cJSON_GetObjectItem(first_desktop_item, "settings") != NULL ||
        cJSON_GetObjectItem(first_desktop_item, "count") != NULL ||
        cJSON_GetObjectItem(first_desktop_item, "inaugural") != NULL;
}


/**
 * @brief Load the flat @p topology.screens.desktops shape
 *
 * Every array entry is a plain desktop, all applied to screen 0.
 *
 * @param desktops_array The 'topology.screens.desktops' array itself
 * @param desktop_count  Number of entries in @p desktops_array,
 *                       already clamped to 'CONFIG_MAX_DESKTOPS'
 * @param config_base    Destination structure
 * @param filename       Path the JSON was read from, for log messages
 *                       only
 *
 * @note Complexity: @e O(d), where @e d is @p desktop_count
 */
static void s_config_load_screens_flat(cJSON *desktops_array,
        unsigned int desktop_count, struct config_base_s *config_base,
        const char *filename)
{
    config_base->screens[0].desktop_count = desktop_count;
    s_config_enforce_min_count(&config_base->screens[0].desktop_count,
            1u, "topology.screens.desktops (count)", filename);

    for (unsigned int i = 0;
            i < desktop_count && i < CONFIG_MAX_DESKTOPS; ++i) {
        cJSON *desktop_item = cJSON_GetArrayItem(desktops_array, (int) i);

        if (desktop_item == NULL) {
            continue;
        }
        s_config_load_desktop_entry(desktop_item,
                config_base->screens[0].desktops[i].name,
                &config_base->screens[0].desktops[i].settings);
    }
}


/**
 * @brief Load one screen's own entry within the per-screen (nested)
 *        @p topology.screens.desktops shape
 *
 * Reads that one screen's @p count / @p inaugural, clamping the
 * inaugural desktop back to 0 if it names one past the screen's own
 * desktop count, then loads every desktop named in its @p settings
 * array.
 *
 * @param desktop_item One entry of 'topology.screens.desktops',
 *                     describing screen @p screen_idx
 * @param screen_idx   Index of the screen this entry describes
 * @param config_base  Destination structure
 * @param filename     Path the JSON was read from, for log messages
 *                     only
 *
 * @note Complexity: @e O(d), where @e d is the number of entries in
 *       this screen's own 'settings' array
 */
static void s_config_load_screen_desktop_settings(cJSON *desktop_item,
        uint32_t screen_idx, struct config_base_s *config_base,
        const char *filename)
{
    cJSON *desktop_settings;
    unsigned int settings_count;

    json_load_uint(desktop_item, "count",
            &config_base->screens[screen_idx].desktop_count);
    s_config_enforce_min_count(
            &config_base->screens[screen_idx].desktop_count, 1u,
            "topology.screens.desktops[].count", filename);

    /* 'config_base->screens[screen_idx].desktops' (config.h) is a
     * fixed-size 'CONFIG_MAX_DESKTOPS' array; unlike the flat shape
     * (@a s_config_load_screens_flat, whose own 'desktop_count'
     * parameter already arrives pre-clamped from its own caller),
     * this one reads "count" fresh from this one screen's own JSON
     * entry, with nothing else clamping it before every later
     * consumer (starting with 'surface_init' at startup, wm.c) takes
     * it as a trusted upper bound for iterating or indexing that same
     * array. */
    if (config_base->screens[screen_idx].desktop_count >
            (uint32_t) CONFIG_MAX_DESKTOPS) {
        LOGGER_WARNING("%s: topology.screens.desktops[%u].count (%u)" \
                " exceeds the configured maximum of %d; clamped",
                filename, screen_idx,
                config_base->screens[screen_idx].desktop_count,
                CONFIG_MAX_DESKTOPS);
        config_base->screens[screen_idx].desktop_count =
            (uint32_t) CONFIG_MAX_DESKTOPS;
    }

    json_load_uint(desktop_item, "inaugural",
            &config_base->screens[screen_idx].desktop_inaugural);

    /* Desktops, as screens, are zero-based indexed, so if the
     * inaugural desktop is a number bigger than the desktop, it
     * reverts to the first desktop of all: the 0th */
    if (config_base->screens[screen_idx].desktop_inaugural >
            config_base->screens[screen_idx].desktop_count - 1) {
        config_base->screens[screen_idx].desktop_inaugural = 0;
    }

    desktop_settings = cJSON_GetObjectItem(desktop_item, "settings");
    if (desktop_settings == NULL || !cJSON_IsArray(desktop_settings)) {
        return;
    }

    settings_count = (unsigned int) cJSON_GetArraySize(desktop_settings);
    for (unsigned int j = 0;
            j < settings_count && j < CONFIG_MAX_DESKTOPS; ++j) {
        cJSON *setting_item = cJSON_GetArrayItem(desktop_settings,
                (int) j);

        if (setting_item == NULL) {
            continue;
        }
        s_config_load_desktop_entry(setting_item,
                config_base->screens[screen_idx].desktops[j].name,
                &config_base->screens[screen_idx].desktops[j].settings);
    }
}


/**
 * @brief Load the per-screen (nested) @p topology.screens.desktops
 *        shape
 *
 * Every array entry describes one whole screen
 *
 * @param desktops_array The 'topology.screens.desktops' array itself
 * @param desktop_count  Number of entries in @p desktops_array,
 *                       already clamped to 'CONFIG_MAX_DESKTOPS';
 *                       reused here against 'CONFIG_MAX_SCREENS'
 *                       instead, since each entry is a screen in this
 *                       shape, not a desktop (see the note below)
 * @param config_base    Destination structure
 * @param filename       Path the JSON was read from, for log messages
 *                       only
 *
 * @note Complexity: @e O(s * d), where @e s is the number of screens
 *       and @e d the number of desktops described per screen
 */
static void s_config_load_screens_nested(cJSON *desktops_array,
        unsigned int desktop_count, struct config_base_s *config_base,
        const char *filename)
{
    /* Each entry of 'desktops_array' represents a screen in this
     * layout, so the bound must be 'CONFIG_MAX_SCREENS', not
     * 'CONFIG_MAX_DESKTOPS'; otherwise 'config_base->screens[i]'
     * would be written out of bounds */
    for (unsigned int i = 0;
            i < desktop_count && i < CONFIG_MAX_SCREENS; ++i) {
        cJSON *desktop_item = cJSON_GetArrayItem(desktops_array, (int) i);

        if (desktop_item != NULL) {
            s_config_load_screen_desktop_settings(desktop_item, i,
                    config_base, filename);
        }
    }
}


/* Load 'topology.screens' (screen count, and each screen's desktop
 * count/inaugural desktop/desktop entries) from parsed 'config.json' */
void ci_config_load_screens(cJSON *json,
        struct config_base_s *config_base, const char *filename)
{
    cJSON *topology;
    cJSON *screen_settings;
    cJSON *desktops_array;
    unsigned int desktop_count;

    topology = cJSON_GetObjectItem(json, "topology");
    screen_settings = (topology != NULL)
        ? cJSON_GetObjectItem(topology, "screens") : NULL;
    if (screen_settings == NULL) {
        LOGGER_WARNING("No 'topology.screens' object found in '%s';" \
                " desktop settings, including background colors," \
                " will keep their default values", filename);
        return;
    }

    json_load_uint(screen_settings, "count", &config_base->screen_count);
    s_config_enforce_min_count(&config_base->screen_count, 1u,
            "topology.screens.count", filename);

    /* 'desktops' sits directly under 'topology.screens'; no intervening
     * 'settings' object (unlike each individual screen entry's own
     * per-desktop 'settings[]' array below, which is a different,
     * unrelated thing this schema keeps as it already was) */
    desktops_array = cJSON_GetObjectItem(screen_settings, "desktops");
    if (desktops_array == NULL || !cJSON_IsArray(desktops_array)) {
        LOGGER_WARNING("No 'desktops' array found under" \
                " 'topology.screens' in '%s'; desktop" \
                " settings, including background colors, will" \
                " keep their default values", filename);
        return;
    }

    desktop_count = (unsigned int) cJSON_GetArraySize(desktops_array);
    desktop_count = (desktop_count > CONFIG_MAX_DESKTOPS)
        ? CONFIG_MAX_DESKTOPS : desktop_count;

    if (s_config_screens_uses_nested_layout(desktops_array)) {
        s_config_load_screens_nested(desktops_array, desktop_count,
                config_base, filename);
    } else {
        s_config_load_screens_flat(desktops_array, desktop_count,
                config_base, filename);
    }
}


/* Load 'desktops' (desktop-navigation and reserved-space behavior)
 * from parsed 'config.json' */
void ci_config_load_desktop_behavior(cJSON *json,
        struct config_desktop_s *config_desktop, const char *filename)
{
    cJSON *desktop_settings;
    cJSON *margins;

    desktop_settings = cJSON_GetObjectItem(json, "desktops");
    if (desktop_settings == NULL) {
        LOGGER_TRACE("No 'desktops' object found in '%s';" \
                " show-overlay, notify-activity, warp-on-edge-drag," \
                " wrap-at-bounds, and margins keep their default values",
                filename);
        return;
    }

    json_load_bool(desktop_settings, "show-overlay",
            &config_desktop->show_overlay);
    json_load_bool(desktop_settings, "notify-activity",
            &config_desktop->notify_activity);
    json_load_bool(desktop_settings, "warp-on-edge-drag",
            &config_desktop->warp_on_edge_drag);
    json_load_bool(desktop_settings, "wrap-at-bounds",
            &config_desktop->wrap_at_bounds);

    margins = cJSON_GetObjectItem(desktop_settings, "margins");
    if (margins != NULL) {
        json_load_uint(margins, "top", &config_desktop->margins.top);
        json_load_uint(margins, "right", &config_desktop->margins.right);
        json_load_uint(margins, "bottom",
                &config_desktop->margins.bottom);
        json_load_uint(margins, "left", &config_desktop->margins.left);
    }
}
