/**
 * @file utils/config/json.c
 *
 * @brief Low-level JSON helper implementation
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
#include <stdio.h>      /* FILE, fopen, fseek, ftell, fread, fclose */
#include <stdlib.h>     /* NULL, free, malloc */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <defs/config.h>
#include <logger.h>

/* Local includes */
#include <utils/config/json.h>

/** Files recorded so far as having failed to parse */
static char s_syntax_error_files[JSON_SYNTAX_ERROR_MAX_FILES]
    [CONFIG_MAX_LENGTH_PATH_CONFIG];

/** Number of entries currently in 's_syntax_error_files' */
static uint32_t s_syntax_error_count = 0u;



/* Convert a hexadecimal color string to an unsigned 32-bit integer */
uint32_t json_hex2uint32(const char *hex_color)
{
    uint32_t color = 0;

    if (hex_color == NULL) {
        return 0;
    }
    if (hex_color[0] == '#') {
        hex_color++;
    }
    if (sscanf(hex_color, "%x", &color) != 1) {
        LOGGER_DEBUG("Could not parse '%s' as a hex-encoded RGB" \
                " color; defaulting to '#000000'", hex_color);
        return 0;
    }

    return color;
}


/* Normalize a JSON field name to a canonical separator form */
bool json_field_normalize(const char *restrict field,
        char *restrict field_norm, size_t size)
{
    size_t i;

    if (field == NULL || field_norm == NULL || size == 0) {
        return false;
    }

    for (i = 0; field[i] != '\0' && i < size - 1; ++i) {
        if (field[i] == '_' || field[i] == '-') {
            field_norm[i] = '-';
        } else if (field[i] >= 'A' && field[i] <= 'Z') {
            field_norm[i] = (char) (field[i] - 'A' + 'a');
        } else {
            field_norm[i] = field[i];
        }
    }

    if (field[i] != '\0') {
        field_norm[0] = '\0';
        return false;
    }

    field_norm[i] = '\0';
    return true;
}


/* Retrieve a JSON object item by canonicalized field name */
cJSON *json_get_item(cJSON *json, const char *field)
{
    cJSON *item;
    char field_norm[JSON_FIELD_MAX];
    char item_norm[JSON_FIELD_MAX];

    if (json == NULL || field == NULL || !cJSON_IsObject(json)) {
        return NULL;
    }

    if (!json_field_normalize(field, field_norm, sizeof(field_norm))) {
        return NULL;
    }

    cJSON_ArrayForEach(item, json) {
        if (item->string == NULL) {
            continue;
        }

        if (!json_field_normalize(item->string, item_norm,
                    sizeof(item_norm))) {
            continue;
        }

        if (safe_strcmp(field_norm, item_norm) == 0) {
            return item;
        }
    }

    return NULL;
}


/* Load a color value from a JSON object into a 'uint32_t' */
int json_load_color(cJSON *json, const char *field, uint32_t *dest)
{
    cJSON *item;

    item = json_get_item(json, field);
    if (item && cJSON_IsString(item)) {
        *dest = json_hex2uint32(item->valuestring);
        return 0;
    }

    LOGGER_DEBUG("JSON color string for key: '%s' is missing;" \
            " using default value", field);
    return 1;
}


/* Load a string value from a JSON object into a buffer */
int json_load_string(cJSON *json, const char *restrict field,
        char *restrict dest, size_t size)
{
    cJSON *item;

    item = json_get_item(json, field);
    if (item && cJSON_IsString(item)) {
        safe_strncpy(dest, item->valuestring, size);
        return 0;
    }

    LOGGER_DEBUG("JSON string for key: '%s' is missing;" \
            " using default value", field);
    return 1;
}


/* Load an unsigned integer value from a JSON object */
int json_load_uint(cJSON *json, const char *field, uint32_t *dest)
{
    cJSON *item;

    item = json_get_item(json, field);
    if (item && cJSON_IsNumber(item)) {
        if (item->valueint < 0) {
            LOGGER_WARNING("JSON unsigned integer for key '%s' is" \
                    " negative (%d); using default value", field,
                    item->valueint);
            return 1;
        }
        *dest = (uint32_t) item->valueint;
        return 0;
    }

    LOGGER_DEBUG("JSON unsigned integer for key '%s' is missing;" \
            " using default value", field);
    return 1;
}


/* Load a boolean value from a JSON object */
int json_load_bool(cJSON *json, const char *field, bool *dest)
{
    cJSON *item;

    item = json_get_item(json, field);
    if (item && cJSON_IsBool(item)) {
        *dest = cJSON_IsTrue(item);
        return 0;
    }

    LOGGER_DEBUG("JSON boolean for key '%s' is missing;" \
            " using default value", field);
    return 1;
}


