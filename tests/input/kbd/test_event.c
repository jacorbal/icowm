/**
 * @file tests/input/kbd/test_event.c
 *
 * @brief Test battery for key-press and key-release event dispatch
 *        (input/kbd/event.c)
 *
 * 'keyboard_handle_release' and 'keyboard_handle_press' are exercised
 * against the real, linked source file, including its own file-static
 * 's_lookup_surface_fallback' helper reached only through either
 * entry point.  Every function past that, 'cycle_is_open'/'cycle_
 * modifier'/'cycle_confirm' (the cycle menu), 'keyboard_is_modifier_
 * for_mask'/'keyboard_keysym_for_state' (input/kbd/bind.c's own
 * table-driven logic, already covered on its own by 'tests/input/
 * kbd/test_bind.c'), 'ik_intercept_keypress' (input/kbd/intercept.c,
 * already covered on its own by 'tests/input/kbd/test_intercept.c'),
 * 'ik_resolve_binding'/'ik_execute_binding' (input/kbd/bind.c's
 * table lookup and input/kbd/execute.c's huge dispatch switch,
 * covered elsewhere), 'lookup_surface_for_root', 'wm_emergency_exit_
 * enable', and 'logger_msg', is a cross-module or heavy dependency
 * this file has no business re-verifying, so each is a controlled or
 * recording stand-in here, letting every scenario below assert on
 * 'event.c's own dispatch order and guard logic directly, the same
 * "stub every heavy cross-module dependency" pattern 'tests/rules/
 * test_apply.c' documents.  'xcb_key_symbols_get_keysym' is a
 * controlled stand-in answering a fixed keysym a scenario registers
 * first, standing in for the real libxcb-keysyms lookup no live X
 * connection here could actually perform.
 *
 * @warning 'keyboard_handle_press' raises a genuine 'SIGTERM' via
 *          'raise' (a real libc call, not something belonging to
 *          this project and thus not a legitimate stand-in target)
 *          whenever 'config.base.shutdown.enable_emergency_shortcut'
 *          is true at the same time as a Backspace keysym, 'Control'
 *          in 'event->state', and 'Mod1' in 'event->state' all hold
 *          together.  No scenario below ever satisfies all four
 *          conditions simultaneously (the one enable_emergency_
 *          shortcut scenario that does hold the other three uses a
 *          different keysym instead), which leaves that one specific
 *          four-way intersection genuinely untested in this process:
 *          verifying it directly would terminate the test binary
 *          itself before any TAP output could report a result at
 *          all.  Every condition on its own, and every three-out-of-
 *          four combination, is independently exercised in scenarios
 *          below that assert the emergency path is NOT taken.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* strtok_r, needed transitively */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Menu includes */
#include <menu/cycle.h>

/* Default initial values */
#include <defs/kbd.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <harness/tap.h>
#include <input/kbd/bind.h>
#include <input/kbd/event.h>
#include <input/kbd/internal.h>
#include <utils/xcb/connection.h>


/** Non-null opaque handle standing in for a real xcb_key_symbols_t */
static int s_fake_keysyms_storage;
static xcb_key_symbols_t *const s_fake_keysyms =
        (xcb_key_symbols_t *) &s_fake_keysyms_storage;

/** Non-null opaque handle standing in for a real xcb_connection_t */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
        (xcb_connection_t *) &s_fake_connection_storage;

/** Fixed keysym @a xcb_key_symbols_get_keysym answers with,
 *  independent of whichever keycode/col it is asked about */
static xcb_keysym_t s_keysym_for_keycode;

/** cycle_is_open/cycle_modifier controllable return values */
static bool s_flag_cycle_open;
static uint16_t s_cycle_modifier_val;

/** keyboard_is_modifier_for_mask controllable return value */
static bool s_flag_is_modifier_for_mask;

/** lookup_surface_for_root controllable return value */
static surface_td *s_lookup_surface_result;

/** ik_intercept_keypress controllable return value */
static bool s_flag_intercept_consumes;

/** ik_resolve_binding controllable return value and out-param */
static enum wm_keybind_type_e s_resolve_binding_result;
static uint16_t s_resolve_binding_modmask;

