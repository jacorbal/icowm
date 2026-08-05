/**
 * @file menu/context/ctxmenu.h
 *
 * @brief Generic context menu library
 *
 * Provides a reusable popup context menu that can be used by the window
 * context menu, the root desktop menu, and the window list menu.  Each
 * menu is described by an array of @c ctxmenu_entry_s structures that
 * the caller fills in before calling @c ctxmenu_show.
 *
 * Entry types:
 *   - @c CTXMENU_COMMAND  -- clickable item with a label and a command.
 *   - @c CTXMENU_SUBMENU  -- entry that opens a nested child menu.
 *   - @c CTXMENU_SEPARATOR -- non-clickable horizontal rule.
 *   - @c CTXMENU_LABEL    -- non-clickable heading text.
 *
 * At most one context menu (at any nesting level) can be visible at
 * a time.  Opening a new menu always closes the currently open one
 * first.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONTEXT_CTXMENU_H
#define MENU_CONTEXT_CTXMENU_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>     /* size_t */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Default initial values */
#include <defs/wm.h>


/**
 * @brief Label prefix prepended to non-clickable desktop headings
 *
 * Used by @c winlist_show to decorate desktop label entries.
 * Change this token to alter the visual style of all label entries.
 */
#define MENU_CONTEXT_CTXMENU_LABEL_PREFIX "--- "

/**
 * @brief Label suffix appended to non-clickable desktop headings
 *
 * Paired with @c MENU_CONTEXT_CTXMENU_LABEL_PREFIX to form the full
 * decoration.
 */
#define MENU_CONTEXT_CTXMENU_LABEL_SUFFIX " ---"


/**
 * @brief Types of entries that a context menu can contain
 */
typedef enum {
    CTXMENU_COMMAND,   /**< Clickable entry that runs a command/action */
    CTXMENU_SUBMENU,   /**< Entry that opens a nested submenu */
    CTXMENU_SEPARATOR, /**< Non-clickable horizontal separator */
    CTXMENU_LABEL      /**< Non-clickable text heading */
} ctxmenu_entry_type_e;


/**
 * @brief A single entry in a context menu
 *
 * For @c CTXMENU_COMMAND the caller provides @p label and @p command
 * (the command string) plus an optional @p on_activate callback.  For
 * @c CTXMENU_SUBMENU @p items and @p item_count describe the child
 * entries.  @c CTXMENU_SEPARATOR and @c CTXMENU_LABEL only use @p label
 * (label may be NULL for separators).  The @p is_disabled flag applies
 * to @c CTXMENU_COMMAND and @c CTXMENU_SUBMENU entries only.
 */
typedef struct ctxmenu_entry_s {
    ctxmenu_entry_type_e type;                  /**< Entry kind */
    char label[WM_CTXMENU_LABEL_MAX_LEN];       /**< Visible text */
    char command[WM_CTXMENU_CMD_MAX_LEN];       /**< Shell command (COMMAND) */
    char class_name[CONFIG_MAX_LENGTH_NAME];    /**< 'WM_CLASS' override */
    bool is_disabled;                           /**< Grayed-out when true */

    /** Optional callback invoked when the entry is activated */
    void (*on_activate)(xcb_connection_t *, void *userdata);
    void *userdata;                             /**< Passed to @p on_activate */

    /** Child entries for @c CTXMENU_SUBMENU */
    struct ctxmenu_entry_s *items;
    int item_count;
} ctxmenu_entry_td;


/**
 * @brief Context menu instance state
 *
 * Caller allocates this on the stack or statically, fills in @p entries
 * and @p entry_count, then passes it to @c ctxmenu_show.  The library
 * owns @p window after @c ctxmenu_show returns.
 */
typedef struct ctxmenu_state_s {
    xcb_window_t window;            /**< XCB window, or @c XCB_WINDOW_NONE */
    ctxmenu_entry_td *entries;      /**< Array of menu entries */
    int entry_count;                /**< Number of entries in @p entries */
    int selected;                   /**< Currently highlighted row index */
    uint16_t width;                 /**< Computed menu window width */
    uint16_t height;                /**< Computed menu window height */
    int16_t origin_x;               /**< Actual X origin after clamping */
    int16_t origin_y;               /**< Actual Y origin after clamping */

    /** Currently open child menu, or NULL */
    struct ctxmenu_state_s *child;
    /** Back-pointer to the parent menu, or NULL */
    struct ctxmenu_state_s *parent;

    xcb_connection_t *connection;   /**< Cached connection for repaints */
    const config_td *config;        /**< Cached configuration */
    surface_td *surface;            /**< Cached surface for activation */
} ctxmenu_state_td;


