/**
 * @file tests/menu/context/test_menujson.c
 *
 * @brief Test battery for the root menu's JSON loader
 *        (menu/context/menujson.c)
 *
 * Both public entry points, and the file-static 's_parse_array' they
 * share, involve no X11 at all: parsing a 'cJSON' tree and building
 * a 'ctxmenu_entry_td' array is ordinary data-structure work, so this
 * file exercises the whole thing through the real 'menujson_load'
 * and 'menujson_free'.
 *
 * The one collaborator 'menujson_load' calls that this file does not
 * link the real implementation of is 'json_load_config' itself
 * (@c utils/config/json.c): that function opens and reads a real file
 * from disk, which the ground rules ask to avoid.  A test-controlled
 * stand-in is used instead, keyed by path, that hands back a 'cJSON'
 * tree already parsed in memory from a string literal via
 * 'cJSON_Parse', exactly the tree a real file with that content would
 * have produced, without ever touching a filesystem.  'logger_msg' is
 * also a link-only stand-in, since the warning and error paths this
 * file deliberately exercises (a missing file, a malformed 'menu'
 * key) would otherwise print to the real log sink.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/menujson.h>
#include <utils/config/json.h>


/** Path and in-memory JSON text this file's 'json_load_config'
 *  stand-in answers from */
static const char *s_next_path;
static const char *s_next_json_text;
static int s_next_return;


/**
 * @brief Test-controlled stand-in for @a json_load_config
 *
 * Parses 's_next_json_text' with 'cJSON_Parse' instead of reading
 * 'filename' from disk, so long as 'filename' matches
 * 's_next_path'; this keeps every test in this file free of real
 * file I/O while still exercising 'menujson_load''s own logic in
 * full.
 *
 * @note Complexity: @e O(n), where @e n is the length of the JSON text
 */