/** Call counters, reset by s_reset before each scenario */
static int s_call_cycle_confirm;
static int s_call_wm_emergency_exit_enable;
static int s_call_ik_intercept_keypress;
static int s_call_ik_execute_binding;
static enum wm_keybind_type_e s_last_execute_btype;


static void s_reset(void)
{
    s_keysym_for_keycode = XCB_NO_SYMBOL;
    s_flag_cycle_open = false;
    s_cycle_modifier_val = 0;
    s_flag_is_modifier_for_mask = false;
    s_lookup_surface_result = NULL;
    s_flag_intercept_consumes = false;
    s_resolve_binding_result = KEYBIND_NONE;
    s_resolve_binding_modmask = 0;

    s_call_cycle_confirm = 0;
    s_call_wm_emergency_exit_enable = 0;
    s_call_ik_intercept_keypress = 0;
    s_call_ik_execute_binding = 0;
    s_last_execute_btype = KEYBIND_NONE;
}


/* Stand-ins */

xcb_keysym_t xcb_key_symbols_get_keysym(xcb_key_symbols_t *syms,
        xcb_keycode_t keycode, int col)
{
    (void) syms;
    (void) keycode;
    (void) col;
    return s_keysym_for_keycode;
}


bool cycle_is_open(void)
{
    return s_flag_cycle_open;
}


uint16_t cycle_modifier(void)
{
    return s_cycle_modifier_val;
}


void cycle_confirm(xcb_connection_t *connection, list_td *surfaces,
        const config_td *cfg)
{
    (void) connection;
    (void) surfaces;
    (void) cfg;
    s_call_cycle_confirm++;
}


bool keyboard_is_modifier_for_mask(xcb_keysym_t keysym, uint16_t mask)
{
    (void) keysym;
    (void) mask;
    return s_flag_is_modifier_for_mask;
}


xcb_keysym_t keyboard_keysym_for_state(xcb_key_symbols_t *keysyms,
        xcb_keycode_t keycode, uint16_t state)
{
    (void) keysyms;
    (void) keycode;
    (void) state;
    return s_keysym_for_keycode;
}


surface_td *lookup_surface_for_root(list_td *surfaces, xcb_window_t root)
{
    (void) surfaces;
    (void) root;
    return s_lookup_surface_result;
}


bool ik_intercept_keypress(xcb_keysym_t keysym, xcb_keysym_t typed_keysym,
        uint16_t state, surface_td *surface, list_td *surfaces,
        const config_td *config)
{
    (void) keysym;
    (void) typed_keysym;
    (void) state;
    (void) surface;
    (void) surfaces;
    (void) config;
    s_call_ik_intercept_keypress++;
    return s_flag_intercept_consumes;
}


enum wm_keybind_type_e ik_resolve_binding(xcb_keysym_t keysym,
        uint16_t state, uint16_t *out_modmask)
{
    (void) keysym;
    (void) state;
    if (out_modmask != NULL) {
        *out_modmask = s_resolve_binding_modmask;
    }
    return s_resolve_binding_result;
}


void ik_execute_binding(wm_td *wm, enum wm_keybind_type_e btype,
        uint16_t modmask, xcb_keycode_t keycode, surface_td *surface,
        list_td *surfaces, const config_td *config)
{
    (void) wm;
    (void) modmask;
    (void) keycode;
    (void) surface;
    (void) surfaces;
    (void) config;
    s_call_ik_execute_binding++;
    s_last_execute_btype = btype;
}


void wm_emergency_exit_enable(void)
{
    s_call_wm_emergency_exit_enable++;
}


/**
 * @brief Link-only stand-in for @a logger_msg
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


/* keyboard_handle_release */

static void s_test_release_null_args_are_noop(void)
{
    xcb_key_release_event_t event;

    s_reset();
    memset(&event, 0, sizeof(event));

    keyboard_handle_release(NULL, &event, NULL, NULL);
    keyboard_handle_release(s_fake_keysyms, NULL, NULL, NULL);

    TAP_EQ_INT(s_call_cycle_confirm, 0,
            "a null keysyms or event pointer never confirms the" \
            " cycle menu");
}


static void s_test_release_cycle_closed_is_noop(void)
{
    xcb_key_release_event_t event;

    s_reset();
    memset(&event, 0, sizeof(event));
    s_flag_cycle_open = false;

    keyboard_handle_release(s_fake_keysyms, &event, NULL, NULL);

    TAP_EQ_INT(s_call_cycle_confirm, 0,
            "keyboard_handle_release does nothing while the cycle" \
            " menu is closed");
}


