/**
 * @file rules/match.c
 *
 * @brief Window matching rules engine implementation
 *
 * Implements the pattern-matching helpers used to evaluate rule entries
 * against a client's properties.  The "when" timing test, the full
 * multi-criterion client matcher, and the layer-name parser live here
 * so that @c rules.c can remain focused on loading, applying, and
 * managing the rules table.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L

/* System includes */
#include <fnmatch.h>
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <client.h>
#include <utils/safe/safestr.h>

/* Local includes */
#include <rules/internal.h>


/**
 * @brief Test whether a shell glob pattern matches a string value
 *
 * Wraps @c fnmatch with default flags, returning @c false whenever
 * either argument is null.
 *
 * @param pattern Shell glob pattern (may contain @c * and @c ?)
 * @param value   String to test against @p pattern
 *
 * @return Whether @p pattern matches @p value
 * @retval  true @p pattern matches @p value
 * @retval false @p pattern does not match, or either argument is null
 *
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static bool s_match_str(const char *pattern, const char *value)
{
    if (pattern == NULL || value == NULL) {
        return false;
    }

    return fnmatch(pattern, value, 0) == 0;
}


/**
 * @brief Test a string value against a list of alternative patterns
 *
 * @param patterns List of shell glob patterns
 * @param count    Number of entries in @p patterns actually in use
 * @param value    String to test against every pattern in the list
 *
 * @return @c true if @p value matches at least one pattern in the list
 *
 * @note Complexity: @e O(n*m), where @e n is @p count and @e m is the
 *       length of @p value
 */
static bool s_match_str_list(
        const char patterns[][CONFIG_MAX_LENGTH_NAME], uint8_t count,
        const char *value)
{
    for (uint8_t i = 0u; i < count; ++i) {
        if (s_match_str(patterns[i], value)) {
            return true;
        }
    }

    return false;
}


/**
 * @brief Test whether a type name string matches a client's type value
 *
 * Compares the lower-case EWMH type name @p type against @p client_type
 * and returns @c true only when they correspond.  Returns @c false for
 * null or unrecognised type names.
 *
 * @param type        Lower-case EWMH type name from the configuration
 * @param client_type @c client_type_e value cast to @c uint16_t from
 *                    the client's property set
 *
 * @return Whether @p type names the same window type as @p client_type
 * @retval  true The names are equivalent
 * @retval false @p type is @c NULL, unrecognised, or does not match
 *
 * @note Complexity: @e O(1)
 */
static bool s_parse_type(const char *type, uint16_t client_type)
{
    if (type == NULL) {
        return false;
    }

    if (safe_strcmp(type, "normal") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_NORMAL;
    }
    if (safe_strcmp(type, "dialog") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_DIALOG;
    }
    if (safe_strcmp(type, "toolbar") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_TOOLBAR;
    }
    if (safe_strcmp(type, "notification") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_NOTIFICATION;
    }
    if (safe_strcmp(type, "menu") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_MENU;
    }
    if (safe_strcmp(type, "desktop") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_DESKTOP;
    }
    if (safe_strcmp(type, "splash") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_SPLASH;
    }
    if (safe_strcmp(type, "utility") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_UTILITY;
    }
    if (safe_strcmp(type, "dock") == 0) {
        return client_type == (uint16_t) CLIENT_TYPE_DOCK;
    }

    return false;
}


/**
 * @brief Test a client's type against a list of alternative type names
 *
 * @param types       List of lower-case EWMH type names
 * @param count       Number of entries in @p types actually in use
 * @param client_type @c client_type_e value cast to @c uint16_t from
 *                    the client's property set
 *
 * @return @c true if @p client_type matches any name in the list
 *
 * @note Complexity: @e O(n), where @e n is @p count
 */
static bool s_parse_type_list(
        const char types[][CONFIG_MAX_LENGTH_NAME], uint8_t count,
        uint16_t client_type)
{
    for (uint8_t i = 0u; i < count; ++i) {
        if (s_parse_type(types[i], client_type)) {
            return true;
        }
    }

    return false;
}


/* Test whether a rule's timing constraint is satisfied */
bool ri_when_matches(enum rules_when_e when,
        enum rules_trigger_e trigger)
{
    if (when == RULES_WHEN_BOTH) {
        return true;
    }

    if (trigger == RULES_TRIGGER_MAP) {
        return when == RULES_WHEN_MAP;
    }

    return when == RULES_WHEN_PROPERTY;
}


/* Test whether all match criteria of a rule entry match a client */
bool ri_client_matches(const struct rules_match_s *match,
        const client_td *client)
{
    const char *instance = (client->info.class_name[0] != NULL)
        ? client->info.class_name[0] : "";
    const char *klass = (client->info.class_name[1] != NULL)
        ? client->info.class_name[1] : "";
    const char *role = (client->info.role_name != NULL)
        ? client->info.role_name : "";
    const char *title = (client->info.name != NULL)
        ? client->info.name : "";

    if (match->has_instance &&
            !s_match_str_list(match->instance, match->instance_count,
                    instance)) {
        return false;
    }
    if (match->has_class &&
            !s_match_str_list(match->klass, match->class_count, klass)) {
        return false;
    }
    if (match->has_role &&
            !s_match_str_list(match->role, match->role_count, role)) {
        return false;
    }
    if (match->has_title &&
            !s_match_str_list(match->title, match->title_count, title)) {
        return false;
    }
    if (match->has_type &&
            !s_parse_type_list(match->type, match->type_count,
                    client->properties.type)) {
        return false;
    }
    if (match->has_transient) {
        bool is_transient = client->transient_for != XCB_WINDOW_NONE;

        if (is_transient != match->transient) {
            return false;
        }
    }

    return true;
}


/* Convert a layer name string to the corresponding client layer */
uint16_t ri_parse_layer(const char *layer)
{
    if (layer == NULL) {
        return (uint16_t) CLIENT_LAYER_NORMAL;
    }

    if (safe_strcmp(layer, "above") == 0) {
        return (uint16_t) CLIENT_LAYER_ABOVE;
    }
    if (safe_strcmp(layer, "below") == 0) {
        return (uint16_t) CLIENT_LAYER_BELOW;
    }

    return (uint16_t) CLIENT_LAYER_NORMAL;
}
