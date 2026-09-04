/**
 * @file tests/input/mouse/test_bind.c
 *
 * @brief Test battery for mouse binding parsing, grab, and binding
 *        table
 *
 * 'xcb_connection_get' (utils/xcb/connection.c), and the passive-grab
 * protocol calls 'mouse_load' issues directly ('xcb_ungrab_button',
 * 'xcb_grab_button_checked', 'xcb_request_check'), all need a live X
 * connection to answer for real, so they are replaced by controlled
 * or recording stand-ins; libxcb is not linked, so these stand-ins are
 * the only definitions the linker finds.  'im_parse_modifier_token'
 * (input/modifier.c) is pure string/config logic with no X or
 * cross-module dependency of its own, so the real source file is
 * linked rather than stubbed, the same way 'input/mouse/resolve.c'
 * links the real bounds/geometry helpers it depends on.  'list.c' is
 * linked for real too, since 'mouse_load' walks a genuine 'list_td' of
 * surfaces built by each scenario below.
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

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>
#include <utils/safe/safestr.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/bind.h>


/** Non-null opaque handle standing in for a real xcb_connection_t */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
        (xcb_connection_t *) &s_fake_connection_storage;

/** Call counters, reset by s_reset before each scenario */
static int s_call_ungrab_button;
static int s_call_grab_button_checked;
static int s_call_request_check;
static int s_grab_button_calls_for_button1;


/**
 * @brief Reset every recording stand-in's state before a scenario
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_ungrab_button = 0;
    s_call_grab_button_checked = 0;
    s_call_request_check = 0;
    s_grab_button_calls_for_button1 = 0;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return s_fake_connection;
}


/**
 * @brief Recording stand-in for @a xcb_ungrab_button
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ungrab_button(xcb_connection_t *connection,
        uint8_t button, xcb_window_t grab_window, uint16_t modifiers)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) button;
    (void) grab_window;
    (void) modifiers;
    memset(&cookie, 0, sizeof(cookie));
    s_call_ungrab_button++;
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_grab_button_checked
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_grab_button_checked(xcb_connection_t *connection,
        uint8_t owner_events, xcb_window_t grab_window, uint16_t event_mask,
        uint8_t pointer_mode, uint8_t keyboard_mode, xcb_window_t confine_to,
        xcb_cursor_t cursor, uint8_t button, uint16_t modifiers)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) owner_events;
    (void) grab_window;
    (void) event_mask;
    (void) pointer_mode;
    (void) keyboard_mode;
    (void) confine_to;
    (void) cursor;
    (void) modifiers;
    memset(&cookie, 0, sizeof(cookie));
    s_call_grab_button_checked++;
    if (button == 1) {
        s_grab_button_calls_for_button1++;
    }

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_request_check
 *
 * Always reports no error, so 'mouse_load' never takes its
 * 'LOGGER_WARNING' + 'free(err)' failure path
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
 * @brief Build a config with every mouse binding string left empty
 *
 * @param config Configuration to zero out
 *
 * @note Complexity: @e O(1)
 */
static void s_config_reset(config_td *config)
{
    memset(config, 0, sizeof(*config));
}


/* Before mouse_load ever runs, the table is empty */
static void s_test_initial_state_empty(void)
{
    xcb_button_index_t button = 99;
    uint16_t modmask = 0xffffu;
    enum wm_mousebind_type_e type;

    /* mousebind_count/mousebind_at read the module's own static
     * table, last populated by whichever scenario ran most recently;
     * this only documents the out-of-range behavior, which holds
     * regardless of what a previous scenario loaded */
    type = mousebind_at(-1, &button, &modmask);
    TAP_EQ_INT((int) type, (int) MOUSEBIND_NONE,
            "a negative index reads back MOUSEBIND_NONE");
    TAP_EQ_INT((int) button, 0, "a negative index zeroes button_out too");
    TAP_EQ_INT(modmask, 0, "a negative index zeroes modmask_out too");
}


/* mouse_load with a NULL config is a no-op: no XCB call at all */
static void s_test_load_null_config(void)
{
    s_reset();

    mouse_load(NULL, NULL);

    TAP_EQ_INT(s_call_ungrab_button, 0,
            "a NULL config never reaches xcb_ungrab_button");
    TAP_EQ_INT(s_call_grab_button_checked, 0,
            "a NULL config never reaches xcb_grab_button_checked");
}


