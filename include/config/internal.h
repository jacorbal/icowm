/**
 * @file config/internal.h
 *
 * @brief Private helpers shared across config implementation modules
 *
 * Declares helper functions that are used by more than one of the
 * config translation units (every @c *.c file in @c config/base/,
 * @c config.c, @c config/randr.c, @c config/memguard.c and its own
 * submodules under @c config/memguard/) but must not be exposed as part
 * of the public configuration API declared in @c config.h.
 *
 * @note This header is private to the config subsystem and must not be
 *       included outside of @c src/config/
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_INTERNAL_H
#define CONFIG_INTERNAL_H


/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/config.h>

/* Project includes */
#include <config.h>


/**
 * @brief Settle a theme's own final display name
 *
 * Called once, right after attempting to load a theme file (whether
 * that attempt succeeded, failed, or was never even made because no
 * theme was named at all), so the answer to "was a theme actually
 * loaded, and did it set its own name" is already known by the time
 * this runs.
 *
 * Three cases, per own naming rule:
 *
 * - No theme file loaded at all (@p theme_file_name empty, or
 *   @p theme_loaded @c false): @p theme's own @c name becomes literally
 *   "Default (built-in)".
 * - A theme file loaded, but it set no @c name of its own (@p theme's
 *   own @c name field, as passed in, is still empty): @p theme's own
 *   @c name becomes @p theme_file_name verbatim.
 * - A theme file loaded and did set its own
 *   @c ("name": "<theme_name>")'s own @c name becomes
 *   "<that name> (<theme_file_name>)".
 *
 * @param theme           Theme structure whose own @c name this
 *                        settles; its @c name field, as passed in, must
 *                        already reflect whichever of the above it
 *                        actually is (empty for the first two cases,
 *                        whatever the file itself set for the third)
 * @param theme_file_name The short name a theme was loaded under (e.g.,
 *                        "default", the same string
 *                        @c ("theme": "<this>") names in
 *                        @c memguard.json/config.json, not a path or
 *                        the @c .json extension), or null/empty if none
 *                        was ever named at all
 * @param theme_loaded    Whether @a config_load_theme actually
 *                        succeeded for @p theme_file_name
 *
 * @note Implemented in @c config.c
 * @note Complexity: @e O(1)
 */
void ci_config_resolve_theme_name(struct config_theme_s *theme,
        const char *theme_file_name, bool theme_loaded);

/**
 * @brief Load @c systray (dock position/monitor/order/layer, and its
 *        nested @c clock, @c battery, and @c text objects) from
 *        a parsed @c config.json or @c memguard.json
 *
 * A no-op, leaving @p config_base's own systray fields at whatever they
 * already held, if @c "systray" itself is absent.  Each of the three
 * nested objects is likewise only consulted if present.  Declared here
 * rather than kept private to @c config/base/systray.c since both it
 * and
 * @c config/memguard.c need this exact same parsing (the @c systray
 * object itself is identical between the two files), and duplicating it
 * would risk the two drifting apart over some future change to one
 * without the other.
 *
 * @param json        Parsed root of @c config.json or @c memguard.json
 * @param config_base Destination structure; its @c systray fields are
 *                    updated here
 *
 * @note Implemented in @c config/base/systray.c
 * @note Complexity: @e O(1)
 */
void ci_config_load_systray(cJSON *json,
        struct config_base_s *config_base);

/**
 * @brief Parse icon placement policy text into configuration
 *        enumeration
 *
 * Declared here rather than kept private to @c config/base/parse.c
 * since both it and @c config/memguard.c need this exact same
 * parsing (the icon
 * placement policy string accepted is identical between @c config.json
 * and @c memguard.json), and duplicating it would risk the two drifting
 * apart over some future change to one without the other.
 *
 * @param value Icon placement string from configuration
 *
 * @return Parsed icon placement policy enumeration value
 *
 * @note Supported values are @c bottom, @c top, @c left, @c right, and
 *       @c smart
 * @note Implemented in @c config/base/parse.c
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
enum config_icon_placement_e ci_config_parse_icon_placement(
        const char *value);

/**
 * @brief Parse window placement policy text into configuration
 *        enumeration
 *
 * Declared here rather than kept private to @c config/base/parse.c
 * since both it and @c config/memguard.c need this exact same
 * parsing (the window
 * placement policy string accepted is identical between @c config.json
 * and @c memguard.json), and duplicating it would risk the two drifting
 * apart over some future change to one without the other.
 *
 * @param value Placement policy string from configuration
 *
 * @return Parsed placement policy enumeration value
 *
 * @note Supported values are @c smart, @c cascade,
 *       @c centered, and @c under-mouse
 * @note Implemented in @c config/base/parse.c
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
enum config_placement_policy_e
    ci_config_parse_placement_policy(const char *value);

/**
 * @brief Apply restricted-memory mode's own theme restrictions on top
 *        of whatever @p config->theme was just loaded from
 *
 * Every font field not already naming some variant of the "fixed"
 * X core font family is replaced outright with plain
 * @c MEMGUARD_FONT_NAME.  @c xsettings publishing, icon pixmaps (both
 * the icon square's own, @c icon.show-pixmaps, and the menu row/cycle
 * row icon shown alongside each entry, @c menu.show-pixmaps), and icon
 * hint indicators are all forced off unconditionally.  Every other
 * theme field, colors, decoration, and @c is-captioned included, is
 * left exactly as the theme file specified: none of those carry the
 * ongoing memory cost the font backend and pixmap compositing do.
 *
 * @param config Configuration structure whose already-loaded theme this
 *               restricts (must not be null)
 *
 * @note Implemented in @c config/memguard/theme.c
 * @note Complexity: @e O(1), a fixed number of fields
 */
