/**
 * @file tests/config/test_base.c
 *
 * @brief Test battery for base configuration loading
 *
 * s_config_load_screens is where most of this file's own real
 * complexity lives: two accepted on-disk shapes for
 * "topology.screens.desktops" (a flat per-desktop list applied to
 * screen 0, or a nested per-screen layout), detected from the first
 * array entry alone. That detection, the clamping to
 * CONFIG_MAX_DESKTOPS/CONFIG_MAX_SCREENS, and the inaugural-desktop
 * bounds check get the most thorough coverage here; the many
 * string-to-enum parsers scattered through the rest of the file
 * follow one identical, already-well-covered-elsewhere pattern
 * (json_field_normalize, then a chain of safe_strcmp checks, falling
 * back to a default), so those get one or two representative values
 * each rather than an exhaustive case per accepted string.
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
#include <defs/desktop.h>

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

    snprintf(path_out, path_out_size, "/tmp/icowm_test_base_%d_%d",
            (int) getpid(), s_counter++);
    f = fopen(path_out, "w");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}


/**
 * @brief Load 'content' into a freshly zeroed config_base_s/
 *        config_desktop_s pair, for tests that only need one call
 */
static int s_load(const char *content, struct config_base_s *base,
        struct config_desktop_s *desktop)
{
    char path[256];
    int status;

    memset(base, 0, sizeof(*base));
    memset(desktop, 0, sizeof(*desktop));
    s_write_temp_file(path, sizeof(path), content);
    status = config_load_base(path, base, desktop);
    unlink(path);
    return status;
}


/* A missing file fails outright */
static void s_test_missing_file(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    memset(&base, 0, sizeof(base));
    memset(&desktop, 0, sizeof(desktop));
    TAP_EQ_INT(config_load_base("/nonexistent/at/all", &base, &desktop),
            1, "a missing file fails");
}


/* The flat desktops shape: a plain list of desktop entries, applied
 * to screen 0, the common single-screen case */
static void s_test_screens_flat_shape(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"name\": \"Web\", \"background-color\": \"#112233\"},"
        "  {\"name\": \"Code\", \"background-color\": \"#445566\"}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_count, 2,
            "flat shape: desktop_count taken from the array length");
    TAP_EQ_STR(base.screens[0].desktops[0].name, "Web",
            "flat shape: first desktop's name loaded");
    TAP_EQ_INT(base.screens[0].desktops[1].settings.background.color,
            0x445566u, "flat shape: second desktop's color loaded");
}


/* A reload whose file no longer names a desktop's own
 * 'background-color' must fall back to the sentinel
 * ('WM_DESKTOP_BG_COLOR_UNSET'), not keep whatever color an earlier
 * load on the very same struct happened to leave there: this is
 * what lets 'desktop_init' (desktop.c) and the reload path in
 * wm/actions.c fall back to the theme's own
 * 'desktop.color.background' correctly. */
static void s_test_background_color_falls_back_after_removal(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;
    char path[256];

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"name\": \"Web\", \"background-color\": \"#112233\"},"
        "  {\"name\": \"Fixed\", \"background-color\": \"#445566\"}"
        "] } } }", &base, &desktop);
    TAP_EQ_INT(base.screens[0].desktops[0].settings.background.color,
            0x112233, "before removal: first desktop's color loaded");

    /* Reload the very same 'base' (no reset in between, the way an
     * actual configuration reload leaves it) from a file that still
     * sets the second desktop's color but no longer sets the
     * first's. */
    s_write_temp_file(path, sizeof(path),
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"name\": \"Web\"},"
        "  {\"name\": \"Fixed\", \"background-color\": \"#445566\"}"
        "] } } }");
    config_load_base(path, &base, &desktop);
    unlink(path);

    TAP_EQ_INT(base.screens[0].desktops[0].settings.background.color,
            WM_DESKTOP_BG_COLOR_UNSET,
            "after removal: first desktop's color falls back to the"
            " sentinel");
    TAP_EQ_INT(base.screens[0].desktops[1].settings.background.color,
            0x445566, "after removal: second desktop's own color,"
            " still set, is unaffected");
}


