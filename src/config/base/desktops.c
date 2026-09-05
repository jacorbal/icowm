/**
 * @file config/base/desktops.c
 *
 * @brief Screen and desktop topology loading
 *
 * One of the files @c config/base/ is made of; everything here loads @c
 * topology.screens (screen count, and each screen's desktop
 * count/inaugural desktop/desktop entries, in either the flat or nested
 * on-disk shape) and @c desktops (desktop-navigation and reserved-space
 * behavior) from parsed @c config.json.  @c ci_config_load_screens and
 * @c ci_config_load_desktop_behavior are the only two entry points
 * @c config/base/load.c's @c config_load_base calls from here;
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
     * first one: a reload whose 'config.json' no longer names
     * a 'background-color' for this desktop must fall back to the
     * theme's 'desktop.color.background' (see 'desktop_init',
     * desktop.c, and its reload-time counterpart in wm/actions.c) the
     * same way a desktop that never had one does, rather than keeping
     * whatever color an earlier load happened to leave here.
     * 'json_load_color' below already logs its DEBUG line when the
     * field is absent, so nothing further is logged here for
     * that, entirely ordinary, case. */
    settings_out->background.color = WM_DESKTOP_BG_COLOR_UNSET;
    (void) json_load_color(desktop_json, "background-color",
            &settings_out->background.color);
}


/**
 * @brief Enforce a minimum on a just-loaded configuration count field,
 *        logging and correcting it in place if it falls short
 *
 * A handful of configuration count fields (number of screens, number of
 * desktops on a screen) are meaningless below 1: a window manager with
 * @c 0 screens or @c 0 desktops has nowhere to put a single window.
 * Centralizes the "warn and default to the floor" behavior every one of
 * them needs, rather than repeating the same check at each call site.
 *
 * @param value       Field to check and, if needed, correct in place
 * @param minimum     Smallest value considered valid; typically 1
 * @param field_label Human-readable name for the log message
 * @param filename    Path the value was loaded from, for the log
 *                    message only
 *
 * @note Complexity: @e O(1)
 */
static void s_config_enforce_min_count(uint32_t *value,
        uint32_t minimum, const char *restrict field_label,
        const char *restrict filename)
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
 * @brief Fall back one screen's @p desktop_layout to a single row, one
 *        column per desktop
 *
 * The exact same reading order the flat desktop list itself already had
 * before layout existed at all, shared by every "layout absent" or
 * "layout invalid" case in @a s_config_load_desktop_layout below, so
 * both log the same way and never drift apart from each other by
 * accident.
 *
 * @param config_base Destination structure
 * @param screen_idx  Index of the screen whose layout to fall back
 *
 * @note Complexity: @e O(1)
 */
static void s_config_desktop_layout_fallback(
        struct config_base_s *config_base, uint32_t screen_idx)
{
    struct config_desktop_layout_s *dest =
        &config_base->screens[screen_idx].desktop_layout;

    dest->orientation = CONFIG_DESKTOP_ORIENTATION_HORIZONTAL;
    dest->corner = CONFIG_DESKTOP_CORNER_TOP_LEFT;
    dest->rows = 1u;
    dest->columns = config_base->screens[screen_idx].desktop_count;
}


/**
 * @brief Load and validate one screen's @p layout object within the
 *        per-screen (nested) @c topology.screens.desktops shape
 *
 * Falls back to a single row, one column per desktop whenever @c layout
 * is absent entirely, or present but invalid: @c rows or @c columns is
 * explicitly @c 0 or above @c CONFIG_MAX_DESKTOPS (checked before
 * either is ever used as a divisor or in an addition below, so neither
 * computing the other, nor the final @c rows x @c columns check, can
 * ever overflow), or @c rows x @c columns ultimately falls short of
 * this screen's, already-finalized @p desktop_count.
 *
 * @c orientation defaults to @c horizontal and @c corner to @c top-left
 * whenever @c layout is present but either key is itself missing, the
 * exact same values @a s_config_desktop_ layout_fallback also falls
 * back to as a whole.  @c rows / @c columns default differently
 * depending on how many of the two are actually named: both missing
 * falls back the same way the whole function does when @c layout itself
 * is absent (a single row); exactly one missing computes it by ceiling
 * division against @p desktop_count, rather than simply defaulting it
 * to @c 1, so a lone @c columns: 1 (a clear "strictly vertical"
 * request) becomes as many rows as needed to hold every desktop in that
 * one column, not a @c 1x1 grid rejected the moment more than a single
 * desktop exists; both given are used exactly as read, validated as
 * a pair the same way as always.
 *
 * @param desktop_item One entry of @c topology.screens.desktops,
 *                     describing screen @p screen_idx; this screen's
 *                     @c desktop_count must already be finalized in
 *                     @p config_base before this call
 * @param screen_idx   Index of the screen this entry describes
 * @param config_base  Destination structure
 * @param filename     Path the JSON was read from, for log messages
 *                     only
 *
 * @note Complexity: @e O(1)
 *
 * @see @a s_config_desktop_layout_fallback
 */
