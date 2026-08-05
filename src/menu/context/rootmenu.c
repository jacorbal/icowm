/**
 * @file menu/context/rootmenu.c
 *
 * @brief Root desktop menu implementation
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdlib.h>     /* malloc, free, calloc */
#include <string.h>     /* memset, snprintf */
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Def includes */
#include <defs/config.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/menujson.h>
#include <menu/context/rootmenu.h>
#include <menu/dialog/quit.h>


/** Surface stored at open time (needed by the exit callback) */
static surface_td *s_surface = NULL;

/** Config stored at open time (needed by the exit callback) */
static const config_td *s_config = NULL;


/**
 * @brief Number of fixed footer entries appended after the JSON
 *        entries: separator, "Reload configuration", "Redraw all
 *        windows", separator, "Exit"
 */
#define ROOTMENU_FOOTER_COUNT (5)

/**
 * @brief Maximum total number of root menu entries
 */
#define ROOTMENU_MAX_ENTRIES \
    (WM_CTXMENU_MAX_ENTRIES + ROOTMENU_FOOTER_COUNT)

/**
 * @brief Maximum path length for the @c menu.json file path
 */
#define ROOTMENU_PATH_MAX (512)


/** Singleton root menu state */
static ctxmenu_state_td s_root;

/** Combined entries: JSON entries + footer entries */
static ctxmenu_entry_td *s_entries = NULL;

/** Total number of entries currently in @a s_entries */
static int s_entry_count = 0;

/** Entries loaded from the JSON file (freed on close) */
static ctxmenu_entry_td *s_json_entries = NULL;

/** Number of JSON-loaded entries */
static int s_json_count = 0;


/**
 * @brief Callback: reload the window manager configuration
 *
 * @param connection XCB connection (unused)
 * @param userdata   Unused
 */
static void s_cb_reload(xcb_connection_t *connection, void *userdata)
{
    (void) connection;
    (void) userdata;
    LOGGER_DEBUG("Reloading root menu configuration", L_NARG);
    (void) wm_action_config_reload();
}


/**
 * @brief Callback: request a full window manager redraw
 *
 * @param connection XCB connection (unused)
 * @param userdata   Unused
 */
static void s_cb_redraw(xcb_connection_t *connection, void *userdata)
{
    (void) connection;
    (void) userdata;
    wm_request_full_redraw();
}


/**
 * @brief Callback: open the quit confirmation dialog
 *
 * Shows the same confirmation dialog as the keyboard "exit" binding
 * instead of exiting directly.
 *
 * @param connection XCB connection
 * @param userdata   Unused
 */
static void s_cb_exit(xcb_connection_t *connection, void *userdata)
{
    (void) userdata;

    if (connection != NULL && s_surface != NULL && s_config != NULL) {
        dialog_quit_show(connection, s_surface, s_config);
    }
}


