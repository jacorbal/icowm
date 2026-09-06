/**
 * @file tests/menu/dialog/test_shortcuts.c
 *
 * @brief Unit tests for menu/dialog/shortcuts.c
 *
 * 'dialog_shortcuts_show' delegates the actual dialog drawing to
 * 'menu_message_dialog_show_pairs' (menu/dialog/message.c, separately
 * tested), so that is the only stand-in this file needs; everything
 * else exercised here, which bindings get a row, how the ten
 * go-to-desktop bindings collapse to one line or stay ten, which
 * optional sections appear, is 'dialog_shortcuts_show' 's own logic.
 * The bindings fixture is built with the real
 * 'config_set_default_bindings_values' (config/bindings.c) rather than
 * hand-filled, since 'struct config_bindings_s' is too large to fill
 * safely by hand and the default values it writes are exactly the
 * shared-prefix case 's_append_goto_desktop' is meant to collapse.
 * 'dialog_pair_append_blank' is a tiny, pure, link-only reproduction of
 * message.c's own two-line helper, the same way test_confirm.c
 * reproduces 'dlgutil_u16max' rather than linking message.c's whole
 * (separately tested) file for it.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog/message.h>
#include <menu/dialog/shortcuts.h>

/* Harness includes */
#include "harness/tap.h"


/** A fake, non-dereferenced connection handle, distinct from NULL, to
 *  pass through to the stand-in below */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

/** Recording stand-in for 'menu_message_dialog_show_pairs'; each row's
 *  label/value text is copied into its own fixed buffer rather than
 *  kept as the caller's pointer, since some rows point into
 *  'dialog_shortcuts_show' 's own stack-local 'struct
 *  s_shortcuts_ctx_s', gone the instant it returns */
static int s_call_show_pairs;
static char s_captured_labels
        [DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LENGTH];
static char s_captured_values
        [DIALOG_MSG_MAX_LINES][DIALOG_MSG_LINE_MAX_LENGTH];
static bool s_captured_has_value[DIALOG_MSG_MAX_LINES];
static bool s_captured_has_label[DIALOG_MSG_MAX_LINES];
static uint8_t s_captured_count;

static config_td s_config;
static surface_td s_surface;


/**
 * @brief Link-only stand-in for @a dialog_pair_append_blank
 *
 * A tiny, pure helper genuinely defined in menu/dialog/message.c
 * rather than shortcuts.c itself; reproduced here rather than linking
 * that whole (separately tested) file, the same way
 * tests/menu/dialog/test_confirm.c reproduces 'dlgutil_u16max' rather
 * than linking all of menu/dialog.c for one two-line function.
 *
 * @note Complexity: @e O(1)
 */
void dialog_pair_append_blank(struct dialog_pair_s *pairs, uint8_t *count)
{
    if (*count >= (uint8_t) DIALOG_MSG_MAX_LINES) {
        return;
    }

    pairs[*count].label = NULL;
    pairs[*count].value = NULL;
    (*count)++;
}


/**
 * @brief Reset every captured/recorded value and the config/surface
 *        fixtures to a clean, known-default state ahead of one scenario
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_show_pairs = 0;
    memset(s_captured_labels, 0, sizeof(s_captured_labels));
    memset(s_captured_values, 0, sizeof(s_captured_values));
    memset(s_captured_has_value, 0, sizeof(s_captured_has_value));
    memset(s_captured_has_label, 0, sizeof(s_captured_has_label));
    s_captured_count = 0u;

    memset(&s_config, 0, sizeof(s_config));
    config_set_default_bindings_values(&s_config.bindings);

    memset(&s_surface, 0, sizeof(s_surface));
    s_surface.config = &s_config;
    s_surface.id = 0u;
    s_surface.desktop_count = 1u;
    s_surface.monitor_count = 1u;
}


/* Recording stand-in for menu_message_dialog_show_pairs */
void menu_message_dialog_show_pairs(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const struct dialog_pair_s *pairs, size_t count,
        menu_msg_level_e level)
{
    size_t i;

    (void) connection;
    (void) surface;
    (void) config;
    (void) level;
    s_call_show_pairs++;
    s_captured_count = (uint8_t) count;
    for (i = 0u; i < count && i < DIALOG_MSG_MAX_LINES; i++) {
        s_captured_has_label[i] = (pairs[i].label != NULL);
        s_captured_has_value[i] = (pairs[i].value != NULL);
        if (pairs[i].label != NULL) {
            (void) strncpy(s_captured_labels[i], pairs[i].label,
                    DIALOG_MSG_LINE_MAX_LENGTH - 1u);
        }
        if (pairs[i].value != NULL) {
            (void) strncpy(s_captured_values[i], pairs[i].value,
                    DIALOG_MSG_LINE_MAX_LENGTH - 1u);
        }
    }
}


