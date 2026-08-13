/**
 * @file rules.c
 *
 * @brief Window matching rules loader and applier implementation
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>     /* calloc, free */
#include <string.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/config.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <config.h>
#include <logger.h>

/* Local includes */
#include <rules.h>
#include <rules/internal.h>


/* Allocate and zero-initialize a new rules table */
rules_td *rules_init(void)
{
    return calloc(1, sizeof(rules_td));
}


/* Destroy a rules table and free its allocated memory */
void rules_destroy(rules_td *rules)
{
    if (rules != NULL) {
        free(rules);
    }
}


/**
 * @brief Load a match criterion that may be a single string or an array
 *        of strings into a fixed-size list
 *
 * Reads @p key from @p match_json: a plain JSON string is stored as the
 * list's only entry; a JSON array has each of its string elements
 * copied in order, up to @c RULES_MATCH_MAX_VALUES (any beyond that are
 * silently ignored).  Non-string array elements are skipped rather than
 * aborting the whole list.  Leaves @p has_flag and @p count untouched
 * (so already-loaded defaults survive) when @p key is absent or is
 * neither a string nor an array.
 *
 * @param match_json   Parsed @c match JSON object
 * @param key          Field name to read (e.g., @c title)
 * @param dest         Destination fixed-size string array
 * @param count_out    Receives the number of values actually stored
 * @param has_flag_out Set to @c true when at least one value was
 *                      stored
 *
 * @note Complexity: @e O(n), where @e n is @c RULES_MATCH_MAX_VALUES
 */
static void s_rules_load_match_list(cJSON *match_json, const char *key,
        char dest[][CONFIG_MAX_LENGTH_NAME], uint8_t *count_out,
        bool *has_flag_out)
{
    cJSON *item;
    cJSON *elem;
    uint8_t n;

    item = json_get_item(match_json, key);
    if (item == NULL) {
        return;
    }

    n = 0u;
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        safe_strncpy(dest[0], item->valuestring, CONFIG_MAX_LENGTH_NAME);
        n = 1u;
    } else if (cJSON_IsArray(item)) {
        cJSON_ArrayForEach(elem, item) {
            if (n >= (uint8_t) RULES_MATCH_MAX_VALUES) {
                break;
            }
            if (cJSON_IsString(elem) && elem->valuestring != NULL) {
                safe_strncpy(dest[n], elem->valuestring,
                        CONFIG_MAX_LENGTH_NAME);
                ++n;
            }
        }
    }

    if (n > 0u) {
        *count_out = n;
        *has_flag_out = true;
    }
}


