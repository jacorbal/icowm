/**
 * @file menu/context/ctxmenu/handle.c
 *
 * @brief Raw keyboard, click, and motion event handling for a single
 *        context menu window
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Default initial values */
#include <defs/kbd.h>

/* Menu includes */
#include <menu/context/ctxmenu/layout.h>
#include <menu/context/ctxmenu/redraw.h>
#include <menu/context/ctxmenu/select.h>

/* Local includes */
#include <menu/context/ctxmenu/handle.h>


/**
 * @brief Whether an entry is eligible to match a typed mnemonic
 *        character and its own first character equals @p target
 *
 * A separator, a label, a disabled entry, or one with an empty label
 * never matches, regardless of what its first character happens to be
 *
 * @param e      Entry to test
 * @param target Already-lowercased character typed by the user
 *
 * @return @c true if @p e is eligible and its own first character,
 *         lowercased, equals @p target
 */
static bool s_entry_matches_typeahead(const ctxmenu_entry_td *e,
        char target)
{
    unsigned char c;

    if (e->is_disabled || e->type == CTXMENU_SEPARATOR ||
            e->type == CTXMENU_LABEL || e->label[0] == '\0') {
        return false;
    }

    c = (unsigned char) e->label[0];
    return (char) tolower((int) c) == target;
}


/* Handle a key-press event while a context menu is open */
bool ctxmenu_handle_keypress(xcb_connection_t *connection,
        stage_td *stage, ctxmenu_state_td *state,
        xcb_keysym_t keysym, const config_td *config)
{
    char target;
    int match_count;
    int match_idx;
    int sel;
    ctxmenu_state_td *root;
    ctxmenu_state_td *child_state;

    if (state == NULL || state->window == XCB_WINDOW_NONE) {
        return false;
    }

    /* Up arrow: move selection to previous selectable entry */
    if (keysym == KS_UP) {
        ctxmenu_selection_move(state, -1);
        return true;
    }

    /* Down arrow: move selection to next selectable entry */
    if (keysym == KS_DOWN) {
        ctxmenu_selection_move(state, 1);
        return true;
    }

    /* Right arrow: open submenu for the selected entry */
    if (keysym == KS_RIGHT) {
        sel = state->selected;
        if (sel >= 0 && sel < state->entry_count &&
                state->entries[sel].type == CTXMENU_SUBMENU &&
                !state->entries[sel].is_disabled) {
            child_state =
                (ctxmenu_state_td *) state->entries[sel].userdata;
            if (child_state != NULL &&
                    state->entries[sel].items != NULL &&
                    state->entries[sel].item_count > 0) {
                struct position_s sub_pos;

                if (state->child != NULL) {
                    ctxmenu_close(state->child);
                    state->child = NULL;
                }
                child_state->entries = state->entries[sel].items;
                child_state->entry_count =
                    state->entries[sel].item_count;
                child_state->parent = state;
                child_state->child = NULL;
                sub_pos.x = state->origin_x + (int32_t) state->width;
                sub_pos.y = state->origin_y +
                        ctxmenu_entry_top_y(state, sel);
                ctxmenu_show(connection, stage, child_state,
                        sub_pos, config);
                state->child = child_state;
            }
        }
        return true;
    }

    /* Left arrow: close this submenu and return to parent */
    if (keysym == KS_LEFT) {
        if (state->parent != NULL) {
            /* No explicit repaint needed here: submenus open clear of
             * the parent's area (see 'sub_x' above), and on the rare
             * occasion one gets clamped close enough to overlap it
             * anyway, destroying it (just below) already makes the
             * X server generate its 'Expose' for whatever area of the
             * parent that uncovers, which 'ctxmenu_tree_redraw_window'
             * already handles. */
            ctxmenu_close(state);
            state->parent->child = NULL;
        }
        return true;
    }

    /* Return / KP_Enter: activate selected entry */
    if (keysym == KS_RETURN || keysym == KS_KP_ENTER) {
        sel = state->selected;
        if (sel >= 0 && sel < state->entry_count) {
            if (state->entries[sel].type == CTXMENU_SUBMENU) {
                /* Open submenu on Enter, same as Right arrow */
                return ctxmenu_handle_keypress(connection, stage,
                        state, KS_RIGHT, config);
            }
            return ctxmenu_entry_activate(state, sel, true);
        }
        return true;
    }

    /* Escape: close the entire menu hierarchy */
    if (keysym == KS_ESCAPE) {
        root = state;
        while (root->parent != NULL) {
            root = root->parent;
        }
        ctxmenu_close(root);
        return true;
    }

    /* Printable character: cycle to the next entry whose own first
     * character matches, wrapping past the end of the list, so that
     * repeated presses of the same letter step through every entry it
     * matches; a menu with only a single match activates it outright
     * instead, the same as Enter does */
    if (keysym > 0xFFu || !isprint((int) keysym)) {
        return false;
    }

    target = (char) tolower((int) ((unsigned char) keysym));
    match_count = 0;
    for (int i = 0; i < state->entry_count; ++i) {
        if (s_entry_matches_typeahead(&state->entries[i], target)) {
            ++match_count;
        }
    }

    if (match_count == 0) {
        return false;
    }

    match_idx = -1;
    for (int i = 1; i <= state->entry_count; ++i) {
        int idx = (state->selected + i) % state->entry_count;

        if (s_entry_matches_typeahead(&state->entries[idx], target)) {
            match_idx = idx;
            break;
        }
    }

    if (match_idx < 0) {
        return false;
    }

    sel = state->selected;
    state->selected = match_idx;
    ctxmenu_redraw_entries(state, sel, match_idx);
    if (match_count == 1) {
        if (state->entries[match_idx].type == CTXMENU_SUBMENU) {
            /* Open submenu on a unique typed-letter match, same as
             * Enter and Right arrow already do; falling through to
             * 'ctxmenu_entry_activate' instead, which has no
             * 'CTXMENU_SUBMENU' case of its own, would silently close
             * the whole menu without ever opening it. */
            return ctxmenu_handle_keypress(connection, stage,
                    state, KS_RIGHT, config);
        }
        return ctxmenu_entry_activate(state, match_idx, true);
    }

    return true;
}


