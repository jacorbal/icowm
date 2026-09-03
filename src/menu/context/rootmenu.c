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
#include <stdint.h>
#include <stdlib.h>     /* free, calloc, getenv */
#include <string.h>     /* memset, snprintf */
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/ctxmenu.h>
#include <defs/uistr.h>
#include <i18n.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <config.h>
#include <enact.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Def includes */
#include <defs/config.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/tree.h>
#include <menu/context/menujson.h>
#include <menu/context/rootmenu.h>
#include <menu/dialog/quit.h>


/**
 * @brief Surface stored at open time (needed by the exit callback)
 */
static surface_td *s_surface = NULL;

/**
 * @brief Config stored at open time (needed by the exit callback)
 */
static const config_td *s_config = NULL;

/**
 * @brief Window manager instance stored at open time (needed by the
 *       "Rearrange" and "Reload configuration" callbacks)
 */
static wm_td *s_wm = NULL;

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
 * @brief Callback: rearrange every visible window on the current
 *        desktop
 *
 * @param connection XCB connection (unused)
 * @param userdata   Unused
 */
static void s_cb_rearrange(xcb_connection_t *connection, void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_surface != NULL) {
        wm_action_rearrange(s_wm, s_surface);
    }
}


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
    (void) wm_action_config_reload(s_wm);
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
 * @brief Callback: toggle whether panel/tray struts are set aside on
 *        the surface this menu was opened on
 *
 * A surface-wide setting, not a per-window one, so it lives here rather
 * than in the window context menu ('Alt+Space').
 *
 * @param connection Unused, matches @c ctxmenu_on_activate_fn's
 *                   signature
 * @param userdata   Unused
 */
static void s_cb_toggle_strutless_maximize(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_surface != NULL) {
        enact_surface_toggle_strutless_maximize(s_surface);
    }
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


/* Load (or reload) 'menu.json''s entries; see this function's comment
 * in 'menu/context/rootmenu.h' */
void rootmenu_menu_json_load(const char *config_dir)
{
    char menu_path[ROOTMENU_PATH_MAX];
    ctxmenu_entry_td *json_entries = NULL;
    int json_count = 0;

    if (s_json_entries != NULL) {
        menujson_free(s_json_entries, s_json_count);
        s_json_entries = NULL;
        s_json_count = 0;
    }

    /* Build path to menu.json */
    if (config_dir != NULL && config_dir[0] != '\0') {
        (void) snprintf(menu_path, sizeof(menu_path),
                "%s/%s", config_dir, CONFIG_FILENAME_MENU);
    } else {
        const char *xdg = getenv("XDG_CONFIG_HOME");
        const char *home = getenv("HOME");

        if (xdg != NULL) {
            (void) snprintf(menu_path, sizeof(menu_path),
                    "%s/%s/%s", xdg,
                    CONFIG_DIR_BASE, CONFIG_FILENAME_MENU);
        } else if (home != NULL) {
            (void) snprintf(menu_path, sizeof(menu_path),
                    "%s/.%s/%s", home,
                    CONFIG_DIR_BASE, CONFIG_FILENAME_MENU);
        } else {
            (void) snprintf(menu_path, sizeof(menu_path),
                    "./%s/%s",
                    CONFIG_DIR_BASE, CONFIG_FILENAME_MENU);
        }
    }

    /* Failure is non-fatal: an absent or unparsable 'menu.json' just
     * leaves the root menu showing its fixed footer with no JSON
     * entries above it.  Not followed by 'wm_json_syntax_errors_warn'
     * here.  Both of this function's callers (startup, in 'wm.c';
     * reload, in 'wm/actions.c') already call it themselves once
     * everything for that pass has finished loading, so calling it here
     * too would just show the same warning dialog for the same pass
     * a second time. */
    (void) menujson_load(menu_path, &json_entries, &json_count);

    s_json_entries = json_entries;
    s_json_count = json_count;
}


/* Free the entries loaded by 'rootmenu_menu_json_load'; see this
 * function's comment in 'menu/context/rootmenu.h' */
void rootmenu_menu_json_free(void)
{
    if (s_json_entries != NULL) {
        menujson_free(s_json_entries, s_json_count);
        s_json_entries = NULL;
        s_json_count = 0;
    }
}


/* Display the root desktop menu; see this function's comment in
 * 'menu/context/rootmenu.h' */
