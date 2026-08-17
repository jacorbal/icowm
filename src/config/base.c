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
#include <stdio.h>      /* snprintf */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/loop.h>
#include <defs/sn.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>


/**
 * @brief Parse focus policy text into configuration enumeration
 *
 * @param value Focus policy string from configuration
 *
 * @return Parsed focus policy enumeration value
 *
 * @note Supported values are @c click and @c sloppy
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_focus_policy_e
    s_config_parse_focus_policy(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_FOCUS_POLICY_CLICK;
    }

    if (safe_strcmp(value_norm, "sloppy") == 0) {
        return CONFIG_FOCUS_POLICY_SLOPPY;
    }

    return CONFIG_FOCUS_POLICY_CLICK;
}


/* Parse placement policy text into configuration enumeration */
enum config_placement_policy_e
    ci_config_parse_placement_policy(const char *value)
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
 * @brief Parse placement monitor text into configuration enumeration
 *
 * @param value Placement monitor string from configuration
 *
 * @return Parsed placement monitor enumeration value
 *
 * @note Supported values are @c pointer and @c primary
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_placement_monitor_e
    s_config_parse_placement_monitor(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_PLACEMENT_MONITOR_POINTER;
    }

    if (safe_strcmp(value_norm, "primary") == 0) {
        return CONFIG_PLACEMENT_MONITOR_PRIMARY;
    }

    return CONFIG_PLACEMENT_MONITOR_POINTER;
}


/**
 * @brief Parse desktop menu position text into configuration
 *        enumeration
 *
 * @param value Menu position string from configuration
 *
 * @return Parsed menu position enumeration value
 *
 * @note Supported values are @c center and @c under-mouse
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_menu_position_e
    s_config_parse_menu_position(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_MENU_POSITION_UNDER_MOUSE;
    }

    if (safe_strcmp(value_norm, "center") == 0) {
        return CONFIG_MENU_POSITION_CENTER;
    }

    return CONFIG_MENU_POSITION_UNDER_MOUSE;
}


/**
 * @brief Parse one scratchpad dimension from either a fixed pixel
 *        count or the string @c "max"
 *
 * @param item Value from configuration, expected to be either a
 *             number or the string @c "max"; any other JSON type,
 *             or a negative number, leaves @p out untouched
 * @param out  Destination dimension
 *
 * @note Complexity: @e O(1)
 */
static void s_config_parse_scratchpad_size(const cJSON *item,
        struct config_scratchpad_size_s *out)
{
    if (item == NULL || out == NULL) {
        return;
    }

    if (cJSON_IsString(item)) {
        char value_norm[CONFIG_MAX_LENGTH_OPTION];

        if (json_field_normalize(item->valuestring, value_norm,
                    sizeof(value_norm)) &&
                safe_strcmp(value_norm, "max") == 0) {
            out->mode = CONFIG_SCRATCHPAD_SIZE_MAX;
        }
        return;
    }

    if (cJSON_IsNumber(item) && item->valuedouble >= 0.0) {
        out->mode = CONFIG_SCRATCHPAD_SIZE_FIXED;
        out->pixels = (uint32_t) item->valuedouble;
    }
}


