/**
 * @file tests/input/mouse/test_bind.c
 *
 * @brief Test battery for mouse binding accessors
 *
 * Covers 'mousebind_count' and 'mousebind_at' entirely through
 * 'mousebind_test_add_binding' (see its own comment in
 * input/mouse.h), never through 'mouse_load' itself: 'mouse_load'
 * parses config strings and installs real passive grabs over an
 * actual XCB connection, neither of which either accessor's own
 * logic depends on.
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

/* Default initial values */
#include <defs/input.h>

/* Local includes */
#include <input/mouse.h>
#include <harness/tap.h>


/* Trivial link-time stand-ins for the raw XCB grab machinery
 * 'mouse_load' alone would otherwise pull in: 'mouse_load' is never
 * called anywhere in this file, so none of these ever actually run,
 * and only need to exist for the linker's own sake. */
xcb_void_cookie_t xcb_grab_button(xcb_connection_t *c,
        uint8_t owner_events, xcb_window_t grab_window,
        uint16_t event_mask, uint8_t pointer_mode,
        uint8_t keyboard_mode, xcb_window_t confine_to,
        xcb_cursor_t cursor, uint8_t button, uint16_t modifiers)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) owner_events;
    (void) grab_window;
    (void) event_mask;
    (void) pointer_mode;
    (void) keyboard_mode;
    (void) confine_to;
    (void) cursor;
    (void) button;
    (void) modifiers;
    return cookie;
}


xcb_void_cookie_t xcb_ungrab_button(xcb_connection_t *c, uint8_t button,
        xcb_window_t grab_window, uint16_t modifiers)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) button;
    (void) grab_window;
    (void) modifiers;
    return cookie;
}


int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


static void s_test_count_and_at_reflect_added_bindings(void)
{
    xcb_button_index_t button_out = 0;
    uint16_t modmask_out = 0u;
    enum wm_mousebind_type_e type;

    mousebind_test_reset();
    TAP_EQ_INT(mousebind_count(), 0, "starts at 0");

    mousebind_test_add_binding(MOUSEBIND_MOVE,
            (xcb_button_index_t) 1, (uint16_t) XCB_MOD_MASK_1);
    mousebind_test_add_binding(MOUSEBIND_RESIZE,
            (xcb_button_index_t) 3, (uint16_t) XCB_MOD_MASK_1);

    TAP_EQ_INT(mousebind_count(), 2,
            "reflects both entries just added");

    type = mousebind_at(1, &button_out, &modmask_out);
    TAP_EQ_INT((int) type, (int) MOUSEBIND_RESIZE,
            "index 1 is the second entry added");
    TAP_EQ_INT((int) button_out, 3,
            "its own button matches what was added");
    TAP_EQ_INT((int) modmask_out, (int) XCB_MOD_MASK_1,
            "its own modmask matches what was added");

    mousebind_test_reset();
}


static void s_test_at_out_of_range_returns_none(void)
{
    xcb_button_index_t button_out = (xcb_button_index_t) 99;
    uint16_t modmask_out = 99u;
    enum wm_mousebind_type_e type;

    mousebind_test_reset();
    mousebind_test_add_binding(MOUSEBIND_LOWER,
            (xcb_button_index_t) 2, (uint16_t) XCB_MOD_MASK_1);

    type = mousebind_at(5, &button_out, &modmask_out);

    TAP_EQ_INT((int) type, (int) MOUSEBIND_NONE,
            "an out-of-range index returns MOUSEBIND_NONE");
    TAP_EQ_INT((int) button_out, 0,
            "and resets button_out to 0");
    TAP_EQ_INT((int) modmask_out, 0,
            "and resets modmask_out to 0");

    mousebind_test_reset();
}


static void s_test_at_negative_index_returns_none(void)
{
    enum wm_mousebind_type_e type;

    mousebind_test_reset();
    mousebind_test_add_binding(MOUSEBIND_LOWER,
            (xcb_button_index_t) 2, (uint16_t) XCB_MOD_MASK_1);

    type = mousebind_at(-1, NULL, NULL);

    TAP_EQ_INT((int) type, (int) MOUSEBIND_NONE,
            "a negative index also returns MOUSEBIND_NONE, and a" \
            " NULL button_out/modmask_out is accepted without" \
            " crashing");

    mousebind_test_reset();
}


static void s_test_add_binding_beyond_capacity_fails(void)
{
    bool ok = true;

    mousebind_test_reset();

    for (int i = 0; i < WM_MAX_MOUSEBINDINGS && ok; ++i) {
        ok = mousebind_test_add_binding(MOUSEBIND_MOVE,
                (xcb_button_index_t) 1, (uint16_t) XCB_MOD_MASK_1);
    }
    TAP_OK(ok, "filling every binding slot succeeds");
    TAP_OK(!mousebind_test_add_binding(MOUSEBIND_RESIZE,
                (xcb_button_index_t) 2, (uint16_t) XCB_MOD_MASK_1),
            "one more past the limit fails instead of overflowing");
    TAP_EQ_INT(mousebind_count(), WM_MAX_MOUSEBINDINGS,
            "count stays at the capacity, not one over it");

    mousebind_test_reset();
}


static void s_test_reset_clears_the_table(void)
{
    mousebind_test_add_binding(MOUSEBIND_MOVE,
            (xcb_button_index_t) 1, (uint16_t) XCB_MOD_MASK_1);
    TAP_OK(mousebind_count() > 0, "something registered before reset");

    mousebind_test_reset();

    TAP_EQ_INT(mousebind_count(), 0,
            "nothing registered immediately after reset");
}


int main(void)
{
    TAP_PLAN(14);

    s_test_count_and_at_reflect_added_bindings();
    s_test_at_out_of_range_returns_none();
    s_test_at_negative_index_returns_none();
    s_test_add_binding_beyond_capacity_fails();
    s_test_reset_clears_the_table();

    return TAP_DONE();
}
