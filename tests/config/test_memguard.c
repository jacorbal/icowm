/**
 * @file tests/config/test_memguard.c
 *
 * @brief Test battery for restricted-memory mode's own configuration
 *        loading
 *
 * s_test_fixed_variant_forms_via_restrict is the regression for a
 * real bug found while writing this battery: s_memguard_is_fixed_
 * variant's own doc comment gave "fixed-14" as an example of a
 * simple-alias font name that should be recognized as a "fixed"
 * variant (and so left alone rather than substituted), but the code
 * only ever looked for a space to end the family name, never a
 * hyphen, so "fixed-14" (no space at all) fell through to the whole
 * string, which is not "fixed", and got needlessly replaced. Fixed
 * to also stop at a hyphen, matching the documented examples.
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
#include <sys/stat.h>
#include <unistd.h>

/* Local includes */
#include <config.h>
#include <config/memguard.h>
#include <harness/tap.h>


/**
 * @brief Create a fresh temporary config directory (with its own
 *        'themes' subdirectory) and return its path (caller must
 *        clean it up when done, e.g. with a plain shell 'rm -rf'
 *        via system(), since there is no portable recursive-delete
 *        in the C standard library worth reaching for here)
 */
static void s_make_temp_config_dir(char *path_out, size_t path_out_size)
{
    static int s_counter = 0;
    char themes_dir[300];

    snprintf(path_out, path_out_size, "/tmp/icowm_test_memguard_%d_%d",
            (int) getpid(), s_counter++);
    mkdir(path_out, 0700);
    snprintf(themes_dir, sizeof(themes_dir), "%s/themes", path_out);
    mkdir(themes_dir, 0700);
}


/**
 * @brief Write 'content' to 'dir/name'
 */
static void s_write_file(const char *dir, const char *name,
        const char *content)
{
    char path[600];
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    f = fopen(path, "w");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}


/**
 * @brief Recursively remove a directory this test created
 */
static void s_remove_temp_dir(const char *path)
{
    char cmd[600];

    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    if (system(cmd) != 0) {
        /* best-effort cleanup only; a leftover /tmp directory does
         * not affect this test's own pass/fail result */
    }
}


/* config_memguard_init allocates a usable, non-NULL structure */
static void s_test_init_memguard(void)
{
    config_td *config = config_memguard_init();

    TAP_NOT_NULL(config, "config_memguard_init returns a non-NULL"
            " pointer");
    free(config);
}


/* config_set_default_values_memguard(NULL) is a safe no-op */
static void s_test_default_values_null_safe(void)
{
    config_set_default_values_memguard(NULL);
    TAP_OK(1, "config_set_default_values_memguard(NULL) does not crash");
}


/* A handful of representative fixed, restricted-memory-appropriate
 * defaults: a single screen and desktop (since neither warp nor
 * cycle mean anything with only one), and a few of the fixed
 * program/window defaults */
static void s_test_default_values_key_fields(void)
{
    config_td *config = config_memguard_init();

    config_set_default_values_memguard(config);

    TAP_EQ_INT((int) config->base.screen_count, 1,
            "a single screen by default");
    TAP_EQ_INT((int) config->base.screens[0].desktop_count, 1,
            "a single desktop by default");
    TAP_OK(!config->desktops.warp_on_edge_drag,
            "edge warp off: meaningless with 1 desktop");
    TAP_OK(!config->base.viewport.pan_on_edge_hover,
            "edge hover pan off: meaningless with no viewport");
    TAP_OK(!config->base.viewport.pan_on_edge_drag,
            "edge drag pan off: meaningless with no viewport");
    TAP_OK(!config->desktops.wrap_at_bounds,
            "circular switching off: meaningless with 1 desktop");
    TAP_EQ_STR(config->base.theme, "", "no theme name until loaded");
    TAP_EQ_INT((int) config->base.windows.move_step, 10,
            "fixed default move step");
    TAP_OK(!config->base.systray.is_embedding_enabled,
            "embedding always off in this mode");
    TAP_OK(!config->randr.is_enabled,
            "RandR profile management never consulted in this mode");

    free(config);
}


/* A missing memguard.json fails config_load_memguard's own return
 * status, but the structure is still left in a usable state (its own
 * fixed defaults), not a crash or garbage data */
static void s_test_load_missing_file(void)
{
    char dir[300];
    config_td *config = config_memguard_init();
    int status;

    s_make_temp_config_dir(dir, sizeof(dir));
    status = config_load_memguard(config, dir);

    TAP_EQ_INT(status, 1,
            "a directory with no memguard.json fails config_load_memguard");
    TAP_EQ_INT((int) config->base.screen_count, 1,
            "defaults are still in place after the failed load");

    free(config);
    s_remove_temp_dir(dir);
}


/* A fully populated memguard.json loads every field it is documented
 * to allow */
static void s_test_load_full_file(void)
{
    char dir[300];
    config_td *config = config_memguard_init();

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "memguard.json",
        "{"
        "\"theme\": \"\","
        "\"programs\": {\"terminal\": \"st\"},"
        "\"desktops\": {\"margins\": {\"top\": 5}},"
        "\"windows\": {\"move-step\": 25,"
        "  \"placement\": {\"policy\": \"cascade\"}},"
        "\"icons\": {\"placement\": {\"policy\": \"grid\"}},"
        "\"shutdown\": {\"enable-emergency-shortcut\": true}"
        "}");

    config_load_memguard(config, dir);

    TAP_EQ_STR(config->base.programs.terminal, "st",
            "programs.terminal loaded");
    TAP_EQ_INT((int) config->desktops.margins.top, 5,
            "desktops.margins.top loaded");
    TAP_EQ_INT((int) config->base.windows.move_step, 25,
            "windows.move-step loaded");
    TAP_OK(config->base.shutdown.enable_emergency_shortcut,
            "shutdown.enable-emergency-shortcut loaded");

    free(config);
    s_remove_temp_dir(dir);
}


