/**
 * @file tests/ipc/test_args.c
 *
 * @brief Test battery for typed IPC argument extraction
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
#include <ipc/args.h>


/* ipc_args_get_uint: present, non-negative number succeeds; missing,
 * wrong-typed, or negative all fail cleanly without touching 'out' */
static void s_test_get_uint(void)
{
    cJSON *args = cJSON_CreateObject();
    uint32_t out = 999u;

    cJSON_AddNumberToObject(args, "id", 42);
    TAP_OK(ipc_args_get_uint(args, "id", &out), "a present, valid uint" \
            " field succeeds");
    TAP_EQ_INT((long) out, 42, "the extracted value is correct");

    out = 999u;
    TAP_OK(!ipc_args_get_uint(args, "missing", &out),
            "a missing field fails");
    TAP_EQ_INT((long) out, 999,
            "'out' is left untouched when the field is missing");

    cJSON_AddStringToObject(args, "name", "hello");
    TAP_OK(!ipc_args_get_uint(args, "name", &out),
            "a string field fails to parse as a uint");

    cJSON_AddNumberToObject(args, "negative", -5);
    TAP_OK(!ipc_args_get_uint(args, "negative", &out),
            "a negative number is rejected for an unsigned field");

    cJSON_Delete(args);
}


/* ipc_args_get_int: present number succeeds (negative allowed);
 * missing or wrong-typed fails */
static void s_test_get_int(void)
{
    cJSON *args = cJSON_CreateObject();
    int32_t out = 999;

    cJSON_AddNumberToObject(args, "x", -150);
    TAP_OK(ipc_args_get_int(args, "x", &out),
            "a present, negative int field succeeds");
    TAP_EQ_INT(out, -150, "the extracted negative value is correct");

    TAP_OK(!ipc_args_get_int(args, "missing", &out),
            "a missing field fails");

    cJSON_AddBoolToObject(args, "flag", 1);
    TAP_OK(!ipc_args_get_int(args, "flag", &out),
            "a boolean field fails to parse as an int");

    cJSON_Delete(args);
}


/* ipc_args_get_string: present string succeeds; missing or
 * wrong-typed fails */
static void s_test_get_string(void)
{
    cJSON *args = cJSON_CreateObject();
    const char *out = NULL;

    cJSON_AddStringToObject(args, "name", "my-client");
    TAP_OK(ipc_args_get_string(args, "name", &out),
            "a present string field succeeds");
    TAP_EQ_STR(out, "my-client", "the extracted string is correct");

    TAP_OK(!ipc_args_get_string(args, "missing", &out),
            "a missing field fails");

    cJSON_AddNumberToObject(args, "count", 5);
    TAP_OK(!ipc_args_get_string(args, "count", &out),
            "a number field fails to parse as a string");

    cJSON_Delete(args);
}


/* ipc_args_get_bool: present boolean (either value) succeeds;
 * missing or wrong-typed fails */
static void s_test_get_bool(void)
{
    cJSON *args = cJSON_CreateObject();
    bool out = false;

    cJSON_AddBoolToObject(args, "enabled", 1);
    TAP_OK(ipc_args_get_bool(args, "enabled", &out),
            "a present true field succeeds");
    TAP_OK(out, "the extracted value is true");

    cJSON_AddBoolToObject(args, "disabled", 0);
    TAP_OK(ipc_args_get_bool(args, "disabled", &out),
            "a present false field succeeds");
    TAP_OK(!out, "the extracted value is false");

    TAP_OK(!ipc_args_get_bool(args, "missing", &out),
            "a missing field fails");

    cJSON_AddStringToObject(args, "text", "true");
    TAP_OK(!ipc_args_get_bool(args, "text", &out),
            "a string field (even \"true\") fails to parse as a bool");

    cJSON_Delete(args);
}


int main(void)
{
    TAP_PLAN(20);

    s_test_get_uint();
    s_test_get_int();
    s_test_get_string();
    s_test_get_bool();

    return TAP_DONE();
}
