/**
 * @file tests/config/test_lint.c
 *
 * @brief Test battery for the configuration file linter
 *
 * config_lint_run's own contract is its return value (the count of
 * unknown keys found, or a negative value if config_dir itself could
 * not be read); every finding is also printed to stderr, which this
 * battery redirects away for the duration of each call so those
 * lines do not interleave with this file's own TAP output.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Local includes */
#include <config/lint.h>
#include <harness/tap.h>


/**
 * @brief Create a fresh temporary config directory and return its
 *        path
 */
static void s_make_temp_config_dir(char *path_out, size_t path_out_size)
{
    static int s_counter = 0;

    snprintf(path_out, path_out_size, "/tmp/icowm_test_lint_%d_%d",
            (int) getpid(), s_counter++);
    mkdir(path_out, 0700);
}


/**
 * @brief Write 'content' to 'dir/name', creating 'dir' itself first
 *        if it does not already exist (used for the 'themes'
 *        subdirectory)
 */
static void s_write_file(const char *dir, const char *name,
        const char *content)
{
    char path[600];
    FILE *f;

    mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    f = fopen(path, "w");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}


static void s_remove_temp_dir(const char *path)
{
    char cmd[600];

    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    if (system(cmd) != 0) {
        /* best-effort cleanup only */
    }
}


/**
 * @brief Run config_lint_run(dir) with stderr redirected to
 *        /dev/null for the duration of the call, so its own
 *        findings never interleave with this file's TAP output
 */
static int s_lint_quietly(const char *dir)
{
    int saved_stderr;
    int devnull;
    int result;

    saved_stderr = dup(STDERR_FILENO);
    devnull = open("/dev/null", O_WRONLY);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    result = config_lint_run(dir);

    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);

    return result;
}


/* A NULL config_dir fails outright */
static void s_test_null_dir(void)
{
    TAP_EQ_INT(s_lint_quietly(NULL), -1, "NULL config_dir returns -1");
}


/* An unreadable/nonexistent config_dir fails outright */
static void s_test_missing_dir(void)
{
    TAP_EQ_INT(s_lint_quietly("/nonexistent/at/all"), -1,
            "a nonexistent config_dir returns -1");
}


/* An entirely empty directory: config.json is required, but a
 * missing required file is reported separately (to stderr), not
 * counted as an unknown key; the return value stays 0 */
static void s_test_empty_dir_missing_required_file(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    TAP_EQ_INT(s_lint_quietly(dir), 0,
            "an empty dir: 0 unknown keys, even though config.json"
            " (required) is missing and reported separately");
    s_remove_temp_dir(dir);
}


/* A recognized key at the top level of a schema is not flagged */
static void s_test_recognized_key_not_flagged(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "randr.json", "{\"is-enabled\": true}");
    TAP_EQ_INT(s_lint_quietly(dir), 0,
            "a recognized top-level key is not flagged");
    s_remove_temp_dir(dir);
}


/* An unrecognized key is flagged, exactly once */
static void s_test_unrecognized_key_flagged(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "randr.json", "{\"bogus-field\": true}");
    TAP_EQ_INT(s_lint_quietly(dir), 1,
            "one unrecognized key is flagged exactly once");
    s_remove_temp_dir(dir);
}


/* Key matching is case-insensitive, and '-'/'_' are equivalent
 * separators, the same as the real loaders */
static void s_test_key_matching_normalizes(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "randr.json", "{\"IS_ENABLED\": true}");
    TAP_EQ_INT(s_lint_quietly(dir), 0,
            "'IS_ENABLED' matches the schema's own 'is-enabled':"
            " case and separator both normalized");
    s_remove_temp_dir(dir);
}


/* A key starting with '-' or '_' is treated as a comment and never
 * flagged, even though it matches nothing in the schema */
static void s_test_comment_keys_never_flagged(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "randr.json",
            "{\"_comment\": \"a note\", \"-author\": \"me\"}");
    TAP_EQ_INT(s_lint_quietly(dir), 0,
            "keys starting with '_' or '-' are comments, never flagged");
    s_remove_temp_dir(dir);
}


