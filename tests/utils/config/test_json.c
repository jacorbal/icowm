/**
 * @file tests/utils/config/test_json.c
 *
 * @brief Test battery for low-level JSON configuration helpers
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <utils/config/json.h>
#include <harness/tap.h>


/**
 * @brief Write 'content' to a fresh temporary file and return its
 *        path (caller must unlink it when done)
 *
 * Builds its own unique name from the process ID and a call counter
 * rather than 'mkstemp' (which needs a newer POSIX feature-test
 * level than this project's own '_POSIX_C_SOURCE 200112L' exposes):
 * test-only code, not a security-sensitive temp-file creation this
 * would matter for.
 */
static void s_write_temp_file(char *path_out, size_t path_out_size,
        const char *content)
{
    static int s_counter = 0;
    FILE *f;

    snprintf(path_out, path_out_size, "/tmp/icowm_test_json_%d_%d",
            (int) getpid(), s_counter++);
    f = fopen(path_out, "w");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}


/* json_hex2uint32: NULL, with/without '#', valid and invalid hex */
static void s_test_hex2uint32(void)
{
    TAP_EQ_INT(json_hex2uint32(NULL), 0u, "NULL input yields 0");
    TAP_EQ_INT(json_hex2uint32("#FF0000"), 0xFF0000u,
            "'#' prefix stripped and parsed");
    TAP_EQ_INT(json_hex2uint32("FF0000"), 0xFF0000u,
            "no '#' prefix parses the same");
    TAP_EQ_INT(json_hex2uint32("#000000"), 0u, "all-zero color");
    TAP_EQ_INT(json_hex2uint32("#123abc"), 0x123ABCu,
            "lowercase hex digits parsed correctly");
    TAP_EQ_INT(json_hex2uint32("not-hex-at-all"), 0u,
            "unparseable input falls back to 0");
}


/* json_field_normalize: NULL arguments, separator canonicalization,
 * case folding, and a buffer too small to hold the result */
static void s_test_field_normalize(void)
{
    char out[64];

    TAP_OK(!json_field_normalize(NULL, out, sizeof(out)),
            "NULL field fails");
    TAP_OK(!json_field_normalize("x", NULL, sizeof(out)),
            "NULL output buffer fails");
    TAP_OK(!json_field_normalize("x", out, 0u),
            "zero-size buffer fails");

    TAP_OK(json_field_normalize("my_field", out, sizeof(out)),
            "underscore normalization succeeds");
    TAP_EQ_STR(out, "my-field", "'_' normalized to '-'");

    TAP_OK(json_field_normalize("MY-FIELD", out, sizeof(out)),
            "uppercase normalization succeeds");
    TAP_EQ_STR(out, "my-field", "uppercase folded to lowercase");

    TAP_OK(json_field_normalize("Mixed_Case-Field", out, sizeof(out)),
            "mixed separators and case succeeds");
    TAP_EQ_STR(out, "mixed-case-field",
            "both separators and case normalized together");

    TAP_OK(!json_field_normalize("way-too-long-to-fit", out, 5u),
            "a result that does not fit the buffer fails");
}


/* json_get_item: NULL/non-object arguments, exact match, and match
 * across differing (but equivalent) separator/case forms */
static void s_test_get_item(void)
{
    cJSON *json = cJSON_Parse(
            "{\"my_field\": 1, \"Other-Field\": 2}");

    TAP_NULL(json_get_item(NULL, "x"), "NULL json yields NULL");
    TAP_NULL(json_get_item(json, NULL), "NULL field yields NULL");
    TAP_NOT_NULL(json_get_item(json, "my_field"),
            "exact-name match found");
    TAP_NOT_NULL(json_get_item(json, "my-field"),
            "'-' finds a field actually spelled with '_'");
    TAP_NOT_NULL(json_get_item(json, "OTHER_FIELD"),
            "case and separator both differ, still matches");
    TAP_NULL(json_get_item(json, "nonexistent"),
            "a field that is not there at all yields NULL");

    cJSON_Delete(json);
}


/* json_load_color/string/uint/bool: success and missing/wrong-type
 * failure, for each of the four typed loaders */
static void s_test_typed_loaders(void)
{
    cJSON *json = cJSON_Parse(
            "{\"color\": \"#FF00FF\", \"name\": \"hello\","
            " \"count\": 42, \"flag\": true, \"wrong_type\": 123}");
    uint32_t color = 0xDEADBEEFu;
    char name[32] = "untouched";
    unsigned int count = 999u;
    bool flag = false;

    TAP_EQ_INT(json_load_color(json, "color", &color), 0,
            "color loads successfully");
    TAP_EQ_INT(color, 0xFF00FFu, "color value is correct");
    TAP_EQ_INT(json_load_color(json, "missing", &color), 1,
            "missing color field fails");
    TAP_EQ_INT(color, 0xFF00FFu,
            "dest left unchanged when the field is missing");

    TAP_EQ_INT(json_load_string(json, "name", name, sizeof(name)), 0,
            "string loads successfully");
    TAP_EQ_STR(name, "hello", "string value is correct");
    TAP_EQ_INT(json_load_string(json, "missing", name, sizeof(name)),
            1, "missing string field fails");

    TAP_EQ_INT(json_load_uint(json, "count", &count), 0,
            "uint loads successfully");
    TAP_EQ_INT(count, 42, "uint value is correct");
    TAP_EQ_INT(json_load_uint(json, "name", &count), 1,
            "wrong-type field (a string) fails for uint");

    TAP_EQ_INT(json_load_bool(json, "flag", &flag), 0,
            "bool loads successfully");
    TAP_OK(flag, "bool value is correct");
    TAP_EQ_INT(json_load_bool(json, "count", &flag), 1,
            "wrong-type field (a number) fails for bool");

    cJSON_Delete(json);
}


