/**
 * @file tests/input/kbd/test_bind.c
 *
 * @brief Test battery for keyboard binding lookup
 *
 * Covers 'keyboard_find', 'keyboard_find_action',
 * 'keyboard_is_modifier_for_mask', and the two accessors, entirely
 * through 'keyboard_test_add_binding' (see its own comment in
 * input/kbd/bind.h), never through 'keyboard_load' itself:
 * 'keyboard_load' parses config strings and installs real passive
 * grabs over an actual XCB connection, neither of which any of the
 * five functions above depend on for their own logic.
 *
 * 'xcb_key_symbols_get_keysym' is stubbed locally to simply return
 * its own keycode argument unchanged, rather than linking the real
 * 'libxcb-keysyms' (whose keycode-to-keysym tables need an actual X
 * server to build): 'keyboard_find_action' itself never inspects
 * this translation's own correctness, only compares whatever comes
 * back against the binding table, so a deterministic keycode-equals-
 * keysym stand-in exercises its own matching logic exactly as well
 * as a real translation would.
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
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Default initial values */
#include <defs/input.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <harness/tap.h>


/* Trivial, deterministic link-time stand-in; see this file's own top
 * comment for why */
xcb_keysym_t xcb_key_symbols_get_keysym(xcb_key_symbols_t *syms,
        xcb_keycode_t keycode, int col)
{
    (void) syms;
    (void) col;
    return (xcb_keysym_t) keycode;
}


/* Trivial link-time stand-ins for the raw XCB grab machinery
 * 'keyboard_load' alone would otherwise pull in: 'keyboard_load' is
 * never called anywhere in this file (see this file's own top
 * comment for why), so none of these ever actually run, and only
 * need to exist for the linker's own sake. */
xcb_void_cookie_t xcb_ungrab_key(xcb_connection_t *c, xcb_keycode_t key,
        xcb_window_t grab_window, uint16_t modifiers)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) key;
    (void) grab_window;
    (void) modifiers;
    return cookie;
}


xcb_keycode_t *xcb_key_symbols_get_keycode(xcb_key_symbols_t *syms,
        xcb_keysym_t keysym)
{
    (void) syms;
    (void) keysym;
    return NULL;
}


xcb_void_cookie_t xcb_grab_key_checked(xcb_connection_t *c,
        uint8_t owner_events, xcb_window_t grab_window,
        uint16_t modifiers, xcb_keycode_t key, uint8_t pointer_mode,
        uint8_t keyboard_mode)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) owner_events;
    (void) grab_window;
    (void) modifiers;
    (void) key;
    (void) pointer_mode;
    (void) keyboard_mode;
    return cookie;
}


xcb_generic_error_t *xcb_request_check(xcb_connection_t *c,
        xcb_void_cookie_t cookie)
{
    (void) c;
    (void) cookie;
    return NULL;
}


int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


#define TEST_KEYSYM_A ((xcb_keysym_t) 'a')
#define TEST_KEYSYM_B ((xcb_keysym_t) 'b')
#define TEST_MOD_1 ((uint16_t) XCB_MOD_MASK_1)
#define TEST_MOD_SHIFT ((uint16_t) XCB_MOD_MASK_SHIFT)


static void s_test_find_locates_a_registered_type(void)
{
    xcb_keysym_t keysym = XCB_NO_SYMBOL;
    uint16_t modmask = 0;
    bool found;

    keyboard_test_reset();
    keyboard_test_add_binding(KEYBIND_WM_QUIT, TEST_KEYSYM_A, TEST_MOD_1);

    found = keyboard_find(KEYBIND_WM_QUIT, &keysym, &modmask);

    TAP_OK(found, "a registered type is found");
    TAP_EQ_INT((int) keysym, (int) TEST_KEYSYM_A,
            "its own keysym is reported back");
    TAP_EQ_INT((int) modmask, (int) TEST_MOD_1,
            "its own modmask is reported back");

    keyboard_test_reset();
}


