/**
 * @file tests/config/test_randr.c
 *
 * @brief Test battery for XRandR output profile configuration
 *        loading
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
#include <string.h>
#include <unistd.h>

/* XCB includes */
#include <xcb/randr.h>

/* Local includes */
#include <config.h>
#include <harness/tap.h>


/**
 * @brief Write 'content' to a fresh temporary file and return its
 *        path (caller must unlink it when done)
 */
static void s_write_temp_file(char *path_out, size_t path_out_size,
        const char *content)
{
    static int s_counter = 0;
    FILE *f;

    snprintf(path_out, path_out_size, "/tmp/icowm_test_randr_%d_%d",
            (int) getpid(), s_counter++);
    f = fopen(path_out, "w");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}


/* A missing file fails outright, leaving the caller free to keep
 * whatever defaults it already had */
static void s_test_missing_file(void)
{
    struct config_randr_s randr;

    TAP_EQ_INT(config_load_randr("/nonexistent/at/all", &randr), 1,
            "a missing file fails");
}


/* 'is-enabled' alone, with no 'outputs' array at all, is a
 * completely valid, minimal file: succeeds, leaves output_count
 * at 0 */
static void s_test_enabled_no_outputs(void)
{
    char path[64];
    struct config_randr_s randr;

    memset(&randr, 0, sizeof(randr));
    s_write_temp_file(path, sizeof(path), "{\"is-enabled\": true}");
    TAP_EQ_INT(config_load_randr(path, &randr), 0,
            "file with only 'is-enabled' loads successfully");
    TAP_OK(randr.is_enabled, "is_enabled loaded correctly");
    TAP_EQ_INT((int) randr.output_count, 0,
            "no 'outputs' array: output_count stays 0");
    unlink(path);
}


/* One fully populated output entry: every field lands in the right
 * place */
static void s_test_one_full_output(void)
{
    char path[512];
    struct config_randr_s randr;

    memset(&randr, 0, sizeof(randr));
    s_write_temp_file(path, sizeof(path),
        "{\"is-enabled\": true, \"outputs\": [ {"
        "\"name\": \"HDMI-1\","
        "\"is-enabled\": true,"
        "\"is-primary\": true,"
        "\"resolution\": {\"w\": 1920, \"h\": 1080},"
        "\"position\": {\"x\": 100, \"y\": 200},"
        "\"rotation\": \"left\""
        "} ] }");

    TAP_EQ_INT(config_load_randr(path, &randr), 0,
            "a fully populated output entry loads successfully");
    TAP_EQ_INT((int) randr.output_count, 1, "exactly one output loaded");
    TAP_EQ_STR(randr.outputs[0].name, "HDMI-1", "name loaded correctly");
    TAP_OK(randr.outputs[0].is_enabled, "is_enabled loaded correctly");
    TAP_OK(randr.outputs[0].is_primary, "is_primary loaded correctly");
    TAP_EQ_INT((int) randr.outputs[0].preferred_res.w, 1920,
            "resolution width loaded correctly");
    TAP_EQ_INT((int) randr.outputs[0].preferred_res.h, 1080,
            "resolution height loaded correctly");
    TAP_EQ_INT(randr.outputs[0].position.x, 100,
            "position x loaded correctly");
    TAP_EQ_INT(randr.outputs[0].position.y, 200,
            "position y loaded correctly");
    TAP_EQ_INT(randr.outputs[0].rotation, XCB_RANDR_ROTATION_ROTATE_90,
            "'left' maps to a 90-degree rotation");
    unlink(path);
}


/* Every recognized rotation string maps to its own correct mask,
 * and an unrecognized one falls back to no rotation at all, the
 * same as a missing rotation field entirely */
static void s_test_rotation_values(void)
{
    static const struct {
        const char *value;
        uint16_t expected;
    } cases[] = {
        { "normal",   XCB_RANDR_ROTATION_ROTATE_0 },
        { "left",     XCB_RANDR_ROTATION_ROTATE_90 },
        { "right",    XCB_RANDR_ROTATION_ROTATE_270 },
        { "inverted", XCB_RANDR_ROTATION_ROTATE_180 },
        { "sideways", XCB_RANDR_ROTATION_ROTATE_0 },  /* unrecognized */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char path[256];
        char content[256];
        struct config_randr_s randr;

        memset(&randr, 0, sizeof(randr));
        snprintf(content, sizeof(content),
                "{\"outputs\": [ {\"name\": \"X\","
                " \"rotation\": \"%s\"} ] }", cases[i].value);
        s_write_temp_file(path, sizeof(path), content);

        config_load_randr(path, &randr);
        TAP_EQ_INT(randr.outputs[0].rotation, cases[i].expected,
                cases[i].value);
        unlink(path);
    }
}