static void s_config_load_desktop_layout(cJSON *desktop_item,
        uint32_t screen_idx, struct config_base_s *config_base,
        const char *filename)
{
    cJSON *layout;
    struct config_desktop_layout_s *dest =
        &config_base->screens[screen_idx].desktop_layout;
    uint32_t desktop_count =
        config_base->screens[screen_idx].desktop_count;
    char orientation_str[CONFIG_MAX_LENGTH_OPTION];
    char corner_str[CONFIG_MAX_LENGTH_OPTION];
    uint32_t rows = 0u;
    uint32_t columns = 0u;
    bool has_rows;
    bool has_columns;
    bool has_orientation;
    bool has_corner;

    layout = cJSON_GetObjectItem(desktop_item, "layout");
    if (layout == NULL) {
        s_config_desktop_layout_fallback(config_base, screen_idx);
        return;
    }

    has_orientation = json_load_string(layout, "orientation",
            orientation_str, sizeof(orientation_str)) == 0;
    has_corner = json_load_string(layout, "corner",
            corner_str, sizeof(corner_str)) == 0;
    has_rows = json_load_uint(layout, "rows", &rows) == 0;
    has_columns = json_load_uint(layout, "columns", &columns) == 0;

    if ((has_rows && (rows == 0u ||
                    rows > (uint32_t) CONFIG_MAX_DESKTOPS)) ||
            (has_columns && (columns == 0u ||
                    columns > (uint32_t) CONFIG_MAX_DESKTOPS))) {
        LOGGER_WARNING("%s: topology.screens.desktops[%u].layout" \
                " (%u rows, %u columns) cannot hold this screen's" \
                " own %u desktop(s); falling back to a single row",
                filename, screen_idx, rows, columns, desktop_count);
        s_config_desktop_layout_fallback(config_base, screen_idx);
        return;
    }

    if (has_rows && !has_columns) {
        columns = (desktop_count + rows - 1u) / rows;
    } else if (!has_rows && has_columns) {
        rows = (desktop_count + columns - 1u) / columns;
    } else if (!has_rows && !has_columns) {
        rows = 1u;
        columns = desktop_count;
    }
    /* Both given: used exactly as read, validated as a pair below. */

    if (rows > (uint32_t) CONFIG_MAX_DESKTOPS ||
            columns > (uint32_t) CONFIG_MAX_DESKTOPS ||
            rows * columns < desktop_count) {
        LOGGER_WARNING("%s: topology.screens.desktops[%u].layout" \
                " (%u rows, %u columns) cannot hold this screen's" \
                " own %u desktop(s); falling back to a single row",
                filename, screen_idx, rows, columns, desktop_count);
        s_config_desktop_layout_fallback(config_base, screen_idx);
        return;
    }

    dest->orientation = has_orientation
        ? ci_config_parse_desktop_orientation(orientation_str)
        : CONFIG_DESKTOP_ORIENTATION_HORIZONTAL;
    dest->corner = has_corner
        ? ci_config_parse_desktop_corner(corner_str)
        : CONFIG_DESKTOP_CORNER_TOP_LEFT;
    dest->rows = rows;
    dest->columns = columns;
}


/**
 * @brief Fall back one screen's @p viewport to a pannable area exactly
 *        the size of the physical screen, i.e., panning disabled
 *
 * Shared by every "viewport absent" or "viewport invalid" case in
 * @a s_config_load_viewport below, so both log the same way and never
 * drift apart from each other by accident.
 *
 * @param config_base Destination structure
 * @param screen_idx  Index of the screen whose viewport to fall back
 *
 * @note Complexity: @e O(1)
 */
