/**
 * @file tests/config/test_a11y.c
 *
 * @brief Test battery for accessibility (a11y) settings loading
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

    snprintf(path_out, path_out_size,
            "/tmp/icowm_test_a11y_%d_%d",
            (int) getpid(), s_counter++);
    f = fopen(path_out, "w");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}


/* A missing file fails outright, leaving the caller free to keep
 * whatever defaults it already had */
static void s_test_missing_file(void)
{
    struct config_a11y_s a11y;

    memset(&a11y, 0, sizeof(a11y));
    TAP_EQ_INT(config_load_a11y("/nonexistent/at/all",
                &a11y), 1, "a missing file fails");
}


/* An empty object is a completely valid, minimal file: succeeds,
 * leaves every field untouched */
static void s_test_empty_object_keeps_existing_values(void)
{
    char path[64];
    struct config_a11y_s a11y;

    memset(&a11y, 0, sizeof(a11y));
    a11y.interaction.double_click_ms = 999u;
    a11y.focus_indicator.min_border_width = 7u;
    a11y.urgency.audible_bell = true;
    a11y.urgency.blink_interval_ms = 111u;

    s_write_temp_file(path, sizeof(path), "{}");
    TAP_EQ_INT(config_load_a11y(path, &a11y), 0,
            "an empty object loads successfully");
    TAP_EQ_INT((int) a11y.interaction.double_click_ms, 999,
            "double_click_ms untouched by an empty file");
    TAP_EQ_INT((int) a11y.focus_indicator.min_border_width, 7,
            "min_border_width untouched by an empty file");
    TAP_OK(a11y.urgency.audible_bell,
            "audible_bell untouched by an empty file");
    TAP_EQ_INT((int) a11y.urgency.blink_interval_ms, 111,
            "blink_interval_ms untouched by an empty file");
    unlink(path);
}


/* Every field fully populated: each one lands in the right place */
static void s_test_full_file(void)
{
    char path[256];
    struct config_a11y_s a11y;

    memset(&a11y, 0, sizeof(a11y));
    s_write_temp_file(path, sizeof(path),
        "{\"interaction\": {\"double-click-ms\": 250},"
        "\"focus-indicator\": {\"min-border-width\": 4},"
        "\"urgency\": {\"audible-bell\": true,"
        "\"blink-interval-ms\": 300}}");

    TAP_EQ_INT(config_load_a11y(path, &a11y), 0,
            "a fully populated file loads successfully");
    TAP_EQ_INT((int) a11y.interaction.double_click_ms, 250,
            "double-click-ms loaded correctly");
    TAP_EQ_INT((int) a11y.focus_indicator.min_border_width, 4,
            "min-border-width loaded correctly");
    TAP_OK(a11y.urgency.audible_bell,
            "audible-bell loaded correctly");
    TAP_EQ_INT((int) a11y.urgency.blink_interval_ms, 300,
            "blink-interval-ms loaded correctly");
    unlink(path);
}


/* Only one of the three top-level objects present: only its own
 * fields change, the other two stay at whatever they already held */
static void s_test_partial_file_only_urgency(void)
{
    char path[128];
    struct config_a11y_s a11y;

    memset(&a11y, 0, sizeof(a11y));
    a11y.interaction.double_click_ms = 400u;
    a11y.focus_indicator.min_border_width = 0u;

    s_write_temp_file(path, sizeof(path),
        "{\"urgency\": {\"blink-interval-ms\": 900}}");

    TAP_EQ_INT(config_load_a11y(path, &a11y), 0,
            "a file with only 'urgency' loads successfully");
    TAP_EQ_INT((int) a11y.interaction.double_click_ms, 400,
            "interaction untouched when the file omits it entirely");
    TAP_EQ_INT((int) a11y.focus_indicator.min_border_width, 0,
            "focus-indicator untouched when the file omits it entirely");
    TAP_EQ_INT((int) a11y.urgency.blink_interval_ms, 900,
            "blink-interval-ms loaded from the one object present");
    unlink(path);
}


int main(void)
{
    TAP_PLAN(15);

    s_test_missing_file();
    s_test_empty_object_keeps_existing_values();
    s_test_full_file();
    s_test_partial_file_only_urgency();

    return TAP_DONE();
}
