/**
 * @file tests/menu/context/ctxmenu/test_layout.c
 *
 * @brief Test battery for context menu row geometry and hit-testing
 *
 * ctxmenu_entry_top_y and ctxmenu_entry_at_y each have two code
 * paths: an O(1)/O(log n) one using state->entry_top_y's own cache
 * (as ctxmenu_show would allocate and ctxmenu_layout_build fill it),
 * and an O(n) fallback walking state->entries directly for when that
 * allocation failed.  Every test below runs both paths against the
 * same entries and checks they agree, since that is exactly the
 * guarantee a caller falling back silently relies on.
 *
 * ctxmenu_width_compute is not covered here: unlike the other three
 * public functions in this file, it needs a real XCB connection and
 * font metrics (through text_renderer_init/menu_draw_measure), not
 * just state->entries/state->config.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/context/ctxmenu/layout.h>


/** Link-only stand-ins: only ctxmenu_width_compute (not covered by
 *  this file) ever calls either of these */
uint16_t menu_draw_measure(const char *text)
{
    (void) text;
    return 0u;
}

int text_renderer_init(xcb_connection_t *connection, const char *font)
{
    (void) connection;
    (void) font;
    return 0;
}


/* Five entries: three CTXMENU_COMMAND rows (20px each), one
 * CTXMENU_SEPARATOR (8px), then one more CTXMENU_COMMAND (20px);
 * padding.vertical = 4.  With the cache: top_y = [4, 24, 44, 64, 72],
 * total height = 4 + 20+20+20+8+20 + 4 = 96 */
static void s_make_state(ctxmenu_state_td *state,
        ctxmenu_entry_td *entries, config_td *config,
        int32_t *cache_storage, bool with_cache)
{
    memset(state, 0, sizeof(*state));
    memset(entries, 0, 5 * sizeof(*entries));
    memset(config, 0, sizeof(*config));

    entries[0].type = CTXMENU_COMMAND;
    entries[1].type = CTXMENU_COMMAND;
    entries[2].type = CTXMENU_COMMAND;
    entries[3].type = CTXMENU_SEPARATOR;
    entries[4].type = CTXMENU_COMMAND;

    config->theme.menu.padding.vertical = 4u;

    state->entries = entries;
    state->entry_count = 5;
    state->config = config;
    state->entry_top_y = with_cache ? cache_storage : NULL;
}


/* ctxmenu_layout_build fills the cache with the correct per-row top-Y
 * offset for a mix of regular and separator rows, and returns the
 * correct total height */
static void s_test_layout_build_mixed_rows(void)
{
    ctxmenu_state_td state;
    ctxmenu_entry_td entries[5];
    config_td config;
    int32_t cache[5];
    uint16_t total_height;

    s_make_state(&state, entries, &config, cache, true);

    total_height = ctxmenu_layout_build(&state);

    TAP_EQ_INT(state.entry_top_y[0], 4, "row 0 starts right after" \
            " the top padding");
    TAP_EQ_INT(state.entry_top_y[1], 24, "row 1 starts after row" \
            " 0's own 20px height");
    TAP_EQ_INT(state.entry_top_y[3], 64, "the separator's own row" \
            " (index 3) starts at 64");
    TAP_EQ_INT(state.entry_top_y[4], 72, "the row after the" \
            " separator only advances by the separator's own 8px," \
            " not 20");
    TAP_EQ_INT((long) total_height, 96,
            "total height is padding + every row's height + padding" \
            " again (4 + 88 + 4)");
}


/* ctxmenu_entry_top_y: the cached path and the fallback walk agree
 * on every row's own top-Y, including the separator */
static void s_test_entry_top_y_cache_and_fallback_agree(void)
{
    ctxmenu_state_td cached;
    ctxmenu_state_td fallback;
    ctxmenu_entry_td entries_a[5];
    ctxmenu_entry_td entries_b[5];
    config_td config_a;
    config_td config_b;
    int32_t cache[5];

    s_make_state(&cached, entries_a, &config_a, cache, true);
    ctxmenu_layout_build(&cached);
    s_make_state(&fallback, entries_b, &config_b, NULL, false);

    for (int i = 0; i < 5; ++i) {
        int cached_y = ctxmenu_entry_top_y(&cached, i);
        int fallback_y = ctxmenu_entry_top_y(&fallback, i);

        TAP_EQ_INT(cached_y, fallback_y,
                "cached and fallback top_y agree for this row");
    }
}


/* ctxmenu_entry_at_y (binary search with the cache) and its own O(n)
 * fallback agree on a representative set of Y values: exactly on a
 * row boundary, mid-row, inside the separator's own thin row, inside
 * the top padding (before any row), and past the last row */
static void s_test_entry_at_y_cache_and_fallback_agree(void)
{
    ctxmenu_state_td cached;
    ctxmenu_state_td fallback;
    ctxmenu_entry_td entries_a[5];
    ctxmenu_entry_td entries_b[5];
    config_td config_a;
    config_td config_b;
    int32_t cache[5];
    const int ys[] = { 0, 4, 23, 24, 44, 68, 71, 72, 91, 92, 200 };

    s_make_state(&cached, entries_a, &config_a, cache, true);
    ctxmenu_layout_build(&cached);
    s_make_state(&fallback, entries_b, &config_b, NULL, false);

    for (size_t i = 0; i < sizeof(ys) / sizeof(ys[0]); ++i) {
        int cached_idx = ctxmenu_entry_at_y(&cached, ys[i]);
        int fallback_idx = ctxmenu_entry_at_y(&fallback, ys[i]);

        TAP_EQ_INT(cached_idx, fallback_idx,
                "cached (binary search) and fallback (linear) agree" \
                " at this y");
    }
}


/* A handful of exact, hand-computed ctxmenu_entry_at_y results,
 * checked against the cached path specifically */
static void s_test_entry_at_y_exact_values(void)
{
    ctxmenu_state_td state;
    ctxmenu_entry_td entries[5];
    config_td config;
    int32_t cache[5];

    s_make_state(&state, entries, &config, cache, true);
    ctxmenu_layout_build(&state);

    TAP_EQ_INT(ctxmenu_entry_at_y(&state, 0), -1,
            "y=0, inside the top padding: no row yet");
    TAP_EQ_INT(ctxmenu_entry_at_y(&state, 4), 0,
            "y=4, exactly where row 0 starts: row 0");
    TAP_EQ_INT(ctxmenu_entry_at_y(&state, 23), 0,
            "y=23, the last pixel still inside row 0's 20px height");
    TAP_EQ_INT(ctxmenu_entry_at_y(&state, 24), 1,
            "y=24, exactly where row 0 ends and row 1 begins: row 1," \
            " not row 0");
    TAP_EQ_INT(ctxmenu_entry_at_y(&state, 64), 3,
            "y=64, the separator's own row");
    TAP_EQ_INT(ctxmenu_entry_at_y(&state, 71), 3,
            "y=71, the separator's own last pixel (8px tall, not 20)");
    TAP_EQ_INT(ctxmenu_entry_at_y(&state, 72), 4,
            "y=72, right after the separator: the next row, not" \
            " still the separator");
    TAP_EQ_INT(ctxmenu_entry_at_y(&state, 92), -1,
            "y=92, exactly past the last row's own end: no row");
}


int main(void)
{
    TAP_PLAN(29);

    s_test_layout_build_mixed_rows();
    s_test_entry_top_y_cache_and_fallback_agree();
    s_test_entry_at_y_cache_and_fallback_agree();
    s_test_entry_at_y_exact_values();

    return TAP_DONE();
}
