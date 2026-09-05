/**
 * @file tests/input/kbd/test_bind.c
 *
 * @brief Test battery for keyboard binding parsing, grab, and binding
 *        table
 *
 * 'keyboard_load' issues passive-grab protocol calls that need a live
 * X connection to answer for real ('xcb_ungrab_key',
 * 'xcb_grab_key_checked', 'xcb_request_check',
 * 'xcb_key_symbols_get_keycode'), so those four are replaced by
 * controlled or recording stand-ins; libxcb is not linked, so these
 * stand-ins are the only definitions the linker finds.
 * 'xcb_connection_get' (utils/xcb/connection.c) is genuine, pure
 * project logic with no server round-trip of its own (it only reads
 * back whatever pointer was last recorded), so the real source file
 * is linked and 'xcb_connection_set' seeds it with a fake non-null
 * handle before any scenario runs.  'im_parse_modifier_token'
 * (input/modifier.c) is likewise pure string/config logic with no X
 * or cross-module dependency, so it is linked for real too, the same
 * way 'input/mouse/bind.c' does for its own binding table.  'list.c'
 * is linked for real as well, since 'keyboard_load' walks a genuine
 * 'list_td' of surfaces built by each scenario below.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* strtok_r */

/* System includes */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>
#include <utils/safe/safestr.h>
#include <utils/xcb/connection.h>

/* Local includes */
#include <harness/tap.h>
#include <input/kbd/bind.h>


/** Non-null opaque handle standing in for a real xcb_connection_t */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
        (xcb_connection_t *) &s_fake_connection_storage;

/** Non-null opaque handle standing in for a real xcb_key_symbols_t */
static int s_fake_keysyms_storage;
static xcb_key_symbols_t *const s_fake_keysyms =
        (xcb_key_symbols_t *) &s_fake_keysyms_storage;

/** Call counters, reset by s_reset before each scenario */
static int s_call_ungrab_key;
static int s_call_grab_key_checked;
static int s_call_request_check;
static int s_grab_key_calls_for_last_keycode;

/** Keycode 'xcb_key_symbols_get_keycode' hands back for any keysym
 *  other than XCB_NO_SYMBOL; a single-entry, null-terminated table,
 *  the shape 'keyboard_load' iterates with its own inner 'for' loop */
static xcb_keycode_t s_stub_keycodes[2] = { 38, 0 };


/**
 * @brief Reset every recording stand-in's state before a scenario
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_ungrab_key = 0;
    s_call_grab_key_checked = 0;
    s_call_request_check = 0;
    s_grab_key_calls_for_last_keycode = 0;
}


/**
 * @brief Recording stand-in for @a xcb_ungrab_key
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ungrab_key(xcb_connection_t *connection,
        xcb_keycode_t key, xcb_window_t grab_window, uint16_t modifiers)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) key;
    (void) grab_window;
    (void) modifiers;
    memset(&cookie, 0, sizeof(cookie));
    s_call_ungrab_key++;
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_grab_key_checked
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_grab_key_checked(xcb_connection_t *connection,
        uint8_t owner_events, xcb_window_t grab_window,
        uint16_t modifiers, xcb_keycode_t key, uint8_t pointer_mode,
        uint8_t keyboard_mode)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) owner_events;
    (void) grab_window;
    (void) modifiers;
    (void) pointer_mode;
    (void) keyboard_mode;
    memset(&cookie, 0, sizeof(cookie));
    s_call_grab_key_checked++;
    if (key == s_stub_keycodes[0]) {
        s_grab_key_calls_for_last_keycode++;
    }

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_request_check
 *
 * Always reports no error, so 'keyboard_load' never takes its
 * 'LOGGER_WARNING' + 'free(err)' failure path.
 *
 * @note Complexity: @e O(1)
 */
xcb_generic_error_t *xcb_request_check(xcb_connection_t *connection,
        xcb_void_cookie_t cookie)
{
    (void) connection;
    (void) cookie;
    s_call_request_check++;
    return NULL;
}


/**
 * @brief Controlled stand-in for @a xcb_key_symbols_get_keycode
 *
 * Hands back a freshly allocated copy of 'keyboard_load's own
 * expected single-keycode, null-terminated table for any keysym
 * other than XCB_NO_SYMBOL, and NULL for XCB_NO_SYMBOL itself,
 * mirroring the real function's documented behavior of answering
 * NULL for a keysym with no matching keycode; the caller frees what
 * this returns, exactly as it does for the real function.
 *
 * @note Complexity: @e O(1)
 */
