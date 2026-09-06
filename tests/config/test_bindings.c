/**
 * @file tests/config/test_bindings.c
 *
 * @brief Test battery for keyboard/mouse bindings configuration
 *        loading
 *
 * s_test_go_to_regression is the exact bug this file was originally
 * written to catch: 'go-to' was looked up from 'wm' but the code
 * path that ran it was nested inside 'if (window)' instead of
 * 'if (wm)', so a file with a 'keyboard.wm.go-to' section but no
 * 'keyboard.window' section at all silently never loaded it.  Both
 * 'go-to' and 'show' have since moved to their own top-level
 * 'keyboard.desktop' section, a sibling of 'keyboard.window' and
 * 'keyboard.wm' rather than nested under either; the two tests below
 * still check the same underlying robustness (loading correctly
 * regardless of which other sections are present in the file), now
 * against that current structure.
 *
 * Given how mechanical this loader is (~50 fields, each just
 * 'json_load_string' into its own nested destination, no branching
 * logic to speak of), this battery does not exercise every single
 * field individually.  It checks one representative field at each
 * level of nesting the file actually has, plus every case where the
 * code's own structure (nested nulls, an unusual variable-sharing
 * pattern) creates a genuine risk of a field silently not loading.
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

    snprintf(path_out, path_out_size, "/tmp/icowm_test_bindings_%d_%d",
            (int) getpid(), s_counter++);
    f = fopen(path_out, "w");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}


/* A missing file fails outright */
static void s_test_missing_file(void)
{
    struct config_bindings_s cb;

    TAP_EQ_INT(config_load_bindings("/nonexistent/at/all", &cb), 1,
            "a missing file fails");
}


/* An empty object is a completely valid, minimal file: succeeds,
 * touches nothing (every destination field is left at whatever the
 * caller already had, per this project's own established
 * json_load_* contract) */
static void s_test_empty_file(void)
{
    char path[64];
    struct config_bindings_s cb;

    memset(&cb, 0, sizeof(cb));
    s_write_temp_file(path, sizeof(path), "{}");
    TAP_EQ_INT(config_load_bindings(path, &cb), 0,
            "an empty object loads successfully");
    TAP_EQ_STR(cb.keyboard.wm.quit, "",
            "with nothing to load, fields stay at their initial value");
    unlink(path);
}


/* 'go-to' lives under its own top-level 'keyboard.desktop' section,
 * a sibling of both 'keyboard.wm' and 'keyboard.window'; it must
 * load correctly even when neither of those two is present in the
 * file at all */
static void s_test_go_to_regression(void)
{
    char path[256];
    struct config_bindings_s cb;

    memset(&cb, 0, sizeof(cb));
    s_write_temp_file(path, sizeof(path),
            "{\"keyboard\": {\"desktop\": {\"go-to\": {"
            "\"desktop0\": \"Mod1+0\", \"desktop9\": \"Mod1+9\""
            "} } } }");
    config_load_bindings(path, &cb);

    TAP_EQ_STR(cb.keyboard.desktop.go_to.desktop[0], "Mod1+0",
            "regression: go-to loads with neither 'wm' nor 'window'" \
            " present");
    TAP_EQ_STR(cb.keyboard.desktop.go_to.desktop[9], "Mod1+9",
            "regression: a second, non-adjacent go-to slot also loads");
    unlink(path);
}


/* 'go-to' still loads correctly when 'keyboard.window' IS also
 * present, same as when it is absent: the fix must not have broken
 * the case that used to work by coincidence */
static void s_test_go_to_still_works_with_window_present(void)
{
    char path[512];
    struct config_bindings_s cb;

    memset(&cb, 0, sizeof(cb));
    s_write_temp_file(path, sizeof(path),
            "{\"keyboard\": {"
            "\"desktop\": {\"go-to\": {\"desktop3\": \"Mod1+3\"}},"
            "\"window\": {\"close\": \"Mod1+F4\"}"
            "} }");
    config_load_bindings(path, &cb);

    TAP_EQ_STR(cb.keyboard.desktop.go_to.desktop[3], "Mod1+3",
            "go-to still loads when 'window' is also present");
    TAP_EQ_STR(cb.keyboard.window.close, "Mod1+F4",
            "'window' fields still load normally too");
    unlink(path);
}


/* One representative field from each top-level and nested section
 * of a fully populated file: modifiers, keyboard.wm (including its
 * own nested 'menus'), keyboard.launch, keyboard.window (including
 * its own nested move.relative / move.absolute / resize),
 * keyboard.cycle, mouse.window, and mouse.cycle */
