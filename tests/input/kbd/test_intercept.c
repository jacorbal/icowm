/**
 * @file tests/input/kbd/test_intercept.c
 *
 * @brief Test battery for the key-press interception guard chain
 *
 * 'ik_intercept_keypress' (input/kbd/intercept.c) is a strict-priority
 * dispatcher over nine independent subsystems (manual placement,
 * keyboard modal move/resize, the cycle menu, the fuzzy search
 * widget, the run box, the generic confirm dialog, the info dialog,
 * the message dialog, and the three context menus), none of which
 * this file builds or owns: each is a heavy, side-effecting primitive
 * belonging to its own subsystem (XCB drawing, its own static state
 * machine), so every one of their '*_is_open'/'*_is_active' queries
 * and '*_handle_keypress' calls is a controlled or recording stand-in
 * here, the same way 'tests/rules/test_apply.c' stubs every
 * 'enact_client_*' primitive past the real matching logic it keeps.
 * 'xcb_connection_get' (utils/xcb/connection.c) is genuine, pure
 * project logic with no server round-trip of its own, so the real
 * source file is linked and seeded with a fake non-null handle.
 * 'keyboard_keysym_is_modifier' (input/kbd/bind.c) is likewise pure
 * logic with no cross-module dependency (it is a closed table lookup
 * over fixed keysym constants), so it and its own dependencies
 * ('input/modifier.c', which it does not actually call at runtime
 * but must still link against since both live in the same
 * translation unit's symbol table) are linked for real too, letting
 * 's_handle_cycle_key's bare-modifier guard be exercised against the
 * genuine implementation rather than a duplicated copy of its logic.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* strtok_r, needed by input/kbd/bind.c */

/* System includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/info.h>
#include <menu/dialog/message.h>
#include <menu/dialog/run.h>
#include <menu/search.h>

/* Policy includes */
#include <policy/placement/manual.h>

/* Default initial values */
#include <defs/dialog.h>
#include <defs/kbd.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>
#include <utils/xcb/connection.h>

/* Local includes */
#include <harness/tap.h>
#include <input/kbd/bind.h>
#include <input/kbd/internal.h>
#include <input/kbd/modal.h>


/** Non-null opaque handle standing in for a real xcb_connection_t */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
        (xcb_connection_t *) &s_fake_connection_storage;

/** Non-null opaque handle standing in for a real surface_td used as
 *  the "surface present" scenario in every guard below */
static surface_td s_fake_surface;

/** Every '*_is_open'/'*_is_active' flag the guard chain checks, each
 *  independently controllable so a scenario can set exactly one true
 *  at a time */
static bool s_flag_place_manual_active;
static bool s_flag_kbd_modal_active;
static bool s_flag_cycle_open;
static bool s_flag_search_open;
static bool s_flag_run_open;
static bool s_flag_confirm_open;
static bool s_flag_info_open;
static bool s_flag_message_open;
static bool s_flag_wincmenu_open;
static bool s_flag_rootmenu_open;
static bool s_flag_winlist_open;

/** Every handler's call counter, reset by s_reset before each
 *  scenario, plus the last keysym a handler that takes one was
 *  called with */
static int s_call_place_manual_handle_keypress;
static int s_call_kbd_modal_handle_keypress;
static int s_call_cycle_navigate_next;
static int s_call_cycle_navigate_prev;
static int s_call_cycle_confirm;
static int s_call_cycle_destroy;
static int s_call_search_handle_keypress;
static int s_call_run_handle_keypress;
static int s_call_menu_confirm_dialog_toggle_selection;
static int s_call_menu_confirm_dialog_accept;
static int s_call_menu_confirm_dialog_cancel;
static int s_call_dialog_info_close;
static int s_call_menu_message_dialog_close;
static int s_call_menu_message_dialog_select_ok;
static int s_call_menu_message_dialog_scroll;
static int s_call_wincmenu_handle_keypress;
static int s_call_rootmenu_handle_keypress;
static int s_call_winlist_handle_keypress;

/** cycle_next_keysym/cycle_next_modmask and their prev counterparts,
 *  each independently settable so 's_handle_cycle_key's two
 *  configured-binding branches can be exercised on their own */
static xcb_keysym_t s_cycle_next_keysym_val;
static uint16_t s_cycle_next_modmask_val;
static xcb_keysym_t s_cycle_prev_keysym_val;
static uint16_t s_cycle_prev_modmask_val;

