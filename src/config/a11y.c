/**
 * @file config/a11y.c
 *
 * @brief Accessibility (a11y) settings loader implementation
 *
 * Parses a JSON file of the form:
 * @code
 * {
 *     "is-enabled": true,
 *     "interaction": {
 *         "double-click-ms": 400
 *     },
 *     "focus-indicator": {
 *         "min-border-width": 0
 *     },
 *     "urgency": {
 *         "audible-bell": false,
 *         "blink-interval-ms": 600
 *     }
 * }
 * @endcode
 *
 * Entirely optional, the same as @c randr.json: a missing file, or
 * one that omits an object or a field within it, leaves whatever the
 * caller already held (its compiled-in default, see
 * @c config_set_default_a11y_values, config.c) untouched for that
 * field.  @c is-enabled (default @c false) gates every other field
 * at once, mirroring @c randr.json's own @c is-enabled: a file that
 * exists but never turns this on is parsed without error, same as
 * ever, but has no effect at all, the same as if it were absent.
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
#include <stdint.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>

/* Default initial values */
#include <defs/input.h>
#include <defs/urgency.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/* Populate default values for one accessibility (a11y) structure */
void config_set_default_a11y_values(struct config_a11y_s *a11y)
{
    if (a11y == NULL) {
        return;
    }

    /* Default 'false', the same opt-in-only posture as
     * 'config_randr_s' own 'is_enabled': a person keeps an
     * 'a11y.json' around without it taking effect until they
     * explicitly turn this on. */
    a11y->is_enabled = false;

    /* The exact same values already in effect before 'a11y.json'
     * existed at all (see 'WM_DOUBLE_CLICK_MS'/
     * 'WM_URGENCY_BLINK_INTERVAL_MS'), so nobody who never enables
     * this file sees any behavior change. */
    a11y->interaction.double_click_ms = WM_DOUBLE_CLICK_MS;
    a11y->focus_indicator.min_border_width = 0u;
    a11y->urgency.audible_bell = false;
    a11y->urgency.blink_interval_ms = WM_URGENCY_BLINK_INTERVAL_MS;
}


/* Load accessibility (a11y) settings from a JSON file */
int config_load_a11y(const char *filename,
        struct config_a11y_s *config_a11y)
{
    cJSON *json;
    cJSON *interaction_item;
    cJSON *focus_indicator_item;
    cJSON *urgency_item;

    if (config_a11y == NULL) {
        return 1;
    }

    /* Reset to the same known-good defaults before every attempt,
     * not just the first: a reload whose file just turned
     * 'is-enabled' to false, dropped it entirely, or dropped a field
     * it used to specify, must fall back cleanly rather than keep
     * whatever an earlier, still-enabled load happened to leave
     * here (see 'config_set_default_a11y_values''s own doc comment,
     * config.h). */
    config_set_default_a11y_values(config_a11y);

    LOGGER_TRACE("Parsing a11y configuration from file '%s'", filename);

    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    json_load_bool(json, "is-enabled", &config_a11y->is_enabled);
    if (!config_a11y->is_enabled) {
        LOGGER_DEBUG("a11y configuration found at '%s' but" \
                " 'is-enabled' is false (or absent); every other" \
                " field stays at its own built-in default",
                filename);
        cJSON_Delete(json);
        return 0;
    }

    interaction_item = cJSON_GetObjectItem(json, "interaction");
    if (interaction_item) {
        json_load_uint(interaction_item, "double-click-ms",
                &config_a11y->interaction.double_click_ms);
    }

    focus_indicator_item = cJSON_GetObjectItem(json, "focus-indicator");
    if (focus_indicator_item) {
        json_load_uint(focus_indicator_item, "min-border-width",
                &config_a11y->focus_indicator.min_border_width);
    }

    urgency_item = cJSON_GetObjectItem(json, "urgency");
    if (urgency_item) {
        json_load_bool(urgency_item, "audible-bell",
                &config_a11y->urgency.audible_bell);
        json_load_uint(urgency_item, "blink-interval-ms",
                &config_a11y->urgency.blink_interval_ms);
    }

    LOGGER_DEBUG("Loaded a11y configuration" \
            " (is-enabled=%d, double-click-ms=%u, min-border-width=%u," \
            " audible-bell=%d, blink-interval-ms=%u)",
            (int) config_a11y->is_enabled,
            config_a11y->interaction.double_click_ms,
            config_a11y->focus_indicator.min_border_width,
            (int) config_a11y->urgency.audible_bell,
            config_a11y->urgency.blink_interval_ms);

    cJSON_Delete(json);
    return 0;
}