xcb_keycode_t *xcb_key_symbols_get_keycode(xcb_key_symbols_t *syms,
        xcb_keysym_t keysym)
{
    xcb_keycode_t *out;

    (void) syms;
    if (keysym == XCB_NO_SYMBOL) {
        return NULL;
    }

    out = malloc(sizeof(s_stub_keycodes));
    if (out != NULL) {
        memcpy(out, s_stub_keycodes, sizeof(s_stub_keycodes));
    }
    return out;
}


/**
 * @brief Link-only stand-in for @a xcb_key_symbols_get_keysym
 *
 * Never actually reached with a real table by
 * 's_test_keysym_for_state_null_keysyms' below, which only exercises
 * the NULL-keysyms guard clause, but the symbol still has to resolve
 * at link time since 'keyboard_keysym_for_state' calls it
 * unconditionally past that guard.
 *
 * @note Complexity: @e O(1)
 */
xcb_keysym_t xcb_key_symbols_get_keysym(xcb_key_symbols_t *syms,
        xcb_keycode_t keycode, int col)
{
    (void) syms;
    (void) keycode;
    (void) col;
    return XCB_NO_SYMBOL;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict msg, ...)
{
    (void) level;
    (void) prefix;
    (void) msg;
    return 0;
}


/**
 * @brief Build a config with every keyboard binding string left
 *        empty and every gating flag off
 *
 * @param config Configuration to zero out
 *
 * @note Complexity: @e O(1)
 */
static void s_config_reset(config_td *config)
{
    memset(config, 0, sizeof(*config));
}


/* Before keyboard_load ever runs, the table is empty */
static void s_test_initial_state_empty(void)
{
    xcb_keysym_t keysym = 0x1234u;
    uint16_t modmask = 0xffffu;
    enum wm_keybind_type_e type;

    /* keyboard_binding_count/keyboard_binding_at read the module's
     * own static table, last populated by whichever scenario ran
     * most recently; this only documents the out-of-range behavior,
     * which holds regardless of what a previous scenario loaded */
    type = keyboard_binding_at(-1, &keysym, &modmask);
    TAP_EQ_INT((int) type, (int) KEYBIND_NONE,
            "a negative index reads back KEYBIND_NONE");
    TAP_EQ_INT((int) keysym, (int) XCB_NO_SYMBOL,
            "a negative index zeroes keysym_out too");
    TAP_EQ_INT(modmask, 0, "a negative index zeroes modmask_out too");
}


/* keyboard_find with NULL output pointers never touches the table */
static void s_test_find_null_outputs(void)
{
    bool found;

    found = keyboard_find(KEYBIND_WM_QUIT, NULL, NULL);
    TAP_OK(!found, "keyboard_find with NULL outputs reports not found");
}


/* keyboard_load with empty surfaces still parses and registers
 * bindings in the table, just never grabs anything */
static void s_test_load_empty_surfaces_registers_binding(void)
{
    config_td config;
    list_td *surfaces;
    xcb_keysym_t keysym = XCB_NO_SYMBOL;
    uint16_t modmask = 0;
    bool found;

    s_config_reset(&config);
    safe_strncpy(config.bindings.mod1, "mod1",
            sizeof(config.bindings.mod1));
    safe_strncpy(config.bindings.keyboard.wm.quit, "mod1+q",
            sizeof(config.bindings.keyboard.wm.quit));

    surfaces = list_init(NULL);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);

    TAP_EQ_INT(s_call_grab_key_checked, 0,
            "no surfaces means xcb_grab_key_checked is never reached");
    found = keyboard_find(KEYBIND_WM_QUIT, &keysym, &modmask);
    TAP_OK(found,
            "the QUIT binding is still registered with no surfaces");
    TAP_OK(modmask != 0, "the QUIT binding's modifier mask is non-zero");

    list_destroy(surfaces);
}


/* An unparseable binding string (empty, and every gated binding left
 * disabled) leaves the table with only the one hardcoded, always-on
 * 'Mod1+space' window-menu binding, which names no configuration
 * field at all and so cannot be left empty by this scenario */
static void s_test_load_unparseable_binding_registers_nothing(void)
{
    config_td config;
    list_td *surfaces;
    int count_after;

    s_config_reset(&config);
    /* Every field left as an empty string is unparseable
     * (s_parse_binding rejects a NULL/empty binding outright), and
     * every gated binding (emergency exit, fortune, per-monitor and
     * per-desktop bindings) stays disabled since 'has_multi_monitor'
     * and 'has_multi_desktop' are both false with no surfaces at
     * all, and every '*_enabled'/'*_shortcut' flag is false too */

    surfaces = list_init(NULL);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);
    count_after = keyboard_binding_count();

    TAP_EQ_INT(count_after, 1,
            "every field left empty parses only the hardcoded" \
            " window-menu binding into the table");

    list_destroy(surfaces);
}