int json_load_config(const char *filename, cJSON **json_out)
{
    if (s_next_return != 0) {
        *json_out = NULL;
        return s_next_return;
    }

    if (filename == NULL || s_next_path == NULL ||
            strcmp(filename, s_next_path) != 0) {
        *json_out = NULL;
        return -1;
    }

    *json_out = cJSON_Parse(s_next_json_text);
    return (*json_out != NULL) ? 0 : -1;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * Reached by the warning and error paths this file deliberately
 * triggers (a load failure, a missing 'menu' array, an allocation
 * failure path never reached in practice).
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


static void s_set_source(const char *path, const char *json_text)
{
    s_next_path = path;
    s_next_json_text = json_text;
    s_next_return = 0;
}


/**
 * @brief Verify @a menujson_load rejects null arguments without
 *        touching @a json_load_config
 */
static void s_test_load_guards(void)
{
    ctxmenu_entry_td *entries = NULL;
    int count = 0;

    s_set_source("/menu.json", "{\"menu\":[]}");

    TAP_OK(!menujson_load(NULL, &entries, &count),
            "a null path is rejected");
    TAP_OK(!menujson_load("/menu.json", NULL, &count),
            "a null out_entries is rejected");
    TAP_OK(!menujson_load("/menu.json", &entries, NULL),
            "a null out_count is rejected");
}


/**
 * @brief Verify @a menujson_load fails cleanly when the underlying
 *        load itself fails
 */
static void s_test_load_file_error(void)
{
    ctxmenu_entry_td *entries = (ctxmenu_entry_td *) 1;
    int count = 99;

    s_next_path = "/missing.json";
    s_next_json_text = NULL;
    s_next_return = -1;

    TAP_OK(!menujson_load("/missing.json", &entries, &count),
            "a load failure is reported as failure");
    TAP_NULL(entries, "a load failure clears out_entries");
    TAP_EQ_INT(count, 0, "a load failure clears out_count");
}


/**
 * @brief Verify @a menujson_load fails cleanly when the JSON parses
 *        but has no top-level @c menu array
 */
static void s_test_load_missing_menu_key(void)
{
    ctxmenu_entry_td *entries = (ctxmenu_entry_td *) 1;
    int count = 99;

    s_set_source("/no_menu.json", "{\"other\":1}");

    TAP_OK(!menujson_load("/no_menu.json", &entries, &count),
            "a missing 'menu' key is reported as failure");
    TAP_NULL(entries, "a missing 'menu' key clears out_entries");
    TAP_EQ_INT(count, 0, "a missing 'menu' key clears out_count");
}


/**
 * @brief Verify @a menujson_load succeeds and returns zero entries
 *        for an empty @c menu array
 */
static void s_test_load_empty_menu(void)
{
    ctxmenu_entry_td *entries = NULL;
    int count = -1;

    s_set_source("/empty.json", "{\"menu\":[]}");

    TAP_OK(menujson_load("/empty.json", &entries, &count),
            "an empty menu array loads successfully");
    TAP_EQ_INT(count, 0, "an empty menu array yields zero entries");
    TAP_NULL(entries, "an empty menu array yields a null array");

    menujson_free(entries, count);
    TAP_OK(true, "freeing a null entry array does not crash");
}


/**
 * @brief Verify every flat entry type is parsed with its label,
 *        command, and class fields set correctly
 */
static void s_test_load_flat_entries(void)
{
    ctxmenu_entry_td *entries = NULL;
    int count = 0;
    static const char *json_text =
        "{\"menu\":["
        "{\"type\":\"command\",\"name\":\"Terminal\","
        "\"command\":\"xterm\",\"class\":\"XTerm\"},"
        "{\"type\":\"separator\"},"
        "{\"type\":\"label\",\"name\":\"Games\"},"
        "{\"type\":\"command\",\"name\":\"No Command\"}"
        "]}";

    s_set_source("/flat.json", json_text);

    TAP_OK(menujson_load("/flat.json", &entries, &count),
            "a flat menu array loads successfully");
    TAP_EQ_INT(count, 4, "the flat menu array yields four entries");
    TAP_NOT_NULL(entries, "the flat menu array yields a real array");

    TAP_EQ_INT((int) entries[0].type, (int) CTXMENU_COMMAND,
            "the first entry is a command");
    TAP_EQ_STR(entries[0].label, "Terminal",
            "the command entry's label is set from 'name'");
    TAP_EQ_STR(entries[0].command, "xterm",
            "the command entry's command is set from 'command'");
    TAP_EQ_STR(entries[0].class_name, "XTerm",
            "the command entry's class name is set from 'class'");

    TAP_EQ_INT((int) entries[1].type, (int) CTXMENU_SEPARATOR,
            "the second entry is a separator");

    TAP_EQ_INT((int) entries[2].type, (int) CTXMENU_LABEL,
            "the third entry is a label");
    TAP_EQ_STR(entries[2].label, "Games",
            "the label entry's text is set from 'name'");

    TAP_EQ_INT((int) entries[3].type, (int) CTXMENU_COMMAND,
            "the fourth entry is a command");
    TAP_NULL(entries[3].command,
            "a command entry with no 'command' key leaves it null");

    menujson_free(entries, count);
}


/**
 * @brief Verify a @c submenu entry recursively parses its own
 *        @c items array and allocates a child @c ctxmenu_state_td in
 *        @c userdata
 */
static void s_test_load_submenu(void)
{
    ctxmenu_entry_td *entries = NULL;
    int count = 0;
    ctxmenu_state_td *child_state;
    static const char *json_text =
        "{\"menu\":["
        "{\"type\":\"submenu\",\"name\":\"Editors\",\"items\":["
        "{\"type\":\"command\",\"name\":\"Vim\",\"command\":\"vim\"},"
        "{\"type\":\"command\",\"name\":\"Emacs\",\"command\":\"emacs\"}"
        "]}"
        "]}";

    s_set_source("/submenu.json", json_text);

    TAP_OK(menujson_load("/submenu.json", &entries, &count),
            "a menu with a submenu loads successfully");
    TAP_EQ_INT(count, 1, "the top level has exactly one entry");
    TAP_EQ_INT((int) entries[0].type, (int) CTXMENU_SUBMENU,
            "the top-level entry is a submenu");
    TAP_EQ_STR(entries[0].label, "Editors",
            "the submenu's label is set from 'name'");
    TAP_EQ_INT(entries[0].item_count, 2,
            "the submenu's item_count reflects its 'items' array");
    TAP_NOT_NULL(entries[0].items,
            "the submenu's child entries are allocated");
    TAP_EQ_STR(entries[0].items[0].label, "Vim",
            "the first child entry's label is set");
    TAP_EQ_STR(entries[0].items[1].command, "emacs",
            "the second child entry's command is set");

    child_state = (ctxmenu_state_td *) entries[0].userdata;
    TAP_NOT_NULL(child_state,
            "a submenu entry gets a heap-allocated child state");
    TAP_EQ_INT((int) child_state->window, (int) XCB_WINDOW_NONE,
            "the child state starts with no window");

    menujson_free(entries, count);
}


/**
 * @brief Verify a non-object array item is skipped but still
 *        occupies a slot in the output array (a zeroed entry with a
 *        'CTXMENU_COMMAND' default type, since the type field is
 *        never set for it)
 */
static void s_test_load_skips_non_object_items(void)
{
    ctxmenu_entry_td *entries = NULL;
    int count = 0;
    static const char *json_text =
        "{\"menu\":[42,{\"type\":\"separator\"}]}";

    s_set_source("/mixed.json", json_text);

    TAP_OK(menujson_load("/mixed.json", &entries, &count),
            "a menu array with a non-object item still loads");
    TAP_EQ_INT(count, 2,
            "the non-object item still occupies a slot in the count");
    TAP_EQ_INT((int) entries[0].type, (int) CTXMENU_COMMAND,
            "the non-object item's slot is left zeroed (default"
            " command type)");
    TAP_EQ_INT((int) entries[1].type, (int) CTXMENU_SEPARATOR,
            "the following real entry still parses correctly");

    menujson_free(entries, count);
}


int main(void)
{
    TAP_PLAN(39);

    s_test_load_guards();
    s_test_load_file_error();
    s_test_load_missing_menu_key();
    s_test_load_empty_menu();
    s_test_load_flat_entries();
    s_test_load_submenu();
    s_test_load_skips_non_object_items();

    return TAP_DONE();
}