/* Findings across multiple files are summed into one total */
static void s_test_findings_summed_across_files(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "randr.json", "{\"bogus-a\": 1}");
    s_write_file(dir, "rules.json", "{\"bogus-b\": 1, \"bogus-c\": 1}");
    TAP_EQ_INT(s_lint_quietly(dir), 3,
            "1 finding in randr.json + 2 in rules.json = 3 total");
    s_remove_temp_dir(dir);
}


/* A file with broken JSON syntax is treated the same as a missing
 * one (json_load_config itself fails), not specially reported and
 * not counted as an unknown key, for a file that is not required */
static void s_test_malformed_json_not_counted(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "randr.json", "{not valid json at all");
    TAP_EQ_INT(s_lint_quietly(dir), 0,
            "malformed JSON in a non-required file: 0 unknown keys,"
            " same as if the file were simply missing");
    s_remove_temp_dir(dir);
}


/* An unrecognized key nested inside a recognized parent is still
 * found, with schema recursion following the JSON structure down */
static void s_test_nested_unrecognized_key_found(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "config.json",
            "{\"desktops\": {\"warp-on-edge-drag\": true, "
            "\"bogus-nested\": 1}}");
    TAP_EQ_INT(s_lint_quietly(dir), 1,
            "an unrecognized key nested under a recognized parent"
            " is still found");
    s_remove_temp_dir(dir);
}


/* The new 'pan-on-edge-drag' desktops key is itself recognized by
 * the schema, not merely tolerated as a side effect of some other
 * key's presence */
static void s_test_pan_on_edge_drag_key_recognized(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "config.json",
            "{\"desktops\": {\"pan-on-edge-drag\": true}}");
    TAP_EQ_INT(s_lint_quietly(dir), 0,
            "'pan-on-edge-drag' is a recognized desktops key");
    s_remove_temp_dir(dir);
}


/* Every *.json file directly under themes/ is checked against the
 * theme schema */
static void s_test_theme_files_checked(void)
{
    char dir[300];
    char themes_dir[350];

    s_make_temp_config_dir(dir, sizeof(dir));
    snprintf(themes_dir, sizeof(themes_dir), "%s/themes", dir);
    s_write_file(themes_dir, "mytheme.json", "{\"bogus-theme-key\": 1}");
    TAP_EQ_INT(s_lint_quietly(dir), 1,
            "an unrecognized key in a themes/*.json file is found");
    s_remove_temp_dir(dir);
}


/* A file under themes/ that does not end in '.json' is ignored
 * entirely, not an error and not checked */
static void s_test_non_json_theme_file_ignored(void)
{
    char dir[300];
    char themes_dir[350];

    s_make_temp_config_dir(dir, sizeof(dir));
    snprintf(themes_dir, sizeof(themes_dir), "%s/themes", dir);
    s_write_file(themes_dir, "README.txt", "{\"bogus-key\": 1}");
    TAP_EQ_INT(s_lint_quietly(dir), 0,
            "a non-'.json' file under themes/ is ignored entirely");
    s_remove_temp_dir(dir);
}


/* A missing themes/ subdirectory entirely is not an error, unlike
 * a missing config.json */
static void s_test_missing_themes_dir_not_an_error(void)
{
    char dir[300];

    s_make_temp_config_dir(dir, sizeof(dir));
    /* no themes/ subdirectory created at all */
    TAP_EQ_INT(s_lint_quietly(dir), 0,
            "no themes/ subdirectory at all: not an error, not counted");
    s_remove_temp_dir(dir);
}


int main(void)
{
    TAP_PLAN(14);

    s_test_null_dir();
    s_test_missing_dir();
    s_test_empty_dir_missing_required_file();
    s_test_recognized_key_not_flagged();
    s_test_unrecognized_key_flagged();
    s_test_key_matching_normalizes();
    s_test_comment_keys_never_flagged();
    s_test_findings_summed_across_files();
    s_test_malformed_json_not_counted();
    s_test_nested_unrecognized_key_found();
    s_test_pan_on_edge_drag_key_recognized();
    s_test_theme_files_checked();
    s_test_non_json_theme_file_ignored();
    s_test_missing_themes_dir_not_an_error();

    return TAP_DONE();
}
