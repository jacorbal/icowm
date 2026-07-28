/**
 * @file menu/cycle.h
 *
 * @brief Window/icon cycle menu interface
 *
 * Declares the public API for the keyboard cycle menu that lets the
 * user switch between windows or iconified clients.  All menu state is
 * private to the implementation.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CYCLE_H
#define MENU_CYCLE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Open the cycle menu for window or icon cycling
 *
 * Collects matching clients from @p desktop, creates the floating menu
 * window, and preselects the entry @p preselect positions away from the
 * active client.  @p modifier is the modifier mask used to open the
 * menu; releasing it auto-confirms the selection.
 *
 * @param surfaces   All managed surfaces (passed to focus_apply)
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose client list will be shown
 * @param is_icon    When @c true, list iconified clients; otherwise
 *                   list non-iconified clients
 * @param preselect  Offset from the active client (+1 next, -1 prev)
 * @param modifier   Modifier mask of the opening key binding (0 if none)
 * @param cfg        Active configuration (for theme colors)
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
void cycle_open(list_td *surfaces,
        xcb_connection_t *connection,
        surface_td *surface,
        desktop_td *desktop,
        bool is_icon,
        int preselect,
        uint16_t modifier,
        const config_td *cfg);

/**
 * @brief Close the cycle menu and restore previous focus
 *
 * Destroys the menu window and returns input focus to the window that
 * held it before the menu was opened.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void cycle_close(xcb_connection_t *connection);

/**
 * @brief Repaint all menu entries
 *
 * Renders all rows, highlighting the currently selected one.
 *
 * @param connection XCB connection
 * @param cfg        Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(n), where @e n is the number of menu entries
 */
void cycle_draw(xcb_connection_t *connection, const config_td *cfg);

/**
 * @brief Confirm the currently selected cycle menu entry
 *
 * For icon menus, restores the selected iconified client.  For window
 * menus, focuses the selected client.  Closes the menu afterwards.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(1)
 */
void cycle_confirm(xcb_connection_t *connection,
        list_td *surfaces,
        const config_td *cfg);

/**
 * @brief Set the selection directly to a given index
 *
 * If @p idx is out of range it is clamped to the last valid entry.
 * Does nothing when the menu has no entries.
 *
 * @param idx Zero-based row index to select
 *
 * @note Complexity: @e O(1)
 */
void cycle_navigate_to(unsigned int idx);

/**
 * @brief Advance the selection by one entry
 *
 * Wraps around at the end of the list.
 *
 * @note Complexity: @e O(1)
 */
void cycle_navigate_next(void);

/**
 * @brief Retreat the selection by one entry
 *
 * Wraps around at the beginning of the list.
 *
 * @note Complexity: @e O(1)
 */
void cycle_navigate_prev(void);

/**
 * @brief Query whether the cycle menu is currently open
 *
 * @return @c true when the menu window exists
 *
 * @note Complexity: @e O(1)
 */
bool cycle_is_open(void);

/**
 * @brief Return the cycle menu window identifier
 *
 * @return The menu's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t cycle_window(void);

/**
 * @brief Return the modifier mask that opened the cycle menu
 *
 * Used by the key-release handler to detect when to auto-confirm.
 *
 * @return The modifier mask, with locking bits already stripped
 *
 * @note Complexity: @e O(1)
 */
uint16_t cycle_modifier(void);

/**
 * @brief Return the keysym configured for cycle-next navigation
 *
 * @return The cycle-next keysym stored at menu open time, or
 *         @c XCB_NO_SYMBOL if none
 *
 * @note Complexity: @e O(1)
 */
xcb_keysym_t cycle_next_keysym(void);

/**
 * @brief Return the modifier mask for the cycle-next binding
 *
 * @return The cycle-next modifier mask, or 0 if none
 *
 * @note Complexity: @e O(1)
 */
uint16_t cycle_next_modmask(void);

/**
 * @brief Return the keysym configured for cycle-prev navigation
 *
 * @return The cycle-prev keysym, or @c XCB_NO_SYMBOL if none
 *
 * @note Complexity: @e O(1)
 */
xcb_keysym_t cycle_prev_keysym(void);

/**
 * @brief Return the modifier mask for the cycle-prev binding
 *
 * @return The cycle-prev modifier mask, or 0 if none
 *
 * @note Complexity: @e O(1)
 */
uint16_t cycle_prev_modmask(void);


#endif  /* ! MENU_CYCLE_H */
