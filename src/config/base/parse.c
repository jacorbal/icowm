/**
 * @file config/base/parse.c
 *
 * @brief Configuration string-to-enumeration parsing helpers
 *
 * One of the files @c config/base/ is made of; everything here parses
 * one JSON string field into its matching configuration enumeration
 * value, shared across @c config/base/desktops.c, @c defaults.c,
 * @c systray.c, and @c load.c (all declared in @c config/internal.h for
 * exactly that reason), so none of it stays static to this file the way
 * it once did as part of a single translation unit.
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

/* Local includes */
#include <config.h>
#include <config/internal.h>


/* Parse focus policy text into configuration enumeration */
enum config_focus_policy_e
    ci_config_parse_focus_policy(const char *value)
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
    if (safe_strcmp(value_norm, "manual") == 0) {
        return CONFIG_PLACEMENT_POLICY_MANUAL;
    }

    return CONFIG_PLACEMENT_POLICY_SMART;
}


/* Parse placement monitor text into configuration enumeration */
enum config_placement_monitor_e
    ci_config_parse_placement_monitor(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_PLACEMENT_MONITOR_POINTER;
    }

    if (safe_strcmp(value_norm, "active") == 0) {
        return CONFIG_PLACEMENT_MONITOR_ACTIVE;
    }

    if (safe_strcmp(value_norm, "primary") == 0) {
        return CONFIG_PLACEMENT_MONITOR_PRIMARY;
    }

    return CONFIG_PLACEMENT_MONITOR_POINTER;
}


/* Parse desktop menu position text into configuration enumeration */
enum config_menu_position_e
    ci_config_parse_menu_position(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_MENU_POSITION_UNDER_MOUSE;
    }

    if (safe_strcmp(value_norm, "center") == 0) {
        return CONFIG_MENU_POSITION_CENTER;
    }
    if (safe_strcmp(value_norm, "top-left") == 0) {
        return CONFIG_MENU_POSITION_TOP_LEFT;
    }
    if (safe_strcmp(value_norm, "top-right") == 0) {
        return CONFIG_MENU_POSITION_TOP_RIGHT;
    }
    if (safe_strcmp(value_norm, "bottom-left") == 0) {
        return CONFIG_MENU_POSITION_BOTTOM_LEFT;
    }
    if (safe_strcmp(value_norm, "bottom-right") == 0) {
        return CONFIG_MENU_POSITION_BOTTOM_RIGHT;
    }

    return CONFIG_MENU_POSITION_UNDER_MOUSE;
}


/* Parse desktop-grid layout orientation text into configuration
 * enumeration */
enum config_desktop_orientation_e
    ci_config_parse_desktop_orientation(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_DESKTOP_ORIENTATION_HORIZONTAL;
    }

    if (safe_strcmp(value_norm, "vertical") == 0) {
        return CONFIG_DESKTOP_ORIENTATION_VERTICAL;
    }

    return CONFIG_DESKTOP_ORIENTATION_HORIZONTAL;
}


/* Parse desktop-grid layout starting-corner text into configuration
 * enumeration */
enum config_desktop_corner_e
    ci_config_parse_desktop_corner(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_DESKTOP_CORNER_TOP_LEFT;
    }

    if (safe_strcmp(value_norm, "top-right") == 0) {
        return CONFIG_DESKTOP_CORNER_TOP_RIGHT;
    }
    if (safe_strcmp(value_norm, "bottom-left") == 0) {
        return CONFIG_DESKTOP_CORNER_BOTTOM_LEFT;
    }
    if (safe_strcmp(value_norm, "bottom-right") == 0) {
        return CONFIG_DESKTOP_CORNER_BOTTOM_RIGHT;
    }

    return CONFIG_DESKTOP_CORNER_TOP_LEFT;
}


/* Parse one scratchpad dimension from a fixed pixel count or "max" */
void ci_config_scratchpad_size_parse(const cJSON *item,
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


/* Parse scratchpad edge text into configuration enumeration */
enum config_scratchpad_edge_e
    ci_config_parse_scratchpad_edge(const char *value)
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


/* Parse systray dock position text into configuration enumeration */
enum config_systray_position_e
    ci_config_parse_systray_position(const char *value)
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


/* Parse systray monitor anchor text into configuration enumeration */
enum config_systray_monitor_anchor_e
    ci_config_parse_systray_monitor_anchor(const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_SYSTRAY_MONITOR_STAGE;
    }

    if (safe_strcmp(value_norm, "primary") == 0) {
        return CONFIG_SYSTRAY_MONITOR_PRIMARY;
    }
    if (safe_strcmp(value_norm, "index") == 0) {
        return CONFIG_SYSTRAY_MONITOR_INDEX;
    }

    return CONFIG_SYSTRAY_MONITOR_STAGE;
}


/* Parse the systray icon ordering policy into its enumeration */
enum config_systray_order_e
    ci_config_parse_systray_order(const char *value)
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


/* Parse systray stacking-layer text into configuration enumeration */
enum config_systray_layer_e
    ci_config_parse_systray_layer(const char *value)
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


/* Parse systray clock/battery text position into configuration */
enum config_systray_text_position_e
    ci_config_parse_systray_text_position(const char *value)
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


/* Parse one systray text item name, "clock" or "battery" */
bool ci_config_systray_text_item_parse(const char *value,
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


/* Parse a systray battery backend type into configuration */
enum config_battery_backend_type_e
    ci_config_parse_battery_backend_type(const char *value)
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


/* Parse default window gravity text into configuration enumeration */
enum config_gravity_e
    ci_config_parse_gravity(const char *value)
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
    if (safe_strcmp(value_norm, "in-place") == 0) {
        return CONFIG_ICON_PLACEMENT_IN_PLACE;
    }

    return CONFIG_ICON_PLACEMENT_SMART;
}