/* A missing rotation field defaults to no rotation, exactly like an
 * unrecognized rotation string does */
static void s_test_missing_rotation_defaults_to_zero(void)
{
    char path[256];
    struct config_randr_s randr;

    memset(&randr, 0, sizeof(randr));
    s_write_temp_file(path, sizeof(path),
            "{\"outputs\": [ {\"name\": \"X\"} ] }");
    config_load_randr(path, &randr);
    TAP_EQ_INT(randr.outputs[0].rotation, XCB_RANDR_ROTATION_ROTATE_0,
            "a missing rotation field defaults to no rotation");
    unlink(path);
}


/* An array entry that is not itself a JSON object (a bare string,
 * mixed in with real output objects) is silently skipped, not
 * counted, and does not corrupt the entries around it */
static void s_test_non_object_entry_is_skipped(void)
{
    char path[256];
    struct config_randr_s randr;

    memset(&randr, 0, sizeof(randr));
    s_write_temp_file(path, sizeof(path),
            "{\"outputs\": [ {\"name\": \"A\"}, \"not-an-object\","
            " {\"name\": \"B\"} ] }");
    config_load_randr(path, &randr);
    TAP_EQ_INT((int) randr.output_count, 2,
            "only the two real object entries are counted");
    TAP_EQ_STR(randr.outputs[0].name, "A",
            "first real entry loaded correctly");
    TAP_EQ_STR(randr.outputs[1].name, "B",
            "second real entry loaded correctly, not skipped over"
            " incorrectly");
    unlink(path);
}


/* More output entries than CONFIG_RANDR_MAX_OUTPUTS are clamped,
 * never overflowing the fixed-size 'outputs' array */
static void s_test_clamped_to_max_outputs(void)
{
    char path[8192];
    char content[8192];
    size_t offset;
    struct config_randr_s randr;

    memset(&randr, 0, sizeof(randr));
    offset = (size_t) snprintf(content, sizeof(content), "{\"outputs\": [");
    for (int i = 0; i < CONFIG_RANDR_MAX_OUTPUTS + 5; ++i) {
        offset += (size_t) snprintf(content + offset,
                sizeof(content) - offset,
                "%s{\"name\": \"out%d\"}", (i > 0) ? "," : "", i);
    }
    snprintf(content + offset, sizeof(content) - offset, "] }");

    s_write_temp_file(path, sizeof(path), content);
    TAP_EQ_INT(config_load_randr(path, &randr), 0,
            "a file with more outputs than the max still succeeds");
    TAP_EQ_INT((int) randr.output_count, CONFIG_RANDR_MAX_OUTPUTS,
            "output_count is clamped to CONFIG_RANDR_MAX_OUTPUTS");
    unlink(path);
}


/* Fields left out of an output entry keep their zero-initialized
 * defaults, rather than being left uninitialized (each output slot
 * is memset to 0 before its own fields are loaded) */
static void s_test_missing_fields_stay_zeroed(void)
{
    char path[256];
    struct config_randr_s randr;

    /* Only the per-output 'outputs[]' slot is ever re-initialized by
     * config_load_randr itself (its own explicit memset, right
     * before loading each entry); the top-level struct as a whole
     * is not, by the same contract every other json_load_* loader in
     * this project already follows (leave the destination alone
     * when the field is missing, so a caller's own already-set
     * default survives), so only 'outputs[0]' itself is poisoned
     * here, not the whole struct: poisoning 'is_enabled' too would
     * be testing a guarantee this function was never meant to make,
     * not a real defect. */
    memset(&randr, 0, sizeof(randr));
    memset(&randr.outputs[0], 0xAA, sizeof(randr.outputs[0]));
    s_write_temp_file(path, sizeof(path),
            "{\"outputs\": [ {\"name\": \"minimal\"} ] }");
    config_load_randr(path, &randr);

    TAP_OK(!randr.outputs[0].is_enabled,
            "unset is_enabled defaults to false, not poisoned data");
    TAP_OK(!randr.outputs[0].is_primary,
            "unset is_primary defaults to false, not poisoned data");
    TAP_EQ_INT((int) randr.outputs[0].preferred_res.w, 0,
            "unset resolution width defaults to 0");
    TAP_EQ_INT((int) randr.outputs[0].preferred_res.h, 0,
            "unset resolution height defaults to 0");
    unlink(path);
}


int main(void)
{
    TAP_PLAN(29);

    s_test_missing_file();
    s_test_enabled_no_outputs();
    s_test_one_full_output();
    s_test_rotation_values();
    s_test_missing_rotation_defaults_to_zero();
    s_test_non_object_entry_is_skipped();
    s_test_clamped_to_max_outputs();
    s_test_missing_fields_stay_zeroed();

    return TAP_DONE();
}
