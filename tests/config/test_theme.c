/**
 * @file tests/config/test_theme.c
 *
 * @brief Test battery for theme configuration loading
 *
 * s_test_button_list_deduplicates_first_wins is the fix for a real
 * discrepancy found while writing this battery, between
 * s_load_button_list's own doc comment ("A button name the theme
 * repeats ... is silently skipped") and what the code actually did
 * (a repeated name was added every time it appeared, up to the
 * CONFIG_MAX_TITLEBAR_BUTTONS cap; nothing checked for a name
 * already present in the list). Fixed in theme.c so a repeat is now
 * kept only for its first occurrence, matching the doc comment.
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

    snprintf(path_out, path_out_size, "/tmp/icowm_test_theme_%d_%d",
            (int) getpid(), s_counter++);
    f = fopen(path_out, "w");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}


/* A missing file fails outright */
static void s_test_missing_file(void)
{
    struct config_theme_s theme;

    TAP_EQ_INT(config_load_theme("/nonexistent/at/all", &theme), 1,
            "a missing file fails");
}


/* An empty object is a valid, minimal file */
static void s_test_empty_file(void)
{
    char path[64];
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path), "{}");
    TAP_EQ_INT(config_load_theme(path, &theme), 0,
            "an empty object loads successfully");
    unlink(path);
}


/* Every recognized titlebar button name maps correctly, an
 * unrecognized one is skipped (not added at all), and a non-string
 * array element is skipped too */
static void s_test_button_list_recognized_and_skipped(void)
{
    char path[512];
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{\"window\": {\"titlebar\": {\"buttons\": {"
        "\"left\": [\"pin\", \"layer\", \"not-a-real-button\", 42,"
        " \"iconize\"]"
        "} } } }");
    config_load_theme(path, &theme);

    TAP_EQ_INT(theme.window.titlebar.buttons.left_count, 3,
            "unrecognized name and non-string element both skipped,"
            " only the 3 real button names counted");
    TAP_EQ_INT(theme.window.titlebar.buttons.left[0],
            CONFIG_TITLEBAR_BUTTON_PIN, "first recognized button correct");
    TAP_EQ_INT(theme.window.titlebar.buttons.left[1],
            CONFIG_TITLEBAR_BUTTON_LAYER, "second recognized button correct");
    TAP_EQ_INT(theme.window.titlebar.buttons.left[2],
            CONFIG_TITLEBAR_BUTTON_ICONIZE,
            "third recognized button correct, right after skipping"
            " the two bad entries in between");
    unlink(path);
}


/* A repeated button name is kept only for its first occurrence;
 * later repeats are skipped, without disturbing other, genuinely
 * distinct button names that appear between the repeats */
static void s_test_button_list_deduplicates_first_wins(void)
{
    char path[256];
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{\"window\": {\"titlebar\": {\"buttons\": {"
        "\"left\": [\"close\", \"pin\", \"close\", \"layer\", \"close\"]"
        "} } } }");
    config_load_theme(path, &theme);

    TAP_EQ_INT(theme.window.titlebar.buttons.left_count, 3,
            "3 distinct names kept; the 2 later repeats of 'close'"
            " dropped, not counted as slots of their own");
    TAP_EQ_INT(theme.window.titlebar.buttons.left[0],
            CONFIG_TITLEBAR_BUTTON_CLOSE,
            "'close' kept at its first-occurrence position");
    TAP_EQ_INT(theme.window.titlebar.buttons.left[1],
            CONFIG_TITLEBAR_BUTTON_PIN,
            "'pin', between the repeats, unaffected");
    TAP_EQ_INT(theme.window.titlebar.buttons.left[2],
            CONFIG_TITLEBAR_BUTTON_LAYER,
            "'layer', between the repeats, unaffected");
    unlink(path);
}


