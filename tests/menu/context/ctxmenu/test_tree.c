/**
 * @file tests/menu/context/ctxmenu/test_tree.c
 *
 * @brief Test battery for dispatch across a context menu's submenu
 *        window tree
 *
 * 'menu/context/ctxmenu/tree.c' sits directly on top of the raw,
 * single-window handlers declared in 'ctxmenu/handle.h' and
 * 'ctxmenu/redraw.h': it never touches XCB itself, only walks the
 * 'child' chain of a 'ctxmenu_state_td' tree looking for the state
 * whose 'window' field matches, then forwards to one of those four
 * handlers.  Every scenario below builds that chain directly on the
 * stack (root, an open child, an open grandchild), the same
 * stack-allocated fixture style 'tests/menu/context/test_winlist.c'
 * uses for its own 'ctxmenu_state_td' trees, and links only
 * 'tree.c' itself, with 'ctxmenu_redraw', 'ctxmenu_handle_motion',
 * 'ctxmenu_handle_click', and 'ctxmenu_handle_keypress' as recording
 * stand-ins that capture which state they were called with instead of
 * ever creating a real window.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Project includes */
#include <config.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/tree.h>


/** Recording stand-ins' own call counters and last-seen arguments,
 *  all reset by s_reset before every scenario */
static int s_call_redraw;
static ctxmenu_state_td *s_redraw_state;
static int s_call_motion;
static ctxmenu_state_td *s_motion_state;
static int s_motion_x;
static int s_motion_y;
static int s_call_click;
static ctxmenu_state_td *s_click_state;
static int s_click_y;
static bool s_click_return;
static int s_call_keypress;
static ctxmenu_state_td *s_keypress_state;
static xcb_keysym_t s_keypress_keysym;
static bool s_keypress_return;


/**
 * @brief Recording stand-in for @a ctxmenu_redraw
 * @note Complexity: @e O(1)
 */
void ctxmenu_redraw(ctxmenu_state_td *state)
{
    s_call_redraw++;
    s_redraw_state = state;
}


/**
 * @brief Recording stand-in for @a ctxmenu_redraw_entries
 *
 * Reached by nothing this file's own scenarios call.
 *
 * @note Complexity: @e O(1)
 */
void ctxmenu_redraw_entries(const ctxmenu_state_td *state,
        int idx_a, int idx_b)
{
    (void) state;
    (void) idx_a;
    (void) idx_b;
}


/**
 * @brief Recording stand-in for @a ctxmenu_handle_motion
 * @note Complexity: @e O(1)
 */
void ctxmenu_handle_motion(ctxmenu_state_td *state, int x, int y)
{
    s_call_motion++;
    s_motion_state = state;
    s_motion_x = x;
    s_motion_y = y;
}


/**
 * @brief Recording stand-in for @a ctxmenu_handle_click
 * @note Complexity: @e O(1)
 */
bool ctxmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        int y, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) config;

    s_call_click++;
    s_click_state = state;
    s_click_y = y;
    return s_click_return;
}


/**
 * @brief Recording stand-in for @a ctxmenu_handle_keypress
 * @note Complexity: @e O(1)
 */
bool ctxmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        xcb_keysym_t keysym, const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) config;

    s_call_keypress++;
    s_keypress_state = state;
    s_keypress_keysym = keysym;
    return s_keypress_return;
}


/** Non-null opaque handles standing in for a real connection, surface,
 *  and config, none of this file's tree.c code path ever dereferences
 *  any of the three itself, only forwards them to the handlers above,
 *  which this file's own stand-ins likewise leave untouched */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;
static int s_fake_surface_storage;
static surface_td *const s_fake_surface =
    (surface_td *) &s_fake_surface_storage;
static config_td s_config;


