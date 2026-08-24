/**
 * @file tests/input/kbd/test_resolve.c
 *
 * @brief Test battery for keyboard binding resolution
 *
 * @a ik_resolve_binding reads no X state, so the binding table it
 * walks is supplied here instead of by @c input/kbd/bind.c, which
 * only ever fills its own from a live connection.  The two entry
 * points it needs are defined below, which is what lets the whole
 * resolution be exercised without a server.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>
#include <stdint.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <input/kbd/internal.h>
#include <harness/tap.h>


/** How many bindings the table below can hold */
#define S_MAX_BINDINGS (8)

/** The binding table this battery resolves against */
static wm_keybinding_td s_table[S_MAX_BINDINGS];

/** How many entries of @c s_table are in use */
static int s_table_count = 0;


/* Number of configured key bindings */
int keyboard_binding_count(void)
{
    return s_table_count;
}


/* Access a binding entry by index */
enum wm_keybind_type_e keyboard_binding_at(int idx,
        xcb_keysym_t *keysym_out, uint16_t *modmask_out)
{
    if (idx < 0 || idx >= s_table_count) {
        if (keysym_out != NULL) {
            *keysym_out = XCB_NO_SYMBOL;
        }
        if (modmask_out != NULL) {
            *modmask_out = 0u;
        }
        return KEYBIND_NONE;
    }

    if (keysym_out != NULL) {
        *keysym_out = s_table[idx].keysym;
    }
    if (modmask_out != NULL) {
        *modmask_out = s_table[idx].modmask;
    }

    return s_table[idx].type;
}


/**
 * @brief Replace the table with a single binding
 *
 * @param keysym  Key symbol to bind
 * @param modmask Modifiers the binding requires
 * @param type    Action the binding names
 */
static void s_bind_one(xcb_keysym_t keysym, uint16_t modmask,
        enum wm_keybind_type_e type)
{
    s_table[0].keysym = keysym;
    s_table[0].modmask = modmask;
    s_table[0].type = type;
    s_table_count = 1;
}


/* An empty table resolves everything to nothing */
static void s_test_empty_table(void)
{
    s_table_count = 0;

    TAP_EQ_INT(ik_resolve_binding(0x61u, 0u, NULL), KEYBIND_NONE,
            "an empty table resolves to KEYBIND_NONE");
}


/* An exact match on keysym and modifiers resolves */
static void s_test_exact_match(void)
{
    uint16_t modmask = 0u;

    s_bind_one(0x61u, XCB_MOD_MASK_4, KEYBIND_WM_ROOT_MENU);

    TAP_EQ_INT(ik_resolve_binding(0x61u, XCB_MOD_MASK_4, &modmask),
            KEYBIND_WM_ROOT_MENU, "exact keysym and modifier match");
    TAP_EQ_INT(modmask, XCB_MOD_MASK_4,
            "the matched binding's own modifier mask comes back");
}


/* A different keysym under the same modifiers does not resolve */
static void s_test_other_keysym(void)
{
    s_bind_one(0x61u, XCB_MOD_MASK_4, KEYBIND_WM_ROOT_MENU);

    TAP_EQ_INT(ik_resolve_binding(0x62u, XCB_MOD_MASK_4, NULL),
            KEYBIND_NONE, "another keysym does not match");
}


/* The same keysym under different modifiers does not resolve */
static void s_test_other_modifiers(void)
{
    s_bind_one(0x61u, XCB_MOD_MASK_4, KEYBIND_WM_ROOT_MENU);

    TAP_EQ_INT(ik_resolve_binding(0x61u, XCB_MOD_MASK_1, NULL),
            KEYBIND_NONE, "another modifier does not match");
    TAP_EQ_INT(ik_resolve_binding(0x61u, 0u, NULL), KEYBIND_NONE,
            "no modifier at all does not match either");
}