/* More than CONFIG_MAX_TITLEBAR_BUTTONS entries are clamped, never
 * overflowing the fixed-size destination array.  Only 8 button names
 * are recognized at all (the same as the cap itself), so any 9th
 * *valid* entry is necessarily a repeat of one already present;
 * deduplication alone would already drop it here too, but the cap
 * is what stops a list of unrecognized-then-recognized entries, or
 * one this test does not happen to construct, from writing past
 * 'dest' regardless of how many of those turn out to be repeats. */
static void s_test_button_list_clamped_to_max(void)
{
    char path[512];
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{\"window\": {\"titlebar\": {\"buttons\": {"
        "\"right\": [\"pin\", \"layer\", \"iconize\", \"hide\","
        " \"shade\", \"maximize\", \"fullscreen\", \"close\", \"pin\"]"
        "} } } }");  /* 9 entries; max is 8 */
    config_load_theme(path, &theme);

    TAP_EQ_INT(theme.window.titlebar.buttons.right_count,
            CONFIG_MAX_TITLEBAR_BUTTONS,
            "8 distinct names fill the cap exactly; the 9th (itself"
            " a repeat) adds nothing further either way");
    unlink(path);
}


/* Every recognized titlebar alignment string maps correctly; an
 * unrecognized one and a missing field both default to left */
static void s_test_titlebar_alignment(void)
{
    static const struct {
        const char *value;
        enum config_titlebar_alignment_e expected;
    } cases[] = {
        { "left",     CONFIG_TITLEBAR_ALIGN_LEFT },
        { "center",   CONFIG_TITLEBAR_ALIGN_CENTER },
        { "right",    CONFIG_TITLEBAR_ALIGN_RIGHT },
        { "bogus",    CONFIG_TITLEBAR_ALIGN_LEFT },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char path[256];
        char content[256];
        struct config_theme_s theme;

        memset(&theme, 0, sizeof(theme));
        snprintf(content, sizeof(content),
                "{\"window\": {\"titlebar\": {\"alignment\": \"%s\"}}}",
                cases[i].value);
        s_write_temp_file(path, sizeof(path), content);
        config_load_theme(path, &theme);
        TAP_EQ_INT(theme.window.titlebar.alignment, cases[i].expected,
                cases[i].value);
        unlink(path);
    }

    /* A missing alignment field also defaults to left */
    {
        char path[64];
        struct config_theme_s theme;

        memset(&theme, 0, sizeof(theme));
        s_write_temp_file(path, sizeof(path),
                "{\"window\": {\"titlebar\": {}}}");
        config_load_theme(path, &theme);
        TAP_EQ_INT(theme.window.titlebar.alignment,
                CONFIG_TITLEBAR_ALIGN_LEFT,
                "a missing alignment field defaults to left");
        unlink(path);
    }
}


/* Every recognized systray text valign string maps correctly; an
 * unrecognized one defaults to center */
static void s_test_systray_text_valign(void)
{
    static const struct {
        const char *value;
        enum config_systray_text_valign_e expected;
    } cases[] = {
        { "top",     CONFIG_SYSTRAY_TEXT_VALIGN_TOP },
        { "bottom",  CONFIG_SYSTRAY_TEXT_VALIGN_BOTTOM },
        { "center",  CONFIG_SYSTRAY_TEXT_VALIGN_CENTER },
        { "bogus",   CONFIG_SYSTRAY_TEXT_VALIGN_CENTER },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char path[256];
        char content[256];
        struct config_theme_s theme;

        memset(&theme, 0, sizeof(theme));
        snprintf(content, sizeof(content),
                "{\"systray\": {\"text\": {\"valign\": \"%s\"}}}",
                cases[i].value);
        s_write_temp_file(path, sizeof(path), content);
        config_load_theme(path, &theme);
        TAP_EQ_INT(theme.systray.text.valign, cases[i].expected,
                cases[i].value);
        unlink(path);
    }
}


/* s_load_theme_colors' full shape (font, color.background/foreground,
 * border.color/width) loads correctly for one themeable surface, and
 * the same block shape is correctly reused for a second, independent
 * surface elsewhere in the file */