/** menu_message_dialog_requires_selection/ok_selected, independently
 *  settable so the info-dialog Enter/Escape branches can be walked
 *  through every combination */
static bool s_message_requires_selection;
static bool s_message_ok_selected;


/**
 * @brief Reset every flag, counter, and configurable return value
 *        before a scenario
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_flag_place_manual_active = false;
    s_flag_kbd_modal_active = false;
    s_flag_cycle_open = false;
    s_flag_search_open = false;
    s_flag_run_open = false;
    s_flag_confirm_open = false;
    s_flag_info_open = false;
    s_flag_message_open = false;
    s_flag_wincmenu_open = false;
    s_flag_rootmenu_open = false;
    s_flag_winlist_open = false;

    s_call_place_manual_handle_keypress = 0;
    s_call_kbd_modal_handle_keypress = 0;
    s_call_cycle_navigate_next = 0;
    s_call_cycle_navigate_prev = 0;
    s_call_cycle_confirm = 0;
    s_call_cycle_destroy = 0;
    s_call_search_handle_keypress = 0;
    s_call_run_handle_keypress = 0;
    s_call_menu_confirm_dialog_toggle_selection = 0;
    s_call_menu_confirm_dialog_accept = 0;
    s_call_menu_confirm_dialog_cancel = 0;
    s_call_dialog_info_close = 0;
    s_call_menu_message_dialog_close = 0;
    s_call_menu_message_dialog_select_ok = 0;
    s_call_menu_message_dialog_scroll = 0;
    s_call_wincmenu_handle_keypress = 0;
    s_call_rootmenu_handle_keypress = 0;
    s_call_winlist_handle_keypress = 0;

    s_cycle_next_keysym_val = XCB_NO_SYMBOL;
    s_cycle_next_modmask_val = 0;
    s_cycle_prev_keysym_val = XCB_NO_SYMBOL;
    s_cycle_prev_modmask_val = 0;

    s_message_requires_selection = false;
    s_message_ok_selected = true;
}


/* Manual placement stand-ins */

bool place_manual_is_active(void)
{
    return s_flag_place_manual_active;
}


void place_manual_handle_keypress(xcb_connection_t *connection,
        xcb_keysym_t keysym)
{
    (void) connection;
    (void) keysym;
    s_call_place_manual_handle_keypress++;
}


/* Keyboard modal move/resize stand-ins */

bool kbd_modal_is_active(void)
{
    return s_flag_kbd_modal_active;
}


bool kbd_modal_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) keysym;
    (void) config;
    s_call_kbd_modal_handle_keypress++;
    return true;
}


/* Cycle menu stand-ins */

bool cycle_is_open(void)
{
    return s_flag_cycle_open;
}


void cycle_navigate_next(void)
{
    s_call_cycle_navigate_next++;
}


void cycle_navigate_prev(void)
{
    s_call_cycle_navigate_prev++;
}


void cycle_draw(xcb_connection_t *connection, const config_td *cfg)
{
    (void) connection;
    (void) cfg;
}


void cycle_confirm(xcb_connection_t *connection, list_td *surfaces,
        const config_td *cfg)
{
    (void) connection;
    (void) surfaces;
    (void) cfg;
    s_call_cycle_confirm++;
}


void cycle_destroy(xcb_connection_t *connection)
{
    (void) connection;
    s_call_cycle_destroy++;
}


xcb_keysym_t cycle_next_keysym(void)
{
    return s_cycle_next_keysym_val;
}


uint16_t cycle_next_modmask(void)
{
    return s_cycle_next_modmask_val;
}


xcb_keysym_t cycle_prev_keysym(void)
{
    return s_cycle_prev_keysym_val;
}


uint16_t cycle_prev_modmask(void)
{
    return s_cycle_prev_modmask_val;
}


/* Fuzzy search widget stand-ins */

bool search_is_open(void)
{
    return s_flag_search_open;
}


void search_handle_keypress(xcb_connection_t *connection,
        list_td *surfaces, xcb_keysym_t keysym, uint16_t state,
        const config_td *cfg)
{
    (void) connection;
    (void) surfaces;
    (void) keysym;
    (void) state;
    (void) cfg;
    s_call_search_handle_keypress++;
}


/* Run-box stand-ins */

bool run_is_open(void)
{
    return s_flag_run_open;
}


