/**
 * @file tests/menu/context/test_rootmenu.c
 *
 * @brief Test battery for the root desktop menu
 *        (menu/context/rootmenu.c)
 *
 * 'rootmenu.c' owns one file-static 'ctxmenu_state_td' singleton and
 * combines whatever 'menu.json' last loaded with a fixed footer, then
 * hands the result to 'ctxmenu_show'.  This file stands in for
 * 'menujson_load'/'menujson_free' with a test-controlled pair that
 * hands back an in-memory array instead of touching a real
 * 'menu.json' on disk, stands in for 'ctxmenu_show' with a capturing
 * recorder the same way 'tests/menu/context/test_winlist.c' captures
 * 'wincmenu_show''s built entries, and stands in for
 * 'ctxmenu_close'/'ctxmenu_is_open' and every 'ctxmenu_tree_*'
 * dispatcher the thin wrapper functions forward to, following
 * 'tests/menu/context/ctxmenu/test_tree.c''s own recording style for
 * those.  'safe_strndup'/'safe_strncpy' are linked for real (pure
 * string helpers, no X, no disk I/O), so the deep copies
 * 'rootmenu_show' makes of each JSON entry's 'command'/'class_name'
 * are exercised genuinely rather than assumed.
 *
 * Not covered: the callbacks wired onto the footer entries
 * ('s_cb_rearrange', 's_cb_reload', 's_cb_redraw',
 * 's_cb_toggle_strutless_maximize', 's_cb_exit') are file-static and
 * unreachable from outside this translation unit, and running them
 * would need a live window manager instance, a live X connection, or
 * both; this file only ever inspects which of those wireless are set
 * on the resulting entry via the recorded 'ctxmenu_show' state
 * (never invoked), never calls them.
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
#include <xcb/xcb_keysyms.h>

/* Local includes */
#include <config.h>
#include <enact.h>
#include <harness/tap.h>
#include <logger.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/tree.h>
#include <menu/context/menujson.h>
#include <menu/context/rootmenu.h>
#include <menu/dialog/quit.h>
#include <surface.h>
#include <wm.h>


/** Test-controlled data fed to the next @a menujson_load call, and
 *  its own call counter */
static ctxmenu_entry_td *s_json_load_entries;
static int s_json_load_count;
static int s_json_load_calls;
static bool s_json_load_return;

/** Test-controlled stand-in for @a menujson_load
 * @note Complexity: @e O(1) */
bool menujson_load(const char *json_path, ctxmenu_entry_td **out_entries,
        int *out_count)
{
    (void) json_path;
    s_json_load_calls++;
    *out_entries = s_json_load_entries;
    *out_count = s_json_load_count;
    return s_json_load_return;
}


/** Recording stand-in for @a menujson_free
 * @note Complexity: @e O(1) */
static int s_json_free_calls;

void menujson_free(ctxmenu_entry_td *entries, int count)
{
    (void) entries;
    (void) count;
    s_json_free_calls++;
}


/** Recording stand-in for @a ctxmenu_show, capturing the state it
 *  was handed so its built entries can be inspected directly */
static int s_show_calls;
static ctxmenu_state_td *s_show_state;
static struct position_s s_show_pos;

void ctxmenu_show(xcb_connection_t *connection, surface_td *surface,
        ctxmenu_state_td *state, struct position_s pos,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) config;
    s_show_calls++;
    s_show_state = state;
    s_show_pos = pos;
}


/** Recording stand-in for @a ctxmenu_close
 * @note Complexity: @e O(1) */
static int s_close_calls;
static ctxmenu_state_td *s_close_state;

void ctxmenu_close(ctxmenu_state_td *state)
{
    s_close_calls++;
    s_close_state = state;
}


/** Recording, test-controlled stand-in for @a ctxmenu_is_open
 * @note Complexity: @e O(1) */
static int s_is_open_calls;
static bool s_is_open_return;

bool ctxmenu_is_open(const ctxmenu_state_td *state)
{
    (void) state;
    s_is_open_calls++;
    return s_is_open_return;
}


/** Recording, test-controlled stand-in for @a ctxmenu_tree_redraw_window
 * @note Complexity: @e O(1) */
static int s_tree_redraw_calls;
static xcb_window_t s_tree_redraw_win;

void ctxmenu_tree_redraw_window(ctxmenu_state_td *root, xcb_window_t win)
{
    (void) root;
    s_tree_redraw_calls++;
    s_tree_redraw_win = win;
}


/** Recording, test-controlled stand-in for
 *  @a ctxmenu_tree_handle_click_window
 * @note Complexity: @e O(1) */
static int s_tree_click_calls;
static xcb_window_t s_tree_click_win;
static int s_tree_click_y;
static bool s_tree_click_return;

