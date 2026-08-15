/**
 * @file tests/ipc/test_response.c
 *
 * @brief Test battery for shared IPC response-building helpers
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

/* Local includes */
#include <harness/tap.h>
#include <ipc/response.h>


/* ipc_response_ok builds exactly {"ok": true}, nothing else */
static void s_test_response_ok(void)
{
    cJSON *resp = ipc_response_ok();
    cJSON *ok_field;

    TAP_NOT_NULL(resp, "ipc_response_ok succeeds");

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_NOT_NULL(ok_field, "the response has an 'ok' field");
    TAP_OK(cJSON_IsBool(ok_field), "'ok' is a boolean");
    TAP_OK(cJSON_IsTrue(ok_field), "'ok' is true");
    TAP_NULL(cJSON_GetObjectItem(resp, "error"),
            "a success response carries no 'error' field");

    cJSON_Delete(resp);
}


/* ipc_response_error builds {"ok": false, "error": <message>},
 * carrying the exact message given */
static void s_test_response_error(void)
{
    cJSON *resp = ipc_response_error("something went wrong");
    cJSON *ok_field;
    cJSON *error_field;

    TAP_NOT_NULL(resp, "ipc_response_error succeeds");

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_NOT_NULL(ok_field, "the response has an 'ok' field");
    TAP_OK(cJSON_IsBool(ok_field), "'ok' is a boolean");
    TAP_OK(!cJSON_IsTrue(ok_field), "'ok' is false");

    error_field = cJSON_GetObjectItem(resp, "error");
    TAP_NOT_NULL(error_field, "the response has an 'error' field");
    TAP_OK(cJSON_IsString(error_field), "'error' is a string");
    TAP_EQ_STR(error_field->valuestring, "something went wrong",
            "'error' carries the exact message given");

    cJSON_Delete(resp);
}


int main(void)
{
    TAP_PLAN(12);

    s_test_response_ok();
    s_test_response_error();

    return TAP_DONE();
}
