/**
 * @file config/internal.h
 *
 * @brief Private helpers shared across config implementation modules
 *
 * Declares static helper functions that are used by more than one of
 * the config translation units (@c config/base.c, @c config.c,
 * @c config/randr.c) but must not be exposed as part of the public
 * configuration API declared in @c config.h.
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
 * @brief Resolve and write the configuration directory base path
 *
 * Writes the effective configuration directory into @p config_dir_base.
 * Resolution order: @p config_dir_prefix (when non-empty) >
 * @c $XDG_CONFIG_HOME/icowm > @c $HOME/.icowm > @c ./icowm.
 *
 * @param config_dir_prefix Caller-supplied prefix, or @c NULL to use
 *                          the environment-based default
 * @param config_dir_base   Buffer that receives the resolved path;
 *                          should be at least
 *                          @c CONFIG_MAX_LENGTH_PATH_BASE bytes long
 *
 * @note Implemented in @c config.c
 * @note Complexity: @e O(1)
 */
void ci_config_dir_set(const char *config_dir_prefix,
        char *config_dir_base);

/**
 * @brief Settle a theme's own final display name
 *
 * Called once, right after attempting to load a theme file (whether
 * that attempt succeeded, failed, or was never even made because no
 * theme was named at all), so the answer to "was a theme actually
 * loaded, and did it set its own name" is already known by the time
 * this runs.  Three cases, per the project's own naming rule:
 *
 * - No theme file loaded at all (@p theme_file_name empty, or @p
 *   theme_loaded @c false): @p theme's own @c name becomes literally
 *   @c "Default (builtin)".
 * - A theme file loaded, but it set no @c "name" of its own (@p
 *   theme's own @c name field, as passed in, is still empty): @p
 *   theme's own @c name becomes @p theme_file_name verbatim.
 * - A theme file loaded and did set its own @c "name": @p theme's own
 *   @c name becomes @c "<that name> (<theme_file_name>)".
 *
 * @param theme           Theme structure whose own @c name this
 *                         settles; its @c name field, as passed in,
 *                         must already reflect whichever of the above
 *                         it actually is (empty for the first two
 *                         cases, whatever the file itself set for the
 *                         third)
 * @param theme_file_name The short name a theme was loaded under
 *                         (e.g. @c "default", the same string
 *                         @c "theme": @c "<this>" names in @c
 *                         memguard.json/config.json, not a path or
 *                         the @c ".json" extension), or @c NULL/empty
 *                         if none was ever named at all
 * @param theme_loaded     Whether @a config_load_theme actually
 *                         succeeded for @p theme_file_name
 *
 * @note Implemented in @c config.c
 * @note Complexity: @e O(1)
 */
void ci_config_resolve_theme_name(struct config_theme_s *theme,
        const char *theme_file_name, bool theme_loaded);

/**
 * @brief Load @c "systray" (dock position/monitor/order/layer, and
 *        its nested @c "clock", @c "battery", and @c "text" objects)
 *        from a parsed @c config.json or @c memguard.json
 *
 * A no-op, leaving @p config_base's own systray fields at whatever
 * they already held, if @c "systray" itself is absent.  Each of the
 * three nested objects is likewise only consulted if present.
 * Declared here rather than kept private to @c config/base.c since
 * both it and @c config/memguard.c need this exact same parsing (the
 * @c "systray" object itself is identical between the two files), and
 * duplicating it would risk the two drifting apart over some future
 * change to one without the other.
 *
 * @param json        Parsed root of @c config.json or @c
 *                    memguard.json
 * @param config_base Destination structure; its @c systray fields are
 *                    updated here
 *
 * @note Implemented in @c config/base.c
 * @note Complexity: @e O(1)
 */
void ci_config_load_systray(cJSON *json, struct config_base_s *config_base);

/**
 * @brief Parse icon placement policy text into configuration
 *        enumeration
 *
 * Declared here rather than kept private to @c config/base.c since
 * both it and @c config/memguard.c need this exact same parsing (the
 * icon placement policy string accepted is identical between @c
 * config.json and @c memguard.json), and duplicating it would risk
 * the two drifting apart over some future change to one without the
 * other.
 *
 * @param value Icon placement string from configuration
 *
 * @return Parsed icon placement policy enumeration value
 *
 * @note Supported values are @c bottom, @c top, @c left, @c right,
 *       and @c smart
 * @note Implemented in @c config/base.c
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
enum config_icon_placement_e ci_config_parse_icon_placement(
        const char *value);

/**
 * @brief Parse window placement policy text into configuration
 *        enumeration
 *
 * Declared here rather than kept private to @c config/base.c since
 * both it and @c config/memguard.c need this exact same parsing (the
 * window placement policy string accepted is identical between @c
 * config.json and @c memguard.json), and duplicating it would risk
 * the two drifting apart over some future change to one without the
 * other.
 *
 * @param value Placement policy string from configuration
 *
 * @return Parsed placement policy enumeration value
 *
 * @note Supported values are @c smart, @c cascade,
 *       @c centered, and @c under-mouse
 * @note Implemented in @c config/base.c
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
enum config_placement_policy_e ci_config_parse_placement_policy(
        const char *value);


#endif  /* ! CONFIG_INTERNAL_H */