static void s_config_viewport_fallback(
        struct config_base_s *config_base, uint32_t screen_idx)
{
    struct config_viewport_s *dest =
        &config_base->screens[screen_idx].viewport;

    dest->columns = 1u;
    dest->rows = 1u;
}


/**
 * @brief Load and validate one screen's @p viewport object within the
 *        per-screen (nested) @c topology.screens.desktops shape
 *
 * Falls back to a pannable area exactly the size of the physical
 * screen (panning disabled) whenever @c viewport is absent entirely,
 * or present but @c columns or @c rows is explicitly @c 0 or above
 * @c CONFIG_VIEWPORT_MAX_PAGES.  Either field missing on an otherwise
 * valid @c viewport object defaults to @c 1 on its own, independently
 * of the other, unlike @a s_config_load_desktop_layout's paired
 * rows/columns inference: a pannable area has no @c desktop_count to
 * size the missing axis against, so there is nothing to infer it
 * from.
 *
 * @param desktop_item One entry of @c topology.screens.desktops,
 *                     describing screen @p screen_idx
 * @param screen_idx   Index of the screen this entry describes
 * @param config_base  Destination structure
 * @param filename     Path the JSON was read from, for log messages
 *                     only
 *
 * @note Complexity: @e O(1)
 *
 * @see @a s_config_viewport_fallback
 */
static void s_config_load_viewport(cJSON *desktop_item,
        uint32_t screen_idx, struct config_base_s *config_base,
        const char *filename)
{
    cJSON *viewport;
    struct config_viewport_s *dest =
        &config_base->screens[screen_idx].viewport;
    uint32_t columns = 1u;
    uint32_t rows = 1u;

    viewport = cJSON_GetObjectItem(desktop_item, "viewport");
    if (viewport == NULL) {
        s_config_viewport_fallback(config_base, screen_idx);
        return;
    }

    json_load_uint(viewport, "columns", &columns);
    json_load_uint(viewport, "rows", &rows);

    if (columns == 0u || columns > (uint32_t) CONFIG_VIEWPORT_MAX_PAGES ||
            rows == 0u || rows > (uint32_t) CONFIG_VIEWPORT_MAX_PAGES) {
        LOGGER_WARNING("%s: topology.screens.desktops[%u].viewport" \
                " (%u columns, %u rows) is out of the accepted" \
                " 1-%d range; falling back to panning disabled",
                filename, screen_idx, columns, rows,
                CONFIG_VIEWPORT_MAX_PAGES);
        s_config_viewport_fallback(config_base, screen_idx);
        return;
    }

    dest->columns = columns;
    dest->rows = rows;
}