/* Read a JSON file into a dynamically allocated string */
int json_load_file(const char *filename, char **data)
{
    FILE *file;
    size_t length;
    size_t nread;
    long file_length;

    LOGGER_DEBUG("Parsing data from file '%s'", filename);

    if (data == NULL) {
        LOGGER_ERROR("Received null output pointer for file '%s'",
                filename);
        return 1;
    }

    *data = NULL;

    LOGGER_TRACE("Opening JSON file '%s'", filename);
    file = fopen(filename, "r");
    if (!file) {
        LOGGER_WARNING("File not found or unable to open:" \
                " '%s'; default values will be used", filename);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        LOGGER_WARNING("Unable to seek JSON file '%s';" \
                " default values will be used", filename);
        return 1;
    }
    file_length = ftell(file);

    if (file_length <= 0) {
        fclose(file);
        LOGGER_WARNING("File '%s' is empty or unreadable;" \
                " default values will be used", filename);
        return 1;
    }
    length = (size_t) file_length;
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        LOGGER_WARNING("Unable to rewind JSON file '%s';" \
                " default values will be used", filename);
        return 1;
    }

    *data = malloc(length + 1);
    if (*data == NULL) {
        LOGGER_ERROR("Failed to allocate memory for file '%s'",
                filename);
        fclose(file);
        return 2;
    }

    LOGGER_TRACE("Reading JSON file '%s'", filename);
    nread = fread(*data, 1, length, file);
    if (nread != length) {
        LOGGER_WARNING("Failed to read full JSON file '%s';" \
                " default values will be used", filename);
        free(*data);
        *data = NULL;
        fclose(file);
        return 1;
    }

    (*data)[length] = '\0';

    LOGGER_TRACE("Closing JSON file '%s'", filename);
    fclose(file);

    return 0;
}


/* Parse a JSON configuration file into a 'cJSON' object */
int json_load_config(const char *filename, cJSON **json_out)
{
    cJSON *json;
    cJSON *json_root;
    char *data;

    if (json_load_file(filename, &data) != 0) {
        return 1;
    }

    json_root = cJSON_Parse(data);
    if (json_root == NULL) {
        LOGGER_WARNING("Failed to parse file '%s';" \
                " default configuration will be used", filename);
        LOGGER_TRACE("JSON parse error: %s", cJSON_GetErrorPtr());
        json_syntax_errors_record(filename);
        free(data);
        return 2;
    }

    json = json_root;
    if (!cJSON_IsObject(json_root)) {
        if (cJSON_IsArray(json_root) &&
                cJSON_GetArraySize(json_root) >= 1) {
            cJSON *const array_first = cJSON_GetArrayItem(json_root, 0);

            if (array_first && cJSON_IsObject(array_first)) {
                json = cJSON_Duplicate(array_first, cJSON_True);
                cJSON_Delete(json_root);
                if (json == NULL) {
                    LOGGER_WARNING("Failed to duplicate object from" \
                            " '%s'; default configuration" \
                            " will be used", filename);
                    free(data);
                    return 2;
                }
                LOGGER_NOTICE("Using first object from top-level array" \
                        " in '%s' as compatibility fallback", filename);
            } else {
                LOGGER_WARNING("Invalid top-level JSON in '%s';" \
                        " expected an object and default" \
                        " configuration will be used", filename);
                json_syntax_errors_record(filename);
                cJSON_Delete(json_root);
                free(data);
                return 2;
            }
        } else {
            LOGGER_WARNING("Invalid top-level JSON in '%s'; expected" \
                    " an object and default configuration will be used",
                    filename);
            json_syntax_errors_record(filename);
            cJSON_Delete(json_root);
            free(data);
            return 2;
        }
    }

    free(data);
    *json_out = json;
    return 0;
}


/* Clear the list of files json_load_config has recorded a syntax
 * error for */
void json_syntax_errors_reset(void)
{
    s_syntax_error_count = 0u;
}


/* Record that 'filename' failed to parse as JSON */
void json_syntax_errors_record(const char *filename)
{
    if (filename == NULL) {
        return;
    }

    /* Do not record the same file twice: 'json_load_config' can be
     * called more than once for the same path within a single load
     * (e.g., a lint pass that re-checks a file already loaded once),
     * and a repeated entry would just be noise in the eventual warning
     * dialog rather than new information. */
    for (uint32_t i = 0u; i < s_syntax_error_count; ++i) {
        if (safe_strcmp(s_syntax_error_files[i], filename) == 0) {
            return;
        }
    }

    if (s_syntax_error_count >= JSON_SYNTAX_ERROR_MAX_FILES) {
        return;
    }

    safe_strncpy(s_syntax_error_files[s_syntax_error_count], filename,
            sizeof(s_syntax_error_files[s_syntax_error_count]));
    ++s_syntax_error_count;
}


/* Number of files currently recorded as having failed to parse */
uint32_t json_syntax_errors_count(void)
{
    return s_syntax_error_count;
}


/* Retrieve one recorded filename by index */
const char *json_syntax_errors_get(uint32_t index)
{
    if (index >= s_syntax_error_count) {
        return NULL;
    }
    return s_syntax_error_files[index];
}
