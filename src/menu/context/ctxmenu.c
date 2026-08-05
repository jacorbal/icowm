/**
 * @file menu/context/ctxmenu.c
 *
 * @brief Generic context menu library implementation
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <lifecycle.h>
#include <render/text.h>
#include <surface.h>

/* Menu includes */
#include <menu/draw.h>

/* Local includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Compute the pixel Y of the top edge of an entry by index
 *
 * @param entries     Array of menu entries
 * @param entry_count Number of entries
 * @param idx         Entry index (0-based)
 *
 * @return Y coordinate (pixels) relative to the menu window top
 *
 * @note Complexity: @e O(n), where @e n is @p idx
 */
static int s_entry_top_y(const ctxmenu_entry_td *entries,
        int entry_count, int idx)
{
    int y = WM_CTXMENU_PAD_Y;

    for (int i = 0; i < idx && i < entry_count; ++i) {
        if (entries[i].type == CTXMENU_SEPARATOR) {
            y += WM_CTXMENU_SEP_HEIGHT;
        } else {
            y += WM_CTXMENU_ROW_HEIGHT;
        }
    }

    return y;
}


/**
 * @brief Compute the total pixel height of a menu from its entries
 *
 * @param entries    Array of menu entries
 * @param entry_count Number of entries
 *
 * @return Total height in pixels
 *
 * @note Complexity: @e O(n), where @e n is @p entry_count
 */
static uint16_t s_compute_height(const ctxmenu_entry_td *entries,
        int entry_count)
{
    int h = WM_CTXMENU_PAD_Y * 2;

    for (int i = 0; i < entry_count; ++i) {
        if (entries[i].type == CTXMENU_SEPARATOR) {
            h += WM_CTXMENU_SEP_HEIGHT;
        } else {
            h += WM_CTXMENU_ROW_HEIGHT;
        }
    }

    return (uint16_t) h;
}


/**
 * @brief Compute the pixel width required to display all menu entries
 *
 * Iterates over all entries and measures each label, adding space for
 * the left padding and the submenu indicator.
 *
 * @param connection  XCB connection
 * @param entries     Array of menu entries
 * @param entry_count Number of entries
 * @param config      Active configuration
 *
 * @return Required width in pixels, at least @c WM_CTXMENU_MIN_WIDTH
 *
 * @note Complexity: @e O(n), where @e n is @p entry_count
 */
static uint16_t s_compute_width(xcb_connection_t *connection,
        const ctxmenu_entry_td *entries, int entry_count,
        const config_td *config)
{
    uint16_t max_w = WM_CTXMENU_MIN_WIDTH;

    text_renderer_init(connection, config->theme.window.active.font);
    for (int i = 0; i < entry_count; ++i) {
        uint16_t w;

        if (entries[i].type == CTXMENU_SEPARATOR) {
            continue;
        }

        w = (uint16_t) (menu_draw_measure(entries[i].label)
                + (uint16_t) (WM_CTXMENU_PAD_X * 2));

        /* Add space for the submenu arrow indicator */
        if (entries[i].type == CTXMENU_SUBMENU) {
            w = (uint16_t) (w + 16u);
        }
        if (w > max_w) {
            max_w = w;
        }
    }

    return max_w;
}


/**
 * @brief Return the row index at the given pixel Y, or -1 if none
 *
 * @param entries     Array of menu entries
 * @param entry_count Number of entries
 * @param y           Pixel Y relative to menu window
 *
 * @return Entry index, or -1 if @p y is outside all rows
 *
 * @note Complexity: @e O(n)
 */
static int s_entry_at_y(const ctxmenu_entry_td *entries,
        int entry_count, int y)
{
    int cur_y = WM_CTXMENU_PAD_Y;

    for (int i = 0; i < entry_count; ++i) {
        int row_h = (entries[i].type == CTXMENU_SEPARATOR)
            ? WM_CTXMENU_SEP_HEIGHT : WM_CTXMENU_ROW_HEIGHT;

        if (y >= cur_y && y < cur_y + row_h) {
            return i;
        }
        cur_y += row_h;
    }

    return -1;
}



