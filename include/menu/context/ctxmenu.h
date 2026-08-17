/**
 * @file menu/context/ctxmenu.h
 *
 * @brief Generic context menu library
 *
 * Provides a reusable popup context menu that can be used by the window
 * context menu, the root desktop menu, and the window list menu.  Each
 * menu is described by an array of @p ctxmenu_entry_s structures that
 * the caller fills in before calling @a ctxmenu_show.
 *
 * Entry types:
 *
 * - @c CTXMENU_COMMAND: clickable item with a label and a command;
 * - @c CTXMENU_SUBMENU: entry that opens a nested child menu;
 * - @c CTXMENU_SEPARATOR: non-clickable horizontal rule; and
 * - @c CTXMENU_LABEL: non-clickable heading text.
 *
 * At most one context menu (at any nesting level) can be visible at
 * a time.  Opening a new menu always closes the currently open one
 * first.
 *
 * @defgroup menu_context Context menus
 * @ingroup menu
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
#include <xcb/xcb_keysyms.h>

/* Project includes */
#include <config.h>
#include <render/wmicon.h>
#include <surface.h>

/* Default initial values */
#include <defs/ctxmenu.h>


/**
 * @brief Label prefix prepended to non-clickable desktop headings
 *
 * Used by @a winlist_show to decorate desktop label entries.  Change
 * this token to alter the visual style of all label entries.
 *
 * @note The prefix goes without separation before the next word unless
 *       it's specified here
 */
#define MENU_CONTEXT_CTXMENU_LABEL_PREFIX ""    //"--- "

/**
 * @brief Label suffix appended to non-clickable desktop headings
 *
 * Paired with @c MENU_CONTEXT_CTXMENU_LABEL_PREFIX to form the full
 * decoration.
 *
 * @note The suffix goes without separation after the next word unless
 *       it's specified here
 */
#define MENU_CONTEXT_CTXMENU_LABEL_SUFFIX ""    //" ---"

/**
 * @brief Label to indicate this menu item is a submenu
 */
#define MENU_CONTEXT_CTXMENU_SUBMENU_ARROW ">"


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
 * (label may be null for separators).  The @p is_disabled flag applies
 * to @c CTXMENU_COMMAND and @c CTXMENU_SUBMENU entries only.
 */
typedef struct ctxmenu_entry_s {
    ctxmenu_entry_type_e type;                  /**< Entry kind */
    char label[WM_CTXMENU_LABEL_MAX_LENGTH];    /**< Visible text */
    char command[WM_CTXMENU_CMD_MAX_LENGTH];    /**< Shell command */
    char class_name[CONFIG_MAX_LENGTH_NAME];    /**< @c WM_CLASS override */
    bool is_disabled;                           /**< Grayed-out if @c true */

    /** Optional callback invoked when the entry is activated */
    void (*on_activate)(xcb_connection_t *, void *userdata);
    void *userdata;     /**< User data passed to @p on_activate */

    /** Child entries for @c CTXMENU_SUBMENU */
    struct ctxmenu_entry_s *items;
    int item_count;

    /**
     * @brief Client window whose own icon to draw to this entry's left,
     *        or @c XCB_WINDOW_NONE for an entry with no associated
     *        client
     *
     * Only ever set by a caller whose entries genuinely represent
     * client windows (@c menu/context/winlist.c); left at its default
     * of @c XCB_WINDOW_NONE elsewhere, which reserves no icon space and
     * draws no icon regardless of @p theme.menu.show-pixmaps.
     *
     * @see For that setting full behavior, see @c config.h
     */
    xcb_window_t icon_window;

    /**
     * @brief That client's own icon cache slot reused across repaints
     *        the same way the client's own desktop icon does
     *
     * Ignored when @p icon_window is @c XCB_WINDOW_NONE.  A pointer
     * into storage this struct does not own, since the client (and its
     * cache slot) outlives any one menu that happens to list it.
     */
    wmicon_cache_td *icon_cache;
} ctxmenu_entry_td;


/**
 * @brief Context menu instance state
 *
 * Caller allocates this on the stack or statically, fills in @p entries
 * and @p entry_count, then passes it to @a ctxmenu_show.  The library
 * owns @p window after @a ctxmenu_show returns.
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

    /**
     * @brief Cached top-Y pixel offset per entry
     *
     * Allocated by @a ctxmenu_show (size @p entry_count) and freed by
     * @a ctxmenu_close.  It's null when the allocation failed, in which
     * case row lookups fall back to walking @p entries directly
     */
    int32_t *entry_top_y;

    /**
     * @brief Window-relative Y of the last @c MotionNotify actually
     *        processed by @a ctxmenu_handle_motion, or @c -1 before the
     *        first one
     *
     * Used to ignore a motion event that reports the exact same
     * position as the last one.  X can deliver such a "no-op" event
     * right after a submenu maps under an already-resting pointer,
     * which would otherwise silently override a selection just made
     * with the keyboard even though the mouse never actually moved.
     */
    int32_t last_motion_y;

    /** Currently open child menu, or null */
    struct ctxmenu_state_s *child;
    /** Back-pointer to the parent menu, or null */
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
 * @c CTXMENU_COMMAND entries, invokes @p on_activate if set and then
 * closes the whole menu hierarchy.  For @c CTXMENU_SUBMENU entries,
 * opens the child menu.  Disabled entries are ignored.
 *
 * @param connection XCB connection
 * @param surface    Surface on which the menu is displayed
 * @param state      Menu state that owns the window receiving the event
 * @param y          Pointer Y in root (screen) coordinates
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        int y, const config_td *config);