static void s_test_theme_colors_full_shape(void)
{
    char path[1024];
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{"
        "\"window\": {\"active\": {"
        "  \"font\": \"Sans 10\","
        "  \"color\": {\"background\": \"#112233\", "
        "\"foreground\": \"#ffffff\"},"
        "  \"border\": {\"color\": \"#000000\", \"width\": 2}"
        "} },"
        "\"icon\": {\"inactive\": {"
        "  \"font\": \"Sans 8\","
        "  \"color\": {\"background\": \"#445566\", "
        "\"foreground\": \"#000000\"}"
        "} }"
        "}");
    config_load_theme(path, &theme);

    TAP_EQ_STR(theme.window.active.font, "Sans 10",
            "window.active.font loaded");
    TAP_EQ_INT(theme.window.active.color.background, 0x112233u,
            "window.active.color.background loaded");
    TAP_EQ_INT(theme.window.active.color.foreground, 0xFFFFFFu,
            "window.active.color.foreground loaded");
    TAP_EQ_INT(theme.window.active.border.color, 0x000000u,
            "window.active.border.color loaded");
    TAP_EQ_INT((int) theme.window.active.border.width, 2,
            "window.active.border.width loaded");
    TAP_EQ_STR(theme.icon.inactive.font, "Sans 8",
            "the same block shape reused for icon.inactive: font loaded");
    TAP_EQ_INT(theme.icon.inactive.color.background, 0x445566u,
            "icon.inactive.color.background loaded, independent of"
            " window.active");
    unlink(path);
}


/* s_load_theme_colors is called unconditionally for 'systray',
 * 'menu.unselected/selected/label', and 'overlay' even when their
 * own parent section is not present in the file at all: this must
 * not crash, relying on the function's own internal NULL check */
static void s_test_unconditional_calls_are_safe_when_absent(void)
{
    char path[64];
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path), "{}");
    TAP_EQ_INT(config_load_theme(path, &theme), 0,
            "a file with none of systray/menu/overlay present still"
            " loads successfully, not a crash");
    unlink(path);
}


/* One representative field from each remaining top-level section:
 * desktop, menu (including its own nested button/disabled/separator/
 * border/padding), dialog (including its own nested label/button),
 * and xsettings (including its own nested theme) */
static void s_test_representative_fields_remaining_sections(void)
{
    char path[2048];
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{"
        "\"name\": \"my-theme\","
        "\"desktop\": {\"color\": {\"background\": \"#abcdef\"}},"
        "\"menu\": {"
        "  \"disabled\": {\"color\": {\"foreground\": \"#333333\"}},"
        "  \"separator\": {\"color\": \"#444444\"},"
        "  \"border\": {\"color\": \"#555555\", \"width\": 1},"
        "  \"padding\": {\"horizontal\": 4, \"vertical\": 2},"
        "  \"show-pixmaps\": true"
        "},"
        "\"dialog\": {"
        "  \"color\": {\"background\": \"#666666\"},"
        "  \"label\": {\"font\": \"Sans 9\"},"
        "  \"button\": {\"gap\": 6}"
        "},"
        "\"xsettings\": {"
        "  \"is-enabled\": true,"
        "  \"dpi\": 96,"
        "  \"theme\": {\"gtk-theme-name\": \"Adwaita\"}"
        "}"
        "}");
    config_load_theme(path, &theme);

    TAP_EQ_STR(theme.name, "my-theme", "top-level name loaded");
    TAP_EQ_INT(theme.desktop.color.background, 0xABCDEFu,
            "desktop.color.background loaded");
    TAP_EQ_INT(theme.menu.disabled_foreground, 0x333333u,
            "menu.disabled.color.foreground loaded");
    TAP_EQ_INT(theme.menu.separator_color, 0x444444u,
            "menu.separator.color loaded");
    TAP_EQ_INT(theme.menu.border.color, 0x555555u,
            "menu.border.color loaded");
    TAP_EQ_INT((int) theme.menu.padding.horizontal, 4,
            "menu.padding.horizontal loaded");
    TAP_OK(theme.menu.show_pixmaps, "menu.show-pixmaps loaded");
    TAP_EQ_INT(theme.dialog.background, 0x666666u,
            "dialog.color.background loaded");
    TAP_EQ_STR(theme.dialog.label.font, "Sans 9",
            "dialog.label.font loaded");
    TAP_EQ_INT((int) theme.dialog.button.gap, 6,
            "dialog.button.gap loaded");
    TAP_OK(theme.xsettings.is_enabled, "xsettings.is-enabled loaded");
    TAP_EQ_INT((int) theme.xsettings.dpi, 96, "xsettings.dpi loaded");
    TAP_EQ_STR(theme.xsettings.theme.gtk_theme_name, "Adwaita",
            "xsettings.theme.gtk-theme-name loaded");
    unlink(path);
}