bool ctxmenu_tree_handle_click_window(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root, xcb_window_t win,
        int y, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) root;
    (void) config;
    s_tree_click_calls++;
    s_tree_click_win = win;
    s_tree_click_y = y;
    return s_tree_click_return;
}


/** Recording, test-controlled stand-in for
 *  @a ctxmenu_tree_state_find_for_window
 * @note Complexity: @e O(1) */
static int s_tree_find_calls;
static xcb_window_t s_tree_find_win;
static ctxmenu_state_td *s_tree_find_return;

ctxmenu_state_td *ctxmenu_tree_state_find_for_window(
        ctxmenu_state_td *root, xcb_window_t win)
{
    (void) root;
    s_tree_find_calls++;
    s_tree_find_win = win;
    return s_tree_find_return;
}


/** Recording, test-controlled stand-in for
 *  @a ctxmenu_tree_handle_keypress_deepest
 * @note Complexity: @e O(1) */
static int s_tree_keypress_calls;
static xcb_keysym_t s_tree_keypress_keysym;
static bool s_tree_keypress_return;

bool ctxmenu_tree_handle_keypress_deepest(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root,
        xcb_keysym_t keysym, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) root;
    (void) config;
    s_tree_keypress_calls++;
    s_tree_keypress_keysym = keysym;
    return s_tree_keypress_return;
}


/** Recording, test-controlled stand-in for
 *  @a ctxmenu_tree_handle_motion_window
 * @note Complexity: @e O(1) */
static int s_tree_motion_calls;
static xcb_window_t s_tree_motion_win;
static int s_tree_motion_x;
static int s_tree_motion_y;

void ctxmenu_tree_handle_motion_window(ctxmenu_state_td *root,
        xcb_window_t win, int x, int y)
{
    (void) root;
    s_tree_motion_calls++;
    s_tree_motion_win = win;
    s_tree_motion_x = x;
    s_tree_motion_y = y;
}


/** Link-only stand-ins for the footer callbacks' own dependencies;
 *  never invoked, since no test here activates an entry */
void wm_action_rearrange(const wm_td *wm, surface_td *surface)
{
    (void) wm;
    (void) surface;
}


int wm_action_config_reload(const wm_td *wm)
{
    (void) wm;
    return 0;
}


void wm_request_full_redraw(void)
{
}


void enact_surface_toggle_strutless_maximize(surface_td *surface)
{
    (void) surface;
}


void dialog_quit_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) config;
}


/** Link-only stand-in for @a logger_msg
 * @note Complexity: @e O(1) */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


static void s_reset(void)
{
    s_json_load_entries = NULL;
    s_json_load_count = 0;
    s_json_load_calls = 0;
    s_json_load_return = true;
    s_json_free_calls = 0;
    s_show_calls = 0;
    s_show_state = NULL;
    s_close_calls = 0;
    s_close_state = NULL;
    s_is_open_calls = 0;
    s_is_open_return = false;
    s_tree_redraw_calls = 0;
    s_tree_click_calls = 0;
    s_tree_click_return = false;
    s_tree_find_calls = 0;
    s_tree_find_return = NULL;
    s_tree_keypress_calls = 0;
    s_tree_keypress_return = false;
    s_tree_motion_calls = 0;
}


/**
 * @brief Verify @a rootmenu_menu_json_load forwards to
 *        @a menujson_load with a path built from @p config_dir, and
 *        frees any previously loaded entries first
 */
static void s_test_json_load(void)
{
    ctxmenu_entry_td fixture[1];

    s_reset();
    memset(fixture, 0, sizeof(fixture));
    fixture[0].type = CTXMENU_COMMAND;
    strcpy(fixture[0].label, "Terminal");
    s_json_load_entries = fixture;
    s_json_load_count = 1;

    rootmenu_menu_json_load("/etc/icowm");
    TAP_EQ_INT(s_json_load_calls, 1,
            "loading menu.json calls menujson_load once");
    TAP_EQ_INT(s_json_free_calls, 0,
            "the first load frees nothing, since nothing was loaded"
            " yet");

    rootmenu_menu_json_load("/etc/icowm");
    TAP_EQ_INT(s_json_free_calls, 1,
            "reloading frees the previous load's entries first");

    rootmenu_menu_json_free();
    TAP_EQ_INT(s_json_free_calls, 2,
            "rootmenu_menu_json_free frees the currently loaded"
            " entries");

    rootmenu_menu_json_free();
    TAP_EQ_INT(s_json_free_calls, 2,
            "freeing again with nothing loaded is a safe no-op");
}


/**
 * @brief Verify @a rootmenu_show rejects null connection, surface,
 *        or config without ever reaching @a ctxmenu_show
 */