/**
 * @brief Parse scratchpad edge text into configuration enumeration
 *
 * @param value Scratchpad edge string from configuration
 *
 * @return Parsed scratchpad edge enumeration value
 *
 * @note Supported values are @c top, @c bottom, @c left, and
 *       @c right
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_scratchpad_edge_e
    s_config_parse_scratchpad_edge(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_SCRATCHPAD_EDGE_TOP;
    }

    if (safe_strcmp(value_norm, "bottom") == 0) {
        return CONFIG_SCRATCHPAD_EDGE_BOTTOM;
    }
    if (safe_strcmp(value_norm, "left") == 0) {
        return CONFIG_SCRATCHPAD_EDGE_LEFT;
    }
    if (safe_strcmp(value_norm, "right") == 0) {
        return CONFIG_SCRATCHPAD_EDGE_RIGHT;
    }

    return CONFIG_SCRATCHPAD_EDGE_TOP;
}


/**
 * @brief Parse systray dock position text into configuration
 *        enumeration
 *
 * @param value Systray position string from configuration
 *
 * @return Parsed systray position enumeration value
 *
 * @note Supported values are @c top-left, @c top-right,
 *       @c bottom-left, and @c bottom-right
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_systray_position_e
    s_config_parse_systray_position(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_SYSTRAY_POSITION_TOP_RIGHT;
    }

    if (safe_strcmp(value_norm, "top-left") == 0) {
        return CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    }
    if (safe_strcmp(value_norm, "bottom-left") == 0) {
        return CONFIG_SYSTRAY_POSITION_BOTTOM_LEFT;
    }
    if (safe_strcmp(value_norm, "bottom-right") == 0) {
        return CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT;
    }

    return CONFIG_SYSTRAY_POSITION_TOP_RIGHT;
}


/**
 * @brief Parse systray monitor anchor text into configuration
 *        enumeration
 *
 * @param value Systray monitor anchor string from configuration
 *
 * @return Parsed systray monitor anchor enumeration value
 *
 * @note Supported values are @c surface, @c primary, and @c index
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_systray_monitor_anchor_e
    s_config_parse_systray_monitor_anchor(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_SYSTRAY_MONITOR_SURFACE;
    }

    if (safe_strcmp(value_norm, "primary") == 0) {
        return CONFIG_SYSTRAY_MONITOR_PRIMARY;
    }
    if (safe_strcmp(value_norm, "index") == 0) {
        return CONFIG_SYSTRAY_MONITOR_INDEX;
    }

    return CONFIG_SYSTRAY_MONITOR_SURFACE;
}


/**
 * @brief Parse systray icon-ordering policy text into configuration
 *        enumeration
 *
 * @param value Systray order string from configuration
 *
 * @return Parsed systray order enumeration value
 *
 * @note Supported values are @c left-to-right, @c right-to-left,
 *       @c ascending, and @c descending
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_systray_order_e
    s_config_parse_systray_order(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
    }

    if (safe_strcmp(value_norm, "right-to-left") == 0) {
        return CONFIG_SYSTRAY_ORDER_RIGHT_TO_LEFT;
    }
    if (safe_strcmp(value_norm, "ascending") == 0) {
        return CONFIG_SYSTRAY_ORDER_ASCENDING;
    }
    if (safe_strcmp(value_norm, "descending") == 0) {
        return CONFIG_SYSTRAY_ORDER_DESCENDING;
    }

    return CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
}


/**
 * @brief Parse systray stacking-layer text into configuration
 *        enumeration
 *
 * @param value Systray layer string from configuration
 *
 * @return Parsed systray layer enumeration value
 *
 * @note Supported values are @c below (the default), @c above, and
 *       @c overlay; an unrecognized value falls back to @c below
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_systray_layer_e
    s_config_parse_systray_layer(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_SYSTRAY_LAYER_BELOW;
    }

    if (safe_strcmp(value_norm, "above") == 0) {
        return CONFIG_SYSTRAY_LAYER_ABOVE;
    }
    if (safe_strcmp(value_norm, "overlay") == 0) {
        return CONFIG_SYSTRAY_LAYER_OVERLAY;
    }

    return CONFIG_SYSTRAY_LAYER_BELOW;
}


/**
 * @brief Parse systray clock/battery text position into configuration
 *
 * @param value Position text from configuration, e.g., @c "right"
 *
 * @return Parsed systray text position enumeration value
 *
 * @note Supported values are @c left and @c right
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_systray_text_position_e
    s_config_parse_systray_text_position(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_SYSTRAY_TEXT_RIGHT;
    }

    if (safe_strcmp(value_norm, "left") == 0) {
        return CONFIG_SYSTRAY_TEXT_LEFT;
    }

    return CONFIG_SYSTRAY_TEXT_RIGHT;
}


/**
 * @brief Parse one systray text item name ("clock" or "battery") into
 *        configuration
 *
 * @param value Item name from configuration
 * @param out   Receives the parsed item; left untouched if @p value
 *              does not match a known item name
 *
 * @return @c true if @p value matched a known item name
 *
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static bool s_config_parse_systray_text_item(const char *value,
        enum config_systray_text_item_e *out)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm)) ||
            out == NULL) {
        return false;
    }

    if (safe_strcmp(value_norm, "clock") == 0) {
        *out = CONFIG_SYSTRAY_TEXT_CLOCK;
        return true;
    }
    if (safe_strcmp(value_norm, "battery") == 0) {
        *out = CONFIG_SYSTRAY_TEXT_BATTERY;
        return true;
    }

    return false;
}


/**
 * @brief Parse a systray battery backend type into configuration
 *
 * @param value Backend type text from configuration, e.g., @c "apm"
 *
 * @return Parsed backend type enumeration value
 *
 * @note Supported values are @c acpi and @c apm
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_battery_backend_type_e
    s_config_parse_battery_backend_type(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_BATTERY_BACKEND_ACPI;
    }

    if (safe_strcmp(value_norm, "apm") == 0) {
        return CONFIG_BATTERY_BACKEND_APM;
    }

    return CONFIG_BATTERY_BACKEND_ACPI;
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


/* Parse icon placement policy text into configuration enumeration */
enum config_icon_placement_e ci_config_parse_icon_placement(
        const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_ICON_PLACEMENT_SMART;
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
    if (safe_strcmp(value_norm, "bottom") == 0) {
        return CONFIG_ICON_PLACEMENT_BOTTOM;
    }

    return CONFIG_ICON_PLACEMENT_SMART;
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
        const char *field_label, const char *filename)
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


