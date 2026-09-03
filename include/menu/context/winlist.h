/**
 * @file menu/context/winlist.h
 *
 * @brief Window list menu (middle-click on empty desktop)
 *
 * Displays a popup menu listing open windows grouped by desktop when
 * the user middle-clicks on the root window (empty desktop).
 *
 * Each desktop group is introduced by a non-clickable label entry
 * holding the prefix, the index, the desktop name and the suffix, or
 * just the prefix, the index and the suffix when the name is empty or
 * null.  Each client window inside that group is listed as a clickable
 * command entry that focuses and raises the window when activated.
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

#ifndef MENU_CONTEXT_WINLIST_H
#define MENU_CONTEXT_WINLIST_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <types/handles.h>
#include <types/pair.h>


/**
 * @brief Maximum desktops shown as top-level entries
 *
 * Tied to @c CONFIG_MAX_DESKTOPS itself, the one real source of truth
 * for how many desktops a surface can ever have, rather than an
 * independent number of its own: a smaller, separately-chosen value
 * here would silently make every desktop past it unreachable from this
 * menu, exactly the kind of drift that set in when
 * @c CONFIG_MAX_DESKTOPS itself was later raised without anything
 * checking whether some other constant had quietly come to assume the
 * two stayed in step.
 */
#define WINLIST_MAX_DESKTOPS CONFIG_MAX_DESKTOPS

/**
 * @brief Maximum entries (windows and application-group submenus
 *        combined) inside a single desktop's submenu
 */
#define WINLIST_MAX_ENTRIES_PER_DESKTOP (68)

/**
 * @brief Maximum simultaneously open application-group submenus, summed
 *        across every desktop submenu
 *
 * Only applications with two or more windows on the same desktop get
 * one of these; single-window applications are listed directly.
 */
#define WINLIST_MAX_APPGROUPS (32)

/**
 * @brief Maximum windows listed inside a single application-group
 *        submenu
 */
#define WINLIST_MAX_APPGROUP_SIZE (32)

/**
 * @brief Size of the scratch buffer used to collect a desktop's
 *        candidate clients before grouping them by application
 */
#define WINLIST_MAX_COLLECTED (128)

/**
 * @brief Maximum pixel width a client entry's label may claim towards
 *        the windows-menu's width
 *
 * Caps how far one very long window title can stretch the whole menu;
 * a label wider than this truncates instead, the same reasoning
 * @c WM_CYCLE_MENU_LABEL_MAX_WIDTH truncates a cycle-menu entry
 * (@c defs/cycle.h).  Application-group and desktop submenu labels are
 * short, fixed phrases the user themselves configures (a desktop's
 * name, an application's class name) rather than an arbitrary window
 * title, so this only applies to the per-client entries.
 *
 * @see @a s_client_label_format in @c menu/context/winlist.c
 */
#define WINLIST_LABEL_MAX_WIDTH (280)

/**
 * @brief Size of the shared pool of per-entry userdata records
 *
 * Sized to cover the worst case at every level: one per desktop (for
 * its "Go there..." entry), one per entry in every desktop submenu, and
 * one per window in every application-group submenu.
 */
#define WINLIST_MAX_ENTRY_DATA \
    (WINLIST_MAX_DESKTOPS + \
     WINLIST_MAX_DESKTOPS * WINLIST_MAX_ENTRIES_PER_DESKTOP + \
     WINLIST_MAX_APPGROUPS * WINLIST_MAX_APPGROUP_SIZE)


/* Public interface */
/**
 * @brief Open the window list menu
 *
 * Iterates over all desktops on @p surface, and for each desktop that
 * has at least one client, adds a label entry for the desktop and one
 * command entry per client.  Any previously open window list menu is
 * closed first.  Runs a first, counting-only pass over every client
 * before actually building anything (see @a s_count_appgroups_needed
 * and @a s_build_desktop_entries's comments), so its
 * dynamically-allocated per-desktop and per-application-group entry
 * storage can be sized to what this exact call actually needs instead
 * of a fixed worst case held throughout the window manager's whole
 * lifetime regardless of how many desktops or applications a given
 * session actually has.
 *
 * @param connection XCB connection
 * @param surface    Surface whose clients are listed
 * @param pos        Requested origin (root coordinates)
 * @param config     Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the total number of clients
 *       across all desktops; visited twice, once to count and once to
 *       build, rather than the single pass this used before its storage
 *       became dynamically sized
 */
void winlist_show(xcb_connection_t *connection,
        surface_td *surface, struct position_s pos,
        const config_td *config);

/**
 * @brief Close the window list menu
 *
 * Frees all three of the arrays @a winlist_show allocates, one of
 * per-desktop submenu entries and two of per-application-group ones,
 * each sized fresh on every call to what that call needs rather than to
 * a worst case held for the whole session.
 *
 * @note Safe to call where the menu was never open, or was closed
 *       already, freeing a null pointer being a no-op in itself
 * @note Complexity: @e O(1)
 */
void winlist_close(void);

/**
 * @brief Repaint the window list menu
 *
 * @param win Window that received the expose event
 *
 * @note Complexity: @e O(n)
 */
void winlist_repaint(xcb_window_t win);

/**
 * @brief Handle a button-press event inside the window list menu
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
bool winlist_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int root_y,
        const config_td *config);

/**
 * @brief Query whether the window list menu is currently open
 *
 * @return @c true when the menu is visible
 *
 * @note Complexity: @e O(1)
 */
bool winlist_is_open(void);

/**
 * @brief Check whether @p win belongs to the window list menu
 *
 * @param win XCB window to test
 *
 * @return @c true if @p win is the window list menu window
 *
 * @note Complexity: @e O(1)
 */
bool winlist_owns_window(xcb_window_t win);

/**
 * @brief Handle a key-press event while the window list menu is open
 *
 * Forwards the key event to the window list menu context.
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
bool winlist_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config);

/**
 * @brief Handle a pointer-motion event over the window list menu window
 *
 * Updates the hover highlight based on the pointer position.
 *
 * @param win Window that received the motion event (unused; window list
 *            has a single-state hierarchy)
 * @param x   Pointer X relative to the menu window
 * @param y   Pointer Y relative to the menu window
 */
void winlist_handle_motion(xcb_window_t win, int x, int y);


#endif  /* ! MENU_CONTEXT_WINLIST_H */