/* The nested shape: each array entry describes one whole screen (its
 * own count/inaugural/settings), detected because the first entry
 * carries one of those three fields */
static void s_test_screens_nested_shape(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 2, \"desktops\": ["
        "  {\"count\": 2, \"inaugural\": 1, \"settings\": ["
        "    {\"name\": \"S0D0\"}, {\"name\": \"S0D1\"}"
        "  ]},"
        "  {\"count\": 1, \"settings\": [{\"name\": \"S1D0\"}]}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_count, 2,
            "nested shape: screen 0's own desktop_count loaded");
    TAP_EQ_INT((int) base.screens[0].desktop_inaugural, 1,
            "nested shape: screen 0's own inaugural desktop loaded");
    TAP_EQ_STR(base.screens[0].desktops[1].name, "S0D1",
            "nested shape: screen 0's second desktop name loaded");
    TAP_EQ_INT((int) base.screens[1].desktop_count, 1,
            "nested shape: screen 1's own desktop_count loaded"
            " independently of screen 0's");
    TAP_EQ_STR(base.screens[1].desktops[0].name, "S1D0",
            "nested shape: screen 1's own desktop name loaded");
}


/* Shape detection looks only at the first array entry: if it has
 * none of settings/count/inaugural, the whole array is treated as
 * flat, even if a later entry happens to have one of those keys
 * (documented, not a bug: a mixed shape is simply not supported) */
static void s_test_shape_detected_from_first_entry_only(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"desktops\": ["
        "  {\"name\": \"Ordinary\"},"
        "  {\"count\": 99, \"name\": \"HasCountToo\"}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_count, 2,
            "flat shape still detected: 2 entries, not screen-per-entry");
    TAP_EQ_STR(base.screens[0].desktops[1].name, "HasCountToo",
            "second entry loaded as an ordinary desktop entry,"
            " its own stray 'count' field simply ignored");
}


/* An inaugural desktop index at or beyond the screen's own desktop
 * count reverts to 0, the first desktop, rather than staying out of
 * range */
static void s_test_inaugural_out_of_range_reverts_to_zero(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"desktops\": ["
        "  {\"count\": 2, \"inaugural\": 5}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_inaugural, 0,
            "inaugural 5 with only 2 desktops reverts to 0");
}


/* topology.screens.count of 0 is meaningless (nowhere to put a
 * window) and is corrected up to the minimum of 1 */
static void s_test_screen_count_enforced_minimum(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load("{\"topology\": {\"screens\": {\"count\": 0} } }",
            &base, &desktop);

    TAP_EQ_INT((int) base.screen_count, 1,
            "screen count of 0 corrected up to the minimum of 1");
}


/* More desktops than CONFIG_MAX_DESKTOPS in the flat shape are
 * clamped, never overflowing the fixed-size 'desktops' array */
static void s_test_flat_shape_clamped_to_max_desktops(void)
{
    char content[1024];
    size_t offset;
    struct config_base_s base;
    struct config_desktop_s desktop;

    offset = (size_t) snprintf(content, sizeof(content),
            "{\"topology\": {\"screens\": {\"desktops\": [");
    for (int i = 0; i < CONFIG_MAX_DESKTOPS + 3; ++i) {
        offset += (size_t) snprintf(content + offset,
                sizeof(content) - offset,
                "%s{\"name\": \"d%d\"}", (i > 0) ? "," : "", i);
    }
    snprintf(content + offset, sizeof(content) - offset, "] } } }");

    s_load(content, &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_count, CONFIG_MAX_DESKTOPS,
            "flat shape clamped to CONFIG_MAX_DESKTOPS");
}


/* No 'layout' at all: falls back to the exact single-row default,
 * the same reading order the desktop list itself already had before
 * 'layout' existed */
