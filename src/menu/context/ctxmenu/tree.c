/**
 * @file menu/context/ctxmenu/tree.c
 *
 * @brief Dispatch across a context menu's submenu window tree
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

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Project includes */
#include <config.h>

/* Menu includes */
#include <menu/context/ctxmenu/handle.h>
#include <menu/context/ctxmenu/redraw.h>

/* Local includes */
#include <menu/context/ctxmenu/tree.h>


/**
 * @brief Find the window of the innermost menu currently open
 *
 * Follows the chain of open submenus from @p state down to the last one
 * that has a window of its own, which is the menu a keypress belongs
 * to.  A key pressed with three menus open is meant for the one the
 * user is looking at, not the one they opened first.
 *
 * @param state Menu to start from, usually the root of the tree
 *
 * @return The innermost open menu's window, @p state's own where it has
 *         no open child, or @c XCB_WINDOW_NONE for a null @p state
 *
 * @note Stops at the first child without a window rather than walking
 *       past it, a child being linked before it is shown
 * @note Complexity: @e O(d), where @e d is the depth of the chain of
 *       open submenus
 */
static xcb_window_t s_deepest_window(const ctxmenu_state_td *state)
{
    const ctxmenu_state_td *cur;

    if (state == NULL) {
        return XCB_WINDOW_NONE;
    }

    cur = state;
    while (cur->child != NULL &&
            cur->child->window != XCB_WINDOW_NONE) {
        cur = cur->child;
    }

    return cur->window;
}


/* Find the state owning the given XCB window */
ctxmenu_state_td *ctxmenu_tree_state_find_for_window(ctxmenu_state_td *state,
        xcb_window_t win)
{
    ctxmenu_state_td *cur;

    if (state == NULL || win == XCB_WINDOW_NONE) {
        return NULL;
    }

    cur = state;
    while (cur != NULL) {
        if (cur->window == win) {
            return cur;
        }
        cur = cur->child;
    }

    return NULL;
}


/* Repaint whichever submenu under 'root' currently owns 'win' */
void ctxmenu_tree_redraw_window(ctxmenu_state_td *root, xcb_window_t win)
{
    ctxmenu_state_td *state;

    state = ctxmenu_tree_state_find_for_window(root, win);
    if (state != NULL) {
        ctxmenu_redraw(state);
    }
}


/* Forward a pointer-motion event to whichever submenu under 'root'
 * currently owns 'win' */
void ctxmenu_tree_handle_motion_window(ctxmenu_state_td *root,
        xcb_window_t win, int x, int y)
{
    ctxmenu_state_td *state;

    state = ctxmenu_tree_state_find_for_window(root, win);
    if (state != NULL) {
        ctxmenu_handle_motion(state, x, y);
    }
}


/* Forward a click, translated to menu-local coordinates, to whichever
 * submenu under 'root' currently owns 'win' */
bool ctxmenu_tree_handle_click_window(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root, xcb_window_t win,
        int y, const config_td *config)
{
    ctxmenu_state_td *state;

    state = ctxmenu_tree_state_find_for_window(root, win);
    if (state == NULL) {
        return false;
    }

    /* The position handed to 'xcb_create_window' is the outside corner
     * of the border, so the interior these rows are measured from
     * begins one border width further in.  Subtracting the origin alone
     * leaves the click that many pixels below where it looks, which is
     * enough to land it on the row after the one the pointer is
     * highlighting: the highlight arrives from the server already
     * relative to that interior and never had to be converted. */
    y -= (int) state->origin_y +
        (int) state->config->theme.menu.border.width;
    return ctxmenu_handle_click(connection, surface, state, y, config);
}


/* Forward a keypress to the deepest open submenu under 'root' */
bool ctxmenu_tree_handle_keypress_deepest(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root,
        xcb_keysym_t keysym, const config_td *config)
{
    ctxmenu_state_td *deepest;

    deepest = ctxmenu_tree_state_find_for_window(root,
            s_deepest_window(root));
    if (deepest == NULL) {
        deepest = root;
    }

    return ctxmenu_handle_keypress(connection, surface, deepest,
            keysym, config);
}