static void s_test_show_guards(void)
{
    surface_td surface;
    config_td config;
    struct position_s pos = { 1, 2 };

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));

    rootmenu_show(NULL, NULL, &surface, pos, &config);
    TAP_EQ_INT(s_show_calls, 0,
            "showing with a null connection shows nothing");

    rootmenu_show(NULL, (xcb_connection_t *) 1, NULL, pos, &config);
    TAP_EQ_INT(s_show_calls, 0,
            "showing with a null surface shows nothing");

    rootmenu_show(NULL, (xcb_connection_t *) 1, &surface, pos, NULL);
    TAP_EQ_INT(s_show_calls, 0,
            "showing with a null config shows nothing");
}


/**
 * @brief Showing the menu with zero JSON entries loaded produces
 *        exactly the fixed 6-row footer, with no leading separator
 *        and no out-of-bounds write into the entry array
 *
 * 'rootmenu_show' reserves 'n = s_json_count + ROOTMENU_FOOTER_COUNT -
 * (s_json_count == 0 ? 1 : 0)' slots, which is 6 when no menu.json
 * entries are loaded ('ROOTMENU_FOOTER_COUNT' is 7, and the leading
 * separator that '- 1' accounts for is genuinely skipped, so the
 * footer alone really does only need 6 slots).  The later clamp on
 * 'copy_count' now floors at 0 instead of letting 'n -
 * ROOTMENU_FOOTER_COUNT' go negative in this exact case, so 'fi'
 * starts the footer-building block at '0' rather than '-1'
 */
static void s_test_show_empty_json(void)
{
    surface_td surface;
    config_td config;
    struct position_s pos = { 0, 0 };

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));
    rootmenu_menu_json_load("/etc/icowm");

    rootmenu_show(NULL, (xcb_connection_t *) 1, &surface, pos, &config);

    TAP_EQ_INT(s_show_state->entry_count, 6,
            "the 6-row footer alone makes 6 rows total when no JSON"
            " entries are loaded");
    TAP_EQ_INT((int) s_show_state->entries[0].type,
            (int) CTXMENU_COMMAND,
            "the first row is the first footer command, not a stray"
            " leading separator");

    rootmenu_menu_json_free();
}


/**
 * @brief Verify JSON entries are prepended (with a leading separator)
 *        before the fixed footer, with independently copied
 *        'command'/'class_name' strings
 */
static void s_test_show_with_json_entries(void)
{
    surface_td surface;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td fixture[2];

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));
    memset(fixture, 0, sizeof(fixture));

    fixture[0].type = CTXMENU_COMMAND;
    strcpy(fixture[0].label, "Terminal");
    fixture[0].command = "xterm";
    fixture[1].type = CTXMENU_COMMAND;
    strcpy(fixture[1].label, "Browser");
    fixture[1].command = "firefox";
    s_json_load_entries = fixture;
    s_json_load_count = 2;
    rootmenu_menu_json_load("/etc/icowm");

    rootmenu_show(NULL, (xcb_connection_t *) 1, &surface, pos, &config);

    TAP_EQ_INT(s_show_state->entry_count, 9,
            "two JSON entries plus a leading separator plus the"
            " 6-row footer makes 9 rows total");
    TAP_EQ_STR(s_show_state->entries[0].label, "Terminal",
            "the first row is the first JSON entry");
    TAP_EQ_STR(s_show_state->entries[0].command, "xterm",
            "the first JSON entry's command was copied");
    TAP_OK(s_show_state->entries[0].command != fixture[0].command,
            "the copied command is an independent allocation, not"
            " the JSON loader's original pointer");
    TAP_EQ_STR(s_show_state->entries[1].label, "Browser",
            "the second row is the second JSON entry");
    TAP_EQ_INT((int) s_show_state->entries[2].type,
            (int) CTXMENU_SEPARATOR,
            "a separator is inserted between the JSON entries and"
            " the footer when JSON entries are present");
    TAP_EQ_STR(s_show_state->entries[3].label, "Strutless maximization",
            "the footer starts right after the separator");

    rootmenu_close();
    TAP_EQ_INT(s_close_calls, 2,
            "closing after showing calls ctxmenu_close once for the"
            " internal close-before-reopen in rootmenu_show and"
            " once for this explicit close");
    TAP_EQ_INT(s_json_free_calls, 0,
            "closing the shown menu never frees the persistent"
            " JSON-loaded entries, only the per-open combined copy");

    rootmenu_menu_json_free();
}


/**
 * @brief Verify the footer's maximization label reflects whether the
 *        surface is already in strutless mode
 */
