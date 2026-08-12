/**
 * @file ipc/args.c
 *
 * @brief Typed extraction of a request's own arguments implementation
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <ipc/args.h>


/* Read a required unsigned integer field */
bool ipc_args_get_uint(const cJSON *args, const char *field,
        uint32_t *out)
{
    cJSON *item = cJSON_GetObjectItem(args, field);

    if (item == NULL || !cJSON_IsNumber(item) || item->valuedouble < 0.0) {
        return false;
    }
    *out = (uint32_t) item->valuedouble;
    return true;
}


/* Read a required signed integer field */
bool ipc_args_get_int(const cJSON *args, const char *field, int32_t *out)
{
    cJSON *item = cJSON_GetObjectItem(args, field);

    if (item == NULL || !cJSON_IsNumber(item)) {
        return false;
    }
    *out = (int32_t) item->valuedouble;
    return true;
}


/* Read a required string field */
bool ipc_args_get_string(const cJSON *args, const char *field,
        const char **out)
{
    cJSON *item = cJSON_GetObjectItem(args, field);

    if (item == NULL || !cJSON_IsString(item) ||
            item->valuestring == NULL) {
        return false;
    }
    *out = item->valuestring;
    return true;
}


/* Read a required boolean field */
bool ipc_args_get_bool(const cJSON *args, const char *field, bool *out)
{
    cJSON *item = cJSON_GetObjectItem(args, field);

    if (item == NULL || !cJSON_IsBool(item)) {
        return false;
    }
    *out = cJSON_IsTrue(item) ? true : false;
    return true;
}