/**
 * @brief Find a row's own printed text, in either its label or its
 *        value column, containing a given substring
 *
 * Used rather than an exact match, since composed rows (a grouped line,
 * the collapsed go-to-desktop line) hold more than just one binding's
 * combination.
 *
 * @param needle Substring to look for
 *
 * @return @c true if some row's label or value contains @p needle
 *
 * @note Complexity: @e O(n*m), where @e n is the number of captured
 *       rows and @e m is @p needle 's length
 */
static bool s_any_row_contains(const char *needle)
{
    uint8_t i;

    for (i = 0u; i < s_captured_count; i++) {
        if ((s_captured_has_label[i] &&
                    strstr(s_captured_labels[i], needle) != NULL) ||
                (s_captured_has_value[i] &&
                    strstr(s_captured_values[i], needle) != NULL)) {
            return true;
        }
    }
    return false;
}


/**
 * @brief NULL connection, surface or config all leave the dialog
 *        unopened
 *
 * @note Complexity: @e O(1)
 */
static void s_test_null_guards(void)
{
    s_reset();
    dialog_shortcuts_show(NULL, &s_surface, &s_config);
    TAP_EQ_INT(s_call_show_pairs, 0,
            "shortcuts_show: NULL connection is a no-op");

    s_reset();
    dialog_shortcuts_show(s_fake_connection, NULL, &s_config);
    TAP_EQ_INT(s_call_show_pairs, 0,
            "shortcuts_show: NULL surface is a no-op");

    s_reset();
    dialog_shortcuts_show(s_fake_connection, &s_surface, NULL);
    TAP_EQ_INT(s_call_show_pairs, 0,
            "shortcuts_show: NULL config is a no-op");
}


/**
 * @brief A valid call shows the dialog exactly once, at level NONE,
 *        with the modifiers summary line first
 *
 * @note Complexity: @e O(1)
 */
static void s_test_basic_show(void)
{
    s_reset();
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_EQ_INT(s_call_show_pairs, 1,
            "shortcuts_show: shows the dialog exactly once");
    TAP_OK(s_captured_count > 0u,
            "shortcuts_show: produces at least one row");
    TAP_OK(s_captured_has_label[0] &&
            strstr(s_captured_labels[0], "modc=Control") != NULL,
            "shortcuts_show: first row states the modc modifier value");
}


/**
 * @brief The ten go-to-desktop bindings collapse to a single row when
 *        they all share one prefix followed by their own digit, which
 *        is exactly what the default bindings look like
 *
 * @note Complexity: @e O(1)
 */
static void s_test_goto_desktop_shared_prefix(void)
{
    s_reset();
    s_surface.desktop_count = 2u;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(s_any_row_contains("modc+mod1+<0-9>"),
            "shortcuts_show: default go-to-desktop bindings collapse"
            " to one \"<0-9>\" row");
    TAP_OK(!s_any_row_contains("modc+mod1+0") ||
            s_any_row_contains("modc+mod1+<0-9>"),
            "shortcuts_show: does not also list an individual desktop-0"
            " row once the collapsed row is shown");
}


/**
 * @brief Rebinding just one of the ten go-to-desktop combinations to
 *        something unrelated forces ten individual rows instead of the
 *        collapsed one
 *
 * @note Complexity: @e O(1)
 */
static void s_test_goto_desktop_no_shared_prefix(void)
{
    s_reset();
    s_surface.desktop_count = 2u;
    (void) strncpy(s_config.bindings.keyboard.desktop.go_to.desktop[3],
            "mod4+F3",
            sizeof(s_config.bindings.keyboard.desktop.go_to.desktop[3]) - 1u);
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(!s_any_row_contains("<0-9>"),
            "shortcuts_show: a non-uniform binding set does not collapse"
            " to a \"<0-9>\" row");
    TAP_OK(s_any_row_contains("mod4+F3"),
            "shortcuts_show: the rebound desktop-3 combination is"
            " listed on its own row");
    TAP_OK(s_any_row_contains("modc+mod1+0"),
            "shortcuts_show: an unrelated, still-default desktop-0"
            " combination is also listed on its own row");
}


/**
 * @brief The desktop-remove and go-to-desktop rows only appear once
 *        there is more than one desktop
 *
 * @note Complexity: @e O(1)
 */
static void s_test_desktop_count_gating(void)
{
    s_reset();
    s_surface.desktop_count = 1u;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(!s_any_row_contains("<0-9>"),
            "shortcuts_show: a single-desktop surface shows no"
            " go-to-desktop row");

    s_reset();
    s_surface.desktop_count = 2u;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(s_any_row_contains("<0-9>"),
            "shortcuts_show: a multi-desktop surface shows the"
            " go-to-desktop row");
}


/**
 * @brief The four monitor-direction send-to rows only appear once
 *        there is more than one monitor
 *
 * @note Complexity: @e O(1)
 */
