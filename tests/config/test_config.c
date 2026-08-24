/**
 * @file tests/config/test_config.c
 *
 * @brief Test battery for config.c's own dispatcher logic
 *
 * Deliberately does not re-test what each individual sub-loader
 * (config_load_base, config_load_bindings, config_load_theme,
 * config_load_randr, config_load_a11y) already covers in its own
 * dedicated test file; this one covers only what config.c itself
 * adds on top: config_init/config_destroy's own lifecycle,
 * config_resolve_dir, config_load's own orchestration and error
 * handling across all five files together, and the missing-theme
 * tracking that only config_load itself (not any one sub-loader)
 * is responsible for.
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
#include <sys/stat.h>
#include <unistd.h>

/* Local includes */
#include <config.h>
#include <harness/tap.h>
#include <utils/safe/safestr.h>


/**
 * @brief Create a fresh temporary directory for one test's own
 *        config files, and return its path (caller must clean it up)
 */
static void s_make_temp_dir(char *path_out, size_t path_out_size)
{
    static int s_counter = 0;

    snprintf(path_out, path_out_size, "/tmp/icowm_test_config_%d_%d",
            (int) getpid(), s_counter++);
    mkdir(path_out, 0700);
}


/**
 * @brief Write 'content' to '<dir>/<filename>'
 */
static void s_write_file(const char *dir, const char *filename,
        const char *content)
{
    char path[512];
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", dir, filename);
    f = fopen(path, "w");
    if (f != NULL) {
        fwrite(content, 1, strlen(content), f);
        fclose(f);
    }
}


/* config_init allocates a non-NULL structure already populated with
 * default values, ready to use without a config_load call */
static void s_test_init_gives_defaults(void)
{
    config_td *config = config_init();

    TAP_NOT_NULL(config, "config_init succeeds");
    TAP_EQ_STR(config->base.theme, "",
            "base.theme defaults to an empty string");

    config_destroy(config);
}


/* config_destroy with a NULL config is safe: nothing crashes */
static void s_test_destroy_null_is_safe(void)
{
    config_destroy(NULL);
    TAP_OK(true, "destroying a NULL config, no crash");
}


/* config_resolve_dir with an explicit prefix uses that prefix
 * (simplified), rather than falling back to any XDG environment
 * variable */
static void s_test_resolve_dir_uses_explicit_prefix(void)
{
    char resolved[CONFIG_MAX_LENGTH_PATH_BASE];

    config_resolve_dir("/tmp/some/explicit/prefix", resolved);

    TAP_EQ_STR(resolved, "/tmp/some/explicit/prefix",
            "an explicit prefix is used as-is (already simplified)");
}


/* config_load, given a directory with minimal ({}) but present base,
 * bindings, randr, and a11y files, succeeds and leaves every
 * sub-structure populated */
static void s_test_load_minimal_directory_succeeds(void)
{
    char dir[256];
    config_td *config = config_init();
    int rc;

    s_make_temp_dir(dir, sizeof(dir));
    s_write_file(dir, "config.json", "{}");
    s_write_file(dir, "bindings.json", "{}");
    s_write_file(dir, "randr.json", "{}");
    s_write_file(dir, "a11y.json", "{}");

    rc = config_load(config, dir);

    TAP_EQ_INT(rc, 0, "loading a minimal but complete directory succeeds");
    TAP_EQ_STR(config->theme.name, "Default (built-in)",
            "no theme specified: theme.name falls back to the" \
            " built-in default");

    config_destroy(config);
}


/* config_load fails when the base config.json itself is entirely
 * missing from the directory, since every other file is optional but
 * that one is not */
static void s_test_load_missing_base_file_fails(void)
{
    char dir[256];
    config_td *config = config_init();
    int rc;

    s_make_temp_dir(dir, sizeof(dir));
    /* No config.json written at all */

    rc = config_load(config, dir);

    TAP_EQ_INT(rc, 1, "a directory missing config.json itself fails" \
            " to load");

    config_destroy(config);
}


/* config_load still succeeds even when bindings.json, randr.json,
 * and a11y.json are all absent: only the base file is mandatory */
static void s_test_load_optional_files_absent_still_succeeds(void)
{
    char dir[256];
    config_td *config = config_init();
    int rc;

    s_make_temp_dir(dir, sizeof(dir));
    s_write_file(dir, "config.json", "{}");
    /* bindings.json, randr.json, a11y.json deliberately not written */

    rc = config_load(config, dir);

    TAP_EQ_INT(rc, 0, "a directory with only the base file still" \
            " loads successfully");

    config_destroy(config);
}


/* When base.theme names a theme file that does not actually exist on
 * disk, config_load falls back to the built-in theme and records the
 * missing path so the caller can warn about it later */
static void s_test_load_records_missing_theme(void)
{
    char dir[256];
    config_td *config = config_init();
    const char *missing;

    s_make_temp_dir(dir, sizeof(dir));
    s_write_file(dir, "config.json", "{\"theme\": \"does-not-exist\"}");

    config_missing_theme_reset();
    config_load(config, dir);
    missing = config_missing_theme_get();

    TAP_NOT_NULL(missing, "a nonexistent theme file is recorded as" \
            " missing");
    TAP_EQ_STR(config->theme.name, "Default (built-in)",
            "the built-in theme is used as a fallback");

    config_destroy(config);
}


/* config_missing_theme_reset clears whatever config_load most
 * recently recorded, back to no missing theme at all */
static void s_test_missing_theme_reset_clears_it(void)
{
    char dir[256];
    config_td *config = config_init();

    s_make_temp_dir(dir, sizeof(dir));
    s_write_file(dir, "config.json", "{\"theme\": \"still-not-there\"}");

    config_load(config, dir);
    TAP_NOT_NULL(config_missing_theme_get(),
            "a missing theme is recorded right after loading");

    config_missing_theme_reset();
    TAP_NULL(config_missing_theme_get(),
            "resetting clears the recorded missing theme");

    config_destroy(config);
}


/* config_set_default_base_values (one of the several per-section
 * default-setters config_init's own internal helper calls in turn)
 * resets an already-modified base section back to defaults */
static void s_test_set_default_values_resets_modified_struct(void)
{
    config_td config;

    memset(&config, 0, sizeof(config));
    safe_strncpy(config.base.theme, "something-custom",
            sizeof(config.base.theme));

    config_set_default_base_values(&config.base, &config.desktops);

    TAP_EQ_STR(config.base.theme, "",
            "a previously modified field is reset back to its own" \
            " default");
}


int main(void)
{
    TAP_PLAN(13);

    s_test_init_gives_defaults();
    s_test_destroy_null_is_safe();
    s_test_resolve_dir_uses_explicit_prefix();
    s_test_load_minimal_directory_succeeds();
    s_test_load_missing_base_file_fails();
    s_test_load_optional_files_absent_still_succeeds();
    s_test_load_records_missing_theme();
    s_test_missing_theme_reset_clears_it();
    s_test_set_default_values_resets_modified_struct();

    return TAP_DONE();
}