/**
 * @brief Load @c topology.screens (screen count, and each screen's
 *        desktop count/inaugural desktop/desktop entries) from parsed
 *        @c config.json
 *
 * Accepts two on-disk shapes for the @p topology.screens.desktops
 * array.  A flat list of desktop entries applied to screen 0 (the
 * common, single-screen case, see @a s_config_load_screens_flat), or,
 * when any entry in that array itself carries its @p settings /
 * @p count / @p inaugural fields, a nested layout where each entry
 * instead describes one whole screen (multi-screen configurations, see
 * @a s_config_load_screens_nested).
 *
 * Which shape is in use is detected from the first array entry alone
 * (see @a s_config_screens_uses_nested_layout).  A missing @p topology
 * or @p screens object, or a missing/non-array @p desktops within it,
 * leaves whatever @p config_base already held (its compiled-in or
 * previously-loaded defaults) untouched, logging why.
 *
 * @p topology (and everything under it, including @p screens) only ever
 * takes effect at startup: unlike the rest of @c config.json,
 * a configuration reload does not re-run this function, since changing
 * screen or desktop counts at runtime would mean deciding what happens
 * to whatever clients, focus, and EWMH state already live on a desktop
 * being removed, which nothing in the window manager currently does
 * (see the "Reload behavior" note).
 *
 * @param json        Parsed root of @c config.json
 * @param config_base Destination structure; its @c screen_count and
 *                    each screen's own desktop settings are updated
 *                    here
 * @param filename    Path @p json was read from, for log messages only
 *
 * @note Complexity: @e O(s * d), where @e s is the number of screens
 *       and @e d the number of desktops described
 */