static void s_test_release_cycle_modifier_zero_is_noop(void)
{
    xcb_key_release_event_t event;

    s_reset();
    memset(&event, 0, sizeof(event));
    s_flag_cycle_open = true;
    s_cycle_modifier_val = 0;
    s_flag_is_modifier_for_mask = true;

    keyboard_handle_release(s_fake_keysyms, &event, NULL, NULL);

    TAP_EQ_INT(s_call_cycle_confirm, 0,
            "a zero cycle_modifier never auto-confirms the cycle" \
            " menu, regardless of the released key");
}


static void s_test_release_wrong_key_is_noop(void)
{
    xcb_key_release_event_t event;

    s_reset();
    memset(&event, 0, sizeof(event));
    s_flag_cycle_open = true;
    s_cycle_modifier_val = XCB_MOD_MASK_1;
    s_flag_is_modifier_for_mask = false;

    keyboard_handle_release(s_fake_keysyms, &event, NULL, NULL);

    TAP_EQ_INT(s_call_cycle_confirm, 0,
            "releasing a key that is not the cycle modifier never" \
            " auto-confirms the cycle menu");
}


static void s_test_release_no_surface_is_noop(void)
{
    xcb_key_release_event_t event;

    s_reset();
    memset(&event, 0, sizeof(event));
    s_flag_cycle_open = true;
    s_cycle_modifier_val = XCB_MOD_MASK_1;
    s_flag_is_modifier_for_mask = true;
    s_lookup_surface_result = NULL;

    keyboard_handle_release(s_fake_keysyms, &event, NULL, NULL);

    TAP_EQ_INT(s_call_cycle_confirm, 0,
            "releasing the cycle modifier with no resolvable" \
            " surface never auto-confirms the cycle menu");
}


static void s_test_release_confirms_on_modifier_release(void)
{
    xcb_key_release_event_t event;
    surface_td fake_surface;

    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_flag_cycle_open = true;
    s_cycle_modifier_val = XCB_MOD_MASK_1;
    s_flag_is_modifier_for_mask = true;
    s_lookup_surface_result = &fake_surface;

    keyboard_handle_release(s_fake_keysyms, &event, NULL, NULL);

    TAP_EQ_INT(s_call_cycle_confirm, 1,
            "releasing the cycle modifier with a resolvable surface" \
            " auto-confirms the cycle menu exactly once");
}


/* keyboard_handle_press */

static void s_test_press_null_args_are_noop(void)
{
    xcb_key_press_event_t event;
    config_td cfg;

    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&cfg, 0, sizeof(cfg));

    keyboard_handle_press(NULL, NULL, &event, NULL, &cfg);
    keyboard_handle_press(NULL, s_fake_keysyms, NULL, NULL, &cfg);
    keyboard_handle_press(NULL, s_fake_keysyms, &event, NULL, NULL);

    TAP_EQ_INT(s_call_ik_intercept_keypress, 0,
            "a null keysyms, event, or config pointer never reaches" \
            " the intercept dispatcher");
}


static void s_test_press_intercepted_stops_dispatch(void)
{
    xcb_key_press_event_t event;
    config_td cfg;

    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&cfg, 0, sizeof(cfg));
    xcb_connection_set(s_fake_connection);
    s_flag_intercept_consumes = true;

    keyboard_handle_press(NULL, s_fake_keysyms, &event, NULL, &cfg);

    TAP_EQ_INT(s_call_ik_intercept_keypress, 1,
            "every press reaches the intercept dispatcher exactly" \
            " once");
    TAP_EQ_INT(s_call_ik_execute_binding, 0,
            "a press consumed by the intercept dispatcher never" \
            " reaches the binding table at all");
    xcb_connection_set(NULL);
}