/* mouse_load with a real config but NULL surfaces still parses and
 * registers bindings in the table, just never grabs anything */
static void s_test_load_null_surfaces(void)
{
    config_td config;

    s_config_reset(&config);
    safe_strncpy(config.bindings.mod1, "mod1", sizeof(config.bindings.mod1));
    safe_strncpy(config.bindings.mouse.window.move, "mod1+button1",
            sizeof(config.bindings.mouse.window.move));

    s_reset();
    mouse_load(NULL, &config);

    TAP_EQ_INT(s_call_grab_button_checked, 0,
            "NULL surfaces never reaches xcb_grab_button_checked");
    TAP_OK(mousebind_count() >= 1,
            "the binding table is still populated with NULL surfaces");
}


/* An unparseable binding string is skipped, never added to the table */
static void s_test_load_unparseable_binding(void)
{
    config_td config;
    int count_after;

    s_config_reset(&config);
    /* Every field left as an empty string: window.move is unparseable
     * (s_parse_mouse_binding rejects a NULL/empty binding outright);
     * 'mouse_load' always resets its own table to empty before
     * parsing, so the only thing worth asserting here is that nothing
     * unparseable still makes it back in */

    s_reset();
    mouse_load(NULL, &config);
    count_after = mousebind_count();

    TAP_EQ_INT(count_after, 0,
            "every field left empty parses nothing into the table" \
            " at all");
}


/* A well-formed binding string round-trips through mousebind_at with
 * its own button, modmask, and type intact */
static void s_test_load_and_read_back_binding(void)
{
    config_td config;
    xcb_button_index_t button = 0;
    uint16_t modmask = 0;
    enum wm_mousebind_type_e type;
    bool found;

    s_config_reset(&config);
    safe_strncpy(config.bindings.mod1, "mod1", sizeof(config.bindings.mod1));
    safe_strncpy(config.bindings.mouse.window.move, "mod1+button1",
            sizeof(config.bindings.mouse.window.move));
    safe_strncpy(config.bindings.mouse.window.resize, "mod1+button3",
            sizeof(config.bindings.mouse.window.resize));
    safe_strncpy(config.bindings.mouse.window.lower, "mod1+button2",
            sizeof(config.bindings.mouse.window.lower));

    s_reset();
    mouse_load(NULL, &config);

    /* Scan the whole table for the MOVE entry rather than assuming a
     * fixed index, since 'defs' order in 'mouse_load' is an
     * implementation detail this test should not depend on */
    found = false;
    for (int i = 0; i < mousebind_count(); ++i) {
        type = mousebind_at(i, &button, &modmask);
        if (type == MOUSEBIND_MOVE) {
            found = true;
            break;
        }
    }

    TAP_OK(found, "a MOUSEBIND_MOVE entry is present after loading");
    TAP_EQ_INT((int) button, (int) XCB_BUTTON_INDEX_1,
            "the MOVE binding's button is button1");
    TAP_OK(modmask != 0, "the MOVE binding's modifier mask is non-zero");
}


/* mouse_load grabs each non-wheel binding's button, with every
 * lock-modifier variant (4), on every surface with a real screen */
static void s_test_load_grabs_on_surfaces(void)
{
    config_td config;
    list_td *surfaces;
    surface_td surface;
    xcb_screen_t screen;

    s_config_reset(&config);
    safe_strncpy(config.bindings.mod1, "mod1", sizeof(config.bindings.mod1));
    safe_strncpy(config.bindings.mouse.window.move, "mod1+button1",
            sizeof(config.bindings.mouse.window.move));

    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    surface.screen = &screen;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, &surface);

    s_reset();
    mouse_load(surfaces, &config);

    TAP_EQ_INT(s_call_ungrab_button, 1,
            "the surface's root is ungrabbed once before re-grabbing");
    /* 4 lock-modifier variants (none, Lock, Mod2, Lock|Mod2) for the
     * single MOVE binding registered above */
    TAP_EQ_INT(s_grab_button_calls_for_button1, 4,
            "button1 is grabbed once per lock-modifier variant (4)");
    TAP_EQ_INT(s_call_request_check, s_call_grab_button_checked,
            "every grab attempt is followed by its own request check");

    list_destroy(surfaces);
}