static void s_config_load_screens(cJSON *json,
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


/**
 * @brief Load @c desktops (desktop-navigation and reserved-space
 *        behavior
 *
 * A sibling of @p topology at the root of @c config.json, not nested
 * inside it (see @p config_desktop_s's comment in @c config.h for why).
 * Unlike @p topology, every field this loads is meant to take effect
 * again on a configuration reload, so @a s_config_load_screens and this
 * function are deliberately kept separate despite both being called
 * from @a config_load_base.  A missing @p desktops object, or a missing
 * @p margins within it, leaves whatever @p config_desktop already held
 * untouched.
 *
 * @param json           Parsed root of @c config.json
 * @param config_desktop Destination structure to populate
 * @param filename       Path @p json was read from, for log messages
 *                       only
 *
 * @note Complexity: @e O(1)
 */
static void s_config_load_desktop_behavior(cJSON *json,
        struct config_desktop_s *config_desktop, const char *filename)
{
    cJSON *desktop_settings;
    cJSON *margins;

    desktop_settings = cJSON_GetObjectItem(json, "desktops");
    if (desktop_settings == NULL) {
        LOGGER_TRACE("No 'desktops' object found in '%s';" \
                " show-overlay, notify-activity, enable-edge-warp," \
                " is-circular, and margins keep their default values",
                filename);
        return;
    }

    json_load_bool(desktop_settings, "show-overlay",
            &config_desktop->show_overlay);
    json_load_bool(desktop_settings, "notify-activity",
            &config_desktop->notify_activity);
    json_load_bool(desktop_settings, "enable-edge-warp",
            &config_desktop->enable_edge_warp);
    json_load_bool(desktop_settings, "is-circular",
            &config_desktop->is_circular);

    margins = cJSON_GetObjectItem(desktop_settings, "margins");
    if (margins != NULL) {
        json_load_uint(margins, "top", &config_desktop->margins.top);
        json_load_uint(margins, "right", &config_desktop->margins.right);
        json_load_uint(margins, "bottom",
                &config_desktop->margins.bottom);
        json_load_uint(margins, "left", &config_desktop->margins.left);
    }
}


/* Populate default values for the base and desktop-navigation
 * configuration structures, used both as the initial process-wide
 * default and, before applying config.json (or 'memguard.json') found,
 * as the known-good starting point that file's own fields then overlay */
void config_set_default_base_values(struct config_base_s *config_base,
        struct config_desktop_s *config_desktop)
{
    LOGGER_TRACE("Setting default base configuration", L_NARG);
    config_base->theme[0] = '\0';
    config_base->screen_count = 1;

    /* Desktop-navigation and reserved-space behavior ('config.json''s
     * top-level 'desktop', a sibling of 'topology'; see
     * config_desktop_s's comment in 'config.h').  Meaningless with only
     * one desktop for 'enable_edge_warp'/'is_circular', but set
     * regardless of how many desktops end up configured, the same as
     * every other default here. */
    config_desktop->show_overlay = true;
    config_desktop->notify_activity = true;
    config_desktop->enable_edge_warp = true;
    config_desktop->is_circular = true;
    config_desktop->margins.top = 0u;
    config_desktop->margins.right = 0u;
    config_desktop->margins.bottom = 0u;
    config_desktop->margins.left = 0u;

    /* Every screen and desktop slot the fixed-size 'screens' and
     * 'desktops' arrays can ever hold gets the sentinel here, not just
     * the ones this function is about to treat as active by default
     * below: 'config_load_base' can fill in far more screens or
     * desktops than that default, straight into these same arrays, and
     * a slot it does not itself set a color for would otherwise still
     * be sitting at zero from this whole structure's initial 'calloc'
     * rather than at the sentinel, which reads as an opaque black
     * background instead of falling back to the theme's own color the
     * way an genuinely unset one should. */
    LOGGER_TRACE("Setting background-color sentinel for every" \
            " possible screen and desktop slot", L_NARG);
    for (unsigned int i = 0; i < CONFIG_MAX_SCREENS; ++i) {
        for (unsigned int j = 0; j < CONFIG_MAX_DESKTOPS; ++j) {
            config_base->screens[i].desktops[j].settings.background.color
                = WM_DESKTOP_BG_COLOR_UNSET;
        }
    }

    LOGGER_TRACE("Setting configuration for each screen", L_NARG);
    for (unsigned int i = 0; i < config_base->screen_count; ++i) {
        /* 4 desktops by default, unless 'CONFIG_MAX_DESKTOPS' itself is
         * smaller than that.  Purely a fallback for when nothing else
         * specifies a count at all: a 'config.json' that specifies its
         * own 'desktops.count' always overrides this default, since
         * 'config_load_base' runs after this and simply replaces it;
         * nothing caps that value back down afterward. */
        uint32_t desktop_default = 4u;

        config_base->screens[i].desktop_count =
            (CONFIG_MAX_DESKTOPS < desktop_default)
                ? CONFIG_MAX_DESKTOPS : desktop_default;
        config_base->screens[i].desktop_inaugural = 0;

        /* All desktop settings */
        LOGGER_TRACE("Setting desktops configuration on screen %u", i);
        for (unsigned int j = 0;
                j < config_base->screens[i].desktop_count;
                ++j) {
            char desktop_name[CONFIG_MAX_LENGTH_NAME];
            snprintf(desktop_name, sizeof(desktop_name),
                    "Desktop %u", j);
            safe_strncpy(config_base->screens[i].desktops[j].name,
                desktop_name, CONFIG_MAX_LENGTH_NAME);
        }
    }

    LOGGER_TRACE("Setting default base programs", L_NARG);
    safe_strncpy(config_base->programs.terminal,
            "xterm", sizeof(config_base->programs.terminal));
    safe_strncpy(config_base->programs.launcher,
            "gmrun", sizeof(config_base->programs.launcher));
    safe_strncpy(config_base->programs.file_manager,
            "pcmanfm", sizeof(config_base->programs.file_manager));
    safe_strncpy(config_base->programs.editor,
            "gvim", sizeof(config_base->programs.editor));
    safe_strncpy(config_base->programs.web_browser,
            "firefox", sizeof(config_base->programs.web_browser));

    LOGGER_TRACE("Setting default prompt configuration", L_NARG);
    config_base->prompt.is_enabled = false;

    LOGGER_TRACE("Setting default scratchpad configuration", L_NARG);
    config_base->scratchpad.is_enabled = true;
    safe_strncpy(config_base->scratchpad.command,
            "xterm -fg black -bg ivory -cr black",
            sizeof(config_base->scratchpad.command));
    config_base->scratchpad.edge = CONFIG_SCRATCHPAD_EDGE_TOP;
    config_base->scratchpad.width.mode = CONFIG_SCRATCHPAD_SIZE_MAX;
    config_base->scratchpad.width.pixels = 0;
    config_base->scratchpad.height.mode = CONFIG_SCRATCHPAD_SIZE_FIXED;
    config_base->scratchpad.height.pixels = 200;
    config_base->scratchpad.ignore_margins = false;

    config_base->windows.move_step = 10;
    /* usually overridden by hints */
    config_base->windows.resize_step = 20;
    config_base->windows.snap = 4;
    config_base->windows.show_geom = true;
    config_base->windows.gravity = CONFIG_GRAVITY_NORTH_WEST;
    config_base->windows.focus_policy = CONFIG_FOCUS_POLICY_CLICK;
    config_base->windows.placement_policy = CONFIG_PLACEMENT_POLICY_SMART;
    config_base->windows.monitor_policy = CONFIG_PLACEMENT_MONITOR_POINTER;
    config_base->windows.group_related = true;
    config_base->windows.focus.is_new_focused = true;
    config_base->windows.focus.is_raised_on_focus = false;
    config_base->icons.placement_policy = CONFIG_ICON_PLACEMENT_SMART;
    config_base->icons.show_geom = false;
    config_base->shutdown.enable_emergency_shortcut = false;
    config_base->shutdown.timeout_seconds = 15u;
    config_base->fortune.is_enabled = true;
    safe_strncpy(config_base->fortune.command, "fortune",
            sizeof(config_base->fortune.command));
    config_base->startup_notification.is_enabled = true;
    config_base->startup_notification.timeout_seconds =
        (uint32_t) SN_TIMEOUT_SECONDS;
    config_base->menus.root.position = CONFIG_MENU_POSITION_UNDER_MOUSE;
    config_base->menus.windows.position = CONFIG_MENU_POSITION_UNDER_MOUSE;
    config_base->systray.is_enabled = true;
    config_base->systray.is_embedding_enabled = true;
    config_base->systray.reserve_space = false;
    config_base->systray.margins.top = 0u;
    config_base->systray.margins.right = 0u;
    config_base->systray.margins.bottom = 0u;
    config_base->systray.margins.left = 0u;
    config_base->systray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    config_base->systray.monitor.anchor = CONFIG_SYSTRAY_MONITOR_SURFACE;
    config_base->systray.monitor.index = 0u;
    config_base->systray.order = CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
    config_base->systray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    config_base->systray.clock.is_enabled = true;
    safe_strncpy(config_base->systray.clock.format,
            "%a %R", sizeof(config_base->systray.clock.format));

    config_base->systray.battery.is_enabled = false;
    config_base->systray.battery.threshold.charged = 100u;
    config_base->systray.battery.threshold.low = 20u;
    config_base->systray.battery.threshold.critical = 5u;
    config_base->systray.battery.backend.type = CONFIG_BATTERY_BACKEND_ACPI;
    config_base->systray.battery.backend.number = 0u;
    config_base->systray.battery.poll_seconds =
        (uint32_t) WM_SYSTRAY_BATTERY_POLL_SECONDS;

    config_base->systray.text.order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    config_base->systray.text.order[1] = CONFIG_SYSTRAY_TEXT_BATTERY;
    config_base->systray.text.order_count = 2u;
    config_base->systray.text.position = CONFIG_SYSTRAY_TEXT_LEFT;
}


/* Load "systray" (dock position/monitor/order/layer, and its nested
 * "clock", "battery", and "text" objects) from a parsed 'config.json'
 * or 'memguard.json' */
void ci_config_load_systray(cJSON *json,
        struct config_base_s *config_base)
{
    cJSON *systray;
    cJSON *margins;
    cJSON *position_item;
    cJSON *monitor_item;
    cJSON *order_item;
    cJSON *layer_item;
    cJSON *clock_item;
    cJSON *battery_item;
    cJSON *text_item;

    systray = cJSON_GetObjectItem(json, "systray");
    if (!systray) {
        return;
    }

    json_load_bool(systray, "is-enabled",
            &config_base->systray.is_enabled);
    json_load_bool(systray, "reserve-space",
            &config_base->systray.reserve_space);

    margins = cJSON_GetObjectItem(systray, "margins");
    if (margins != NULL) {
        json_load_uint(margins, "top",
                &config_base->systray.margins.top);
        json_load_uint(margins, "right",
                &config_base->systray.margins.right);
        json_load_uint(margins, "bottom",
                &config_base->systray.margins.bottom);
        json_load_uint(margins, "left",
                &config_base->systray.margins.left);
    }

    position_item = json_get_item(systray, "position");
    if (position_item != NULL && cJSON_IsString(position_item)) {
        config_base->systray.position =
            s_config_parse_systray_position(
                    position_item->valuestring);
    }
    monitor_item = cJSON_GetObjectItem(systray, "monitor");
    if (monitor_item) {
        cJSON *anchor_item;
        cJSON *index_item;

        anchor_item = json_get_item(monitor_item, "anchor");
        if (anchor_item != NULL && cJSON_IsString(anchor_item)) {
            config_base->systray.monitor.anchor =
                s_config_parse_systray_monitor_anchor(
                        anchor_item->valuestring);
        }
        index_item = json_get_item(monitor_item, "index");
        if (cJSON_IsNumber(index_item) && index_item->valueint >= 0) {
            config_base->systray.monitor.index =
                (uint32_t) index_item->valueint;
        }
    }
    order_item = json_get_item(systray, "order");
    if (order_item != NULL && cJSON_IsString(order_item)) {
        config_base->systray.order =
            s_config_parse_systray_order(order_item->valuestring);
    }
    layer_item = json_get_item(systray, "layer");
    if (layer_item != NULL && cJSON_IsString(layer_item)) {
        config_base->systray.layer =
            s_config_parse_systray_layer(layer_item->valuestring);
    }

    clock_item = cJSON_GetObjectItem(systray, "clock");
    if (clock_item) {
        json_load_bool(clock_item, "is-enabled",
                &config_base->systray.clock.is_enabled);
        json_load_string(clock_item, "format",
                config_base->systray.clock.format,
                sizeof(config_base->systray.clock.format));
    }

    battery_item = cJSON_GetObjectItem(systray, "battery");
    if (battery_item) {
        cJSON *threshold_item;
        cJSON *backend_item;

        json_load_bool(battery_item, "is-enabled",
                &config_base->systray.battery.is_enabled);

        threshold_item = cJSON_GetObjectItem(battery_item,
                "threshold");
        if (threshold_item) {
            json_load_uint(threshold_item, "charged",
                    &config_base->systray.battery.threshold.charged);
            json_load_uint(threshold_item, "low",
                    &config_base->systray.battery.threshold.low);
            json_load_uint(threshold_item, "critical",
                    &config_base->systray.battery.threshold.critical);
        }

        backend_item = cJSON_GetObjectItem(battery_item, "backend");
        if (backend_item) {
            cJSON *backend_type_item = json_get_item(backend_item,
                    "type");

            if (backend_type_item != NULL &&
                    cJSON_IsString(backend_type_item)) {
                config_base->systray.battery.backend.type =
                    s_config_parse_battery_backend_type(
                            backend_type_item->valuestring);
            }
            json_load_uint(backend_item, "number",
                    &config_base->systray.battery.backend.number);
        }

        json_load_uint(battery_item, "poll-seconds",
                &config_base->systray.battery.poll_seconds);
    }

    text_item = cJSON_GetObjectItem(systray, "text");
    if (text_item) {
        cJSON *text_position_item;
        cJSON *text_order_item;

        text_position_item = json_get_item(text_item, "position");
        if (text_position_item != NULL &&
                cJSON_IsString(text_position_item)) {
            config_base->systray.text.position =
                s_config_parse_systray_text_position(
                        text_position_item->valuestring);
        }

        text_order_item = cJSON_GetObjectItem(text_item, "order");
        if (text_order_item != NULL &&
                cJSON_IsArray(text_order_item)) {
            uint8_t out_count = 0u;
            int arr_size = cJSON_GetArraySize(text_order_item);

            for (int i = 0;
                    i < arr_size && out_count < 2u; ++i) {
                cJSON *elem = cJSON_GetArrayItem(text_order_item, i);
                enum config_systray_text_item_e parsed;

                if (elem != NULL && cJSON_IsString(elem) &&
                        s_config_parse_systray_text_item(
                                elem->valuestring, &parsed)) {
                    config_base->systray.text.order[out_count] =
                        parsed;
                    ++out_count;
                }
            }
            config_base->systray.text.order_count = out_count;
        }
    }
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

    s_config_load_screens(json, config_base, filename);
    s_config_load_desktop_behavior(json, config_desktop, filename);

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
        s_config_parse_scratchpad_size(
                json_get_item(scratchpad, "width"),
                &config_base->scratchpad.width);
        s_config_parse_scratchpad_size(
                json_get_item(scratchpad, "height"),
                &config_base->scratchpad.height);
        json_load_bool(scratchpad, "ignore-margins",
                &config_base->scratchpad.ignore_margins);

        edge_item = json_get_item(scratchpad, "edge");
        if (edge_item != NULL && cJSON_IsString(edge_item)) {
            config_base->scratchpad.edge =
                s_config_parse_scratchpad_edge(edge_item->valuestring);
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
                    s_config_parse_placement_monitor(
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
                    s_config_parse_menu_position(
                            position_item->valuestring);
            }
        }

        windows_menu = cJSON_GetObjectItem(menus, "windows");
        if (windows_menu) {
            position_item = json_get_item(windows_menu, "position");
            if (position_item != NULL && cJSON_IsString(position_item)) {
                config_base->menus.windows.position =
                    s_config_parse_menu_position(
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