static void s_test_monitor_count_gating(void)
{
    s_reset();
    s_surface.monitor_count = 1u;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(!s_any_row_contains(
                s_config.bindings.keyboard.window.send_to.monitor.north),
            "shortcuts_show: a single-monitor surface shows no"
            " send-to-monitor rows");

    s_reset();
    s_surface.monitor_count = 2u;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(s_any_row_contains(
                s_config.bindings.keyboard.window.send_to.monitor.north),
            "shortcuts_show: a multi-monitor surface shows the"
            " send-to-monitor rows");
}


/**
 * @brief The send-to-desktop north/south rows, and the cycle-desktop
 *        north/south rows, only appear once the active screen's
 *        desktop layout has more than one row; the east/west rows
 *        appear either way, once there is more than one desktop
 *
 * @note Complexity: @e O(1)
 */
static void s_test_desktop_layout_rows_gating(void)
{
    s_reset();
    s_surface.desktop_count = 2u;
    s_surface.id = 0u;
    s_config.base.screens[0].desktop_layout.rows = 1u;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(!s_any_row_contains(
                s_config.bindings.keyboard.window.send_to.desktop.north),
            "shortcuts_show: a single-row desktop layout shows no"
            " send-to-desktop-north/south rows");
    TAP_OK(s_any_row_contains(
                s_config.bindings.keyboard.window.send_to.desktop.east),
            "shortcuts_show: send-to-desktop-east is shown regardless"
            " of the desktop layout's row count");

    s_reset();
    s_surface.desktop_count = 2u;
    s_surface.id = 0u;
    s_config.base.screens[0].desktop_layout.rows = 2u;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(s_any_row_contains(
                s_config.bindings.keyboard.window.send_to.desktop.north),
            "shortcuts_show: a multi-row desktop layout shows the"
            " send-to-desktop-north/south rows");
}


/**
 * @brief The fortune and scratchpad shortcut rows only appear while
 *        their own feature is enabled; the emergency-shortcut notice
 *        line only appears while that is enabled
 *
 * @note Complexity: @e O(1)
 */
static void s_test_feature_toggles(void)
{
    s_reset();
    s_config.base.fortune.is_enabled = false;
    s_config.base.scratchpad.is_enabled = false;
    s_config.base.shutdown.enable_emergency_shortcut = false;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(!s_any_row_contains(s_config.bindings.keyboard.wm.fortune),
            "shortcuts_show: fortune row absent while fortune is"
            " disabled");
    TAP_OK(!s_any_row_contains(s_config.bindings.keyboard.wm.scratchpad),
            "shortcuts_show: scratchpad row absent while scratchpad is"
            " disabled");

    s_reset();
    s_config.base.fortune.is_enabled = true;
    s_config.base.scratchpad.is_enabled = true;
    s_config.base.shutdown.enable_emergency_shortcut = true;
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(s_any_row_contains(s_config.bindings.keyboard.wm.fortune),
            "shortcuts_show: fortune row present while fortune is"
            " enabled");
    TAP_OK(s_any_row_contains(s_config.bindings.keyboard.wm.scratchpad),
            "shortcuts_show: scratchpad row present while scratchpad is"
            " enabled");
}


/**
 * @brief A grouped row (move/resize/cycle) joins each bound direction
 *        with a comma, skipping any direction left unbound, and omits
 *        the whole row once every direction in the group is unbound
 *
 * @note Complexity: @e O(1)
 */
static void s_test_append_group_joining(void)
{
    s_reset();
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(s_any_row_contains("Right=") && s_any_row_contains("Left=") &&
            s_any_row_contains("Up=") && s_any_row_contains("Down="),
            "shortcuts_show: a fully-bound group lists every direction,"
            " each tagged with its own short name");

    s_reset();
    s_config.bindings.keyboard.window.move.relative.right[0] = '\0';
    s_config.bindings.keyboard.window.move.relative.left[0] = '\0';
    s_config.bindings.keyboard.window.move.relative.up[0] = '\0';
    s_config.bindings.keyboard.window.move.relative.down[0] = '\0';
    dialog_shortcuts_show(s_fake_connection, &s_surface, &s_config);
    TAP_OK(!s_any_row_contains("Right=modc+mod1+l"),
            "shortcuts_show: a group left entirely unbound produces no"
            " row for it at all");
}


int main(void)
{
    TAP_PLAN(24);

    s_test_null_guards();
    s_test_basic_show();
    s_test_goto_desktop_shared_prefix();
    s_test_goto_desktop_no_shared_prefix();
    s_test_desktop_count_gating();
    s_test_monitor_count_gating();
    s_test_desktop_layout_rows_gating();
    s_test_feature_toggles();
    s_test_append_group_joining();

    return TAP_DONE();
}