void ci_memguard_restrict_theme(config_td *config);

/**
 * @brief Load @c memguard.json's own configurable fields into @p config
 *
 * Everything restricted-memory mode still lets a person configure: the
 * active theme's name, launched programs, desktop margins, the window
 * move step and placement policy (via
 * @a ci_config_parse_placement_policy, shared verbatim with
 * @c config.json's own identical parsing), the icon placement policy
 * (via @a ci_config_parse_icon_placement, likewise shared), the systray
 * block (via @a ci_config_load_systray, shared verbatim with
 * @c config.json's own identical @c systray object, minus its own
 * @c text.position and @c order fields, which this mode always keeps at
 * their own fixed defaults regardless of what the file specifies), and
 * the emergency shortcut.
 *
 * @param filename Path to @c memguard.json
 * @param config   Configuration structure to update
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval  1 if @p filename could not be loaded or parsed
 *
 * @note A no-op, leaving every field at whatever
 *       @a config_set_default_values_memguard already set, for any of
 *       these not present in the file
 * @note Implemented in @c config/memguard/load.c
 * @note Complexity: @e O(n), where @e n is the size of @p filename
 */
int ci_memguard_load_json(const char *filename, config_td *config);


/**
 * @brief Parse focus policy text into configuration enumeration
 *
 * @param value Focus policy string from configuration
 *
 * @return Parsed focus policy enumeration value
 *
 * @note Supported values are @c click and @c sloppy
 * @note Complexity: @e O(n), where @e n is the length of @p value
 * @note Implemented in @c config/base/parse.c
 */
enum config_focus_policy_e
    ci_config_parse_focus_policy(const char *value);

/**
 * @brief Parse placement monitor text into configuration enumeration
 *
 * @param value Placement monitor string from configuration
 *
 * @return Parsed placement monitor enumeration value
 *
 * @note Supported values are @c pointer and @c primary
 * @note Complexity: @e O(n), where @e n is the length of @p value
 * @note Implemented in @c config/base/parse.c
 */
enum config_placement_monitor_e
    ci_config_parse_placement_monitor(const char *value);

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
 * @note Implemented in @c config/base/parse.c
 */
enum config_menu_position_e
    ci_config_parse_menu_position(const char *value);

/**
 * @brief Parse desktop-grid layout orientation text into
 *        configuration enumeration
 *
 * @param value Orientation string from configuration
 *
 * @return Parsed orientation enumeration value
 *
 * @note Supported values are @c horizontal and @c vertical
 * @note Complexity: @e O(n), where @e n is the length of @p value
 * @note Implemented in @c config/base/parse.c
 */
enum config_desktop_orientation_e
    ci_config_parse_desktop_orientation(const char *value);

/**
 * @brief Parse desktop-grid layout starting-corner text into
 *        configuration enumeration
 *
 * @param value Corner string from configuration
 *
 * @return Parsed corner enumeration value
 *
 * @note Supported values are @c top-left, @c top-right,
 *       @c bottom-left, and @c bottom-right
 * @note Complexity: @e O(n), where @e n is the length of @p value
 * @note Implemented in @c config/base/parse.c
 */
enum config_desktop_corner_e
    ci_config_parse_desktop_corner(const char *value);

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
 * @note Implemented in @c config/base/parse.c
 */
void ci_config_parse_scratchpad_size(const cJSON *item,
        struct config_scratchpad_size_s *out);

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
 * @note Implemented in @c config/base/parse.c
 */