/**
 * @brief Activate the entry at the given index in a context menu
 *
 * Invokes the entry's @p on_activate callback if set, or calls
 * @c lifecycle_dispatch_launch for command entries that carry a shell
 * command string.  For separator, label, or disabled entries no action
 * is taken but @c true is returned to consume the event.  Closes the
 * entire menu hierarchy (root and all children) after activation.
 *
 * @param state Menu state that contains the entry
 * @param idx   Zero-based index of the entry to activate
 *
 * @return @c true if the event was consumed, @c false when @p state or
 *         @p idx is out of range
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
static bool s_ctxmenu_activate_entry(ctxmenu_state_td *state, int idx)
{
    ctxmenu_entry_td *e;
    ctxmenu_state_td *root;

    if (state == NULL || idx < 0 || idx >= state->entry_count) {
        return false;
    }

    e = &state->entries[idx];
    if (e->type == CTXMENU_SEPARATOR || e->type == CTXMENU_LABEL ||
            e->is_disabled) {
        return true;
    }

    if (e->on_activate != NULL) {
        e->on_activate(state->connection, e->userdata);
    } else if (e->command[0] != '\0') {
        lifecycle_dispatch_launch(state->surface,
                e->command, e->class_name);
    }

    root = state;
    while (root->parent != NULL) {
        root = root->parent;
    }

    ctxmenu_close(root);
    return true;
}




/**
 * @brief Draw a single menu entry row
 *
 * Renders the background, optional selection highlight, and the entry
 * label.  Separator entries are drawn as a horizontal line.  Disabled
 * entries use a dimmed foreground color.  Submenu entries append " >".
 *
 * @param state Menu state
 * @param idx   Entry index
 *
 * @note Complexity: @e O(1)
 */
static void s_draw_entry(const ctxmenu_state_td *state, int idx)
{
    const ctxmenu_entry_td *e;
    int top_y;
    int row_h;
    bool is_sel;
    uint32_t bg;
    uint32_t fg;
    char label_buf[WM_CTXMENU_LABEL_MAX_LEN + 4];
    uint16_t arrow_w;
    int16_t arrow_x;
    xcb_gcontext_t gc;
    uint32_t gc_vals[1];
    xcb_rectangle_t rect;
    xcb_connection_t *conn;

    if (state == NULL || state->connection == NULL ||
            state->config == NULL || idx < 0 ||
            idx >= state->entry_count) {
        return;
    }

    conn = state->connection;
    e = &state->entries[idx];
    top_y = s_entry_top_y(state->entries, state->entry_count, idx);
    row_h = (e->type == CTXMENU_SEPARATOR)
        ? WM_CTXMENU_SEP_HEIGHT : WM_CTXMENU_ROW_HEIGHT;
    is_sel = (idx == state->selected) && !e->is_disabled
        && (e->type == CTXMENU_COMMAND || e->type == CTXMENU_SUBMENU);


    if (e->type == CTXMENU_LABEL) {
        bg = state->config->theme.window.inactive.background_color;
        fg = state->config->theme.window.inactive.foreground_color;
    } else {
        bg = (is_sel)
            ? state->config->theme.window.active.border_color
            : state->config->theme.window.active.background_color;
        fg = (e->is_disabled)
            ? state->config->theme.window.inactive.foreground_color
            : state->config->theme.window.active.foreground_color;
    }

    menu_draw_row_bg(conn, state->window, bg,
            (int16_t) top_y, (uint16_t) row_h, state->width);

    if (e->type == CTXMENU_SEPARATOR) {
        /* Draw a centered horizontal line for the separator */
        gc = xcb_generate_id(conn);
        gc_vals[0] = state->config->theme.window.active.foreground_color;
        xcb_create_gc(conn, gc, state->window,
                XCB_GC_FOREGROUND, gc_vals);
        rect.x = (int16_t) WM_CTXMENU_PAD_X;
        rect.y = (int16_t) (top_y + WM_CTXMENU_SEP_HEIGHT / 2);
        rect.width = (uint16_t) (state->width -
                (uint16_t) (WM_CTXMENU_PAD_X * 2));
        rect.height = 1;
        xcb_poly_fill_rectangle(conn, state->window, gc, 1, &rect);
        xcb_free_gc(conn, gc);
        return;
    }

    (void) snprintf(label_buf, sizeof(label_buf), "%s", e->label);

    text_renderer_init(conn, state->config->theme.window.active.font);
    text_renderer_set_color(fg, bg);
    menu_draw_label(conn, state->window,
            (int16_t) WM_CTXMENU_PAD_X,
            (int16_t) (top_y + WM_CTXMENU_ROW_HEIGHT - 5),
            label_buf);

    if (e->type == CTXMENU_SUBMENU) {
        arrow_w = menu_draw_measure(MENU_CONTEXT_CTXMENU_SUBMENU_ARROW);
        arrow_x = (int16_t) (state->width - (uint16_t) WM_CTXMENU_PAD_X -
                arrow_w);
        menu_draw_label(conn, state->window,
                arrow_x,
                (int16_t) (top_y + WM_CTXMENU_ROW_HEIGHT - 5),
                MENU_CONTEXT_CTXMENU_SUBMENU_ARROW);
    }
}


