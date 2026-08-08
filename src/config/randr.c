/**
 * @file config/randr.c
 *
 * @brief XRandR output profile configuration loader implementation
 *
 * Parses a JSON file of the form:
 * @code
 * {
 *     "is-enabled": true,
 *     "outputs": [
 *     {
 *         "name": "HDMI-1",
 *         "is-enabled": true,
 *         "is-primary": true,
 *         "resolution": { "w": 1920, "h": 1080 },
 *         "position": { "x": 0, "y": 0 },
 *         "rotation": "normal"
 *     }
 *     ]
 * }
 * @endcode
 *
 * Rotation values: "normal", "left", "right", "inverted".
 * Missing fields keep their zero-initialized defaults.
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
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/randr.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/**
 * @brief Parse a rotation string into the corresponding XRandR mask
 *
 * Recognized values (case-insensitive after normalisation):
 * @c normal, @c left, @c right, @c inverted.
 *
 * @param value Rotation string from the JSON file
 *
 * @return XRandR rotation mask, or
 *         @c XCB_RANDR_ROTATION_ROTATE_0 on unrecognized input
 */
static uint16_t s_config_randr_parse_rotation(const char *value)
{
    char norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, norm, sizeof(norm))) {
        return (uint16_t) XCB_RANDR_ROTATION_ROTATE_0;
    }

    if (safe_strcmp(norm, "left") == 0) {
        return (uint16_t) XCB_RANDR_ROTATION_ROTATE_90;
    }
    if (safe_strcmp(norm, "inverted") == 0) {
        return (uint16_t) XCB_RANDR_ROTATION_ROTATE_180;
    }
    if (safe_strcmp(norm, "right") == 0) {
        return (uint16_t) XCB_RANDR_ROTATION_ROTATE_270;
    }

    return (uint16_t) XCB_RANDR_ROTATION_ROTATE_0;
}


/* Load XRandR output profiles from a JSON file */
int config_load_randr(const char *filename,
        struct config_randr_s *config_randr)
{
    cJSON *json;
    cJSON *outputs_arr;
    int arr_len;

    LOGGER_TRACE("Parsing RandR configuration from file '%s'", filename);

    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    json_load_bool(json, "is-enabled", &config_randr->is_enabled);
    LOGGER_DEBUG("Loaded RandR configuration (is-enabled=%d)",
            (int) config_randr->is_enabled);

    outputs_arr = cJSON_GetObjectItem(json, "outputs");
    if (outputs_arr == NULL || !cJSON_IsArray(outputs_arr)) {
        LOGGER_DEBUG("No 'outputs' array in '%s'", filename);
        cJSON_Delete(json);
        return 0;
    }

    arr_len = cJSON_GetArraySize(outputs_arr);
    if (arr_len > CONFIG_RANDR_MAX_OUTPUTS) {
        LOGGER_NOTICE("RandR config: %d outputs specified; " \
                "clamping to %d", arr_len, CONFIG_RANDR_MAX_OUTPUTS);
        arr_len = CONFIG_RANDR_MAX_OUTPUTS;
    }

    config_randr->output_count = 0u;

    for (int i = 0; i < arr_len; i++) {
        cJSON *entry = cJSON_GetArrayItem(outputs_arr, i);
        cJSON *rot_item;
        cJSON *res_obj;
        cJSON *pos_obj;
        struct config_randr_output_s *out;

        if (entry == NULL || !cJSON_IsObject(entry)) {
            continue;
        }

        out = &config_randr->outputs[config_randr->output_count];
        memset(out, 0, sizeof(*out));

        json_load_string(entry, "name",
                out->name, CONFIG_RANDR_OUTPUT_NAME_LEN);
        json_load_bool(entry, "is-enabled", &out->is_enabled);
        json_load_bool(entry, "is-primary",  &out->is_primary);

        res_obj = cJSON_GetObjectItem(entry, "resolution");
        if (res_obj != NULL && cJSON_IsObject(res_obj)) {
            json_load_uint(res_obj, "w", &out->preferred_res.w);
            json_load_uint(res_obj, "h", &out->preferred_res.h);
        }

        pos_obj = cJSON_GetObjectItem(entry, "position");
        if (pos_obj != NULL && cJSON_IsObject(pos_obj)) {
            cJSON *xv = cJSON_GetObjectItem(pos_obj, "x");
            cJSON *yv = cJSON_GetObjectItem(pos_obj, "y");

            if (xv != NULL && cJSON_IsNumber(xv)) {
                out->position.x = (int32_t) xv->valueint;
            }
            if (yv != NULL && cJSON_IsNumber(yv)) {
                out->position.y = (int32_t) yv->valueint;
            }
        }

        rot_item = cJSON_GetObjectItem(entry, "rotation");
        if (rot_item != NULL && cJSON_IsString(rot_item)) {
            out->rotation =
                s_config_randr_parse_rotation(rot_item->valuestring);
        } else {
            out->rotation = (uint16_t) XCB_RANDR_ROTATION_ROTATE_0;
        }

        LOGGER_DEBUG("Loaded RandR profile %u (name='%s'," \
                " is-enabled=%d, is-primary=%d, res=%ux%u," \
                " pos=%d+%d, rot=%u)",
                config_randr->output_count,
                out->name,
                (int) out->is_enabled, (int) out->is_primary,
                out->preferred_res.w, out->preferred_res.h,
                out->position.x, out->position.y,
                (unsigned int) out->rotation);

        config_randr->output_count++;
    }

    cJSON_Delete(json);
    return 0;
}