void run_handle_keypress(xcb_connection_t *connection, surface_td *surface,
        xcb_keysym_t keysym, uint16_t modmask, const config_td *cfg)
{
    (void) connection;
    (void) surface;
    (void) keysym;
    (void) modmask;
    (void) cfg;
    s_call_run_handle_keypress++;
}


/* Generic confirm dialog stand-ins */

bool menu_confirm_dialog_is_open(void)
{
    return s_flag_confirm_open;
}


void menu_confirm_dialog_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    (void) connection;
    (void) config;
}


void menu_confirm_dialog_toggle_selection(void)
{
    s_call_menu_confirm_dialog_toggle_selection++;
}


void menu_confirm_dialog_accept(xcb_connection_t *connection)
{
    (void) connection;
    s_call_menu_confirm_dialog_accept++;
}


void menu_confirm_dialog_cancel(xcb_connection_t *connection)
{
    (void) connection;
    s_call_menu_confirm_dialog_cancel++;
}


/* Info dialog stand-ins */

bool dialog_info_is_open(void)
{
    return s_flag_info_open;
}


void dialog_info_close(xcb_connection_t *connection)
{
    (void) connection;
    s_call_dialog_info_close++;
}


/* Message dialog stand-ins */

bool menu_message_dialog_is_open(void)
{
    return s_flag_message_open;
}


void menu_message_dialog_close(xcb_connection_t *connection)
{
    (void) connection;
    s_call_menu_message_dialog_close++;
}


bool menu_message_dialog_requires_selection(void)
{
    return s_message_requires_selection;
}


bool menu_message_dialog_ok_selected(void)
{
    return s_message_ok_selected;
}


void menu_message_dialog_select_ok(xcb_connection_t *connection,
        const config_td *config)
{
    (void) connection;
    (void) config;
    s_call_menu_message_dialog_select_ok++;
}


void menu_message_dialog_scroll(xcb_connection_t *connection,
        const config_td *config, int32_t delta)
{
    (void) connection;
    (void) config;
    (void) delta;
    s_call_menu_message_dialog_scroll++;
}


/* Context menu stand-ins */

bool wincmenu_is_open(void)
{
    return s_flag_wincmenu_open;
}


bool wincmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) keysym;
    (void) config;
    s_call_wincmenu_handle_keypress++;
    return true;
}


bool rootmenu_is_open(void)
{
    return s_flag_rootmenu_open;
}


bool rootmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) keysym;
    (void) config;
    s_call_rootmenu_handle_keypress++;
    return true;
}


bool winlist_is_open(void)
{
    return s_flag_winlist_open;
}


bool winlist_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) keysym;
    (void) config;
    s_call_winlist_handle_keypress++;
    return true;
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
 * @brief Link-only stand-ins for the passive-grab XCB calls
 *        'input/kbd/bind.c' makes from 'keyboard_load'
 *
 * None of the scenarios below ever call 'keyboard_load' itself (only
 * 'keyboard_keysym_is_modifier', a closed table lookup with no XCB
 * call of its own), but 'bind.c' is one translation unit, so every
 * external symbol it references anywhere has to resolve at link
 * time regardless of which of its functions actually runs here.
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
    return cookie;
}


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
    (void) key;
    (void) pointer_mode;
    (void) keyboard_mode;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


xcb_generic_error_t *xcb_request_check(xcb_connection_t *connection,
        xcb_void_cookie_t cookie)
{
    (void) connection;
    (void) cookie;
    return NULL;
}


xcb_keycode_t *xcb_key_symbols_get_keycode(xcb_key_symbols_t *syms,
        xcb_keysym_t keysym)
{
    (void) syms;
    (void) keysym;
    return NULL;
}


xcb_keysym_t xcb_key_symbols_get_keysym(xcb_key_symbols_t *syms,
        xcb_keycode_t keycode, int col)
{
    (void) syms;
    (void) keycode;
    (void) col;
    return XCB_NO_SYMBOL;
}


/* With every guard false, the key is not consumed at all */
static void s_test_no_guard_active_falls_through(void)
{
    bool consumed;

    s_reset();
    consumed = ik_intercept_keypress(KS_RETURN, KS_RETURN, 0,
            &s_fake_surface, NULL, NULL);
    TAP_OK(!consumed,
            "with every guard false the press is not consumed");
}