/* Handle a key-press event while a context menu is open */
bool ctxmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        xcb_keysym_t keysym, const config_td *config)
{
    char target;
    int match_count;
    int match_idx;
    int sel;
    int i;
    int step;
    int next;
    ctxmenu_state_td *root;
    ctxmenu_state_td *child_state;
    int16_t sub_x;
    int16_t sub_y;

    if (state == NULL || state->window == XCB_WINDOW_NONE) {
        return false;
    }

    /* Up arrow: move selection to previous selectable entry */
    if (keysym == 0xff52u) {
        sel = state->selected;
        step = -1;
        next = (sel < 0) ? state->entry_count - 1 : sel + step;
        for (i = 0; i < state->entry_count; ++i) {
            if (next < 0) { next = state->entry_count - 1; }
            if (next >= state->entry_count) { next = 0; }
            if (state->entries[next].type != CTXMENU_SEPARATOR &&
                    state->entries[next].type != CTXMENU_LABEL &&
                    !state->entries[next].is_disabled) {
                break;
            }
            next += step;
        }
        if (i < state->entry_count) {
            state->selected = next;
            ctxmenu_repaint(state);
        }
        return true;
    }

    /* Down arrow: move selection to next selectable entry */
    if (keysym == 0xff54u) {
        sel = state->selected;
        step = 1;
        next = (sel < 0) ? 0 : sel + step;
        for (i = 0; i < state->entry_count; ++i) {
            if (next < 0) { next = state->entry_count - 1; }
            if (next >= state->entry_count) { next = 0; }
            if (state->entries[next].type != CTXMENU_SEPARATOR &&
                    state->entries[next].type != CTXMENU_LABEL &&
                    !state->entries[next].is_disabled) {
                break;
            }
            next += step;
        }
        if (i < state->entry_count) {
            state->selected = next;
            ctxmenu_repaint(state);
        }
        return true;
    }

    /* Right arrow: open submenu for the selected entry */
    if (keysym == 0xff53u) {
        sel = state->selected;
        if (sel >= 0 && sel < state->entry_count &&
                state->entries[sel].type == CTXMENU_SUBMENU) {
            child_state =
                (ctxmenu_state_td *) state->entries[sel].userdata;
            if (child_state != NULL &&
                    state->entries[sel].items != NULL &&
                    state->entries[sel].item_count > 0) {
                if (state->child != NULL) {
                    ctxmenu_close(state->child);
                    state->child = NULL;
                }
                child_state->entries = state->entries[sel].items;
                child_state->entry_count = state->entries[sel].item_count;
                child_state->parent = state;
                child_state->child = NULL;
                sub_x = (int16_t) (state->origin_x +
                        (int16_t) state->width);
                sub_y = (int16_t) (state->origin_y
                        + (int16_t) s_entry_top_y(state->entries,
                                state->entry_count, sel));
                ctxmenu_show(connection, surface, child_state,
                        sub_x, sub_y, config);
                state->child = child_state;
            }
        }
        return true;
    }

    /* Left arrow: close this submenu and return to parent */
    if (keysym == 0xff51u) {
        if (state->parent != NULL) {
            ctxmenu_close(state);
            state->parent->child = NULL;
            ctxmenu_repaint(state->parent);
        }
        return true;
    }

    /* Return / KP_Enter: activate selected entry */
    if (keysym == 0xff0du || keysym == 0xff8du) {
        sel = state->selected;
        if (sel >= 0 && sel < state->entry_count) {
            if (state->entries[sel].type == CTXMENU_SUBMENU) {
                /* Open submenu on Enter, same as Right arrow */
                return ctxmenu_handle_keypress(connection, surface,
                        state, 0xff53u, config);
            }
            return s_ctxmenu_activate_entry(state, sel);
        }
        return true;
    }

    /* Escape: close the entire menu hierarchy */
    if (keysym == 0xff1bu) {
        root = state;
        while (root->parent != NULL) {
            root = root->parent;
        }
        ctxmenu_close(root);
        return true;
    }

    /* Printable character: jump to first matching entry */
    if (keysym > 0xFFu || !isprint((int) keysym)) {
        return false;
    }

    target = (char) tolower((int) ((unsigned char) keysym));
    match_count = 0;
    match_idx = -1;
    for (i = 0; i < state->entry_count; ++i) {
        const ctxmenu_entry_td *e = &state->entries[i];
        unsigned char c;

        if (e->is_disabled || e->type == CTXMENU_SEPARATOR ||
                e->type == CTXMENU_LABEL || e->label[0] == '\0') {
            continue;
        }

        c = (unsigned char) e->label[0];
        if ((char) tolower((int) c) == target) {
            ++match_count;
            match_idx = i;
        }
    }

    if (match_count == 0 || match_idx < 0) {
        return false;
    }

    state->selected = match_idx;
    ctxmenu_repaint(state);
    if (match_count == 1) {
        return s_ctxmenu_activate_entry(state, match_idx);
    }

    return true;

}


