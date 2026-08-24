/**
 * @file tests/input/mouse/test_resolve.c
 *
 * @brief Test battery for mouse binding resolution
 *
 * @a im_resolve_binding reads no X state, so the binding table it
 * walks is supplied here instead of by @c input/mouse/bind.c, which
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
#include <input/mouse/bind.h>
#include <input/mouse/internal.h>
#include <harness/tap.h>


/** How many bindings the table below can hold */
#define S_MAX_BINDINGS (8)

/** One entry of the binding table this battery resolves against */
typedef struct {
    xcb_button_index_t button;
    uint16_t modmask;
    enum wm_mousebind_type_e type;
} s_binding_td;

/** The binding table this battery resolves against */
static s_binding_td s_table[S_MAX_BINDINGS];

/** How many entries of @c s_table are in use */
static int s_table_count = 0;


/* Number of configured mouse bindings */
int mousebind_count(void)
{
    return s_table_count;
}


/* Access a mouse binding entry by index */
enum wm_mousebind_type_e mousebind_at(int idx,
        xcb_button_index_t *button_out, uint16_t *modmask_out)
{
    if (idx < 0 || idx >= s_table_count) {
        if (button_out != NULL) {
            *button_out = XCB_BUTTON_INDEX_ANY;
        }
        if (modmask_out != NULL) {
            *modmask_out = 0u;
        }
        return MOUSEBIND_NONE;
    }

    if (button_out != NULL) {
        *button_out = s_table[idx].button;
    }
    if (modmask_out != NULL) {
        *modmask_out = s_table[idx].modmask;
    }

    return s_table[idx].type;
}


/**
 * @brief Replace the table with a single binding
 *
 * @param button  Button to bind
 * @param modmask Modifiers the binding requires
 * @param type    Action the binding names
 */
static void s_bind_one(xcb_button_index_t button, uint16_t modmask,
        enum wm_mousebind_type_e type)
{
    s_table[0].button = button;
    s_table[0].modmask = modmask;
    s_table[0].type = type;
    s_table_count = 1;
}


/* An empty table resolves everything to nothing */
static void s_test_empty_table(void)
{
    s_table_count = 0;

    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1, 0u),
            MOUSEBIND_NONE,
            "an empty table resolves to MOUSEBIND_NONE");
}


/* An exact match on button and modifiers resolves */
static void s_test_exact_match(void)
{
    s_bind_one(XCB_BUTTON_INDEX_1, XCB_MOD_MASK_1, MOUSEBIND_MOVE);

    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1, XCB_MOD_MASK_1),
            MOUSEBIND_MOVE, "exact button and modifier match");
}


/* Another button under the same modifiers does not resolve */
static void s_test_other_button(void)
{
    s_bind_one(XCB_BUTTON_INDEX_1, XCB_MOD_MASK_1, MOUSEBIND_MOVE);

    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_3, XCB_MOD_MASK_1),
            MOUSEBIND_NONE, "another button does not match");
}


/*
 * A binding that names modifiers tolerates others held alongside
 *
 * This is where mouse resolution parts company with keyboard
 * resolution, which requires the mask to match exactly.
 */
static void s_test_extra_modifiers_tolerated(void)
{
    s_bind_one(XCB_BUTTON_INDEX_1, XCB_MOD_MASK_1, MOUSEBIND_MOVE);

    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1,
                (uint16_t) (XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT)),
            MOUSEBIND_MOVE,
            "a modifier held alongside the required one still matches");
    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1,
                XCB_MOD_MASK_SHIFT), MOUSEBIND_NONE,
            "the required modifier missing does not match");
}


/* A binding requiring no modifiers matches whatever is held */
static void s_test_no_modifier_binding(void)
{
    s_bind_one(XCB_BUTTON_INDEX_1, 0u, MOUSEBIND_LOWER);

    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1, 0u),
            MOUSEBIND_LOWER,
            "a modifier-less binding matches a bare press");
    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1, XCB_MOD_MASK_4),
            MOUSEBIND_LOWER,
            "a modifier-less binding matches a modified press too");
}


/* Caps Lock and Num Lock never take part in a match */
static void s_test_lock_modifiers_ignored(void)
{
    const uint16_t caps = XCB_MOD_MASK_LOCK;
    const uint16_t num = XCB_MOD_MASK_2;

    s_bind_one(XCB_BUTTON_INDEX_1, XCB_MOD_MASK_1, MOUSEBIND_MOVE);

    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1,
                (uint16_t) (XCB_MOD_MASK_1 | caps)), MOUSEBIND_MOVE,
            "Caps Lock is ignored while held");
    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1,
                (uint16_t) (XCB_MOD_MASK_1 | num)), MOUSEBIND_MOVE,
            "Num Lock is ignored while held");
}


/* An entry naming no action at all is skipped, not matched */
static void s_test_none_entry_skipped(void)
{
    s_table[0].button = XCB_BUTTON_INDEX_1;
    s_table[0].modmask = XCB_MOD_MASK_1;
    s_table[0].type = MOUSEBIND_NONE;
    s_table[1].button = XCB_BUTTON_INDEX_1;
    s_table[1].modmask = XCB_MOD_MASK_1;
    s_table[1].type = MOUSEBIND_RESIZE;
    s_table_count = 2;

    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_1, XCB_MOD_MASK_1),
            MOUSEBIND_RESIZE,
            "an entry naming no action is skipped over");
}


/* Resolution walks the whole table, not just its head */
static void s_test_matches_last_entry(void)
{
    for (int i = 0; i < S_MAX_BINDINGS; ++i) {
        s_table[i].button = XCB_BUTTON_INDEX_2;
        s_table[i].modmask = XCB_MOD_MASK_1;
        s_table[i].type = MOUSEBIND_MOVE;
    }
    s_table[S_MAX_BINDINGS - 1].button = XCB_BUTTON_INDEX_5;
    s_table[S_MAX_BINDINGS - 1].type = MOUSEBIND_DESKTOP_EAST;
    s_table_count = S_MAX_BINDINGS;

    TAP_EQ_INT(im_resolve_binding(XCB_BUTTON_INDEX_5, XCB_MOD_MASK_1),
            MOUSEBIND_DESKTOP_EAST, "the last entry is reached");
}


int main(void)
{
    TAP_PLAN(11);

    s_test_empty_table();
    s_test_exact_match();
    s_test_other_button();
    s_test_extra_modifiers_tolerated();
    s_test_no_modifier_binding();
    s_test_lock_modifiers_ignored();
    s_test_none_entry_skipped();
    s_test_matches_last_entry();

    return TAP_DONE();
}