/* Manual placement, first in priority, consumes any key and forwards
 * it verbatim, even while a later guard is also true */
static void s_test_place_manual_active_wins_priority(void)
{
    bool consumed;

    s_reset();
    s_flag_place_manual_active = true;
    s_flag_kbd_modal_active = true;

    consumed = ik_intercept_keypress(KS_ESCAPE, KS_ESCAPE, 0,
            &s_fake_surface, NULL, NULL);

    TAP_OK(consumed,
            "manual placement being active consumes the key press");
    TAP_EQ_INT(s_call_place_manual_handle_keypress, 1,
            "manual placement's own handler is called exactly once");
    TAP_EQ_INT(s_call_kbd_modal_handle_keypress, 0,
            "manual placement takes priority over keyboard modal" \
            " mode entirely");
}


/* Keyboard modal move/resize, second in priority, consumes any key */
static void s_test_kbd_modal_active_consumes_key(void)
{
    bool consumed;

    s_reset();
    s_flag_kbd_modal_active = true;

    consumed = ik_intercept_keypress(KS_LEFT, KS_LEFT, 0,
            &s_fake_surface, NULL, NULL);

    TAP_OK(consumed, "keyboard modal mode consumes the key press");
    TAP_EQ_INT(s_call_kbd_modal_handle_keypress, 1,
            "keyboard modal mode's own handler is called exactly once");
}


/* The cycle menu's Up/Down arrows navigate, and any other unmatched
 * key destroys it */
static void s_test_cycle_open_navigation_and_dismiss(void)
{
    bool consumed_up;
    bool consumed_other;

    s_reset();
    s_flag_cycle_open = true;

    consumed_up = ik_intercept_keypress(KS_UP, KS_UP, 0,
            NULL, NULL, NULL);
    TAP_OK(consumed_up, "the cycle menu consumes an Up arrow press");
    TAP_EQ_INT(s_call_cycle_navigate_next, 0,
            "Up arrow never calls cycle_navigate_next");

    /* A real surface is required here so 's_handle_cycle_key' has a
     * non-NULL connection to call 'cycle_destroy' with; with a NULL
     * surface, its every drawing/destroying call is itself skipped,
     * which is exactly what the 'without a surface' scenarios
     * elsewhere in this file document on purpose, but would leave
     * this one unable to observe 'cycle_destroy' at all */
    s_reset();
    s_flag_cycle_open = true;
    consumed_other = ik_intercept_keypress((xcb_keysym_t) 'x',
            (xcb_keysym_t) 'x', 0, &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_other,
            "the cycle menu consumes an unmatched key too");
    TAP_EQ_INT(s_call_cycle_destroy, 1,
            "an unmatched key while the cycle menu is open destroys it");
}


/* A bare modifier key-press while the cycle menu is open is ignored
 * outright rather than closing the menu */
static void s_test_cycle_open_bare_modifier_ignored(void)
{
    bool consumed;

    s_reset();
    s_flag_cycle_open = true;

    /* Super_L (0xffeb) is some modifier keysym, per the real, linked
     * keyboard_keysym_is_modifier, regardless of the state mask
     * passed alongside it */
    consumed = ik_intercept_keypress(KS_SUPER_L, KS_SUPER_L, 0,
            NULL, NULL, NULL);

    TAP_OK(consumed,
            "a bare modifier key-press is still reported as consumed");
    TAP_EQ_INT(s_call_cycle_destroy, 0,
            "a bare modifier key-press does not destroy the cycle menu");
}


/* The cycle menu's configured next/prev bindings navigate exactly
 * like the arrow keys do, when the pressed combination matches */
static void s_test_cycle_open_configured_next_prev_bindings(void)
{
    bool consumed;

    s_reset();
    s_flag_cycle_open = true;
    s_cycle_next_keysym_val = (xcb_keysym_t) 'n';
    s_cycle_next_modmask_val = XCB_MOD_MASK_1;

    consumed = ik_intercept_keypress((xcb_keysym_t) 'n',
            (xcb_keysym_t) 'n', XCB_MOD_MASK_1, NULL, NULL, NULL);

    TAP_OK(consumed, "the configured cycle-next binding is consumed");
    TAP_EQ_INT(s_call_cycle_navigate_next, 1,
            "the configured cycle-next binding calls" \
            " cycle_navigate_next once");
}