/**
 * @brief Query whether the context menu (or any child) is currently
 *        open
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
 * Dispatches navigation and activation keys:
 *
 * - @c Up / @c Down arrows: move the selection highlight to the
 *   previous or next selectable entry (skipping separators and labels),
 *   wrapping around at the ends.
 * - @c Right arrow: if the currently selected entry is a submenu, open
 *   it; otherwise no action.
 * - @c Left arrow: if @p state has a parent (i.e., it is a submenu),
 *   close this submenu and return focus to the parent.
 * - @c Return / @c KP_Enter: activate the currently selected entry.
 * - @c Escape: close the entire menu hierarchy from the root.
 * - Any printable character: scan entries whose label begins with that
 *   character (case-insensitive); if exactly one match is found the
 *   entry is activated immediately; if more than one match is found the
 *   first match is highlighted without activating.
 *
 * The @p state parameter should be the deepest currently open level
 * (i.e., the visible submenu, or the root if no submenu is open).
 *
 * @param connection XCB connection (used to open submenus)
 * @param surface    Surface on which the menu is displayed
 * @param state      Deepest open menu state
 * @param keysym     X keysym of the pressed key
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
bool ctxmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        xcb_keysym_t keysym, const config_td *config);

/**
 * @brief Handle a pointer-motion event inside a context menu window
 *
 * Updates the hover highlight to the entry under the pointer position
 * @p y (relative to the menu window top edge).  Non-selectable entries
 * (separators, labels, disabled items) clear the selection instead of
 * highlighting.  Repaints the menu only when the selection changes.
 *
 * @param state Menu state that owns the window the pointer is over
 * @param x     Pointer X relative to the menu window (unused; kept for
 *              future use)
 * @param y     Pointer Y relative to the menu window top edge
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
void ctxmenu_handle_motion(ctxmenu_state_td *state, int x, int y);

/**
 * @brief Query whether the last activated entry was triggered by the
 *        keyboard rather than a mouse click
 *
 * Set right before an entry's @p on_activate callback runs: @c true
 * when activation came from @c Return / @c KP_Enter or a printable
 * character shortcut inside @a ctxmenu_handle_keypress, @c false when
 * it came from @a ctxmenu_handle_click.  Callbacks that need to behave
 * differently for keyboard vs. mouse activation (e.g., window move or
 * resize, which use keyboard modal mode vs. a pointer drag) should
 * query this at the top of @p on_activate.
 *
 * @return @c true if the most recent activation was keyboard-driven
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_last_activation_was_keyboard(void);

/**
 * @brief Repaint whichever submenu under @p root currently owns @p win
 *
 * Shared by every concrete menu's own @c X_repaint (root menu, window
 * menu, window list): each one only differs in which @p root state it
 * passes, so this one function replaces an identical lookup-then-
 * repaint sequence that used to be copied into each of them.
 *
 * @param root Top-level state of the concrete menu's own submenu tree
 * @param win  Window the repaint request arrived for
 *
 * @note No-op if @p win does not belong to any submenu under @p root
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void ctxmenu_repaint_window(ctxmenu_state_td *root, xcb_window_t win);

/**
 * @brief Forward a pointer-motion event to whichever submenu under
 *        @p root currently owns @p win
 *
 * Shared by every concrete menu's own @c X_handle_motion; see
 * @a ctxmenu_repaint_window's comment for the general reasoning.
 *
 * @param root Top-level state of the concrete menu's own submenu tree
 * @param win  Window the motion event arrived for
 * @param x    Pointer X position, in @p win's own coordinates
 * @param y    Pointer Y position, in @p win's own coordinates
 *
 * @note No-op if @p win does not belong to any submenu under @p root
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void ctxmenu_handle_motion_window(ctxmenu_state_td *root,
        xcb_window_t win, int x, int y);

/**
 * @brief Forward a click, translated to menu-local coordinates, to
 *        whichever submenu under @p root currently owns @p win
 *
 * Shared by every concrete menu's own @c X_handle_click.
 *
 * @param connection XCB connection
 * @param surface    Surface the click occurred on
 * @param root       Top-level state of the concrete menu's own
 *                   submenu tree
 * @param win        Window the click event arrived for
 * @param y          Pointer Y position, in @p win's own coordinates
 * @param config     Active configuration
 *
 * @return @c true if @p win belonged to a submenu under @p root and
 *         the click was forwarded
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 *
 * @see @a ctxmenu_repaint_window
 */
bool ctxmenu_handle_click_window(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root, xcb_window_t win,
        int y, const config_td *config);

/**
 * @brief Forward a keypress to the deepest currently open submenu
 *        under @p root
 *
 * Applies the keypress to the deepest open submenu, not always @p root
 * itself.  Without this, arrow keys would keep moving the selection in
 * a top-level list even while a nested submenu was open in front of it,
 * making that submenu look unresponsive to the keyboard.  Shared by
 * every concrete menu's own @c X_handle_keypress.
 *
 * @param connection XCB connection
 * @param surface    Surface the key press occurred on
 * @param root       Top-level state of the concrete menu's own
 *                   submenu tree
 * @param keysym     Keysym of the pressed key
 * @param config     Active configuration
 *
 * @return @c true if the key was consumed
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
bool ctxmenu_handle_keypress_deepest(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root,
        xcb_keysym_t keysym, const config_td *config);


#endif  /* ! MENU_CONTEXT_CTXMENU_H */