/* A titlebar shorter than the button size plus its own vertical
 * padding is floored to that exact threshold; 0 (which disables the
 * titlebar entirely) is left untouched, and a value already at or
 * above the threshold is left untouched too */
static void s_test_titlebar_height_floor(void)
{
    char path[256];
    struct config_theme_s theme;

    /* Below the floor, no padding block in the file, so
     * padding.vertical stays at whatever the caller's own struct
     * already held (0 here, freshly zeroed): raised to
     * WM_DECOR_BTN_SIZE (12) + 2 * 0 = 12 */
    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{\"window\": {\"titlebar\": {\"height\": 1} } }");
    config_load_theme(path, &theme);
    TAP_EQ_INT((int) theme.window.titlebar.height, 12,
            "height below the floor raised to the button size alone"
            " when vertical padding is 0");
    unlink(path);

    /* Below the floor, custom padding.vertical (5) from the same
     * file: raised to 12 + 2 * 5 = 22, confirming the floor uses the
     * padding actually loaded, not a hardcoded default */
    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{\"window\": {\"titlebar\": {\"height\": 1,"
        " \"padding\": {\"vertical\": 5} } } }");
    config_load_theme(path, &theme);
    TAP_EQ_INT((int) theme.window.titlebar.height, 22,
            "height below the floor raised using the theme's own"
            " loaded vertical padding, not a hardcoded one");
    unlink(path);

    /* 0 disables the titlebar entirely; never touched by the floor */
    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{\"window\": {\"titlebar\": {\"height\": 0} } }");
    config_load_theme(path, &theme);
    TAP_EQ_INT((int) theme.window.titlebar.height, 0,
            "height of 0 (disables the titlebar) left untouched");
    unlink(path);

    /* Already at or above the floor: left untouched */
    memset(&theme, 0, sizeof(theme));
    s_write_temp_file(path, sizeof(path),
        "{\"window\": {\"titlebar\": {\"height\": 30} } }");
    config_load_theme(path, &theme);
    TAP_EQ_INT((int) theme.window.titlebar.height, 30,
            "height already above the floor left untouched");
    unlink(path);

    /* The realistic production sequence: defaults applied first
     * (padding.vertical 2, per config_set_default_theme_values), then
     * a file overriding only height; raised to 12 + 2 * 2 = 16 using
     * that already-defaulted padding, confirmed independently of the
     * bare-struct cases above */
    config_set_default_theme_values(&theme);
    s_write_temp_file(path, sizeof(path),
        "{\"window\": {\"titlebar\": {\"height\": 1} } }");
    config_load_theme(path, &theme);
    TAP_EQ_INT((int) theme.window.titlebar.height, 16,
            "height below the floor raised using the default vertical"
            " padding when a file does not override it either");
    unlink(path);
}


int main(void)
{
    TAP_PLAN(46);

    s_test_missing_file();
    s_test_empty_file();
    s_test_button_list_recognized_and_skipped();
    s_test_button_list_deduplicates_first_wins();
    s_test_button_list_clamped_to_max();
    s_test_titlebar_alignment();
    s_test_titlebar_height_floor();
    s_test_systray_text_valign();
    s_test_theme_colors_full_shape();
    s_test_unconditional_calls_are_safe_when_absent();
    s_test_representative_fields_remaining_sections();

    return TAP_DONE();
}