/* Handle a button-press event inside a context menu window */
bool ctxmenu_handle_click(xcb_connection_t *connection,
        stage_td *stage, ctxmenu_state_td *state,
        int y, const config_td *config)
{
    int idx;
    ctxmenu_state_td *child_state;

    if (state == NULL || state->window == XCB_WINDOW_NONE) {
        return false;
    }

    idx = ctxmenu_entry_at_y(state, y);
    if (idx < 0 || idx >= state->entry_count) {
        return false;
    }

    if (state->entries[idx].type == CTXMENU_SEPARATOR ||
            state->entries[idx].type == CTXMENU_LABEL) {
        return true;    /* consumed but no action */
    }

    if (state->entries[idx].is_disabled) {
        return true;
    }

    if (state->entries[idx].type == CTXMENU_SUBMENU) {
        struct position_s sub_pos;

        /* Open or re-open the child submenu to the right.  The caller
         * stores the child 'ctxmenu_state_td' pointer in the entry's
         * 'userdata' field. */
        child_state =
            (ctxmenu_state_td *) state->entries[idx].userdata;
        if (child_state == NULL ||
                state->entries[idx].items == NULL ||
                state->entries[idx].item_count <= 0) {
            return true;
        }

        if (state->child != NULL) {
            ctxmenu_close(state->child);
            state->child = NULL;
        }

        /* Mark this entry selected (a click may land here with no prior
         * hover over this exact row, e.g., the pointer already resting
         * here when the menu first mapped) so it stays visibly
         * highlighted for as long as its submenu is open, the same as
         * the keyboard path already shows via whatever row
         * 'state->selected' was left on by prior up/down navigation. */
        if (state->selected != idx) {
            int prev_sel = state->selected;

            state->selected = idx;
            ctxmenu_redraw_entries(state, prev_sel, idx);
        }

        child_state->entries = state->entries[idx].items;
        child_state->entry_count = state->entries[idx].item_count;
        child_state->parent = state;
        child_state->child = NULL;

        sub_pos.x = state->origin_x + (int32_t) state->width;
        sub_pos.y = state->origin_y +
                ctxmenu_entry_top_y(state, idx);

        ctxmenu_show(connection, stage, child_state,
                sub_pos, config);
        state->child = child_state;
        return true;
    }

    return ctxmenu_entry_activate(state, idx, false);
}


/* Handle a pointer-motion event inside a context menu window */
void ctxmenu_handle_motion(ctxmenu_state_td *state, int x, int y)
{
    int idx;
    int prev_sel;

    if (state == NULL || state->window == XCB_WINDOW_NONE) {
        return;
    }

    /* Ignore a motion event reporting the exact same position as the
     * last one actually processed: X can deliver one of these right
     * after a submenu maps under an already-resting pointer, which
     * would otherwise silently steal a selection just made with the
     * keyboard even though the mouse never actually moved (vid.
     * 'last_motion_y' in 'ctxmenu.h') */
    if (y == state->last_motion_y) {
        return;
    }
    state->last_motion_y = y;

    /* Ignore X coordinate: entries span the full width */
    (void) x;

    idx = ctxmenu_entry_at_y(state, y);

    /* Clear selection when pointer leaves all entries */
    if (idx < 0 || idx >= state->entry_count ||
            state->entries[idx].type == CTXMENU_SEPARATOR ||
            state->entries[idx].type == CTXMENU_LABEL ||
            state->entries[idx].is_disabled) {
        if (state->selected >= 0) {
            prev_sel = state->selected;
            state->selected = -1;
            ctxmenu_redraw_entries(state, prev_sel, -1);
        }
        return;
    }

    if (idx == state->selected) {
        return;
    }

    prev_sel = state->selected;
    state->selected = idx;
    ctxmenu_redraw_entries(state, prev_sel, idx);
}