static void s_test_layout_absent_defaults_to_linear(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"count\": 5}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT(base.screens[0].desktop_layout.orientation,
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL,
            "no layout: orientation defaults to horizontal");
    TAP_EQ_INT(base.screens[0].desktop_layout.corner,
            CONFIG_DESKTOP_CORNER_TOP_LEFT,
            "no layout: corner defaults to top-left");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.rows, 1,
            "no layout: rows defaults to 1");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.columns, 5,
            "no layout: columns defaults to desktop_count");
}


/* All four keys named explicitly: used exactly as read */
static void s_test_layout_all_fields_explicit(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"count\": 6, \"layout\": {\"orientation\": \"vertical\","
        "    \"corner\": \"bottom-right\", \"rows\": 2, \"columns\": 3}}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT(base.screens[0].desktop_layout.orientation,
            CONFIG_DESKTOP_ORIENTATION_VERTICAL,
            "all explicit: orientation loaded");
    TAP_EQ_INT(base.screens[0].desktop_layout.corner,
            CONFIG_DESKTOP_CORNER_BOTTOM_RIGHT,
            "all explicit: corner loaded");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.rows, 2,
            "all explicit: rows loaded");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.columns, 3,
            "all explicit: columns loaded");
}


/* Only 'rows' named: 'columns' is computed by ceiling division
 * against desktop_count, not simply defaulted to 1 */
static void s_test_layout_only_rows_computes_columns(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"count\": 7, \"layout\": {\"rows\": 3}}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_layout.rows, 3,
            "only rows: rows loaded as given");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.columns, 3,
            "only rows: columns computed by ceiling division,"
            " ceil(7/3) = 3, not defaulted to 1");
}


/* Only 'columns' named, specifically 'columns: 1': a "strictly
 * vertical" request, 'rows' computed to hold every desktop in that
 * one column rather than rejecting a 1x1 grid the moment more than
 * one desktop exists */
static void s_test_layout_only_columns_one_is_strictly_vertical(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"count\": 4, \"layout\": {\"columns\": 1}}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_layout.columns, 1,
            "only columns=1: columns loaded as given");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.rows, 4,
            "only columns=1: rows computed to fit all 4 desktops in"
            " that single column, not defaulted to 1");
}


/* 'rows * columns' exceeding 'count' is accepted, not rejected: a
 * deliberate, desktop-less gap cell */
static void s_test_layout_gap_accepted(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"count\": 5, \"layout\": {\"rows\": 2, \"columns\": 3}}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_layout.rows, 2,
            "gap: rows kept as given, not rejected");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.columns, 3,
            "gap: columns kept as given, even though 2x3=6 exceeds"
            " the 5 real desktops");
}


/* 'rows' explicitly 0 is rejected outright, falling back to the
 * single-row default, the same as an absent 'layout' */
static void s_test_layout_rows_zero_rejected(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"count\": 4, \"layout\": {\"rows\": 0, \"columns\": 2}}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_layout.rows, 1,
            "rows=0: rejected, falls back to the linear default");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.columns, 4,
            "rows=0: columns falls back to desktop_count too");
}


/* A rows/columns combination too small to ever hold every desktop
 * at all, no matter how arranged, is rejected the same way */
static void s_test_layout_too_small_rejected(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"count\": 9, \"layout\": {\"rows\": 2, \"columns\": 3}}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_layout.rows, 1,
            "2x3=6 cannot hold 9 desktops: rejected");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.columns, 9,
            "falls back to the linear default");
}


/* Above CONFIG_MAX_DESKTOPS is rejected the same way, even for a
 * single named dimension, checked before it is ever used as a
 * divisor to compute the other */