/* A desktop wheel binding (MOUSEBIND_DESKTOP_*) is registered in the
 * table but never passively grabbed on the root window */
static void s_test_load_skips_grab_for_wheel_bindings(void)
{
    config_td config;
    list_td *surfaces;
    surface_td surface;
    xcb_screen_t screen;
    xcb_button_index_t button = 0;
    uint16_t modmask = 0;
    enum wm_mousebind_type_e type;
    bool found;

    s_config_reset(&config);
    safe_strncpy(config.bindings.mod1, "mod1", sizeof(config.bindings.mod1));
    safe_strncpy(config.bindings.mouse.cycle.desktop.north, "mod1+button4",
            sizeof(config.bindings.mouse.cycle.desktop.north));

    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    surface.screen = &screen;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, &surface);

    s_reset();
    mouse_load(surfaces, &config);

    TAP_EQ_INT(s_call_grab_button_checked, 0,
            "a lone DESKTOP_NORTH wheel binding is never passively" \
            " grabbed");

    found = false;
    for (int i = 0; i < mousebind_count(); ++i) {
        type = mousebind_at(i, &button, &modmask);
        if (type == MOUSEBIND_DESKTOP_NORTH) {
            found = true;
            break;
        }
    }
    TAP_OK(found,
            "the DESKTOP_NORTH binding is still registered in the table");

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
    safe_strncpy(config.bindings.mod1, "mod1", sizeof(config.bindings.mod1));
    safe_strncpy(config.bindings.mouse.window.move, "mod1+button1",
            sizeof(config.bindings.mouse.window.move));

    memset(&surface, 0, sizeof(surface));
    surface.screen = NULL;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, &surface);

    s_reset();
    mouse_load(surfaces, &config);

    TAP_EQ_INT(s_call_ungrab_button, 0,
            "a NULL-screen surface is skipped by the ungrab pass");
    TAP_EQ_INT(s_call_grab_button_checked, 0,
            "a NULL-screen surface is skipped by the grab pass too");

    list_destroy(surfaces);
}


/* mouse_load replaces the whole table on a second call: reloading
 * with fewer bindings does not leave stale entries behind */
static void s_test_load_reload_replaces_table(void)
{
    config_td config_many;
    config_td config_one;
    int count_after_many;
    int count_after_one;

    s_config_reset(&config_many);
    safe_strncpy(config_many.bindings.mod1, "mod1",
            sizeof(config_many.bindings.mod1));
    safe_strncpy(config_many.bindings.mouse.window.move, "mod1+button1",
            sizeof(config_many.bindings.mouse.window.move));
    safe_strncpy(config_many.bindings.mouse.window.resize, "mod1+button3",
            sizeof(config_many.bindings.mouse.window.resize));
    safe_strncpy(config_many.bindings.mouse.window.lower, "mod1+button2",
            sizeof(config_many.bindings.mouse.window.lower));

    s_reset();
    mouse_load(NULL, &config_many);
    count_after_many = mousebind_count();

    s_config_reset(&config_one);
    safe_strncpy(config_one.bindings.mod1, "mod1",
            sizeof(config_one.bindings.mod1));
    safe_strncpy(config_one.bindings.mouse.window.move, "mod1+button1",
            sizeof(config_one.bindings.mouse.window.move));

    mouse_load(NULL, &config_one);
    count_after_one = mousebind_count();

    TAP_OK(count_after_many >= 3,
            "loading three bindings registers at least three entries");
    TAP_EQ_INT(count_after_one, 1,
            "reloading with a single binding shrinks the table back" \
            " to exactly one entry");
}


int main(void)
{
    TAP_PLAN(20);

    s_test_initial_state_empty();
    s_test_load_null_config();
    s_test_load_null_surfaces();
    s_test_load_unparseable_binding();
    s_test_load_and_read_back_binding();
    s_test_load_grabs_on_surfaces();
    s_test_load_skips_grab_for_wheel_bindings();
    s_test_load_skips_null_screen_surface();
    s_test_load_reload_replaces_table();

    return TAP_DONE();
}