/* Load window matching rules from the JSON configuration file */
int rules_load(rules_td *rules, const char *config_dir_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char rules_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    cJSON *json = NULL;
    cJSON *rules_array;
    cJSON *rule_json;
    cJSON *match_json;
    cJSON *apply_json;
    cJSON *item;
    cJSON *x;
    cJSON *y;
    cJSON *w;
    cJSON *h;
    struct rules_rule_s *rule;
    uint32_t loaded = 0u;

    if (rules == NULL) {
        return 1;
    }

    memset(rules, 0, sizeof(*rules));

    config_resolve_dir(config_dir_prefix, config_dir);
    snprintf(rules_file, sizeof(rules_file), "%s/%s",
            config_dir, CONFIG_FILENAME_RULES);

    if (json_load_config(rules_file, &json) != 0 || json == NULL) {
        LOGGER_DEBUG("Rules file '%s' not loaded;" \
                " continuing without rules", rules_file);
        return 0;
    }

    rules_array = (cJSON_IsArray(json))
        ? json : json_get_item(json, "rules");
    if (!cJSON_IsArray(rules_array)) {
        cJSON_Delete(json);
        return 0;
    }

    cJSON_ArrayForEach(rule_json, rules_array) {
        if (!cJSON_IsObject(rule_json) || loaded >= RULES_MAX) {
            continue;
        }

        rule = &rules->rules[loaded];
        rule->when = RULES_WHEN_MAP;

        item = json_get_item(rule_json, "when");
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            if (safe_strcmp(item->valuestring, "property") == 0) {
                rule->when = RULES_WHEN_PROPERTY;
            } else if (safe_strcmp(item->valuestring, "both") == 0) {
                rule->when = RULES_WHEN_BOTH;
            }
        }

        match_json = json_get_item(rule_json, "match");
        if (cJSON_IsObject(match_json)) {
            s_rules_load_match_list(match_json, "instance",
                    rule->match.instance, &rule->match.instance_count,
                    &rule->match.has_instance);
            s_rules_load_match_list(match_json, "class",
                    rule->match.klass, &rule->match.class_count,
                    &rule->match.has_class);
            s_rules_load_match_list(match_json, "role",
                    rule->match.role, &rule->match.role_count,
                    &rule->match.has_role);
            s_rules_load_match_list(match_json, "title",
                    rule->match.title, &rule->match.title_count,
                    &rule->match.has_title);
            s_rules_load_match_list(match_json, "type",
                    rule->match.type, &rule->match.type_count,
                    &rule->match.has_type);

            item = json_get_item(match_json, "transient");
            if (cJSON_IsBool(item)) {
                rule->match.has_transient = true;
                rule->match.transient = cJSON_IsTrue(item);
            }
        }

        apply_json = json_get_item(rule_json, "apply");
        if (!cJSON_IsObject(apply_json)) {
            continue;
        }

        item = json_get_item(apply_json, "desktop");
        if (cJSON_IsNumber(item) && item->valueint >= 0) {
            rule->apply.has_desktop = true;
            rule->apply.desktop = (uint32_t) item->valueint;
        }

        item = json_get_item(apply_json, "monitor");
        if (cJSON_IsNumber(item) && item->valueint >= 0) {
            rule->apply.has_monitor = true;
            rule->apply.monitor = (uint32_t) item->valueint;
        }

        item = json_get_item(apply_json, "layer");
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            rule->apply.has_layer = true;
            rule->apply.layer = ri_parse_layer(item->valuestring);
        }

        item = json_get_item(apply_json, "focus");
        if (cJSON_IsBool(item)) {
            rule->apply.has_focus = true;
            rule->apply.focus = cJSON_IsTrue(item);
        }

        item = json_get_item(apply_json, "pinned");
        if (cJSON_IsBool(item)) {
            rule->apply.has_sticky = true;
            rule->apply.pinned = cJSON_IsTrue(item);
        }

        item = json_get_item(apply_json, "decorated");
        if (cJSON_IsBool(item)) {
            rule->apply.has_decorated = true;
            rule->apply.decorated = cJSON_IsTrue(item);
        }

        item = json_get_item(apply_json, "position");
        if (cJSON_IsObject(item)) {
            x = json_get_item(item, "x");
            y = json_get_item(item, "y");

            if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
                rule->apply.has_position = true;
                rule->apply.position_centered = false;
                rule->apply.x = x->valueint;
                rule->apply.y = y->valueint;
            }
        } else if (cJSON_IsString(item) && item->valuestring != NULL &&
                safe_strcmp(item->valuestring, "center") == 0) {
            rule->apply.has_position = true;
            rule->apply.position_centered = true;
        }

        item = json_get_item(apply_json, "size");
        if (cJSON_IsObject(item)) {
            w = json_get_item(item, "width");
            h = json_get_item(item, "height");

            if (cJSON_IsNumber(w) && cJSON_IsNumber(h) &&
                    w->valueint > 0 && h->valueint > 0) {
                rule->apply.has_size = true;
                rule->apply.w = (uint32_t) w->valueint;
                rule->apply.h = (uint32_t) h->valueint;
            }
        }

        loaded++;
    }

    rules->count = loaded;
    cJSON_Delete(json);

    LOGGER_DEBUG("Loaded %u window rule(s) from '%s'", rules->count,
            rules_file);
    return 0;
}