static void s_test_layout_rows_above_max_rejected(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"topology\": {\"screens\": {\"count\": 1, \"desktops\": ["
        "  {\"count\": 4, \"layout\": {\"rows\": 999}}"
        "] } } }", &base, &desktop);

    TAP_EQ_INT((int) base.screens[0].desktop_layout.rows, 1,
            "rows above CONFIG_MAX_DESKTOPS: rejected");
    TAP_EQ_INT((int) base.screens[0].desktop_layout.columns, 4,
            "falls back to the linear default");
}


/* A missing 'topology.screens' object leaves defaults untouched,
 * rather than crashing or zeroing anything */
static void s_test_missing_topology_leaves_defaults(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    base.screen_count = 42u;  /* a sentinel, pre-set before loading */
    memset(&desktop, 0, sizeof(desktop));

    char path[256];
    s_write_temp_file(path, sizeof(path), "{}");
    config_load_base(path, &base, &desktop);
    unlink(path);

    TAP_EQ_INT((int) base.screen_count, 42,
            "no 'topology.screens' at all: screen_count left untouched");
}


/* A representative field from each remaining config_load_base
 * section: theme, programs, windows (gravity/focus/placement;
 * windows.snap itself is deliberately excluded here, since neither
 * this file's own base/parse.c nor any other file actually parses
 * a "snap" key from JSON at all, despite the struct field existing
 * in config.h), icons, the 3 boolean shortcuts, startup-notification,
 * and menus */
static void s_test_representative_fields(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{"
        "\"theme\": \"my-theme\","
        "\"programs\": {\"terminal\": \"alacritty\"},"
        "\"windows\": {"
        "  \"gravity\": \"center\","
        "  \"focus\": {\"policy\": \"sloppy\","
        "    \"focus-new\": false},"
        "  \"placement\": {\"policy\": \"cascade\", \"monitor\": \"primary\"}"
        "},"
        "\"icons\": {\"placement\": {\"policy\": \"top\"}},"
        "\"shutdown\": {\"enable-emergency-shortcut\": true,"
        "  \"timeout-seconds\": 20},"
        "\"fortune\": {\"is-enabled\": true,"
        "  \"command\": \"fortune -s\"},"
        "\"startup-notification\": {\"is-enabled\": true,"
        "  \"timeout-seconds\": 15},"
        "\"menus\": {\"root\": {\"position\": \"center\"}}"
        "}", &base, &desktop);

    TAP_EQ_STR(base.theme, "my-theme", "theme name loaded");
    TAP_EQ_STR(base.programs.terminal, "alacritty", "programs.terminal");
    TAP_EQ_INT(base.windows.gravity, CONFIG_GRAVITY_CENTER,
            "windows.gravity");
    TAP_EQ_INT(base.windows.focus_policy, CONFIG_FOCUS_POLICY_SLOPPY,
            "windows.focus.policy");
    TAP_OK(!base.windows.focus.focus_new,
            "windows.focus.focus-new");
    TAP_EQ_INT(base.windows.placement_policy,
            CONFIG_PLACEMENT_POLICY_CASCADE, "windows.placement.policy");
    TAP_EQ_INT(base.windows.monitor_policy,
            CONFIG_PLACEMENT_MONITOR_PRIMARY, "windows.placement.monitor");
    TAP_EQ_INT(base.icons.placement_policy, CONFIG_ICON_PLACEMENT_TOP,
            "icons.placement.policy");
    TAP_OK(base.shutdown.enable_emergency_shortcut,
            "shutdown.enable-emergency-shortcut");
    TAP_EQ_INT((int) base.shutdown.timeout_seconds, 20,
            "shutdown.timeout-seconds");
    TAP_OK(base.fortune.is_enabled, "fortune.is-enabled");
    TAP_EQ_STR(base.fortune.command, "fortune -s", "fortune.command");
    TAP_OK(base.startup_notification.is_enabled,
            "startup-notification.is-enabled");
    TAP_EQ_INT((int) base.startup_notification.timeout_seconds, 15,
            "startup-notification.timeout-seconds");
    TAP_EQ_INT(base.menus.root.position, CONFIG_MENU_POSITION_CENTER,
            "menus.root.position");
}