/**
 * @brief Reset every recording stand-in's call counters and captured
 *        arguments
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_redraw = 0;
    s_redraw_state = NULL;
    s_call_motion = 0;
    s_motion_state = NULL;
    s_motion_x = 0;
    s_motion_y = 0;
    s_call_click = 0;
    s_click_state = NULL;
    s_click_y = 0;
    s_click_return = false;
    s_call_keypress = 0;
    s_keypress_state = NULL;
    s_keypress_keysym = 0;
    s_keypress_return = false;
    memset(&s_config, 0, sizeof(s_config));
}


/**
 * @brief Build a three-level open chain: root, an open child, and an
 *        open grandchild, each with a distinct window ID
 *
 * @param root       Filled in as the tree's root, parent NULL
 * @param child      Filled in as root's open child
 * @param grandchild Filled in as child's open grandchild
 *
 * @note Complexity: @e O(1)
 */
static void s_build_chain(ctxmenu_state_td *root, ctxmenu_state_td *child,
        ctxmenu_state_td *grandchild)
{
    memset(root, 0, sizeof(*root));
    memset(child, 0, sizeof(*child));
    memset(grandchild, 0, sizeof(*grandchild));

    root->window = (xcb_window_t) 100;
    root->child = child;
    root->parent = NULL;
    root->config = &s_config;
    root->origin_y = 5;

    child->window = (xcb_window_t) 200;
    child->child = grandchild;
    child->parent = root;
    child->config = &s_config;
    child->origin_y = 7;

    grandchild->window = (xcb_window_t) 300;
    grandchild->child = NULL;
    grandchild->parent = child;
    grandchild->config = &s_config;
    grandchild->origin_y = 11;
}


/* ctxmenu_tree_state_find_for_window finds the root itself when 'win'
 * is the root's own window */
static void s_test_find_matches_root(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    ctxmenu_state_td *found;

    s_build_chain(&root, &child, &grandchild);

    found = ctxmenu_tree_state_find_for_window(&root, root.window);
    TAP_OK(found == &root, "a window matching the root itself"
            " returns the root");
}


/* ctxmenu_tree_state_find_for_window finds a deeper descendant when
 * 'win' matches its window, not the root's */
static void s_test_find_matches_descendant(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    ctxmenu_state_td *found;

    s_build_chain(&root, &child, &grandchild);

    found = ctxmenu_tree_state_find_for_window(&root, grandchild.window);
    TAP_OK(found == &grandchild, "a window matching the grandchild"
            " is found by walking down through the child");
}


/* ctxmenu_tree_state_find_for_window returns NULL for a window
 * belonging to none of the open states */
static void s_test_find_no_match_returns_null(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    ctxmenu_state_td *found;

    s_build_chain(&root, &child, &grandchild);

    found = ctxmenu_tree_state_find_for_window(&root,
            (xcb_window_t) 999);
    TAP_OK(found == NULL, "a window matching none of the open"
            " states returns NULL");
}


/* ctxmenu_tree_state_find_for_window returns NULL for a NULL root or
 * a NONE window, without dereferencing anything */
static void s_test_find_null_guards(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    ctxmenu_state_td *found;

    s_build_chain(&root, &child, &grandchild);

    found = ctxmenu_tree_state_find_for_window(NULL, root.window);
    TAP_OK(found == NULL, "a NULL state returns NULL rather than"
            " crashing");

    found = ctxmenu_tree_state_find_for_window(&root, XCB_WINDOW_NONE);
    TAP_OK(found == NULL, "an XCB_WINDOW_NONE window returns NULL"
            " rather than matching an uninitialized field");
}


/* ctxmenu_tree_state_find_for_window walks the 'child' chain
 * regardless of any intermediate state's own window value, unlike
 * 's_deepest_window' (used only by the keypress-deepest path below):
 * a grandchild is still found even when its parent's own window has
 * been reset to XCB_WINDOW_NONE */
static void s_test_find_walks_past_unmapped_intermediate(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    ctxmenu_state_td *found;

    s_build_chain(&root, &child, &grandchild);
    child.window = XCB_WINDOW_NONE;

    found = ctxmenu_tree_state_find_for_window(&root, grandchild.window);
    TAP_OK(found == &grandchild, "the search itself follows every"
            " 'child' link regardless of that link's own window"
            " value, so a grandchild is still found even past an"
            " intermediate whose window was reset to"
            " XCB_WINDOW_NONE");
}