/**
 * @brief Detect which of the two accepted @p topology.screens.desktops
 *        shapes a JSON array is using, looking at its first entry alone
 *
 * The flat, single-screen shape has plain desktop entries
 * (name/background color and the like).  The per-screen shape instead
 * has each entry carrying its @p settings / @p count / @p inaugural
 * fields describing a whole screen.
 *
 * @param desktops_array The @c topology.screens.desktops array itself
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
 * Every array entry is a plain desktop, all applied to screen @c 0.
 *
 * @param desktops_array The @c topology.screens.desktops array itself
 * @param desktop_count  Number of entries in @p desktops_array,
 *                       already clamped to @c CONFIG_MAX_DESKTOPS
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
        cJSON *const desktop_item =
            cJSON_GetArrayItem(desktops_array, (int) i);

        if (desktop_item == NULL) {
            continue;
        }
        s_config_load_desktop_entry(desktop_item,
                config_base->screens[0].desktops[i].name,
                &config_base->screens[0].desktops[i].settings);
    }
}


/**
 * @brief Load one screen's entry within the per-screen (nested)
 *        @c topology.screens.desktops shape
 *
 * Reads that one screen's @c count / @c inaugural, clamping the
 * inaugural desktop back to 0 if it names one past the screen's desktop
 * count, then loads every desktop named in its @c settings array.
 *
 * @param desktop_item One entry of 'topology.screens.desktops',
 *                     describing screen @p screen_idx
 * @param screen_idx   Index of the screen this entry describes
 * @param config_base  Destination structure
 * @param filename     Path the JSON was read from, for log messages
 *                     only
 *
 * @note Complexity: @e O(d), where @e d is the number of entries in
 *       this screen's 'settings' array
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

    /* 'config_base->screens[screen_idx].desktops' (config.h) is
     * a fixed-size 'CONFIG_MAX_DESKTOPS' array; unlike the flat shape
     * (@a s_config_load_screens_flat, whose 'desktop_count' parameter
     * already arrives pre-clamped from its caller), this one reads
     * "count" fresh from this one screen's JSON entry, with nothing
     * else clamping it before every later consumer (starting with
     * 'surface_init' at startup, wm.c) takes it as a trusted upper
     * bound for iterating or indexing that same array. */
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

    /* This screen's 'desktop_count' is fully finalized as of right
     * here, the exact precondition 's_config_load_desktop_layout'
     * itself depends on for its 'rows * columns' validation just
     * below. */
    s_config_load_desktop_layout(desktop_item, screen_idx, config_base,
            filename);
    s_config_load_viewport(desktop_item, screen_idx, config_base,
            filename);

    json_load_uint(desktop_item, "inaugural",
            &config_base->screens[screen_idx].desktop_inaugural);

    /* Desktops, as screens, are zero-based indexed, so if the inaugural
     * desktop is a number bigger than the desktop, it reverts to the
     * first desktop of all: the 0th */
    if (config_base->screens[screen_idx].desktop_inaugural >=
            config_base->screens[screen_idx].desktop_count) {
        config_base->screens[screen_idx].desktop_inaugural = 0;
    }

    desktop_settings = cJSON_GetObjectItem(desktop_item, "settings");
    if (desktop_settings == NULL || !cJSON_IsArray(desktop_settings)) {
        return;
    }

    settings_count = (unsigned int) cJSON_GetArraySize(desktop_settings);
    for (unsigned int j = 0;
            j < settings_count && j < CONFIG_MAX_DESKTOPS; ++j) {
        cJSON *const setting_item = cJSON_GetArrayItem(desktop_settings,
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
 * @brief Load the per-screen (nested) @c topology.screens.desktops
 *        shape
 *
 * Every array entry describes one whole screen.
 *
 * @param desktops_array The @c topology.screens.desktops array itself
 * @param desktop_count  Number of entries in @p desktops_array, already
 *                       clamped to @c CONFIG_MAX_DESKTOPS; reused here
 *                       against @c CONFIG_MAX_SCREENS instead, since
 *                       each entry is a screen in this shape, not
 *                       a desktop (see the note below)
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
     * 'CONFIG_MAX_DESKTOPS'; otherwise 'config_base->screens[i]' would
     * be written out of bounds */
    for (unsigned int i = 0;
            i < desktop_count && i < CONFIG_MAX_SCREENS; ++i) {
        cJSON *const desktop_item =
            cJSON_GetArrayItem(desktops_array, (int) i);

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
    if (config_base->screen_count > (uint32_t) CONFIG_MAX_SCREENS) {
        LOGGER_WARNING("%s: topology.screens.count (%u) exceeds the" \
                " configured maximum of %d; clamped",
                filename, config_base->screen_count, CONFIG_MAX_SCREENS);
        config_base->screen_count = (uint32_t) CONFIG_MAX_SCREENS;
    }

    /* 'desktops' sits directly under 'topology.screens'; no intervening
     * 'settings' object (unlike each individual screen entry's
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


/* Load 'desktops' (desktop-navigation and reserved-space behavior) from
 * parsed 'config.json' */
void ci_config_load_desktop_behavior(cJSON *json,
        struct config_desktop_s *config_desktop, const char *filename)
{
    cJSON *desktop_settings;
    cJSON *margins;

    desktop_settings = cJSON_GetObjectItem(json, "desktops");
    if (desktop_settings == NULL) {
        LOGGER_TRACE("No 'desktops' object found in '%s';" \
                " show-overlay, notify-activity, warp-on-edge-drag," \
                " pan-on-edge-hover, wrap-at-bounds, and margins keep" \
                " their default values", filename);
        return;
    }

    json_load_bool(desktop_settings, "show-overlay",
            &config_desktop->show_overlay);
    json_load_bool(desktop_settings, "notify-activity",
            &config_desktop->notify_activity);
    json_load_bool(desktop_settings, "warp-on-edge-drag",
            &config_desktop->warp_on_edge_drag);
    json_load_bool(desktop_settings, "pan-on-edge-hover",
            &config_desktop->pan_on_edge_hover);
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