/* Load and display the root desktop menu */
void rootmenu_show(xcb_connection_t *connection,
        surface_td *surface, int16_t x, int16_t y,
        const config_td *config, const char *config_dir)
{
    char menu_path[ROOTMENU_PATH_MAX];
    int n;
    int copy_count;
    int fi;
    ctxmenu_entry_td *json_entries = NULL;
    int json_count = 0;
    const char *xdg;
    const char *home;

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    rootmenu_close();

    /* Cache surface and config for use by the exit callback */
    s_surface = surface;
    s_config = config;

    /* Build path to menu.json */
    if (config_dir != NULL && config_dir[0] != '\0') {
        (void) snprintf(menu_path, sizeof(menu_path),
                "%s/%s", config_dir, CONFIG_FILENAME_MENU);
    } else {
        xdg = getenv("XDG_CONFIG_HOME");
        home = getenv("HOME");
        if (xdg != NULL) {
            (void) snprintf(menu_path, sizeof(menu_path),
                    "%s/%s/%s", xdg, CONFIG_DIR_BASE, CONFIG_FILENAME_MENU);
        } else if (home != NULL) {
            (void) snprintf(menu_path, sizeof(menu_path),
                    "%s/.%s/%s", home, CONFIG_DIR_BASE, CONFIG_FILENAME_MENU);
        } else {
            (void) snprintf(menu_path, sizeof(menu_path),
                    "./%s/%s", CONFIG_DIR_BASE, CONFIG_FILENAME_MENU);
        }
    }

    /* Try to load entries from the JSON file (failure is non-fatal) */
    (void) menujson_load(menu_path, &json_entries, &json_count);

    /* Total entries: JSON entries + footer */
    /* The leading separator is only added when JSON entries are present
     * so the footer is not preceded by a bare separator when
     * 'menu.json' is missing or empty */
    n = json_count + ROOTMENU_FOOTER_COUNT - (json_count == 0 ? 1 : 0);
    if (n > ROOTMENU_MAX_ENTRIES) {
        n = ROOTMENU_MAX_ENTRIES;
    }

    s_entries = (ctxmenu_entry_td *) calloc((size_t) n,
            sizeof(ctxmenu_entry_td));
    if (s_entries == NULL) {
        menujson_free(json_entries, json_count);
        return;
    }

    /* Copy JSON-loaded entries */
    copy_count = json_count;
    for (int i = 0; i < copy_count; ++i) {
        s_entries[i] = json_entries[i];
    }
    s_json_entries = json_entries;
    s_json_count = json_count;

    /* Footer: [<separator> if JSON is present], Reload, Redraw,
     * <separator>, Exit */
    fi = copy_count;

    if (copy_count > 0) {
        s_entries[fi].type = CTXMENU_SEPARATOR;
        ++fi;
    }

    s_entries[fi].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[fi].label, "Reload configuration",
            sizeof(s_entries[fi].label) - 1u);
    s_entries[fi].on_activate = s_cb_reload;
    ++fi;

    s_entries[fi].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[fi].label, "Redraw all windows",
            sizeof(s_entries[fi].label) - 1u);
    s_entries[fi].on_activate = s_cb_redraw;
    ++fi;

    s_entries[fi].type = CTXMENU_SEPARATOR;
    ++fi;

    s_entries[fi].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[fi].label, "Exit",
            sizeof(s_entries[fi].label) - 1u);
    s_entries[fi].on_activate = s_cb_exit;
    ++fi;

    s_entry_count = fi;

    memset(&s_root, 0, sizeof(s_root));
    s_root.window = XCB_WINDOW_NONE;
    s_root.entries = s_entries;
    s_root.entry_count = s_entry_count;

    ctxmenu_show(connection, surface, &s_root, x, y, config);
}


/* Close the root desktop menu */
void rootmenu_close(void)
{
    ctxmenu_close(&s_root);

    if (s_json_entries != NULL) {
        menujson_free(s_json_entries, s_json_count);
        s_json_entries = NULL;
        s_json_count = 0;
    }

    if (s_entries != NULL) {
        free(s_entries);
        s_entries = NULL;
    }

    s_entry_count = 0;
    s_surface = NULL;
    s_config = NULL;
}


/* Repaint the root desktop menu */
void rootmenu_repaint(xcb_window_t win)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state != NULL) {
        ctxmenu_repaint(state);
    }
}


/* Handle a button-press event inside the root desktop menu */
bool rootmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int x, int y,
        const config_td *config)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state == NULL) {
        return false;
    }
    return ctxmenu_handle_click(connection, surface, state,
            x, y, config);
}


/* Query whether the root desktop menu is currently open */
bool rootmenu_is_open(void)
{
    return ctxmenu_is_open(&s_root);
}


/* Return the root desktop menu XCB window */
xcb_window_t rootmenu_window(void)
{
    return s_root.window;
}


/* Check whether 'win' belongs to the root menu hierarchy */
bool rootmenu_owns_window(xcb_window_t win)
{
    return ctxmenu_find_state_for_window(&s_root, win) != NULL;
}