static void s_test_press_emergency_exit_needs_all_four_conditions(void)
{
    xcb_key_press_event_t event;
    config_td cfg;

    /* Missing condition 1: shutdown.enable_emergency_shortcut off */
    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&cfg, 0, sizeof(cfg));
    cfg.base.shutdown.enable_emergency_shortcut = false;
    s_keysym_for_keycode = KS_BACKSPACE;
    event.state = (uint16_t) (XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1);
    keyboard_handle_press(NULL, s_fake_keysyms, &event, NULL, &cfg);
    TAP_EQ_INT(s_call_wm_emergency_exit_enable, 0,
            "the emergency exit never fires with the feature" \
            " disabled, even with Backspace+Ctrl+Mod1 all held");

    /* Missing condition 2: wrong keysym (feature enabled, both
     * modifiers held) */
    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&cfg, 0, sizeof(cfg));
    cfg.base.shutdown.enable_emergency_shortcut = true;
    s_keysym_for_keycode = KS_TAB;
    event.state = (uint16_t) (XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1);
    keyboard_handle_press(NULL, s_fake_keysyms, &event, NULL, &cfg);
    TAP_EQ_INT(s_call_wm_emergency_exit_enable, 0,
            "the emergency exit never fires for a key other than" \
            " Backspace, even with both modifiers held and the" \
            " feature enabled");

    /* Missing condition 3: Ctrl not held (feature enabled,
     * Backspace, Mod1 held) */
    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&cfg, 0, sizeof(cfg));
    cfg.base.shutdown.enable_emergency_shortcut = true;
    s_keysym_for_keycode = KS_BACKSPACE;
    event.state = (uint16_t) XCB_MOD_MASK_1;
    keyboard_handle_press(NULL, s_fake_keysyms, &event, NULL, &cfg);
    TAP_EQ_INT(s_call_wm_emergency_exit_enable, 0,
            "the emergency exit never fires without Ctrl held, even" \
            " with Backspace+Mod1 and the feature enabled");

    /* Missing condition 4: Mod1 not held (feature enabled,
     * Backspace, Ctrl held) */
    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&cfg, 0, sizeof(cfg));
    cfg.base.shutdown.enable_emergency_shortcut = true;
    s_keysym_for_keycode = KS_BACKSPACE;
    event.state = (uint16_t) XCB_MOD_MASK_CONTROL;
    keyboard_handle_press(NULL, s_fake_keysyms, &event, NULL, &cfg);
    TAP_EQ_INT(s_call_wm_emergency_exit_enable, 0,
            "the emergency exit never fires without Mod1 held, even" \
            " with Backspace+Ctrl and the feature enabled");
}


static void s_test_press_no_binding_matched_is_noop(void)
{
    xcb_key_press_event_t event;
    config_td cfg;

    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&cfg, 0, sizeof(cfg));
    s_keysym_for_keycode = (xcb_keysym_t) 'x';
    s_resolve_binding_result = KEYBIND_NONE;

    keyboard_handle_press(NULL, s_fake_keysyms, &event, NULL, &cfg);

    TAP_EQ_INT(s_call_ik_execute_binding, 0,
            "KEYBIND_NONE never reaches ik_execute_binding");
}


static void s_test_press_resolved_binding_is_executed(void)
{
    xcb_key_press_event_t event;
    config_td cfg;

    s_reset();
    memset(&event, 0, sizeof(event));
    memset(&cfg, 0, sizeof(cfg));
    s_keysym_for_keycode = (xcb_keysym_t) 'x';
    s_resolve_binding_result = KEYBIND_LAUNCH_TERMINAL;

    keyboard_handle_press(NULL, s_fake_keysyms, &event, NULL, &cfg);

    TAP_EQ_INT(s_call_ik_execute_binding, 1,
            "a resolved binding reaches ik_execute_binding exactly" \
            " once");
    TAP_OK(s_last_execute_btype == KEYBIND_LAUNCH_TERMINAL,
            "ik_execute_binding receives the exact binding type" \
            " ik_resolve_binding resolved");
}


int main(void)
{
    TAP_PLAN(16);

    s_test_release_null_args_are_noop();
    s_test_release_cycle_closed_is_noop();
    s_test_release_cycle_modifier_zero_is_noop();
    s_test_release_wrong_key_is_noop();
    s_test_release_no_surface_is_noop();
    s_test_release_confirms_on_modifier_release();

    s_test_press_null_args_are_noop();
    s_test_press_intercepted_stops_dispatch();
    s_test_press_emergency_exit_needs_all_four_conditions();
    s_test_press_no_binding_matched_is_noop();
    s_test_press_resolved_binding_is_executed();

    return TAP_DONE();
}