/* keyboard_load grabs each registered binding's keycode, with every
 * lock-modifier variant (4), on every surface with a real screen */
static void s_test_load_grabs_on_surfaces(void)
{
    config_td config;
    list_td *surfaces;
    surface_td surface;
    xcb_screen_t screen;

    s_config_reset(&config);
    safe_strncpy(config.bindings.mod1, "mod1",
            sizeof(config.bindings.mod1));
    safe_strncpy(config.bindings.keyboard.wm.quit, "mod1+q",
            sizeof(config.bindings.keyboard.wm.quit));

    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    surface.screen = &screen;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, &surface);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);

    TAP_EQ_INT(s_call_ungrab_key, 1,
            "the surface's root is ungrabbed once before re-grabbing");
    /* 4 lock-modifier variants (none, Lock, Mod2, Lock|Mod2) for each
     * of the two bindings that end up registered here (the QUIT
     * binding configured above, plus the hardcoded, always-on
     * 'Mod1+space' window-menu binding), and the stub
     * 'xcb_key_symbols_get_keycode' hands every keysym the same
     * single stub keycode, so both land on it too */
    TAP_EQ_INT(s_grab_key_calls_for_last_keycode, 8,
            "the keycode is grabbed 4 lock-modifier variants for" \
            " each of the 2 registered bindings (8)");
    TAP_EQ_INT(s_call_request_check, s_call_grab_key_checked,
            "every grab attempt is followed by its own request check");

    list_destroy(surfaces);
}


/* A surface with a NULL screen is skipped by both the ungrab pass and
 * the grab pass, without crashing */
static void s_test_load_skips_null_screen_surface(void)
{
    config_td config;
    list_td *surfaces;
    surface_td surface;

    s_config_reset(&config);
    safe_strncpy(config.bindings.mod1, "mod1",
            sizeof(config.bindings.mod1));
    safe_strncpy(config.bindings.keyboard.wm.quit, "mod1+q",
            sizeof(config.bindings.keyboard.wm.quit));

    memset(&surface, 0, sizeof(surface));
    surface.screen = NULL;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, &surface);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);

    TAP_EQ_INT(s_call_ungrab_key, 0,
            "a NULL-screen surface is skipped by the ungrab pass");
    TAP_EQ_INT(s_call_grab_key_checked, 0,
            "a NULL-screen surface is skipped by the grab pass too");

    list_destroy(surfaces);
}


/* The emergency-exit binding is skipped entirely when its flag is
 * off, even though its combination is hardcoded and unconditional
 * otherwise */
static void s_test_load_emergency_exit_disabled_by_default(void)
{
    config_td config;
    list_td *surfaces;
    xcb_keysym_t keysym = XCB_NO_SYMBOL;
    uint16_t modmask = 0;
    bool found;

    s_config_reset(&config);
    config.base.shutdown.enable_emergency_shortcut = false;

    surfaces = list_init(NULL);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);

    found = keyboard_find(KEYBIND_WM_EMERGENCY_EXIT, &keysym, &modmask);
    TAP_OK(!found,
            "the emergency exit binding is absent when its flag is off");

    list_destroy(surfaces);
}


/* Enabling the emergency-exit flag registers 'Ctrl+Mod1+Backspace'
 * for it, and any other binding resolving to that same combination
 * is dropped in its favor rather than registered too */
static void s_test_load_emergency_exit_enabled_wins_collision(void)
{
    config_td config;
    list_td *surfaces;
    xcb_keysym_t emergency_keysym = XCB_NO_SYMBOL;
    uint16_t emergency_modmask = 0;
    xcb_keysym_t quit_keysym = XCB_NO_SYMBOL;
    uint16_t quit_modmask = 0;
    bool emergency_found;
    bool quit_found;

    s_config_reset(&config);
    config.base.shutdown.enable_emergency_shortcut = true;
    /* 'ctrl'/'mod1' below are resolved by 'im_parse_modifier_token'
     * as literal modifier names, not through the 'mod1'/'modc'
     * config aliases, so no alias field needs to be set at all for
     * this to parse the same combination as the hardcoded one */
    safe_strncpy(config.bindings.keyboard.wm.quit, "ctrl+mod1+backspace",
            sizeof(config.bindings.keyboard.wm.quit));

    surfaces = list_init(NULL);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);

    emergency_found = keyboard_find(KEYBIND_WM_EMERGENCY_EXIT,
            &emergency_keysym, &emergency_modmask);
    quit_found = keyboard_find(KEYBIND_WM_QUIT, &quit_keysym,
            &quit_modmask);

    TAP_OK(emergency_found,
            "the emergency exit binding is present once its flag" \
            " is on");
    TAP_OK(!quit_found,
            "a QUIT binding colliding with the emergency exit" \
            " combination is dropped in its favor");

    list_destroy(surfaces);
}