static void s_test_find_missing_type_fails(void)
{
    bool found;
    xcb_keysym_t keysym = (xcb_keysym_t) 999;
    uint16_t modmask = 999u;

    keyboard_test_reset();
    keyboard_test_add_binding(KEYBIND_WM_QUIT, TEST_KEYSYM_A, TEST_MOD_1);

    found = keyboard_find(KEYBIND_CLIENT_CLOSE, &keysym, &modmask);

    TAP_OK(!found, "a type that was never registered is not found");
    TAP_EQ_INT((int) keysym, (int) XCB_NO_SYMBOL,
            "keysym_out is reset to XCB_NO_SYMBOL regardless");
    TAP_EQ_INT((int) modmask, 0,
            "modmask_out is reset to 0 regardless");

    keyboard_test_reset();
}


static void s_test_find_returns_the_first_match(void)
{
    xcb_keysym_t keysym = XCB_NO_SYMBOL;
    uint16_t modmask = 0;

    keyboard_test_reset();
    keyboard_test_add_binding(KEYBIND_WM_QUIT, TEST_KEYSYM_A, TEST_MOD_1);
    keyboard_test_add_binding(KEYBIND_WM_QUIT, TEST_KEYSYM_B,
            TEST_MOD_SHIFT);

    (void) keyboard_find(KEYBIND_WM_QUIT, &keysym, &modmask);

    TAP_EQ_INT((int) keysym, (int) TEST_KEYSYM_A,
            "two entries sharing a type: the first one registered" \
            " wins, not the last");

    keyboard_test_reset();
}


static void s_test_find_action_matches_keysym_and_modmask(void)
{
    xcb_key_press_event_t event;
    xcb_key_symbols_t *dummy_syms = (xcb_key_symbols_t *) 1;
    enum wm_keybind_type_e type_out = KEYBIND_NONE;
    uint16_t raw_modmask_out = 0u;
    bool found;

    keyboard_test_reset();
    keyboard_test_add_binding(KEYBIND_CLIENT_CLOSE, TEST_KEYSYM_A,
            TEST_MOD_1);

    event.detail = (xcb_keycode_t) TEST_KEYSYM_A;
    event.state = TEST_MOD_1;

    found = keyboard_find_action(dummy_syms, &event, &type_out,
            &raw_modmask_out);

    TAP_OK(found, "a matching keysym and modmask resolves to an" \
            " action");
    TAP_EQ_INT((int) type_out, (int) KEYBIND_CLIENT_CLOSE,
            "the correct action type is reported");
    TAP_EQ_INT((int) raw_modmask_out, (int) TEST_MOD_1,
            "the binding's own raw modmask is reported, not the" \
            " event's own (identical here, but a different thing)");

    keyboard_test_reset();
}


static void s_test_find_action_ignores_caps_and_num_lock(void)
{
    xcb_key_press_event_t event;
    xcb_key_symbols_t *dummy_syms = (xcb_key_symbols_t *) 1;
    enum wm_keybind_type_e type_out = KEYBIND_NONE;
    uint16_t raw_modmask_out = 0u;
    bool found;

    keyboard_test_reset();
    keyboard_test_add_binding(KEYBIND_CLIENT_CLOSE, TEST_KEYSYM_A,
            TEST_MOD_1);

    event.detail = (xcb_keycode_t) TEST_KEYSYM_A;
    /* Caps Lock and Num Lock both held too, alongside the real
     * modifier the binding itself actually needs */
    event.state = (uint16_t) (TEST_MOD_1 | XCB_MOD_MASK_LOCK |
            XCB_MOD_MASK_2);

    found = keyboard_find_action(dummy_syms, &event, &type_out,
            &raw_modmask_out);

    TAP_OK(found,
            "still matches with Caps Lock and Num Lock also held," \
            " since INPUT_STRIP_LOCK_MASK strips both before" \
            " comparing");

    keyboard_test_reset();
}


static void s_test_find_action_wrong_modmask_fails(void)
{
    xcb_key_press_event_t event;
    xcb_key_symbols_t *dummy_syms = (xcb_key_symbols_t *) 1;
    enum wm_keybind_type_e type_out = KEYBIND_NONE;
    uint16_t raw_modmask_out = 0u;
    bool found;

    keyboard_test_reset();
    keyboard_test_add_binding(KEYBIND_CLIENT_CLOSE, TEST_KEYSYM_A,
            TEST_MOD_1);

    event.detail = (xcb_keycode_t) TEST_KEYSYM_A;
    event.state = TEST_MOD_SHIFT;

    found = keyboard_find_action(dummy_syms, &event, &type_out,
            &raw_modmask_out);

    TAP_OK(!found,
            "the same key with a different modmask does not match");

    keyboard_test_reset();
}