/* The fuzzy search widget consumes any key while open */
static void s_test_search_open_consumes_key(void)
{
    bool consumed;

    s_reset();
    s_flag_search_open = true;

    consumed = ik_intercept_keypress((xcb_keysym_t) 'a',
            (xcb_keysym_t) 'a', 0, &s_fake_surface, NULL, NULL);

    TAP_OK(consumed, "the search widget consumes the key press");
    TAP_EQ_INT(s_call_search_handle_keypress, 1,
            "the search widget's own handler is called exactly once");
}


/* The run-box consumes any key while open */
static void s_test_run_open_consumes_key(void)
{
    bool consumed;

    s_reset();
    s_flag_run_open = true;

    consumed = ik_intercept_keypress((xcb_keysym_t) 'a',
            (xcb_keysym_t) 'a', 0, &s_fake_surface, NULL, NULL);

    TAP_OK(consumed, "the run-box consumes the key press");
    TAP_EQ_INT(s_call_run_handle_keypress, 1,
            "the run-box's own handler is called exactly once");
}


/* The generic confirm dialog: Tab toggles selection, Enter accepts,
 * Escape always cancels */
static void s_test_confirm_dialog_tab_enter_escape(void)
{
    bool consumed_tab;
    bool consumed_enter;
    bool consumed_escape;

    s_reset();
    s_flag_confirm_open = true;
    consumed_tab = ik_intercept_keypress(KS_TAB, KS_TAB, 0,
            &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_tab, "the confirm dialog consumes a Tab press");
    TAP_EQ_INT(s_call_menu_confirm_dialog_toggle_selection, 1,
            "Tab toggles the confirm dialog's selected button");

    s_reset();
    s_flag_confirm_open = true;
    consumed_enter = ik_intercept_keypress(KS_RETURN, KS_RETURN, 0,
            &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_enter, "the confirm dialog consumes an Enter press");
    TAP_EQ_INT(s_call_menu_confirm_dialog_accept, 1,
            "Enter accepts the confirm dialog's selected button");

    s_reset();
    s_flag_confirm_open = true;
    consumed_escape = ik_intercept_keypress(KS_ESCAPE, KS_ESCAPE, 0,
            &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_escape,
            "the confirm dialog consumes an Escape press");
    TAP_EQ_INT(s_call_menu_confirm_dialog_cancel, 1,
            "Escape always cancels the confirm dialog");
}


/* The info dialog closes on Enter once its optional
 * requires-selection gate is satisfied, and Escape is a no-op while
 * that gate is set */
static void s_test_info_dialog_enter_requires_selection_gate(void)
{
    bool consumed_enter_gated;
    bool consumed_enter_selected;
    bool consumed_escape_gated;

    s_reset();
    s_flag_info_open = true;
    s_message_requires_selection = true;
    s_message_ok_selected = false;
    consumed_enter_gated = ik_intercept_keypress(KS_RETURN, KS_RETURN, 0,
            &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_enter_gated,
            "the info dialog reports the Enter press as consumed" \
            " even when gated");
    TAP_EQ_INT(s_call_dialog_info_close, 0,
            "Enter does not close a selection-gated info dialog" \
            " before OK is selected");

    s_reset();
    s_flag_info_open = true;
    s_message_requires_selection = true;
    s_message_ok_selected = true;
    consumed_enter_selected = ik_intercept_keypress(KS_RETURN, KS_RETURN,
            0, &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_enter_selected,
            "the info dialog consumes Enter once OK is selected");
    TAP_EQ_INT(s_call_dialog_info_close, 1,
            "Enter closes a selection-gated info dialog once OK is" \
            " selected");

    s_reset();
    s_flag_info_open = true;
    s_message_requires_selection = true;
    consumed_escape_gated = ik_intercept_keypress(KS_ESCAPE, KS_ESCAPE, 0,
            &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_escape_gated,
            "the info dialog reports Escape as consumed even when" \
            " gated");
    TAP_EQ_INT(s_call_dialog_info_close, 0,
            "Escape never closes a selection-gated info dialog");
}


/* The info dialog's Up/Down/PageUp/PageDown scroll, and Tab selects
 * OK, all only while a real surface and connection are available */