/* The fortune easter egg binding is likewise skipped when its own
 * flag is off */
static void s_test_load_fortune_disabled_by_default(void)
{
    config_td config;
    list_td *surfaces;
    xcb_keysym_t keysym = XCB_NO_SYMBOL;
    uint16_t modmask = 0;
    bool found;

    s_config_reset(&config);
    config.base.fortune.is_enabled = false;
    safe_strncpy(config.bindings.keyboard.wm.fortune, "mod4+ctrl+backspace",
            sizeof(config.bindings.keyboard.wm.fortune));

    surfaces = list_init(NULL);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);

    found = keyboard_find(KEYBIND_WM_FORTUNE, &keysym, &modmask);
    TAP_OK(!found,
            "the fortune binding is absent when its flag is off");

    list_destroy(surfaces);
}


/* A "move to monitor" direction binding is skipped unless at least
 * one surface reports more than one monitor */
static void s_test_load_monitor_bindings_need_multi_monitor(void)
{
    config_td config;
    list_td *surfaces;
    surface_td surface;
    xcb_keysym_t keysym = XCB_NO_SYMBOL;
    uint16_t modmask = 0;
    bool found_single;
    bool found_multi;

    s_config_reset(&config);
    safe_strncpy(config.bindings.keyboard.window.send_to.monitor.north,
            "ctrl+up",
            sizeof(config.bindings.keyboard.window.send_to.monitor.north));

    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 1u;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, &surface);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);
    found_single = keyboard_find(KEYBIND_CLIENT_MOVE_MONITOR_NORTH,
            &keysym, &modmask);
    TAP_OK(!found_single,
            "a move-to-monitor binding is absent with only one" \
            " monitor");

    surface.monitor_count = 2u;
    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);
    found_multi = keyboard_find(KEYBIND_CLIENT_MOVE_MONITOR_NORTH,
            &keysym, &modmask);
    TAP_OK(found_multi,
            "the same binding is present once a surface has more" \
            " than one monitor");

    list_destroy(surfaces);
}


/* A desktop-cycling binding is skipped unless at least one surface
 * reports more than one desktop */
static void s_test_load_desktop_bindings_need_multi_desktop(void)
{
    config_td config;
    list_td *surfaces;
    surface_td surface;
    xcb_keysym_t keysym = XCB_NO_SYMBOL;
    uint16_t modmask = 0;
    bool found_single;
    bool found_multi;

    s_config_reset(&config);
    safe_strncpy(config.bindings.keyboard.cycle.desktop.north, "ctrl+right",
            sizeof(config.bindings.keyboard.cycle.desktop.north));

    memset(&surface, 0, sizeof(surface));
    surface.desktop_count = 1u;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, &surface);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);
    found_single = keyboard_find(KEYBIND_DESKTOP_NORTH, &keysym, &modmask);
    TAP_OK(!found_single,
            "a desktop-cycling binding is absent with only one" \
            " desktop");

    surface.desktop_count = 2u;
    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);
    found_multi = keyboard_find(KEYBIND_DESKTOP_NORTH, &keysym, &modmask);
    TAP_OK(found_multi,
            "the same binding is present once a surface has more" \
            " than one desktop");

    list_destroy(surfaces);
}


/* A viewport-pan binding is skipped unless at least one surface's
 * configured viewport is wider or taller than a single screen */
static void s_test_load_viewport_bindings_need_viewport(void)
{
    config_td config;
    list_td *surfaces;
    surface_td surface;
    xcb_keysym_t keysym = XCB_NO_SYMBOL;
    uint16_t modmask = 0;
    bool found_1x1;
    bool found_wide;

    s_config_reset(&config);
    safe_strncpy(config.bindings.keyboard.viewport.pan.east, "ctrl+right",
            sizeof(config.bindings.keyboard.viewport.pan.east));

    memset(&surface, 0, sizeof(surface));
    surface.config = &config;
    surface.id = 0u;
    config.base.screens[0].viewport.columns = 1u;
    config.base.screens[0].viewport.rows = 1u;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, &surface);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);
    found_1x1 = keyboard_find(KEYBIND_VIEWPORT_PAN_EAST, &keysym, &modmask);
    TAP_OK(!found_1x1,
            "a viewport-pan binding is absent with a 1x1 viewport");

    config.base.screens[0].viewport.columns = 2u;
    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config);
    found_wide = keyboard_find(KEYBIND_VIEWPORT_PAN_EAST, &keysym, &modmask);
    TAP_OK(found_wide,
            "the same binding is present once a surface's viewport" \
            " has more than one column");

    list_destroy(surfaces);
}