/* Create and show a context menu window */
void ctxmenu_show(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        int16_t x, int16_t y, const config_td *config)
{
    uint32_t mask;
    uint32_t values[3];
    uint32_t stk[1];
    int16_t clamped_x;
    int16_t clamped_y;
    int32_t max_x;
    int32_t max_y;
    uint32_t work_x;
    uint32_t work_y;
    uint32_t work_w;
    uint32_t work_h;
    desktop_td *desktop;

    if (connection == NULL || surface == NULL || state == NULL ||
            config == NULL || state->entries == NULL ||
            state->entry_count <= 0) {
        return;
    }

    ctxmenu_close(state);

    state->connection = connection;
    state->surface = surface;
    state->config = config;
    state->selected = -1;

    state->width = s_compute_width(connection, state->entries,
            state->entry_count, config);
    state->height = s_compute_height(state->entries, state->entry_count);

    /* Use the active desktop's work area to clamp position */
    work_x = 0;
    work_y = 0;
    work_w = surface->properties.dim.w;
    work_h = surface->properties.dim.h;
    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop != NULL) {
        work_x = (uint32_t) desktop->workarea.pos.x;
        work_y = (uint32_t) desktop->workarea.pos.y;
        work_w = desktop->workarea.dim.w;
        work_h = desktop->workarea.dim.h;
    }

    max_x = (int32_t) (work_x + work_w) - (int32_t) state->width;
    max_y = (int32_t) (work_y + work_h) - (int32_t) state->height;

    clamped_x = x;
    clamped_y = y;
    if (clamped_x > (int16_t) max_x) { clamped_x = (int16_t) max_x; }
    if (clamped_y > (int16_t) max_y) { clamped_y = (int16_t) max_y; }
    if (clamped_x < (int16_t) work_x) { clamped_x = (int16_t) work_x; }
    if (clamped_y < (int16_t) work_y) { clamped_y = (int16_t) work_y; }

    state->origin_x = clamped_x;
    state->origin_y = clamped_y;

    state->window = xcb_generate_id(connection);
    mask = XCB_CW_BACK_PIXEL        |
           XCB_CW_OVERRIDE_REDIRECT |
           XCB_CW_EVENT_MASK;
    values[0] = config->theme.window.active.background_color;
    values[1] = 1;  /* override_redirect: prevent WM from managing it */
    values[2] = XCB_EVENT_MASK_EXPOSURE     |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_POINTER_MOTION;

    xcb_create_window(connection, XCB_COPY_FROM_PARENT,
            state->window, surface->screen->root,
            clamped_x, clamped_y,
            state->width, state->height,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* Raise to the top */
    stk[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, state->window,
            XCB_CONFIG_WINDOW_STACK_MODE, stk);

    xcb_map_window(connection, state->window);

    /* Grab keyboard and pointer for the root menu only (not submenus).
     * The keyboard grab redirects all key events (including 'Escape') to
     * the window manager so the menu can be dismissed without the
     * focused application consuming those keys first.  The pointer grab
     * ensures that clicks outside the menu hierarchy are seen by the WM
     * even when an application holds an active pointer grab. */
    if (state->parent == NULL) {
        xcb_grab_keyboard(connection,
                0,                      /* owner_events */
                surface->screen->root,
                XCB_CURRENT_TIME,
                XCB_GRAB_MODE_ASYNC,    /* pointer events unaffected */
                XCB_GRAB_MODE_ASYNC);   /* keyboard events delivered async */
        xcb_grab_pointer(connection,
                0,                      /* owner_events */
                surface->screen->root,
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE,
                XCB_GRAB_MODE_ASYNC,
                XCB_GRAB_MODE_ASYNC,
                XCB_NONE,               /* confine to no window */
                XCB_NONE,               /* no cursor override */
                XCB_CURRENT_TIME);
    }

    xcb_flush(connection);
}