static void s_test_is_modifier_for_mask_detects_shift(void)
{
    TAP_OK(keyboard_is_modifier_for_mask(
                (xcb_keysym_t) 0xffe1 /* Shift_L */,
                (uint16_t) XCB_MOD_MASK_SHIFT),
            "Shift_L is a modifier under XCB_MOD_MASK_SHIFT");
    TAP_OK(!keyboard_is_modifier_for_mask(
                (xcb_keysym_t) 0xffe1 /* Shift_L */,
                (uint16_t) XCB_MOD_MASK_CONTROL),
            "but not under XCB_MOD_MASK_CONTROL");
}


static void s_test_is_modifier_for_mask_rejects_a_plain_key(void)
{
    TAP_OK(!keyboard_is_modifier_for_mask(TEST_KEYSYM_A,
                (uint16_t) (XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL |
                    XCB_MOD_MASK_1 | XCB_MOD_MASK_2 | XCB_MOD_MASK_4 |
                    XCB_MOD_MASK_5)),
            "an ordinary letter key is never a modifier, under any" \
            " mask");
}


static void s_test_binding_count_and_at(void)
{
    xcb_keysym_t keysym_out = XCB_NO_SYMBOL;
    uint16_t modmask_out = 0u;
    enum wm_keybind_type_e type;

    keyboard_test_reset();
    TAP_EQ_INT(keyboard_binding_count(), 0, "starts at 0");

    keyboard_test_add_binding(KEYBIND_WM_QUIT, TEST_KEYSYM_A,
            TEST_MOD_1);
    keyboard_test_add_binding(KEYBIND_CLIENT_CLOSE, TEST_KEYSYM_B,
            TEST_MOD_SHIFT);

    TAP_EQ_INT(keyboard_binding_count(), 2,
            "reflects both entries just added");

    type = keyboard_binding_at(1, &keysym_out, &modmask_out);
    TAP_EQ_INT((int) type, (int) KEYBIND_CLIENT_CLOSE,
            "index 1 is the second entry added");
    TAP_EQ_INT((int) keysym_out, (int) TEST_KEYSYM_B,
            "its own keysym matches what was added");

    type = keyboard_binding_at(99, &keysym_out, &modmask_out);
    TAP_EQ_INT((int) type, (int) KEYBIND_NONE,
            "an out-of-range index returns KEYBIND_NONE");
    TAP_EQ_INT((int) keysym_out, (int) XCB_NO_SYMBOL,
            "and resets keysym_out to XCB_NO_SYMBOL");

    keyboard_test_reset();
}


static void s_test_add_binding_beyond_capacity_fails(void)
{
    bool ok = true;

    keyboard_test_reset();

    for (int i = 0; i < WM_MAX_KEYBINDINGS && ok; ++i) {
        ok = keyboard_test_add_binding(KEYBIND_CLIENT_CLOSE,
                (xcb_keysym_t) i, TEST_MOD_1);
    }
    TAP_OK(ok, "filling every binding slot succeeds");
    TAP_OK(!keyboard_test_add_binding(KEYBIND_WM_QUIT,
                (xcb_keysym_t) 99999, TEST_MOD_1),
            "one more past the limit fails instead of overflowing");

    keyboard_test_reset();
}


int main(void)
{
    TAP_PLAN(23);

    s_test_find_locates_a_registered_type();
    s_test_find_missing_type_fails();
    s_test_find_returns_the_first_match();
    s_test_find_action_matches_keysym_and_modmask();
    s_test_find_action_ignores_caps_and_num_lock();
    s_test_find_action_wrong_modmask_fails();
    s_test_is_modifier_for_mask_detects_shift();
    s_test_is_modifier_for_mask_rejects_a_plain_key();
    s_test_binding_count_and_at();
    s_test_add_binding_beyond_capacity_fails();

    return TAP_DONE();
}