/* Caps Lock and Num Lock never take part in a match */
static void s_test_lock_modifiers_ignored(void)
{
    const uint16_t caps = XCB_MOD_MASK_LOCK;
    const uint16_t num = XCB_MOD_MASK_2;

    s_bind_one(0x61u, XCB_MOD_MASK_4, KEYBIND_WM_ROOT_MENU);

    TAP_EQ_INT(ik_resolve_binding(0x61u,
                (uint16_t) (XCB_MOD_MASK_4 | caps), NULL),
            KEYBIND_WM_ROOT_MENU, "Caps Lock is ignored while held");
    TAP_EQ_INT(ik_resolve_binding(0x61u,
                (uint16_t) (XCB_MOD_MASK_4 | num), NULL),
            KEYBIND_WM_ROOT_MENU, "Num Lock is ignored while held");
    TAP_EQ_INT(ik_resolve_binding(0x61u,
                (uint16_t) (XCB_MOD_MASK_4 | caps | num), NULL),
            KEYBIND_WM_ROOT_MENU, "both locks together are ignored");

    /* And on the configured side too, not just the reported one */
    s_bind_one(0x61u, (uint16_t) (XCB_MOD_MASK_4 | caps),
            KEYBIND_WM_ROOT_MENU);
    TAP_EQ_INT(ik_resolve_binding(0x61u, XCB_MOD_MASK_4, NULL),
            KEYBIND_WM_ROOT_MENU,
            "a lock bit configured into a binding is ignored too");
}


/* A binding requiring no modifiers matches a bare press only */
static void s_test_no_modifier_binding(void)
{
    s_bind_one(0x61u, 0u, KEYBIND_WM_REDRAW);

    TAP_EQ_INT(ik_resolve_binding(0x61u, 0u, NULL), KEYBIND_WM_REDRAW,
            "a modifier-less binding matches a bare press");
    TAP_EQ_INT(ik_resolve_binding(0x61u, XCB_MOD_MASK_4, NULL),
            KEYBIND_NONE,
            "a modifier-less binding rejects a modified press");
}


/* The first matching entry wins, and a later duplicate is never
 * reached */
static void s_test_first_match_wins(void)
{
    uint16_t modmask = 0u;

    s_table[0].keysym = 0x61u;
    s_table[0].modmask = XCB_MOD_MASK_4;
    s_table[0].type = KEYBIND_WM_ROOT_MENU;
    s_table[1].keysym = 0x61u;
    s_table[1].modmask = XCB_MOD_MASK_4;
    s_table[1].type = KEYBIND_WM_QUIT;
    s_table_count = 2;

    TAP_EQ_INT(ik_resolve_binding(0x61u, XCB_MOD_MASK_4, &modmask),
            KEYBIND_WM_ROOT_MENU,
            "the first of two identical bindings wins");
}


/* Resolution walks the whole table, not just its head */
static void s_test_matches_last_entry(void)
{
    for (int i = 0; i < S_MAX_BINDINGS; ++i) {
        s_table[i].keysym = (xcb_keysym_t) (0x70u + i);
        s_table[i].modmask = XCB_MOD_MASK_1;
        s_table[i].type = KEYBIND_WM_REDRAW;
    }
    s_table[S_MAX_BINDINGS - 1].type = KEYBIND_WM_QUIT;
    s_table_count = S_MAX_BINDINGS;

    TAP_EQ_INT(ik_resolve_binding(
                (xcb_keysym_t) (0x70u + S_MAX_BINDINGS - 1),
                XCB_MOD_MASK_1, NULL),
            KEYBIND_WM_QUIT, "the last entry is reached");
}


/* A null output pointer is accepted, and left alone on a miss */
static void s_test_null_output(void)
{
    uint16_t modmask = 0xBEEFu;

    s_bind_one(0x61u, XCB_MOD_MASK_4, KEYBIND_WM_ROOT_MENU);

    TAP_EQ_INT(ik_resolve_binding(0x61u, XCB_MOD_MASK_4, NULL),
            KEYBIND_WM_ROOT_MENU, "a null modmask pointer is fine");
    TAP_EQ_INT(ik_resolve_binding(0x62u, XCB_MOD_MASK_4, &modmask),
            KEYBIND_NONE, "a miss resolves to KEYBIND_NONE");
    TAP_EQ_INT(modmask, 0xBEEF,
            "a miss leaves the caller's own modmask untouched");
}


int main(void)
{
    TAP_PLAN(17);

    s_test_empty_table();
    s_test_exact_match();
    s_test_other_keysym();
    s_test_other_modifiers();
    s_test_lock_modifiers_ignored();
    s_test_no_modifier_binding();
    s_test_first_match_wins();
    s_test_matches_last_entry();
    s_test_null_output();

    return TAP_DONE();
}