/* 'systray.text.position' and 'systray.text.order', unlike an
 * ordinary session's config.json, are deliberately not configurable
 * here: even if memguard.json specifies them, they are forced back
 * to their own fixed defaults right after loading */
static void s_test_systray_text_fields_forced_to_default(void)
{
    char dir[300];
    config_td *config = config_memguard_init();

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "memguard.json",
        "{\"systray\": {\"text\": {\"position\": \"right\"},"
        " \"order\": [\"battery\", \"clock\"]} }");

    config_load_memguard(config, dir);

    TAP_EQ_INT(config->base.systray.text.position,
            CONFIG_SYSTRAY_TEXT_LEFT,
            "text.position forced back to its own fixed default,"
            " despite the file asking for 'right'");
    TAP_EQ_INT(config->base.systray.order,
            CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT,
            "order forced back to its own fixed default too");

    free(config);
    s_remove_temp_dir(dir);
}


/* Regression: every documented simple-alias and XLFD form of
 * "fixed" is recognized and left alone by s_memguard_restrict_theme
 * (verified indirectly through a loaded theme, since the checking
 * function itself is static); anything else is replaced with the
 * fixed MEMGUARD_FONT_NAME */
static void s_test_fixed_variant_forms_via_restrict(void)
{
    char dir[300];
    config_td *config = config_memguard_init();

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "memguard.json", "{\"theme\": \"mytheme\"}");
    s_write_file(dir, "themes/mytheme.json",
        "{"
        "\"window\": {"
        "  \"active\": {\"font\": \"fixed\"},"
        "  \"inactive\": {\"font\": \"fixed bold\"}"
        "},"
        "\"icon\": {\"active\": {\"font\": \"fixed-14\"}},"
        "\"menu\": {\"unselected\": {\"font\":"
        " \"-misc-fixed-bold-r-normal--0-120-75-75-c-0-iso10646-1\"}},"
        "\"dialog\": {\"label\": {\"font\": \"DejaVu Sans 10\"}}"
        "}");

    config_load_memguard(config, dir);

    TAP_EQ_STR(config->theme.icon.active.font, "fixed-14",
            "regression: 'fixed-14' (hyphen, no space) recognized"
            " as a fixed variant and left untouched");
    TAP_EQ_STR(config->theme.menu.unselected.font,
            "-misc-fixed-bold-r-normal--0-120-75-75-c-0-iso10646-1",
            "a full XLFD naming the 'fixed' family also left untouched");
    TAP_EQ_STR(config->theme.dialog.label.font, "fixed",
            "a genuinely different font family (DejaVu Sans) is"
            " replaced with the fixed MEMGUARD_FONT_NAME");

    free(config);
    s_remove_temp_dir(dir);
}


/* xsettings publishing, icon pixmaps (both the icon square's own and
 * the menu row icon), and icon hint indicators are all forced off
 * unconditionally, even when the loaded theme explicitly turned them
 * on */
static void s_test_restrict_forces_pixmaps_and_xsettings_off(void)
{
    char dir[300];
    config_td *config = config_memguard_init();

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "memguard.json", "{\"theme\": \"mytheme\"}");
    s_write_file(dir, "themes/mytheme.json",
        "{"
        "\"icon\": {\"show-pixmaps\": true, \"show-hints\": true},"
        "\"menu\": {\"show-pixmaps\": true},"
        "\"xsettings\": {\"is-enabled\": true}"
        "}");

    config_load_memguard(config, dir);

    TAP_OK(!config->theme.icon.show_pixmaps,
            "icon.show-pixmaps forced off despite the theme");
    TAP_OK(!config->theme.icon.show_hints,
            "icon.show-hints forced off despite the theme");
    TAP_OK(!config->theme.menu.show_pixmaps,
            "menu.show-pixmaps forced off despite the theme");
    TAP_OK(!config->theme.xsettings.is_enabled,
            "xsettings.is-enabled forced off despite the theme");

    free(config);
    s_remove_temp_dir(dir);
}


/* config_load_memguard also loads bindings.json as part of the same
 * call, using the ordinary config_load_bindings under the hood */
static void s_test_load_memguard_also_loads_bindings(void)
{
    char dir[300];
    config_td *config = config_memguard_init();

    s_make_temp_config_dir(dir, sizeof(dir));
    s_write_file(dir, "bindings.json",
        "{\"keyboard\": {\"wm\": {\"quit\": \"Mod1+q\"}}}");

    config_load_memguard(config, dir);

    TAP_EQ_STR(config->bindings.keyboard.wm.quit, "Mod1+q",
            "bindings.json is loaded as part of config_load_memguard");

    free(config);
    s_remove_temp_dir(dir);
}


int main(void)
{
    TAP_PLAN(28);

    s_test_init_memguard();
    s_test_default_values_null_safe();
    s_test_default_values_key_fields();
    s_test_load_missing_file();
    s_test_load_full_file();
    s_test_systray_text_fields_forced_to_default();
    s_test_fixed_variant_forms_via_restrict();
    s_test_restrict_forces_pixmaps_and_xsettings_off();
    s_test_load_memguard_also_loads_bindings();

    return TAP_DONE();
}