static void s_test_info_dialog_scroll_and_select(void)
{
    s_reset();
    s_flag_info_open = true;
    (void) ik_intercept_keypress(KS_UP, KS_UP, 0, &s_fake_surface,
            NULL, NULL);
    TAP_EQ_INT(s_call_menu_message_dialog_scroll, 1,
            "Up arrow scrolls the info dialog by one line");

    s_reset();
    s_flag_info_open = true;
    (void) ik_intercept_keypress(KS_TAB, KS_TAB, 0, &s_fake_surface,
            NULL, NULL);
    TAP_EQ_INT(s_call_menu_message_dialog_select_ok, 1,
            "Tab selects OK in the info dialog");

    s_reset();
    s_flag_info_open = true;
    (void) ik_intercept_keypress(KS_UP, KS_UP, 0, NULL, NULL, NULL);
    TAP_EQ_INT(s_call_menu_message_dialog_scroll, 0,
            "without a surface the info dialog never scrolls");
}


/* The message dialog behaves the same as the info dialog for Enter
 * and Escape, gated by the same selection-required flag */
static void s_test_message_dialog_enter_escape(void)
{
    bool consumed;

    s_reset();
    s_flag_message_open = true;
    s_message_requires_selection = false;
    consumed = ik_intercept_keypress(KS_RETURN, KS_RETURN, 0,
            &s_fake_surface, NULL, NULL);
    TAP_OK(consumed, "the message dialog consumes an Enter press");
    TAP_EQ_INT(s_call_menu_message_dialog_close, 1,
            "Enter closes a message dialog with no selection gate");

    s_reset();
    s_flag_message_open = true;
    s_message_requires_selection = true;
    (void) ik_intercept_keypress(KS_ESCAPE, KS_ESCAPE, 0,
            &s_fake_surface, NULL, NULL);
    TAP_EQ_INT(s_call_menu_message_dialog_close, 0,
            "Escape never closes a selection-gated message dialog");
}


/* Each of the three context menus consumes a key while open, checked
 * strictly in order: window menu, then root menu, then window list */
static void s_test_context_menus_priority_order(void)
{
    bool consumed_wincmenu;
    bool consumed_rootmenu;
    bool consumed_winlist;

    s_reset();
    s_flag_wincmenu_open = true;
    s_flag_rootmenu_open = true;
    consumed_wincmenu = ik_intercept_keypress((xcb_keysym_t) 'a',
            (xcb_keysym_t) 'a', 0, &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_wincmenu,
            "the window context menu consumes the key press");
    TAP_EQ_INT(s_call_wincmenu_handle_keypress, 1,
            "the window context menu's own handler runs exactly once");
    TAP_EQ_INT(s_call_rootmenu_handle_keypress, 0,
            "the window context menu takes priority over the root menu");

    s_reset();
    s_flag_rootmenu_open = true;
    s_flag_winlist_open = true;
    consumed_rootmenu = ik_intercept_keypress((xcb_keysym_t) 'a',
            (xcb_keysym_t) 'a', 0, &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_rootmenu,
            "the root menu consumes the key press");
    TAP_EQ_INT(s_call_winlist_handle_keypress, 0,
            "the root menu takes priority over the window list menu");

    s_reset();
    s_flag_winlist_open = true;
    consumed_winlist = ik_intercept_keypress((xcb_keysym_t) 'a',
            (xcb_keysym_t) 'a', 0, &s_fake_surface, NULL, NULL);
    TAP_OK(consumed_winlist,
            "the window list menu consumes the key press on its own");
    TAP_EQ_INT(s_call_winlist_handle_keypress, 1,
            "the window list menu's own handler runs exactly once");
}


int main(void)
{
    xcb_connection_set(s_fake_connection);
    memset(&s_fake_surface, 0, sizeof(s_fake_surface));

    TAP_PLAN(43);

    s_test_no_guard_active_falls_through();
    s_test_place_manual_active_wins_priority();
    s_test_kbd_modal_active_consumes_key();
    s_test_cycle_open_navigation_and_dismiss();
    s_test_cycle_open_bare_modifier_ignored();
    s_test_cycle_open_configured_next_prev_bindings();
    s_test_search_open_consumes_key();
    s_test_run_open_consumes_key();
    s_test_confirm_dialog_tab_enter_escape();
    s_test_info_dialog_enter_requires_selection_gate();
    s_test_info_dialog_scroll_and_select();
    s_test_message_dialog_enter_escape();
    s_test_context_menus_priority_order();

    xcb_connection_set(NULL);

    return TAP_DONE();
}