/* ctxmenu_tree_redraw_window repaints exactly the state owning 'win' */
static void s_test_redraw_window_dispatches_to_owner(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;

    s_reset();
    s_build_chain(&root, &child, &grandchild);

    ctxmenu_tree_redraw_window(&root, child.window);
    TAP_EQ_INT(s_call_redraw, 1, "ctxmenu_redraw is called exactly"
            " once for a window owned by the child");
    TAP_OK(s_redraw_state == &child, "and it is called with the"
            " child's own state, not the root's");
}


/* ctxmenu_tree_redraw_window is a no-op when 'win' belongs to nothing
 * under 'root' */
static void s_test_redraw_window_no_match_is_noop(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;

    s_reset();
    s_build_chain(&root, &child, &grandchild);

    ctxmenu_tree_redraw_window(&root, (xcb_window_t) 999);
    TAP_EQ_INT(s_call_redraw, 0, "ctxmenu_redraw is never called"
            " for a window nothing under root owns");
}


/* ctxmenu_tree_handle_motion_window forwards to the owning state with
 * the exact x/y it was given */
static void s_test_motion_window_dispatches_to_owner(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;

    s_reset();
    s_build_chain(&root, &child, &grandchild);

    ctxmenu_tree_handle_motion_window(&root, grandchild.window, 42, 84);
    TAP_EQ_INT(s_call_motion, 1, "ctxmenu_handle_motion is called"
            " exactly once for a window owned by the grandchild");
    TAP_OK(s_motion_state == &grandchild, "and it is called with the"
            " grandchild's own state");
    TAP_EQ_INT(s_motion_x, 42, "the x coordinate is forwarded"
            " unchanged");
    TAP_EQ_INT(s_motion_y, 84, "the y coordinate is forwarded"
            " unchanged");
}


/* ctxmenu_tree_handle_motion_window is a no-op when 'win' matches
 * nothing under 'root' */
static void s_test_motion_window_no_match_is_noop(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;

    s_reset();
    s_build_chain(&root, &child, &grandchild);

    ctxmenu_tree_handle_motion_window(&root, (xcb_window_t) 999, 1, 1);
    TAP_EQ_INT(s_call_motion, 0, "ctxmenu_handle_motion is never"
            " called for a window nothing under root owns");
}


/* ctxmenu_tree_handle_click_window forwards to the owning state with
 * 'y' translated to that state's local coordinates, and returns
 * whatever ctxmenu_handle_click answers */
static void s_test_click_window_translates_y_and_forwards(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    bool result;

    s_reset();
    s_config.theme.menu.border.width = 2;
    s_build_chain(&root, &child, &grandchild);
    s_click_return = true;

    /* y=50, origin_y=7, border.width=2 => local y = 50 - 7 - 2 = 41 */
    result = ctxmenu_tree_handle_click_window(s_fake_connection,
            s_fake_surface, &root, child.window, 50, &s_config);

    TAP_EQ_INT(s_call_click, 1, "ctxmenu_handle_click is called"
            " exactly once for a window owned by the child");
    TAP_OK(s_click_state == &child, "and it is called with the"
            " child's own state");
    TAP_EQ_INT(s_click_y, 41, "the click y is translated by"
            " subtracting the state's origin_y and border width");
    TAP_OK(result == true, "the handler's own return value is"
            " propagated back to the caller");
}


/* ctxmenu_tree_handle_click_window returns false, without calling the
 * handler at all, when 'win' matches nothing under 'root' */
static void s_test_click_window_no_match_returns_false(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    bool result;

    s_reset();
    s_build_chain(&root, &child, &grandchild);

    result = ctxmenu_tree_handle_click_window(s_fake_connection,
            s_fake_surface, &root, (xcb_window_t) 999, 50, &s_config);

    TAP_OK(result == false, "a window matching nothing under root"
            " returns false");
    TAP_EQ_INT(s_call_click, 0, "ctxmenu_handle_click is never called"
            " when there is no owning state to forward to");
}


