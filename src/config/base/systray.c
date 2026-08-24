/**
 * @file config/base/systray.c
 *
 * @brief Systray configuration loading
 *
 * One of the files @c config/base/ is made of;
 * @c ci_config_load_systray is also called from @c config/memguard.c
 * (declared in @c config/internal.h for exactly that reason, not just
 * for the split), since both share this exact same @c systray object
 * parsing.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>



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
    json_load_bool(systray, "avoid-overlap",
            &config_base->systray.avoid_overlap);

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
            ci_config_parse_systray_position(
                    position_item->valuestring);
    }
    monitor_item = cJSON_GetObjectItem(systray, "monitor");
    if (monitor_item) {
        cJSON *anchor_item;
        cJSON *index_item;

        anchor_item = json_get_item(monitor_item, "anchor");
        if (anchor_item != NULL && cJSON_IsString(anchor_item)) {
            config_base->systray.monitor.anchor =
                ci_config_parse_systray_monitor_anchor(
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
            ci_config_parse_systray_order(order_item->valuestring);
    }
    layer_item = json_get_item(systray, "layer");
    if (layer_item != NULL && cJSON_IsString(layer_item)) {
        config_base->systray.layer =
            ci_config_parse_systray_layer(layer_item->valuestring);
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
            cJSON *const backend_type_item = json_get_item(backend_item,
                    "type");

            if (backend_type_item != NULL &&
                    cJSON_IsString(backend_type_item)) {
                config_base->systray.battery.backend.type =
                    ci_config_parse_battery_backend_type(
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
                ci_config_parse_systray_text_position(
                        text_position_item->valuestring);
        }

        text_order_item = cJSON_GetObjectItem(text_item, "order");
        if (text_order_item != NULL &&
                cJSON_IsArray(text_order_item)) {
            uint8_t out_count = 0u;
            int arr_size = cJSON_GetArraySize(text_order_item);

            for (int i = 0;
                    i < arr_size && out_count < 2u; ++i) {
                cJSON *const elem = cJSON_GetArrayItem(text_order_item, i);
                enum config_systray_text_item_e parsed;

                if (elem != NULL && cJSON_IsString(elem) &&
                        ci_config_parse_systray_text_item(
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