static void s_test_representative_fields_at_every_level(void)
{
    char path[2048];
    struct config_bindings_s cb;

    memset(&cb, 0, sizeof(cb));
    s_write_temp_file(path, sizeof(path),
        "{"
        "\"modifiers\": {\"mod1\": \"Mod1\"},"
        "\"keyboard\": {"
        "  \"wm\": {"
        "    \"menus\": {\"root\": \"Mod1+F1\"},"
        "    \"search\": \"Mod1+slash\""
        "  },"
        "  \"launch\": {\"terminal\": \"Mod1+Return\"},"
        "  \"window\": {"
        "    \"iconify\": \"Mod1+n\","
        "    \"move\": {"
        "      \"relative\": {\"right\": \"Mod1+Right\"},"
        "      \"absolute\": {\"center\": \"Mod1+c\"}"
        "    },"
        "    \"resize\": {\"right\": \"Mod1+Shift+Right\"}"
        "  },"
        "  \"cycle\": {\"window\": {\"next\": \"Mod1+Tab\"}}"
        "},"
        "\"mouse\": {"
        "  \"window\": {\"move\": \"Mod1+Button1\"},"
        "  \"cycle\": {\"desktop\": {\"east\": \"Button5\"}}"
        "}"
        "}");
    config_load_bindings(path, &cb);

    TAP_EQ_STR(cb.mod1, "Mod1", "top-level modifiers field loaded");
    TAP_EQ_STR(cb.keyboard.wm.menus.root, "Mod1+F1",
            "keyboard.wm.menus (double-nested) loaded");
    TAP_EQ_STR(cb.keyboard.wm.search, "Mod1+slash",
            "keyboard.wm field loaded");
    TAP_EQ_STR(cb.keyboard.launch.terminal, "Mod1+Return",
            "keyboard.launch field loaded");
    TAP_EQ_STR(cb.keyboard.window.iconify, "Mod1+n",
            "keyboard.window field loaded");
    TAP_EQ_STR(cb.keyboard.window.move.relative.right, "Mod1+Right",
            "keyboard.window.move.relative (triple-nested) loaded");
    TAP_EQ_STR(cb.keyboard.window.move.absolute.center, "Mod1+c",
            "keyboard.window.move.absolute (triple-nested) loaded");
    TAP_EQ_STR(cb.keyboard.window.resize.right, "Mod1+Shift+Right",
            "keyboard.window.resize (double-nested) loaded");
    TAP_EQ_STR(cb.keyboard.cycle.window.next, "Mod1+Tab",
            "keyboard.cycle.window (double-nested) loaded");
    TAP_EQ_STR(cb.mouse.window.move, "Mod1+Button1",
            "mouse.window field loaded");
    TAP_EQ_STR(cb.mouse.cycle.desktop.east, "Button5",
            "mouse.cycle.desktop (double-nested) loaded");
}


/* 'show' lives under its own top-level 'keyboard.desktop' section;
 * the same key placed under either 'keyboard.wm' or
 * 'keyboard.window' instead is simply unrecognized there and has no
 * effect at all, the same as any other typo would */
static void s_test_show_desktop_only_desktop_section_accepted(void)
{
    char path[512];
    struct config_bindings_s cb;

    /* Present under 'desktop', where it belongs: loads normally */
    memset(&cb, 0, sizeof(cb));
    s_write_temp_file(path, sizeof(path),
            "{\"keyboard\": {\"desktop\": {"
            "\"show\": \"from-desktop\"} } }");
    config_load_bindings(path, &cb);
    TAP_EQ_STR(cb.keyboard.desktop.show, "from-desktop",
            "'desktop.show' loads correctly");
    unlink(path);

    /* Present only under 'wm': has no effect at all */
    memset(&cb, 0, sizeof(cb));
    s_write_temp_file(path, sizeof(path),
            "{\"keyboard\": {\"wm\": {\"show\": \"from-wm\"} } }");
    config_load_bindings(path, &cb);
    TAP_EQ_STR(cb.keyboard.desktop.show, "",
            "'wm.show' is not a recognized key; ignored");
    unlink(path);

    /* Present in both 'wm' and 'desktop': only 'desktop's own value
     * is ever used, the 'wm' one simply never read at all */
    memset(&cb, 0, sizeof(cb));
    s_write_temp_file(path, sizeof(path),
            "{\"keyboard\": {"
            "\"wm\": {\"show\": \"from-wm\"},"
            "\"desktop\": {\"show\": \"from-desktop\"}"
            "} }");
    config_load_bindings(path, &cb);
    TAP_EQ_STR(cb.keyboard.desktop.show, "from-desktop",
            "with both present, only 'desktop's own value is ever used");
    unlink(path);
}


/* 'viewport.go-to' is a sibling of 'viewport.pan', loaded whether
 * or not 'pan' is present in the same file; its keys are the
 * zero-based 'page0'..'page9', mapped one-to-one to zero-based array
 * slots, exactly like 'desktop.go-to' */
static void s_test_viewport_go_to_loads(void)
{
    char path[256];
    struct config_bindings_s cb;

    memset(&cb, 0, sizeof(cb));
    s_write_temp_file(path, sizeof(path),
            "{\"keyboard\": {\"viewport\": {\"go-to\": {"
            "\"page0\": \"modc+mod4+0\", \"page9\": \"modc+mod4+9\""
            "} } } }");
    config_load_bindings(path, &cb);

    TAP_EQ_STR(cb.keyboard.viewport.go_to.page[0], "modc+mod4+0",
            "'viewport.go-to.page0' loads into the first array slot");
    TAP_EQ_STR(cb.keyboard.viewport.go_to.page[9], "modc+mod4+9",
            "'viewport.go-to.page9' loads into the last array slot");
    unlink(path);
}


int main(void)
{
    TAP_PLAN(23);

    s_test_missing_file();
    s_test_empty_file();
    s_test_go_to_regression();
    s_test_go_to_still_works_with_window_present();
    s_test_representative_fields_at_every_level();
    s_test_show_desktop_only_desktop_section_accepted();
    s_test_viewport_go_to_loads();

    return TAP_DONE();
}