/* icons.placement backward-compatibility: the modern
 * 'icons.placement.policy' object form */
static void s_test_icons_placement_modern_object_form(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load("{\"icons\": {\"placement\": {\"policy\": \"left\"}}}",
            &base, &desktop);
    TAP_EQ_INT(base.icons.placement_policy, CONFIG_ICON_PLACEMENT_LEFT,
            "modern object form: icons.placement.policy");
}


/* icons.placement backward-compatibility: a bare string directly at
 * 'icons.placement', not wrapped in its own object */
static void s_test_icons_placement_bare_string_form(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load("{\"icons\": {\"placement\": \"right\"}}", &base, &desktop);
    TAP_EQ_INT(base.icons.placement_policy, CONFIG_ICON_PLACEMENT_RIGHT,
            "bare-string form: icons.placement itself is the policy");
}


/* icons.placement backward-compatibility: the legacy location under
 * 'windows.icons.placement', used only when 'icons' itself is absent */
static void s_test_icons_placement_legacy_windows_location(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load("{\"windows\": {\"icons\": {\"placement\": \"bottom\"}}}",
            &base, &desktop);
    TAP_EQ_INT(base.icons.placement_policy, CONFIG_ICON_PLACEMENT_BOTTOM,
            "legacy location: windows.icons.placement used when"
            " top-level 'icons' is absent");
}


/* desktops.show-overlay/warp-on-edge-drag/wrap-at-bounds/margins, a
 * sibling of 'topology' at the config root, meant to still apply on
 * every reload (unlike topology) */
static void s_test_desktop_behavior(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"desktops\": {\"show-overlay\": true,"
        " \"warp-on-edge-drag\": true, \"wrap-at-bounds\": true,"
        " \"margins\": {\"top\": 3, \"left\": 7}}}", &base, &desktop);

    TAP_OK(desktop.show_overlay, "desktops.show-overlay");
    TAP_OK(desktop.warp_on_edge_drag, "desktops.warp-on-edge-drag");
    TAP_OK(desktop.wrap_at_bounds, "desktops.wrap-at-bounds");
    TAP_EQ_INT((int) desktop.margins.top, 3, "desktops.margins.top");
    TAP_EQ_INT((int) desktop.margins.left, 7, "desktops.margins.left");
}


/* A representative sample of ci_config_load_systray's own fields:
 * is-enabled, margins, position, monitor (anchor/index), order,
 * layer, clock, battery (threshold/backend), and text (position) */
static void s_test_systray_representative_fields(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"systray\": {"
        "\"is-enabled\": true,"
        "\"margins\": {\"top\": 2},"
        "\"position\": \"bottom-left\","
        "\"monitor\": {\"anchor\": \"index\", \"index\": 1},"
        "\"order\": \"descending\","
        "\"layer\": \"overlay\","
        "\"clock\": {\"is-enabled\": true, \"format\": \"%H:%M\"},"
        "\"battery\": {\"is-enabled\": true,"
        "  \"threshold\": {\"low\": 15},"
        "  \"backend\": {\"type\": \"apm\", \"number\": 2}},"
        "\"text\": {\"position\": \"left\"}"
        "} }", &base, &desktop);

    TAP_OK(base.systray.is_enabled, "systray.is-enabled");
    TAP_EQ_INT((int) base.systray.margins.top, 2, "systray.margins.top");
    TAP_EQ_INT(base.systray.position, CONFIG_SYSTRAY_POSITION_BOTTOM_LEFT,
            "systray.position");
    TAP_EQ_INT(base.systray.monitor.anchor, CONFIG_SYSTRAY_MONITOR_INDEX,
            "systray.monitor.anchor");
    TAP_EQ_INT((int) base.systray.monitor.index, 1,
            "systray.monitor.index");
    TAP_EQ_INT(base.systray.order, CONFIG_SYSTRAY_ORDER_DESCENDING,
            "systray.order");
    TAP_EQ_INT(base.systray.layer, CONFIG_SYSTRAY_LAYER_OVERLAY,
            "systray.layer");
    TAP_OK(base.systray.clock.is_enabled, "systray.clock.is-enabled");
    TAP_EQ_STR(base.systray.clock.format, "%H:%M",
            "systray.clock.format");
    TAP_OK(base.systray.battery.is_enabled,
            "systray.battery.is-enabled");
    TAP_EQ_INT((int) base.systray.battery.threshold.low, 15,
            "systray.battery.threshold.low");
    TAP_EQ_INT(base.systray.battery.backend.type,
            CONFIG_BATTERY_BACKEND_APM, "systray.battery.backend.type");
    TAP_EQ_INT((int) base.systray.battery.backend.number, 2,
            "systray.battery.backend.number");
    TAP_EQ_INT(base.systray.text.position, CONFIG_SYSTRAY_TEXT_LEFT,
            "systray.text.position");
}