/* Public interface */
/**
 * @brief Create and show a context menu window
 *
 * Creates an XCB override-redirect popup window at (@p x, @p y),
 * clamped so the menu never extends beyond the work area of @p surface.
 * The menu grabs the pointer.  Any previously open context menu at the
 * same nesting level is closed first.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to display the menu
 * @param state      Menu state structure; @p entries and @p entry_count
 *                   must already be set by the caller
 * @param x          Requested X origin (root coordinates)
 * @param y          Requested Y origin (root coordinates)
 * @param config     Active configuration (theme colors and font)
 *
 * @note Complexity: @e O(n), where @e n is @p entry_count
 */
void ctxmenu_show(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        int16_t x, int16_t y, const config_td *config);

/**
 * @brief Close a context menu and its entire descendant chain
 *
 * Destroys the XCB window for @p state and recursively closes any child
 * menus that are open.  Resets all state fields to their initial
 * values.
 *
 * @param state Menu state to close
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void ctxmenu_close(ctxmenu_state_td *state);

/**
 * @brief Repaint the context menu window
 *
 * Called from the expose handler when @p state->window receives an
 * expose event.  Redraws all entries using the cached configuration.
 *
 * @param state Menu state to repaint
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
void ctxmenu_repaint(ctxmenu_state_td *state);

/**
 * @brief Handle a button-press event inside a context menu window
 *
 * Activates the entry at the pointer coordinates.  For
 * @c CTXMENU_COMMAND entries, invokes @c on_activate if set and then
 * closes the whole menu hierarchy.  For @c CTXMENU_SUBMENU entries,
 * opens the child menu.  Disabled entries are ignored.
 *
 * @param connection XCB connection
 * @param surface    Surface on which the menu is displayed
 * @param state      Menu state that owns the window receiving the event
 * @param x          Pointer X relative to the menu window
 * @param y          Pointer Y relative to the menu window
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed, @c false otherwise
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        int x, int y, const config_td *config);

/**
 * @brief Query whether the context menu (or any child) is currently open
 *
 * @param state Menu state to inspect
 *
 * @return @c true when the menu window exists
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_is_open(const ctxmenu_state_td *state);

/**
 * @brief Return the deepest open window in the menu hierarchy
 *
 * Walks the child chain from @p state and returns the window of the
 * deepest open menu.  If @p state itself has no child, returns
 * @p state->window.
 *
 * @param state Root menu state
 *
 * @return Window ID of the deepest open menu, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
xcb_window_t ctxmenu_deepest_window(const ctxmenu_state_td *state);

/**
 * @brief Find the state owning the given XCB window
 *
 * Searches @p state and all its open descendants for the one whose
 * @p window matches @p win.
 *
 * @param state Root menu state to search from
 * @param win   XCB window to find
 *
 * @return Pointer to the matching state, or @c NULL if not found
 *
 * @note Complexity: @e O(d), where @e d is the nesting depth
 */
ctxmenu_state_td *ctxmenu_find_state_for_window(ctxmenu_state_td *state,
        xcb_window_t win);

/**
 * @brief Close the context menu when a click occurs outside all its
 *        windows
 *
 * Called when a button-press event arrives on a window that is not part
 * of the open menu hierarchy.  Closes the entire menu.
 *
 * @param state Root menu state
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void ctxmenu_close_on_outside_click(ctxmenu_state_td *state);

/**
 * @brief Handle a key-press event while a context menu is open
 *
 * When a printable character is pressed, scans the open @p state for
 * entries whose label starts with that character (case-insensitive).
 * If exactly one match is found the entry is activated immediately.  If
 * more than one match is found the first match is highlighted and the
 * function returns @c true without activating.
 *
 * @param state  Root menu state (the currently visible level)
 * @param keysym X keysym of the pressed key
 *
 * @return @c true if the event was consumed, @c false otherwise
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
bool ctxmenu_handle_keypress(ctxmenu_state_td *state,
        xcb_keysym_t keysym);


#endif  /* ! MENU_CONTEXT_CTXMENU_H */