static void s_test_show_strutted_label(void)
{
    surface_td surface;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td fixture[1];

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));
    memset(fixture, 0, sizeof(fixture));
    surface.strutless_maximize = true;

    /* A JSON entry is loaded here purely so 'entries[2]' below lands
     * on the footer's first row, right after the leading separator
     * that only appears when JSON entries are present */
    fixture[0].type = CTXMENU_COMMAND;
    strcpy(fixture[0].label, "Terminal");
    s_json_load_entries = fixture;
    s_json_load_count = 1;
    rootmenu_menu_json_load("/etc/icowm");

    rootmenu_show(NULL, (xcb_connection_t *) 1, &surface, pos, &config);

    TAP_EQ_STR(s_show_state->entries[2].label, "Strutted maximization",
            "an already-strutless surface offers to switch back to"
            " strutted maximization");

    rootmenu_close();
    rootmenu_menu_json_free();
}


/**
 * @brief Verify @a rootmenu_show closes any previously open menu
 *        before building the new one
 */
static void s_test_show_closes_previous(void)
{
    surface_td surface;
    config_td config;
    struct position_s pos = { 0, 0 };
    ctxmenu_entry_td fixture[1];

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));
    memset(fixture, 0, sizeof(fixture));

    /* At least one JSON entry is loaded here for the same reason as
     * in 's_test_show_strutted_label'. */
    fixture[0].type = CTXMENU_COMMAND;
    strcpy(fixture[0].label, "Terminal");
    s_json_load_entries = fixture;
    s_json_load_count = 1;
    rootmenu_menu_json_load("/etc/icowm");

    rootmenu_show(NULL, (xcb_connection_t *) 1, &surface, pos, &config);
    TAP_EQ_INT(s_close_calls, 1,
            "showing the menu the first time still closes the"
            " (not yet open) singleton first");

    rootmenu_show(NULL, (xcb_connection_t *) 1, &surface, pos, &config);
    TAP_EQ_INT(s_close_calls, 2,
            "showing again closes the previous open menu first");

    rootmenu_close();
    rootmenu_menu_json_free();
}


/**
 * @brief Verify the thin wrapper functions forward to their
 *        respective tree/ctxmenu dispatchers with the singleton state
 */
static void s_test_wrappers(void)
{
    bool click_result;
    bool keypress_result;
    ctxmenu_state_td *found;

    s_reset();

    rootmenu_repaint((xcb_window_t) 7);
    TAP_EQ_INT(s_tree_redraw_calls, 1,
            "repaint forwards to ctxmenu_tree_redraw_window once");
    TAP_EQ_INT((int) s_tree_redraw_win, 7,
            "repaint forwards the window it was given");

    s_tree_click_return = true;
    click_result = rootmenu_handle_click((xcb_connection_t *) 1, NULL,
            (xcb_window_t) 8, 20, NULL);
    TAP_OK(click_result,
            "handle_click returns the tree dispatcher's result");
    TAP_EQ_INT((int) s_tree_click_win, 8,
            "handle_click forwards the window it was given");
    TAP_EQ_INT(s_tree_click_y, 20,
            "handle_click forwards the y it was given");

    s_is_open_return = true;
    TAP_OK(rootmenu_is_open(),
            "is_open returns ctxmenu_is_open's result for the"
            " singleton");
    TAP_EQ_INT(s_is_open_calls, 1,
            "is_open calls ctxmenu_is_open exactly once");

    s_tree_find_return = (ctxmenu_state_td *) 0x1234;
    found = (ctxmenu_state_td *) (rootmenu_owns_window((xcb_window_t) 9)
            ? (void *) 1 : NULL);
    TAP_NOT_NULL(found,
            "owns_window returns true when the tree finder finds a"
            " match");
    TAP_EQ_INT((int) s_tree_find_win, 9,
            "owns_window forwards the window it was given");

    s_tree_keypress_return = true;
    keypress_result = rootmenu_handle_keypress((xcb_connection_t *) 1,
            NULL, 0x61u, NULL);
    TAP_OK(keypress_result,
            "handle_keypress returns the deepest-dispatcher's result");
    TAP_EQ_INT((int) s_tree_keypress_keysym, 0x61,
            "handle_keypress forwards the keysym it was given");

    rootmenu_handle_motion((xcb_window_t) 10, 1, 2);
    TAP_EQ_INT(s_tree_motion_calls, 1,
            "handle_motion forwards to ctxmenu_tree_handle_motion_"
            "window once");
    TAP_EQ_INT((int) s_tree_motion_win, 10,
            "handle_motion forwards the window it was given");
}


int main(void)
{
    TAP_PLAN(35);

    s_test_json_load();
    s_test_show_guards();
    s_test_show_empty_json();
    s_test_show_with_json_entries();
    s_test_show_strutted_label();
    s_test_show_closes_previous();
    s_test_wrappers();

    return TAP_DONE();
}
