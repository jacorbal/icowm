/**
 * @file menu/context/ctxmenu.h
 *
 * @brief Generic context menu library: shared types and window
 *        lifecycle
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
 * Split by competency into @c ctxmenu/layout.h (row geometry and
 * hit-testing), @c ctxmenu/redraw.h (painting), @c ctxmenu/select.h
 * (selection and activation), @c ctxmenu/handle.h (raw event
 * handling for a single window, private to this subsystem), and
 * @c ctxmenu/tree.h (dispatch across a submenu window tree, the
 * public entry point every concrete menu actually uses).  This
 * header keeps only the shared types every one of those needs, plus
 * the window lifecycle (@a ctxmenu_show, @a ctxmenu_close) and
 * @a ctxmenu_is_open, which belong to no single one of them.
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
    /**
     * @brief Shell command, and an optional @c WM_CLASS override to
     *        raise instead of relaunching if a matching window
     *        already exists, both heap-allocated (@c NULL when unset)
     *
     * Only ever set for a @c CTXMENU_COMMAND entry built from
     * @c menu.json (@c menu/context/menujson.c); every other entry
     * (every one built directly from C code instead, with its own
     * @p on_activate below) leaves both @c NULL.  Kept as owned,
     * individually allocated strings rather than fixed-size buffers
     * inline in this struct, since only a small, session-long-lived
     * set of entries (the ones @c menu.json itself defines) ever
     * needs them at all, while this struct's own many other array
     * slots (one per window in the window list, one per layer choice,
     * and so on) never do, and would otherwise all pay for
     * @c WM_CTXMENU_CMD_MAX_LENGTH + @c CONFIG_MAX_LENGTH_NAME bytes
     * apiece regardless.
     */
    char *command;

    char *class_name;

    /** Optional callback invoked when the entry is activated */
    void (*on_activate)(xcb_connection_t *, void *userdata);

    void *userdata;     /**< User data passed to @p on_activate */

    /** Child entries for @c CTXMENU_SUBMENU */
    struct ctxmenu_entry_s *items;

    /**
     * @brief That client's own icon cache slot reused across repaints
     *        the same way the client's own desktop icon does
     *
     * Ignored when @p icon_window is @c XCB_WINDOW_NONE.  A pointer
     * into storage this struct does not own, since the client (and its
     * cache slot) outlives any one menu that happens to list it.
     */
    wmicon_cache_td *icon_cache;

    ctxmenu_entry_type_e type;                  /**< Entry kind */
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

    bool is_disabled;                           /**< Grayed-out if @c true */
    char label[WM_CTXMENU_LABEL_MAX_LENGTH];    /**< Visible text */
} ctxmenu_entry_td;


/**
 * @brief Context menu instance state
 *
 * Caller allocates this on the stack or statically, fills in @p entries
 * and @p entry_count, then passes it to @a ctxmenu_show.  The library
 * owns @p window after @a ctxmenu_show returns.
 */
typedef struct ctxmenu_state_s {
    ctxmenu_entry_td *entries;      /**< Array of menu entries */

    /**
     * @brief Cached top-Y pixel offset per entry
     *
     * Allocated by @a ctxmenu_show (size @p entry_count) and freed by
     * @a ctxmenu_close.  It's null when the allocation failed, in which
     * case row lookups fall back to walking @p entries directly
     */
    int32_t *entry_top_y;

    /** Currently open child menu, or null */
    struct ctxmenu_state_s *child;

    /** Back-pointer to the parent menu, or null */
    struct ctxmenu_state_s *parent;

    xcb_connection_t *connection;   /**< Cached connection for repaints */
    const config_td *config;        /**< Cached configuration */
    surface_td *surface;            /**< Cached surface for activation */
    xcb_window_t window;            /**< XCB window, or @c XCB_WINDOW_NONE */
    int entry_count;                /**< Number of entries in @p entries */
    int selected;                   /**< Currently highlighted row index */

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

    uint16_t width;                 /**< Computed menu window width */
    uint16_t height;                /**< Computed menu window height */
    int16_t origin_x;               /**< Actual X origin after clamping */
    int16_t origin_y;               /**< Actual Y origin after clamping */
} ctxmenu_state_td;


/* Public interface */
/**
 * @brief Create and show a context menu window
 *
 * Creates an XCB override-redirect popup window at @p pos,
 * clamped so the menu never extends beyond the work area of @p surface.
 * The menu grabs the pointer.  Any previously open context menu at the
 * same nesting level is closed first.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to display the menu
 * @param state      Menu state structure; @p entries and @p entry_count
 *                   must already be set by the caller
 * @param pos        Requested origin (root coordinates)
 * @param config     Active configuration (theme colors and font)
 *
 * @note Complexity: @e O(n), where @e n is @p entry_count
 */
void ctxmenu_show(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        struct position_s pos, const config_td *config);

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


#endif  /* ! MENU_CONTEXT_CTXMENU_H */