void rootmenu_show(wm_td *wm, xcb_connection_t *connection,
        surface_td *surface, struct position_s pos,
        const config_td *config)
{
    int n;
    int copy_count;
    int fi;

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    rootmenu_close();

    /* Cache surface, config, and wm for use by the callbacks below */
    s_surface = surface;
    s_config = config;
    s_wm = wm;

    /* Total entries: JSON entries + footer */
    /* The leading separator is only added when JSON entries are present
     * so the footer is not preceded by a bare separator when
     * 'menu.json' is missing or empty */
    n = s_json_count + ROOTMENU_FOOTER_COUNT -
        ((s_json_count == 0) ? 1 : 0);
    if (n > ROOTMENU_MAX_ENTRIES) {
        n = ROOTMENU_MAX_ENTRIES;
    }

    s_entries = (ctxmenu_entry_td *) calloc((size_t) n,
            sizeof(ctxmenu_entry_td));
    if (s_entries == NULL) {
        return;
    }

    /* Copy the already-loaded JSON entries.  'command'/'class_name' are
     * deliberately deep-copied here via their fresh 'safe_strndup', not
     * shared with 's_json_entries' strings: 'reload_config' (also
     * reachable through the IPC command of the same name, not just the
     * keybind this menu's keyboard grab would otherwise block while
     * open) can free and replace 's_json_entries' at any moment,
     * including while this exact 's_entries' copy is still the one
     * 'ctxmenu_show' is actively displaying; sharing the pointer
     * instead would leave 's_entries' holding a dangling one the
     * instant that happened. */
    /* Clamped to the room the array has, since the footer below is
     * written after these entries and needs the slots reserved for it.
     * 'menu.json' may hold any number of them */
    copy_count = s_json_count;
    if (copy_count > n - ROOTMENU_FOOTER_COUNT) {
        copy_count = n - ROOTMENU_FOOTER_COUNT;
        LOGGER_WARNING("Root menu holds %d entries; showing the first" \
                " %d, which is all '%s' has room for",
                s_json_count, copy_count, "WM_CTXMENU_MAX_ENTRIES");
    }

    for (int i = 0; i < copy_count; ++i) {
        s_entries[i] = s_json_entries[i];
        s_entries[i].command = safe_strndup(s_json_entries[i].command,
                (size_t) WM_CTXMENU_CMD_MAX_LENGTH - 1u);
        s_entries[i].class_name = safe_strndup(
                s_json_entries[i].class_name,
                (size_t) CONFIG_MAX_LENGTH_NAME - 1u);
    }

    /* Footer: [<separator> if JSON is present], Reload, Redraw,
     * <separator>, Exit */
    fi = copy_count;

    if (copy_count > 0) {
        s_entries[fi].type = CTXMENU_SEPARATOR;
        ++fi;
    }

    s_entries[fi].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[fi].label,
            (surface->strutless_maximize)
                ? _(STR_ROOTMENU_STRUTTED_MAXIMIZATION)
                : _(STR_ROOTMENU_STRUTLESS_MAXIMIZATION),
            sizeof(s_entries[fi].label) - 1u);
    s_entries[fi].on_activate = s_cb_toggle_strutless_maximize;
    ++fi;

    s_entries[fi].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[fi].label, _(STR_ROOTMENU_REARRANGE),
            sizeof(s_entries[fi].label) - 1u);
    s_entries[fi].on_activate = s_cb_rearrange;
    ++fi;

    s_entries[fi].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[fi].label, _(STR_ROOTMENU_RELOAD_CONFIG),
            sizeof(s_entries[fi].label) - 1u);
    s_entries[fi].on_activate = s_cb_reload;
    ++fi;

    s_entries[fi].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[fi].label, _(STR_ROOTMENU_REDRAW_ALL),
            sizeof(s_entries[fi].label) - 1u);
    s_entries[fi].on_activate = s_cb_redraw;
    ++fi;

    s_entries[fi].type = CTXMENU_SEPARATOR;
    ++fi;

    s_entries[fi].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[fi].label, _(STR_ROOTMENU_EXIT),
            sizeof(s_entries[fi].label) - 1u);
    s_entries[fi].on_activate = s_cb_exit;
    ++fi;

    s_entry_count = fi;

    memset(&s_root, 0, sizeof(s_root));
    s_root.window = XCB_WINDOW_NONE;
    s_root.entries = s_entries;
    s_root.entry_count = s_entry_count;

    ctxmenu_show(connection, surface, &s_root, pos, config);
}


/* Close the root desktop menu */
void rootmenu_close(void)
{
    ctxmenu_close(&s_root);

    /* Deliberately not freed here: 's_json_entries' persists across
     * opens/closes, and is only ever replaced (on reload) or freed (at
     * shutdown) by 'rootmenu_menu_json_load' or by
     * 'rootmenu_menu_json_free' (read their comments for a change) */
    if (s_entries != NULL) {
        /* 'command'/'class_name' alone, of everything in each entry,
         * are 's_entries' independent copies rather than shared with
         * 's_json_entries'; see 'rootmenu_show''s comment on why, right
         * where they are copied. */
        for (int i = 0; i < s_entry_count; ++i) {
            free(s_entries[i].command);
            free(s_entries[i].class_name);
        }
        free(s_entries);
        s_entries = NULL;
    }

    s_entry_count = 0;
    s_surface = NULL;
    s_config = NULL;
    s_wm = NULL;
}


/* Repaint the root desktop menu */
void rootmenu_repaint(xcb_window_t win)
{
    ctxmenu_tree_redraw_window(&s_root, win);
}


/* Handle a button-press event inside the root desktop menu */
bool rootmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int y,
        const config_td *config)
{
    return ctxmenu_tree_handle_click_window(connection, surface,
            &s_root, win, y, config);
}


/* Query whether the root desktop menu is currently open */
bool rootmenu_is_open(void)
{
    return ctxmenu_is_open(&s_root);
}


/* Check whether 'win' belongs to the root menu hierarchy */
bool rootmenu_owns_window(xcb_window_t win)
{
    return ctxmenu_tree_state_find_for_window(&s_root, win) != NULL;
}


/* Handle a key-press event while the root desktop menu is open */
bool rootmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config)
{
    return ctxmenu_tree_handle_keypress_deepest(connection, surface,
            &s_root, keysym, config);
}


/* Handle a pointer-motion event over the root desktop menu */
void rootmenu_handle_motion(xcb_window_t win, int x, int y)
{
    ctxmenu_tree_handle_motion_window(&s_root, win, x, y);
}
