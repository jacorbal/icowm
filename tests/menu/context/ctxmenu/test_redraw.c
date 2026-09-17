/**
 * @file tests/menu/context/ctxmenu/test_redraw.c
 *
 * @brief Test battery for context menu row painting
 *        (menu/context/ctxmenu/redraw.c)
 *
 * Every real line of drawing in this file, in the file-static
 * 's_draw_entry' and in the two public entry points alike, runs only
 * once 'xcb_connection_get' answers non-null: reaching it at all
 * needs a live X connection to allocate a graphics context, measure
 * a font, and draw real rectangles and glyphs, none of which this
 * file can do.  What is genuinely reachable without one is the guard
 * clause each public function opens with, which this file links the
 * real 'utils/xcb/connection.c' to exercise honestly rather than
 * asserting it by construction: 'xcb_connection_set' is never
 * called, so the getter answers null throughout, the same as before
 * 'wm_start' opens a connection, and both 'ctxmenu_redraw' and
 * 'ctxmenu_redraw_entries' take their early return on every call
 * here, verified by the complete absence of any drawing call (all of
 * them, from 'menu_draw_row_bg' down, are link-only stand-ins that
 * record whether they were ever reached).
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
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <client.h>
#include <config.h>
#include <harness/tap.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/layout.h>
#include <menu/context/ctxmenu/redraw.h>
#include <menu/draw.h>
#include <render/text.h>
#include <render/wmicon.h>
#include <stage.h>
#include <utils/xcb/connection.h>


/** Set by every drawing stand-in below when reached, so a test can
 *  assert none of them ever ran */
static int s_draw_calls;


/** Link-only stand-in for @a ctxmenu_entry_top_y
 *
 * Never reached: every call in this file takes the null-connection
 * guard clause before 's_draw_entry' can call this.
 * @note Complexity: @e O(1) */
int ctxmenu_entry_top_y(const ctxmenu_state_td *state, int idx)
{
    (void) state;
    (void) idx;
    s_draw_calls++;
    return 0;
}


/** Link-only stand-in for @a menu_draw_row_bg
 * @note Complexity: @e O(1) */
void menu_draw_row_bg(xcb_connection_t *connection, xcb_window_t window,
        uint32_t color, int16_t y, uint16_t height, uint16_t width)
{
    (void) connection;
    (void) window;
    (void) color;
    (void) y;
    (void) height;
    (void) width;
    s_draw_calls++;
}


/** Link-only stand-in for @a menu_draw_label
 * @note Complexity: @e O(1) */
void menu_draw_label(xcb_connection_t *connection, xcb_window_t window,
        struct position_s pos, const char *label)
{
    (void) connection;
    (void) window;
    (void) pos;
    (void) label;
    s_draw_calls++;
}


/** Link-only stand-in for @a menu_draw_measure
 * @note Complexity: @e O(1) */
uint16_t menu_draw_measure(const char *label)
{
    (void) label;
    s_draw_calls++;
    return 0;
}


/** Link-only stand-in for @a text_renderer_use_font
 * @note Complexity: @e O(1) */
int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    s_draw_calls++;
    return 0;
}


/** Link-only stand-in for @a text_renderer_set_color
 * @note Complexity: @e O(1) */
void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
    s_draw_calls++;
}


/** Link-only stand-in for @a wmicon_draw_at
 * @note Complexity: @e O(1) */
void wmicon_draw_at(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t icon_window,
        xcb_window_t target, struct position_s pos, uint16_t size,
        uint32_t fg, uint32_t bg, wmicon_cache_td *cache)
{
    (void) connection;
    (void) ewmh;
    (void) icon_window;
    (void) target;
    (void) pos;
    (void) size;
    (void) fg;
    (void) bg;
    (void) cache;
    s_draw_calls++;
}


/**
 * @brief Build a minimal but fully populated menu state, well formed
 *        enough that only the connection guard, not a null field
 *        elsewhere, could stop either public function
 */
static void s_make_state(ctxmenu_entry_td entries[2],
        ctxmenu_state_td *state, config_td *config)
{
    memset(entries, 0, 2 * sizeof(*entries));
    entries[0].type = CTXMENU_COMMAND;
    strcpy(entries[0].label, "Alpha");
    entries[1].type = CTXMENU_SEPARATOR;

    memset(config, 0, sizeof(*config));
    memset(state, 0, sizeof(*state));
    state->entries = entries;
    state->entry_count = 2;
    state->config = config;
    state->window = (xcb_window_t) 42;
    state->width = 100;
    state->height = 60;
    state->selected = -1;
}


/**
 * @brief Verify @a ctxmenu_redraw takes its guard clause and never
 *        reaches a single drawing call when there is no live
 *        connection
 */
static void s_test_redraw_no_connection(void)
{
    ctxmenu_entry_td entries[2];
    ctxmenu_state_td state;
    config_td config;

    s_draw_calls = 0;
    s_make_state(entries, &state, &config);

    ctxmenu_redraw(&state);

    TAP_EQ_INT(s_draw_calls, 0,
            "redrawing with no live connection draws nothing");
}


/**
 * @brief Verify @a ctxmenu_redraw is a no-op on a null state or a
 *        state with no window, same as with no connection
 */
static void s_test_redraw_guards(void)
{
    ctxmenu_entry_td entries[2];
    ctxmenu_state_td state;
    config_td config;

    s_draw_calls = 0;
    ctxmenu_redraw(NULL);
    TAP_EQ_INT(s_draw_calls, 0, "redrawing a null state draws nothing");

    s_make_state(entries, &state, &config);
    state.window = XCB_WINDOW_NONE;
    ctxmenu_redraw(&state);
    TAP_EQ_INT(s_draw_calls, 0,
            "redrawing a windowless state draws nothing");
}


/**
 * @brief Verify @a ctxmenu_redraw_entries takes its guard clause and
 *        never reaches a single drawing call when there is no live
 *        connection, for either one or two indices
 */
static void s_test_redraw_entries_no_connection(void)
{
    ctxmenu_entry_td entries[2];
    ctxmenu_state_td state;
    config_td config;

    s_draw_calls = 0;
    s_make_state(entries, &state, &config);

    ctxmenu_redraw_entries(&state, 0, -1);
    TAP_EQ_INT(s_draw_calls, 0,
            "redrawing one entry with no live connection draws"
            " nothing");

    ctxmenu_redraw_entries(&state, 0, 1);
    TAP_EQ_INT(s_draw_calls, 0,
            "redrawing two entries with no live connection draws"
            " nothing");
}


/**
 * @brief Verify @a ctxmenu_redraw_entries is a no-op on a null state
 *        or a state with no window
 */
static void s_test_redraw_entries_guards(void)
{
    ctxmenu_entry_td entries[2];
    ctxmenu_state_td state;
    config_td config;

    s_draw_calls = 0;
    ctxmenu_redraw_entries(NULL, 0, 1);
    TAP_EQ_INT(s_draw_calls, 0,
            "redrawing entries on a null state draws nothing");

    s_make_state(entries, &state, &config);
    state.window = XCB_WINDOW_NONE;
    ctxmenu_redraw_entries(&state, 0, 1);
    TAP_EQ_INT(s_draw_calls, 0,
            "redrawing entries on a windowless state draws nothing");
}


int main(void)
{
    TAP_PLAN(7);

    s_test_redraw_no_connection();
    s_test_redraw_guards();
    s_test_redraw_entries_no_connection();
    s_test_redraw_entries_guards();

    return TAP_DONE();
}