/* ctxmenu_tree_handle_keypress_deepest forwards to the deepest open
 * state in the chain (the grandchild here), not the root it was
 * actually given */
static void s_test_keypress_deepest_reaches_innermost(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    bool result;

    s_reset();
    s_build_chain(&root, &child, &grandchild);
    s_keypress_return = true;

    result = ctxmenu_tree_handle_keypress_deepest(s_fake_connection,
            s_fake_surface, &root, (xcb_keysym_t) 0xff52, &s_config);

    TAP_EQ_INT(s_call_keypress, 1, "ctxmenu_handle_keypress is called"
            " exactly once");
    TAP_OK(s_keypress_state == &grandchild, "and it is called with the"
            " deepest open state (the grandchild), not the root"
            " actually passed in");
    TAP_EQ_INT((long) s_keypress_keysym, (long) 0xff52,
            "the keysym is forwarded unchanged");
    TAP_OK(result == true, "the handler's own return value is"
            " propagated back to the caller");
}


/* With no submenu open at all (root's own child is unmapped), the
 * keypress goes to the root itself */
static void s_test_keypress_deepest_falls_back_to_root(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;
    bool result;

    s_reset();
    s_build_chain(&root, &child, &grandchild);
    root.child = NULL;

    result = ctxmenu_tree_handle_keypress_deepest(s_fake_connection,
            s_fake_surface, &root, (xcb_keysym_t) 0xff0d, &s_config);

    TAP_OK(s_keypress_state == &root, "with no submenu open, the"
            " keypress is handled by the root itself");
    TAP_OK(result == false, "and the stand-in's own default return"
            " value (false) still comes back to the caller");
}


/* ctxmenu_tree_handle_keypress_deepest stops descending at a child
 * linked but not yet shown (window == XCB_WINDOW_NONE), landing on
 * its parent instead of a state that was never actually opened */
static void s_test_keypress_deepest_stops_at_unmapped_child(void)
{
    ctxmenu_state_td root;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;

    s_reset();
    s_build_chain(&root, &child, &grandchild);
    child.window = XCB_WINDOW_NONE;

    (void) ctxmenu_tree_handle_keypress_deepest(s_fake_connection,
            s_fake_surface, &root, (xcb_keysym_t) 0x61, &s_config);

    TAP_OK(s_keypress_state == &root, "a linked-but-unmapped child"
            " is not treated as the deepest open menu; the root, its"
            " last mapped ancestor, handles the key instead");
}


/* A NULL root handed to ctxmenu_tree_handle_keypress_deepest still
 * dispatches somewhere sane: s_deepest_window(NULL) answers
 * XCB_WINDOW_NONE, the search then finds nothing, so 'deepest' falls
 * back to 'root' itself, which is NULL, and is forwarded as such */
static void s_test_keypress_deepest_null_root(void)
{
    s_reset();
    s_keypress_return = false;

    (void) ctxmenu_tree_handle_keypress_deepest(s_fake_connection,
            s_fake_surface, NULL, (xcb_keysym_t) 0x62, &s_config);

    TAP_EQ_INT(s_call_keypress, 1, "even a NULL root still reaches the"
            " keypress handler exactly once");
    TAP_OK(s_keypress_state == NULL, "with NULL forwarded as the"
            " state to handle it, rather than some stale pointer");
}


int main(void)
{
    TAP_PLAN(29);

    s_test_find_matches_root();
    s_test_find_matches_descendant();
    s_test_find_no_match_returns_null();
    s_test_find_null_guards();
    s_test_find_walks_past_unmapped_intermediate();
    s_test_redraw_window_dispatches_to_owner();
    s_test_redraw_window_no_match_is_noop();
    s_test_motion_window_dispatches_to_owner();
    s_test_motion_window_no_match_is_noop();
    s_test_click_window_translates_y_and_forwards();
    s_test_click_window_no_match_returns_false();
    s_test_keypress_deepest_reaches_innermost();
    s_test_keypress_deepest_falls_back_to_root();
    s_test_keypress_deepest_stops_at_unmapped_child();
    s_test_keypress_deepest_null_root();

    return TAP_DONE();
}
