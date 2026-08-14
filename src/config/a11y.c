/**
 * @file config/a11y.c
 *
 * @brief Accessibility (a11y) settings loader implementation
 *
 * Parses a JSON file of the form:
 * @code
 * {
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
 * @c config_set_default_values, config.c) untouched for that field.
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

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/* Load accessibility (a11y) settings from a JSON file */
int config_load_a11y(const char *filename,
        struct config_a11y_s *config_a11y)
{
    cJSON *json;
    cJSON *interaction_item;
    cJSON *focus_indicator_item;
    cJSON *urgency_item;

    LOGGER_TRACE("Parsing a11y configuration from file '%s'", filename);

    if (json_load_config(filename, &json) != 0) {
        return 1;
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
            " (double-click-ms=%u, min-border-width=%u," \
            " audible-bell=%d, blink-interval-ms=%u)",
            config_a11y->interaction.double_click_ms,
            config_a11y->focus_indicator.min_border_width,
            (int) config_a11y->urgency.audible_bell,
            config_a11y->urgency.blink_interval_ms);

    cJSON_Delete(json);
    return 0;
}