/* json_load_file: an ordinary file, a missing file, an empty file,
 * and a NULL output pointer */
static void s_test_load_file(void)
{
    char path[64];
    char *data = NULL;

    s_write_temp_file(path, sizeof(path), "hello world");
    TAP_EQ_INT(json_load_file(path, &data), 0,
            "ordinary file reads successfully");
    TAP_EQ_STR(data, "hello world", "file content read correctly");
    free(data);
    unlink(path);

    data = (char *) 0x1;  /* sentinel: must be reset to NULL on failure */
    TAP_EQ_INT(json_load_file("/nonexistent/path/at/all", &data), 1,
            "missing file fails");
    TAP_NULL(data, "*data reset to NULL on failure");

    TAP_EQ_INT(json_load_file("/tmp", NULL), 1,
            "NULL output pointer fails cleanly, not a crash");

    s_write_temp_file(path, sizeof(path), "");
    data = (char *) 0x1;
    TAP_EQ_INT(json_load_file(path, &data), 1, "empty file fails");
    unlink(path);
}


/* json_load_config: a valid object, a syntax error, the single-
 * element-array compatibility fallback, and a top-level value that
 * is neither an object nor a usable array */
static void s_test_load_config(void)
{
    char path[64];
    cJSON *json = NULL;

    json_syntax_errors_reset();

    s_write_temp_file(path, sizeof(path), "{\"a\": 1}");
    TAP_EQ_INT(json_load_config(path, &json), 0,
            "a plain JSON object loads successfully");
    TAP_OK(cJSON_IsObject(json), "result is a JSON object");
    cJSON_Delete(json);
    unlink(path);

    s_write_temp_file(path, sizeof(path), "{not valid json");
    TAP_EQ_INT(json_load_config(path, &json), 2,
            "malformed JSON fails with status 2");
    TAP_EQ_INT((int) json_syntax_errors_count(), 1,
            "the syntax error is recorded");
    unlink(path);

    json_syntax_errors_reset();
    s_write_temp_file(path, sizeof(path), "[{\"a\": 1}]");
    TAP_EQ_INT(json_load_config(path, &json), 0,
            "single-element array: compatibility fallback succeeds");
    TAP_OK(cJSON_IsObject(json), "result is the array's own object");
    cJSON_Delete(json);
    unlink(path);

    s_write_temp_file(path, sizeof(path), "42");
    TAP_EQ_INT(json_load_config(path, &json), 2,
            "a bare number at the top level fails");
    unlink(path);

    s_write_temp_file(path, sizeof(path), "[]");
    TAP_EQ_INT(json_load_config(path, &json), 2,
            "an empty array fails: no first element to use");
    unlink(path);

    TAP_EQ_INT(json_load_config("/nonexistent/at/all", &json), 1,
            "a missing file fails with status 1, not 2");
}


/* json_syntax_errors_*: reset, record (including de-duplication and
 * the cap on how many are tracked), count, and get */
static void s_test_syntax_error_tracking(void)
{
    json_syntax_errors_reset();
    TAP_EQ_INT((int) json_syntax_errors_count(), 0,
            "reset leaves the count at 0");

    json_syntax_errors_record("a.json");
    TAP_EQ_INT((int) json_syntax_errors_count(), 1,
            "recording one file brings the count to 1");
    TAP_EQ_STR(json_syntax_errors_get(0u), "a.json",
            "the recorded filename comes back correctly");
    TAP_NULL(json_syntax_errors_get(1u),
            "an out-of-range index yields NULL");

    json_syntax_errors_record("a.json");
    TAP_EQ_INT((int) json_syntax_errors_count(), 1,
            "recording the same file twice does not duplicate it");

    json_syntax_errors_record(NULL);
    TAP_EQ_INT((int) json_syntax_errors_count(), 1,
            "recording NULL is a safe no-op, not a crash");

    json_syntax_errors_reset();
    for (int i = 0; i < JSON_SYNTAX_ERROR_MAX_FILES + 3; ++i) {
        char name[32];

        snprintf(name, sizeof(name), "file%d.json", i);
        json_syntax_errors_record(name);
    }
    TAP_EQ_INT((int) json_syntax_errors_count(),
            JSON_SYNTAX_ERROR_MAX_FILES,
            "recording past the cap stops growing at the cap");

    json_syntax_errors_reset();
}


int main(void)
{
    TAP_PLAN(57);

    s_test_hex2uint32();
    s_test_field_normalize();
    s_test_get_item();
    s_test_typed_loaders();
    s_test_load_file();
    s_test_load_config();
    s_test_syntax_error_tracking();

    return TAP_DONE();
}
