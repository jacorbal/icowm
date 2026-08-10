/**
 * @file menu/context/menujson.c
 *
 * @brief JSON loader for the root desktop menu (menu.json)
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
#include <stddef.h>     /* NULL, size_t */
#include <stdlib.h>     /* free, calloc */

/* JSON includes */
#include <cjson/cJSON.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>
#include <utils/config/json.h>
#include <utils/safe/safemem.h>

/* Local includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/menujson.h>


/**
 * @brief Parse a JSON array of menu entries into an allocated array
 *
 * Recursively processes @p submenu entries.  Each @c CTXMENU_SUBMENU
 * entry gets a heap-allocated @c ctxmenu_state_td in its @p userdata
 * field that @a ctxmenu_handle_click can use to open the child menu.
 *
 * @param arr       cJSON array node
 * @param out       Receives the allocated entry array
 * @param out_count Receives the number of allocated entries
 *
 * @return @c true on success
 *
 * @note Complexity: @e O(n), where @e n is the total number of entries
 *       across all nesting levels
 */
static bool s_parse_array(const cJSON *arr,
        ctxmenu_entry_td **out, int *out_count)
{
    int count;
    ctxmenu_entry_td *entries;
    const cJSON *item;
    int i;
    const cJSON *type_node;
    const cJSON *name_node;
    const cJSON *class_node;
    const cJSON *cmd_node;
    const cJSON *items_node;
    const char *type_str;

    if (arr == NULL || !cJSON_IsArray(arr)) {
        return false;
    }

    count = cJSON_GetArraySize(arr);
    if (count <= 0) {
        *out = NULL;
        *out_count = 0;
        return true;
    }

    entries = (ctxmenu_entry_td *) calloc((size_t) count,
            sizeof(ctxmenu_entry_td));
    if (entries == NULL) {
        LOGGER_ERROR("Failed to allocate %d entries in JSON menu", count);
        return false;
    }

    i = 0;
    cJSON_ArrayForEach(item, arr) {
        if (!cJSON_IsObject(item)) {
            ++i;
            continue;
        }

        type_node = cJSON_GetObjectItemCaseSensitive(item, "type");
        name_node = cJSON_GetObjectItemCaseSensitive(item, "name");
        class_node = cJSON_GetObjectItemCaseSensitive(item, "class");
        cmd_node  = cJSON_GetObjectItemCaseSensitive(item, "command");
        items_node = cJSON_GetObjectItemCaseSensitive(item, "items");

        type_str = (cJSON_IsString(type_node))
            ? type_node->valuestring : "";

        if (safe_strcmp(type_str, "separator") == 0) {
            entries[i].type = CTXMENU_SEPARATOR;

        } else if (safe_strcmp(type_str, "label") == 0) {
            entries[i].type = CTXMENU_LABEL;
            if (cJSON_IsString(name_node)) {
                safe_strncpy(entries[i].label, name_node->valuestring,
                        sizeof(entries[i].label) - 1u);
            }
        } else if (safe_strcmp(type_str, "command") == 0) {
            entries[i].type = CTXMENU_COMMAND;
            if (cJSON_IsString(name_node)) {
                safe_strncpy(entries[i].label, name_node->valuestring,
                        sizeof(entries[i].label) - 1u);
            }
            if (cJSON_IsString(cmd_node)) {
                safe_strncpy(entries[i].command, cmd_node->valuestring,
                        sizeof(entries[i].command) - 1u);
            }
            if (cJSON_IsString(class_node)) {
                safe_strncpy(entries[i].class_name,
                        class_node->valuestring,
                        sizeof(entries[i].class_name) - 1u);
            }
        } else if (safe_strcmp(type_str, "submenu") == 0) {
            entries[i].type = CTXMENU_SUBMENU;
            if (cJSON_IsString(name_node)) {
                safe_strncpy(entries[i].label, name_node->valuestring,
                        sizeof(entries[i].label) - 1u);
            }
            if (cJSON_IsArray(items_node)) {
                ctxmenu_entry_td *sub_entries = NULL;
                int sub_count = 0;
                ctxmenu_state_td *child_state = NULL;

                if (s_parse_array(items_node, &sub_entries, &sub_count)
                        && sub_entries != NULL) {
                    entries[i].items = sub_entries;
                    entries[i].item_count = sub_count;

                    /* Allocate a state object for the child menu */
                    child_state = (ctxmenu_state_td *) calloc(1u,
                                sizeof(ctxmenu_state_td));
                    if (child_state != NULL) {
                        child_state->window = XCB_WINDOW_NONE;
                        entries[i].userdata = child_state;
                    }
                }
            }
        }

        ++i;
    }

    *out = entries;
    *out_count = count;
    return true;
}


/* Load a context menu entry array from a JSON file */
bool menujson_load(const char *json_path,
        ctxmenu_entry_td **out_entries, int *out_count)
{
    cJSON *json;
    cJSON *menu_arr;
    bool ok;

    if (json_path == NULL || out_entries == NULL || out_count == NULL) {
        return false;
    }

    *out_entries = NULL;
    *out_count = 0;

    if (json_load_config(json_path, &json) != 0 || json == NULL) {
        LOGGER_WARNING("Failed to load menu file '%s'", json_path);
        return false;
    }

    menu_arr = cJSON_GetObjectItemCaseSensitive(json, "menu");
    if (!cJSON_IsArray(menu_arr)) {
        LOGGER_WARNING("Menu file '%s' has no 'menu' array", json_path);
        cJSON_Delete(json);
        return false;
    }

    ok = s_parse_array(menu_arr, out_entries, out_count);
    cJSON_Delete(json);
    return ok;
}


/* Free the entry array returned by 'menujson_load' */
void menujson_free(ctxmenu_entry_td *entries, int count)
{
    if (entries == NULL) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        if (entries[i].type == CTXMENU_SUBMENU) {
            /* Free the child state allocated in 'userdata' */
            if (entries[i].userdata != NULL) {
                free(entries[i].userdata);
                entries[i].userdata = NULL;
            }
            /* Recursively free sub-entries */
            if (entries[i].items != NULL) {
                menujson_free(entries[i].items, entries[i].item_count);
                entries[i].items = NULL;
            }
        }
    }

    free(entries);
}
