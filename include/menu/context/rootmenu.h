/**
 * @file menu/context/rootmenu.h
 *
 * @brief Root desktop menu (right-click on empty desktop)
 *
 * Displays a popup menu when the user right-clicks on the root window
 * (empty desktop).  The menu entries come from @c menu.json in the
 * configuration directory, followed by a fixed footer:
 *
 * @code{.unparsed}
 * ----------------------
 * Strutless maximization
 * Rearrange windows
 * Reload configuration
 * Redraw all windows
 * ----------------------
 * Exit
 * @endcode
 *
 * @note "Strutless maximization", "Rearrange windows", "Reload
 *       configuration", "Redraw all windows", and "Exit" map directly
 *       to the corresponding window manager keyboard actions, if set
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

#ifndef MENU_CONTEXT_ROOTMENU_H
#define MENU_CONTEXT_ROOTMENU_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Default initial values */
#include <defs/ctxmenu.h>



/**
 * @brief Number of fixed footer entries appended after the JSON
 *        entries: <separator>, "Strutless maximization"/"Strutted
 *        maximization", "Rearrange windows", "Reload configuration",
 *        "Redraw all windows", <separator>, "Exit"
 */
#define ROOTMENU_FOOTER_COUNT (7)

/**
 * @brief Maximum total number of root menu entries
 */
#define ROOTMENU_MAX_ENTRIES \
    (WM_CTXMENU_MAX_ENTRIES + ROOTMENU_FOOTER_COUNT)

/**
 * @brief Maximum path length for the @c menu.json file path
 */
#define ROOTMENU_PATH_MAX (512)


/**
 * @brief Load (or reload) @c menu.json's entries
 *
 * Parses @c menu.json (located in the active configuration directory)
 * once into a persistent buffer that every subsequent @c rootmenu_show
 * reuses as-is, rather than re-parsing the file from disk on every
 * single menu open the way every other one of this window manager's
 * JSON configuration files is not.
 *
 * @param config_dir Path to the configuration directory (used to locate
 *                   @c menu.json)
 *
 * @note A no-op, not a failure, if @c menu.json does not exist or fails
 *       to parse; the root menu simply shows its fixed footer with no
 *       JSON entries above it
 * @note Call once at startup and again on every configuration reload,
 *       never from @a rootmenu_show itself
 * @note Safe to call again later; a previous call's entries, if any,
 *       are freed first
 * @note Complexity: @e O(n), where @e n is the total number of menu
 *       entries in @c menu.json
 */
void rootmenu_menu_json_load(const char *config_dir);

/**
 * @brief Free the entries loaded by @c rootmenu_menu_json_load
 *
 * @note Call at window manager shutdown
 * @note Safe to call even if nothing was ever loaded
 * @note Complexity: @e O(1)
 */
void rootmenu_menu_json_free(void);

/**
 * @brief Display the root desktop menu
 *
 * Combines the entries @a rootmenu_menu_json_load already parsed with
 * the fixed footer entries, and shows the result at @p pos.
 *
 * @param wm         Window manager instance, cached for the callbacks
 *                   of the "Rearrange" and "Reload configuration"
 *                   entries
 * @param connection XCB connection
 * @param surface    Surface on which to display the menu
 * @param pos        Requested origin (root coordinates)
 * @param config     Active configuration
 *
 * @note Any previously open root menu is closed first
 * @note Complexity: @e O(n), where @e n is the total number of menu
 *       entries
 */
void rootmenu_show(wm_td *wm, xcb_connection_t *connection,
        surface_td *surface, struct position_s pos,
        const config_td *config);

/**
 * @brief Close the root desktop menu
 *
 * Destroys the menu window and frees the combined entry buffer built
 * for this particular open (JSON entries plus footer).  The underlying
 * JSON-loaded entries themselves are untouched, as they stay loaded for
 * the next @a rootmenu_show, and are only freed by
 * @a rootmenu_menu_json_load (on the next reload) or
 * @a rootmenu_menu_json_free (at shutdown).
 *
 * @note Complexity: @e O(n)
 */
void rootmenu_close(void);

/**
 * @brief Repaint the root desktop menu
 *
 * @param win Window that received the expose event
 *
 * @note Complexity: @e O(n)
 */
void rootmenu_repaint(xcb_window_t win);

/**
 * @brief Handle a button-press event inside the root desktop menu
 *
 * @param connection XCB connection
 * @param surface    Surface associated with the event
 * @param win        Window that received the press
 * @param root_y     Pointer Y in root (screen) coordinates
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(1)
 */
bool rootmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int root_y,
        const config_td *config);

/**
 * @brief Query whether the root desktop menu is currently open
 *
 * @return @c true when the menu is visible
 *
 * @note Complexity: @e O(1)
 */
bool rootmenu_is_open(void);

/**
 * @brief Check whether @p win belongs to the root menu hierarchy
 *
 * @param win XCB window to test
 *
 * @return @c true if @p win is part of the currently open menu
 *
 * @note Complexity: @e O(d)
 */
bool rootmenu_owns_window(xcb_window_t win);

/**
 * @brief Handle a key-press event while the root desktop menu is open
 *
 * Forwards the key event to the deepest open menu level in the root
 * menu hierarchy.
 *
 * @param connection XCB connection
 * @param surface    Surface on which the menu is displayed
 * @param keysym     X keysym of the pressed key
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(n), where @e n is the number of menu entries
 */
bool rootmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config);

/**
 * @brief Handle a pointer-motion event over a root desktop menu window
 *
 * Finds the menu state that owns @p win and updates its hover
 * highlight.
 *
 * @param win Window that received the motion event
 * @param x   Pointer X relative to @p win
 * @param y   Pointer Y relative to @p win
 */
void rootmenu_handle_motion(xcb_window_t win, int x, int y);


#endif  /* ! MENU_CONTEXT_ROOTMENU_H */