/* keyboard_load replaces the whole table on a second call: reloading
 * with fewer bindings does not leave stale entries behind */
static void s_test_load_reload_replaces_table(void)
{
    config_td config_many;
    config_td config_one;
    list_td *surfaces;
    int count_many;
    int count_one;

    s_config_reset(&config_many);
    safe_strncpy(config_many.bindings.keyboard.wm.quit, "q",
            sizeof(config_many.bindings.keyboard.wm.quit));
    safe_strncpy(config_many.bindings.keyboard.wm.redraw, "r",
            sizeof(config_many.bindings.keyboard.wm.redraw));
    safe_strncpy(config_many.bindings.keyboard.wm.reload, "l",
            sizeof(config_many.bindings.keyboard.wm.reload));

    s_config_reset(&config_one);
    safe_strncpy(config_one.bindings.keyboard.wm.quit, "q",
            sizeof(config_one.bindings.keyboard.wm.quit));

    surfaces = list_init(NULL);

    s_reset();
    keyboard_load(surfaces, s_fake_keysyms, &config_many);
    count_many = keyboard_binding_count();

    keyboard_load(surfaces, s_fake_keysyms, &config_one);
    count_one = keyboard_binding_count();

    /* Both counts include the hardcoded, always-on 'Mod1+space'
     * window-menu binding on top of whichever configured ones each
     * config names */
    TAP_OK(count_many >= 4,
            "loading three bindings registers at least four entries" \
            " (plus the always-on window-menu binding)");
    TAP_EQ_INT(count_one, 2,
            "reloading with a single binding shrinks the table back" \
            " to exactly two entries (it plus the always-on" \
            " window-menu binding)");

    list_destroy(surfaces);
}


/* keyboard_is_modifier_for_mask matches only the modifier named by
 * the mask, and keyboard_keysym_is_modifier matches any of them */
static void s_test_modifier_keysym_checks(void)
{
    TAP_OK(keyboard_is_modifier_for_mask(0xffe1u, XCB_MOD_MASK_SHIFT),
            "Shift_L matches a mask naming XCB_MOD_MASK_SHIFT");
    TAP_OK(!keyboard_is_modifier_for_mask(0xffe1u, XCB_MOD_MASK_CONTROL),
            "Shift_L does not match a mask naming Control instead");
    TAP_OK(keyboard_is_modifier_for_mask(0xffe9u, XCB_MOD_MASK_1),
            "Alt_L matches a mask naming XCB_MOD_MASK_1");
    TAP_OK(keyboard_keysym_is_modifier(0xffebu),
            "Super_L is recognized as some modifier regardless of mask");
    TAP_OK(!keyboard_keysym_is_modifier((xcb_keysym_t) 'a'),
            "the plain letter 'a' is not any modifier at all");
}


/* keyboard_keysym_for_state asks column 0 with no modifiers held, and
 * returns XCB_NO_SYMBOL for a NULL keysyms table */
static void s_test_keysym_for_state_null_keysyms(void)
{
    xcb_keysym_t result;

    result = keyboard_keysym_for_state(NULL, 38, 0);
    TAP_EQ_INT((int) result, (int) XCB_NO_SYMBOL,
            "a NULL keysyms table answers XCB_NO_SYMBOL");
}


int main(void)
{
    xcb_connection_set(s_fake_connection);

    TAP_PLAN(31);

    s_test_initial_state_empty();
    s_test_find_null_outputs();
    s_test_load_empty_surfaces_registers_binding();
    s_test_load_unparseable_binding_registers_nothing();
    s_test_load_grabs_on_surfaces();
    s_test_load_skips_null_screen_surface();
    s_test_load_emergency_exit_disabled_by_default();
    s_test_load_emergency_exit_enabled_wins_collision();
    s_test_load_fortune_disabled_by_default();
    s_test_load_monitor_bindings_need_multi_monitor();
    s_test_load_desktop_bindings_need_multi_desktop();
    s_test_load_viewport_bindings_need_viewport();
    s_test_load_reload_replaces_table();
    s_test_modifier_keysym_checks();
    s_test_keysym_for_state_null_keysyms();

    xcb_connection_set(NULL);

    return TAP_DONE();
}
