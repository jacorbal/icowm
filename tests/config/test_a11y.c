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

/* Default initial values */
#include <defs/input.h>
#include <defs/urgency.h>

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


/* An empty object is a completely valid, minimal file: succeeds, and
 * (unlike a merge) resets every field to its own built-in default,
 * not whatever the struct happened to hold coming in */
static void s_test_empty_object_resets_to_defaults(void)
{
    char path[64];
    struct config_a11y_s a11y;

    memset(&a11y, 0, sizeof(a11y));
    a11y.is_enabled = true;
    a11y.interaction.double_click_ms = 999u;
    a11y.focus_indicator.min_border_width = 7u;
    a11y.urgency.audible_bell = true;
    a11y.urgency.blink_interval_ms = 111u;

    s_write_temp_file(path, sizeof(path), "{}");
    TAP_EQ_INT(config_load_a11y(path, &a11y), 0,
            "an empty object loads successfully");
    TAP_OK(!a11y.is_enabled,
            "is_enabled resets to false (its own built-in default)");
    TAP_EQ_INT((int) a11y.interaction.double_click_ms,
            WM_DOUBLE_CLICK_MS,
            "double_click_ms resets to its own built-in default");
    TAP_EQ_INT((int) a11y.focus_indicator.min_border_width, 0,
            "min_border_width resets to its own built-in default");
    TAP_OK(!a11y.urgency.audible_bell,
            "audible_bell resets to its own built-in default");
    TAP_EQ_INT((int) a11y.urgency.blink_interval_ms,
            WM_URGENCY_BLINK_INTERVAL_MS,
            "blink_interval_ms resets to its own built-in default");
    unlink(path);
}


/* 'is-enabled' absent from an otherwise fully populated file: every
 * other field is parsed but never applied, since 'is-enabled'
 * defaults to false, the same opt-in-only posture as randr.json's
 * own */
static void s_test_is_enabled_absent_ignores_other_fields(void)
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
            "loads successfully even without 'is-enabled'");
    TAP_OK(!a11y.is_enabled,
            "is_enabled defaults to false when absent");
    TAP_EQ_INT((int) a11y.interaction.double_click_ms,
            WM_DOUBLE_CLICK_MS,
            "double-click-ms ignored while not enabled");
    TAP_EQ_INT((int) a11y.focus_indicator.min_border_width, 0,
            "min-border-width ignored while not enabled");
    TAP_OK(!a11y.urgency.audible_bell,
            "audible-bell ignored while not enabled");
    TAP_EQ_INT((int) a11y.urgency.blink_interval_ms,
            WM_URGENCY_BLINK_INTERVAL_MS,
            "blink-interval-ms ignored while not enabled");
    unlink(path);
}


/* 'is-enabled' explicitly false behaves exactly like it being absent:
 * every other field present in the very same file is still ignored */
static void s_test_is_enabled_explicit_false_ignores_other_fields(void)
{
    char path[256];
    struct config_a11y_s a11y;

    memset(&a11y, 0, sizeof(a11y));
    s_write_temp_file(path, sizeof(path),
        "{\"is-enabled\": false,"
        "\"interaction\": {\"double-click-ms\": 250}}");

    TAP_EQ_INT(config_load_a11y(path, &a11y), 0,
            "loads successfully with 'is-enabled': false");
    TAP_EQ_INT((int) a11y.interaction.double_click_ms,
            WM_DOUBLE_CLICK_MS,
            "double-click-ms still ignored: 'is-enabled' is false");
    unlink(path);
}


/* 'is-enabled': true with every field fully populated: each one
 * lands in the right place */
static void s_test_full_file_enabled(void)
{
    char path[256];
    struct config_a11y_s a11y;

    memset(&a11y, 0, sizeof(a11y));
    s_write_temp_file(path, sizeof(path),
        "{\"is-enabled\": true,"
        "\"interaction\": {\"double-click-ms\": 250},"
        "\"focus-indicator\": {\"min-border-width\": 4},"
        "\"urgency\": {\"audible-bell\": true,"
        "\"blink-interval-ms\": 300}}");

    TAP_EQ_INT(config_load_a11y(path, &a11y), 0,
            "a fully populated, enabled file loads successfully");
    TAP_OK(a11y.is_enabled, "is-enabled loaded correctly");
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


/* Enabled, but only one of the three top-level objects present: only
 * its own fields change from their built-in default; the other two
 * objects being entirely absent still resets, rather than merges,
 * same as 's_test_empty_object_resets_to_defaults' above */
static void s_test_partial_file_only_urgency(void)
{
    char path[128];
    struct config_a11y_s a11y;

    memset(&a11y, 0, sizeof(a11y));
    s_write_temp_file(path, sizeof(path),
        "{\"is-enabled\": true,"
        "\"urgency\": {\"blink-interval-ms\": 900}}");

    TAP_EQ_INT(config_load_a11y(path, &a11y), 0,
            "a file with only 'urgency' loads successfully");
    TAP_EQ_INT((int) a11y.interaction.double_click_ms,
            WM_DOUBLE_CLICK_MS,
            "interaction resets to default: the file omits it");
    TAP_EQ_INT((int) a11y.focus_indicator.min_border_width, 0,
            "focus-indicator resets to default: the file omits it");
    TAP_EQ_INT((int) a11y.urgency.blink_interval_ms, 900,
            "blink-interval-ms loaded from the one object present");
    unlink(path);
}


/* A reload that turns 'is-enabled' back to false must fall back
 * cleanly to every field's own built-in default, not keep whatever
 * an earlier, still-enabled load on the very same struct left there
 * (the same class of bug already fixed for topology's own per-
 * desktop 'background-color'; see test_base.c) */
static void s_test_reload_disabling_resets_stale_values(void)
{
    struct config_a11y_s a11y;
    char path[256];

    memset(&a11y, 0, sizeof(a11y));
    s_write_temp_file(path, sizeof(path),
        "{\"is-enabled\": true,"
        "\"interaction\": {\"double-click-ms\": 250},"
        "\"urgency\": {\"blink-interval-ms\": 300}}");
    config_load_a11y(path, &a11y);
    unlink(path);
    TAP_EQ_INT((int) a11y.interaction.double_click_ms, 250,
            "before disabling: double-click-ms loaded");

    /* Reload the very same 'a11y' (no reset in between, the way an
     * actual configuration reload leaves it) from a file that turns
     * 'is-enabled' back off. */
    s_write_temp_file(path, sizeof(path), "{\"is-enabled\": false}");
    config_load_a11y(path, &a11y);
    unlink(path);

    TAP_EQ_INT((int) a11y.interaction.double_click_ms,
            WM_DOUBLE_CLICK_MS,
            "after disabling: double-click-ms falls back to default,"
            " not the stale 250");
    TAP_EQ_INT((int) a11y.urgency.blink_interval_ms,
            WM_URGENCY_BLINK_INTERVAL_MS,
            "after disabling: blink-interval-ms falls back to"
            " default, not the stale 300");
}


int main(void)
{
    TAP_PLAN(28);

    s_test_missing_file();
    s_test_empty_object_resets_to_defaults();
    s_test_is_enabled_absent_ignores_other_fields();
    s_test_is_enabled_explicit_false_ignores_other_fields();
    s_test_full_file_enabled();
    s_test_partial_file_only_urgency();
    s_test_reload_disabling_resets_stale_values();

    return TAP_DONE();
}
