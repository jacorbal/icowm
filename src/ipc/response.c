/**
 * @file ipc/response.c
 *
 * @brief Shared IPC response-building helpers implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <ipc/response.h>


/* Build a bare success response */
cJSON *ipc_response_ok(void)
{
    cJSON *resp = cJSON_CreateObject();

    if (resp == NULL) {
        return NULL;
    }
    cJSON_AddBoolToObject(resp, "ok", 1);
    return resp;
}


/* Build a standard failure response */
cJSON *ipc_response_error(const char *message)
{
    cJSON *resp = cJSON_CreateObject();

    if (resp == NULL) {
        return NULL;
    }
    cJSON_AddBoolToObject(resp, "ok", 0);
    cJSON_AddStringToObject(resp, "error", message);
    return resp;
}
