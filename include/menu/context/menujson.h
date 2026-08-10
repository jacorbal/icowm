/**
 * @file menu/context/menujson.h
 *
 * @brief JSON loader for the root desktop menu
 *
 * Parses a JSON file that describes the user-configurable entries of
 * the root desktop menu and populates a flat @c ctxmenu_entry_td array
 * that can be passed directly to @a ctxmenu_show.
 *
 * The JSON schema mirrors the example in the project documentation:
 * @code
 * {
 *   "menu": [
 *     { "type": "command", "name": "Terminal", "command": "xterm" },
 *     { "type": "separator" },
 *     { "type": "label",   "name": "Games" },
 *     { "type": "submenu", "name": "Editors",
 *       "items": [ ... ] }
 *   ]
 * }
 * @endcode
 *
 * Memory allocation: @a menujson_load allocates the entry array and all
 * sub-arrays.  The caller must free them with @a menujson_free when
 * done.
 *
 * @ingroup menu_context
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONTEXT_MENUJSON_H
#define MENU_CONTEXT_MENUJSON_H


/* System includes */
#include <stddef.h>     /* size_t */

/* Local includes */
#include <menu/context/ctxmenu.h>


/* Public interface */
/**
 * @brief Load a context menu entry array from a JSON file
 *
 * Opens and parses @p json_path, which must conform to the menu JSON
 * schema.  Allocates and populates a flat array of @c ctxmenu_entry_td
 * structs (including nested sub-arrays for @p submenu entries).  Each
 * @c CTXMENU_SUBMENU entry has its @p userdata pointer set to
 * a heap-allocated @c ctxmenu_state_td so that @a ctxmenu_handle_click
 * can open the child menu without additional caller setup.
 *
 * @param json_path   Path to the JSON file
 * @param out_entries Receives the allocated entry array (caller must
 *                    free with @a menujson_free)
 * @param out_count   Receives the number of top-level entries
 *
 * @return @c true on success; @c false on file error or parse failure
 *
 * @note Complexity: @e O(n), where @e n is the total number of entries
 *       across all nesting levels
 */
bool menujson_load(const char *json_path,
        ctxmenu_entry_td **out_entries, int *out_count);

/**
 * @brief Free the entry array returned by @a menujson_load
 *
 * Recursively frees all sub-entry arrays and the associated
 * @c ctxmenu_state_td objects allocated in the @p userdata pointers of
 * @c CTXMENU_SUBMENU entries, then frees @p entries itself.
 *
 * @param entries Entry array to free
 * @param count   Number of entries in @p entries
 *
 * @note Complexity: @e O(n), where @e n is the total number of entries
 */
void menujson_free(ctxmenu_entry_td *entries, int count);


#endif  /* ! MENU_CONTEXT_MENUJSON_H */