/* Close a context menu and its entire descendant chain */
void ctxmenu_close(ctxmenu_state_td *state)
{
    if (state == NULL) {
        return;
    }

    /* Close children first */
    if (state->child != NULL) {
        ctxmenu_close(state->child);
        state->child = NULL;
    }

    if (state->connection != NULL && state->window != XCB_WINDOW_NONE) {
        xcb_destroy_window(state->connection, state->window);

        /* Release keyboard and pointer grabs when the root menu closes */
        if (state->parent == NULL) {
            xcb_ungrab_keyboard(state->connection, XCB_CURRENT_TIME);
            xcb_ungrab_pointer(state->connection, XCB_CURRENT_TIME);
        }

        xcb_flush(state->connection);
    }

    state->window = XCB_WINDOW_NONE;
    state->selected = -1;
    state->width = 0;
    state->height = 0;
    state->connection = NULL;
    state->surface = NULL;
    state->config = NULL;
}


/* Repaint the context menu window */
void ctxmenu_repaint(ctxmenu_state_td *state)
{
    if (state == NULL || state->connection == NULL ||
            state->window == XCB_WINDOW_NONE) {
        return;
    }

    /* Clear background */
    menu_draw_row_bg(state->connection, state->window,
            state->config->theme.window.active.background_color,
            0, state->height, state->width);

    for (int i = 0; i < state->entry_count; ++i) {
        s_draw_entry(state, i);
    }

    xcb_flush(state->connection);
}


/* Handle a button-press event inside a context menu window */
bool ctxmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        int x, int y, const config_td *config)
{
    int idx;
    int16_t sub_x;
    int16_t sub_y;
    ctxmenu_state_td *child_state;

    if (state == NULL || state->window == XCB_WINDOW_NONE) {
        return false;
    }

    idx = s_entry_at_y(state->entries, state->entry_count, y);
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
        /* Open or re-open the child submenu to the right.
         * The caller stores the child 'ctxmenu_state_td' pointer in the
         * entry's 'userdata' field. */
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

        child_state->entries = state->entries[idx].items;
        child_state->entry_count = state->entries[idx].item_count;
        child_state->parent = state;
        child_state->child = NULL;

        sub_x = (int16_t) (state->origin_x + (int16_t) state->width);
        sub_y = (int16_t) (state->origin_y
                + (int16_t) s_entry_top_y(state->entries,
                        state->entry_count, idx));

        ctxmenu_show(connection, surface, child_state,
                sub_x, sub_y, config);
        state->child = child_state;
        return true;
    }

    return s_ctxmenu_activate_entry(state, idx);
}


/* Query whether the context menu is currently open */
bool ctxmenu_is_open(const ctxmenu_state_td *state)
{
    return state != NULL && state->window != XCB_WINDOW_NONE;
}


/* Return the deepest open window in the menu hierarchy */
xcb_window_t ctxmenu_deepest_window(const ctxmenu_state_td *state)
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
ctxmenu_state_td *ctxmenu_find_state_for_window(ctxmenu_state_td *state,
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


/* Close the context menu when a click occurs outside all its windows */
void ctxmenu_close_on_outside_click(ctxmenu_state_td *state)
{
    ctxmenu_state_td *root;

    if (state == NULL) {
        return;
    }

    root = state;
    while (root->parent != NULL) {
        root = root->parent;
    }

    ctxmenu_close(root);
}