enum config_scratchpad_edge_e
    ci_config_parse_scratchpad_edge(const char *value);

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
 * @note Implemented in @c config/base/parse.c
 */
enum config_systray_position_e
    ci_config_parse_systray_position(const char *value);

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
 * @note Implemented in @c config/base/parse.c
 */
enum config_systray_monitor_anchor_e
    ci_config_parse_systray_monitor_anchor(const char *value);

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
 * @note Implemented in @c config/base/parse.c
 */
enum config_systray_order_e
    ci_config_parse_systray_order(const char *value);

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
 * @note Implemented in @c config/base/parse.c
 */
enum config_systray_layer_e
    ci_config_parse_systray_layer(const char *value);

/**
 * @brief Parse systray clock/battery text position into configuration
 *
 * @param value Position text from configuration, e.g., @c "right"
 *
 * @return Parsed systray text position enumeration value
 *
 * @note Supported values are @c left and @c right
 * @note Complexity: @e O(n), where @e n is the length of @p value
 * @note Implemented in @c config/base/parse.c
 */
enum config_systray_text_position_e
    ci_config_parse_systray_text_position(const char *value);

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
 * @note Implemented in @c config/base/parse.c
 */
bool ci_config_parse_systray_text_item(const char *value,
        enum config_systray_text_item_e *out);

/**
 * @brief Parse a systray battery backend type into configuration
 *
 * @param value Backend type text from configuration, e.g., @c "apm"
 *
 * @return Parsed backend type enumeration value
 *
 * @note Supported values are @c acpi and @c apm
 * @note Complexity: @e O(n), where @e n is the length of @p value
 * @note Implemented in @c config/base/parse.c
 */
enum config_battery_backend_type_e
    ci_config_parse_battery_backend_type(const char *value);

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
 * @note Implemented in @c config/base/parse.c
 */
enum config_gravity_e
    ci_config_parse_gravity(const char *value);

/**
 * @brief Load @c topology.screens (screen count, and each screen's
 *        desktop count/inaugural desktop/desktop entries) from parsed
 *        @c config.json
 *
 * Accepts two on-disk shapes for the @p topology.screens.desktops
 * array.  A flat list of desktop entries applied to screen 0 (the
 * common, single-screen case), or, when any entry in that array
 * itself carries its @p settings / @p count / @p inaugural fields, a
 * nested layout where each entry instead describes one whole screen
 * (multi-screen configurations).
 *
 * Which shape is in use is detected from the first array entry alone.
 * A missing @p topology or @p screens object, or a missing/non-array
 * @p desktops within it, leaves whatever @p config_base already held
 * (its compiled-in or previously-loaded defaults) untouched, logging
 * why.
 *
 * @p topology (and everything under it, including @p screens) only ever
 * takes effect at startup: unlike the rest of @c config.json,
 * a configuration reload does not re-run this function, since changing
 * screen or desktop counts at runtime would mean deciding what happens
 * to whatever clients, focus, and EWMH state already live on a desktop
 * being removed, which nothing in the window manager currently does.
 *
 * @param json        Parsed root of @c config.json
 * @param config_base Destination structure; its @c screen_count and
 *                    each screen's own desktop settings are updated
 *                    here
 * @param filename    Path @p json was read from, for log messages only
 *
 * @note Implemented in @c config/base/desktops.c
 * @note Complexity: @e O(s * d), where @e s is the number of screens
 *       and @e d the number of desktops described
 */
void ci_config_load_screens(cJSON *json,
        struct config_base_s *config_base, const char *filename);

/**
 * @brief Load @c desktops (desktop-navigation and reserved-space
 *        behavior) from parsed @c config.json
 *
 * A sibling of @p topology at the root of @c config.json, not nested
 * inside it.  Unlike @p topology, every field this loads is meant to
 * take effect again on a configuration reload, so
 * @a ci_config_load_screens and this function are deliberately kept
 * separate despite both being called from @a config_load_base.  A
 * missing @p desktops object, or a missing @p margins within it,
 * leaves whatever @p config_desktop already held untouched.
 *
 * @param json           Parsed root of @c config.json
 * @param config_desktop Destination structure to populate
 * @param filename       Path @p json was read from, for log messages
 *                       only
 *
 * @note Implemented in @c config/base/desktops.c
 * @note Complexity: @e O(1)
 */
void ci_config_load_desktop_behavior(cJSON *json,
        struct config_desktop_s *config_desktop, const char *filename);


#endif  /* ! CONFIG_INTERNAL_H */