/* systray.text.order: up to 2 recognized items are loaded, in the
 * order given; an unrecognized item is skipped, not counted */
static void s_test_systray_text_order(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"systray\": {\"text\": {\"order\":"
        " [\"battery\", \"bogus\", \"clock\"]}}}", &base, &desktop);

    TAP_EQ_INT((int) base.systray.text.order_count, 2,
            "2 recognized items counted; the unrecognized one skipped");
    TAP_EQ_INT(base.systray.text.order[0], CONFIG_SYSTRAY_TEXT_BATTERY,
            "first recognized item, in file order");
    TAP_EQ_INT(base.systray.text.order[1], CONFIG_SYSTRAY_TEXT_CLOCK,
            "second recognized item, right after skipping the bad one");
}


/* Observed (not asserted as either correct or a bug) behavior:
 * systray.text.order has no deduplication of its own, unlike theme's
 * titlebar button list; a repeated item is counted twice, up to the
 * 2-item cap */
static void s_test_systray_text_order_no_dedup(void)
{
    struct config_base_s base;
    struct config_desktop_s desktop;

    s_load(
        "{\"systray\": {\"text\": {\"order\":"
        " [\"clock\", \"clock\"]}}}", &base, &desktop);

    TAP_EQ_INT((int) base.systray.text.order_count, 2,
            "observed: 'clock' repeated twice is counted twice,"
            " not de-duplicated");
    TAP_EQ_INT(base.systray.text.order[0], CONFIG_SYSTRAY_TEXT_CLOCK,
            "both slots hold the same repeated item");
    TAP_EQ_INT(base.systray.text.order[1], CONFIG_SYSTRAY_TEXT_CLOCK,
            "both slots hold the same repeated item");
}


int main(void)
{
    TAP_PLAN(81);

    s_test_missing_file();
    s_test_screens_flat_shape();
    s_test_background_color_falls_back_after_removal();
    s_test_screens_nested_shape();
    s_test_shape_detected_from_first_entry_only();
    s_test_inaugural_out_of_range_reverts_to_zero();
    s_test_screen_count_enforced_minimum();
    s_test_flat_shape_clamped_to_max_desktops();
    s_test_layout_absent_defaults_to_linear();
    s_test_layout_all_fields_explicit();
    s_test_layout_only_rows_computes_columns();
    s_test_layout_only_columns_one_is_strictly_vertical();
    s_test_layout_gap_accepted();
    s_test_layout_rows_zero_rejected();
    s_test_layout_too_small_rejected();
    s_test_layout_rows_above_max_rejected();
    s_test_missing_topology_leaves_defaults();
    s_test_representative_fields();
    s_test_icons_placement_modern_object_form();
    s_test_icons_placement_bare_string_form();
    s_test_icons_placement_legacy_windows_location();
    s_test_desktop_behavior();
    s_test_systray_representative_fields();
    s_test_systray_text_order();
    s_test_systray_text_order_no_dedup();

    return TAP_DONE();
}
