/**
 * @file menu/context/ctxmenu/tree.c
 *
 * @brief Dispatch across a context menu's own submenu window tree
 *
 * One of the files
 * @c menu/context/ctxmenu/ is made of.
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

/* Local includes */
#include <menu/context/ctxmenu/handle.h>
#include <menu/context/ctxmenu/redraw.h>
#include <menu/context/ctxmenu/tree.h>


/* Return the deepest open window in the menu hierarchy */
static xcb_window_t s_deepest_window(const ctxmenu_state_td *state)
{
    const ctxmenu_state_td *cur;

    if (state == NULL) {
        return XCB_WINDOW_NONE;
    }

    cur = state;
    while (cur->child != NULL && cur->child->window != XCB_WINDOW_NONE) {
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

    y -= state->origin_y;
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
