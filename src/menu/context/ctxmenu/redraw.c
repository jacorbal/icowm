/**
 * @file menu/context/ctxmenu/redraw.c
 *
 * @brief Painting a context menu's own rows
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
#include <stdint.h>
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <menu/draw.h>
#include <render/text.h>
#include <render/wmicon.h>

#include <config.h>
#include <client.h>
#include <surface.h>

/* Local includes */
#include <menu/context/ctxmenu/layout.h>
#include <menu/context/ctxmenu/redraw.h>


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
    bool draw_icon;
    uint32_t bg;
    uint32_t fg;
    uint32_t border_color;
    uint32_t border_width;
    char label_buf[WM_CTXMENU_LABEL_MAX_LENGTH + 4];
    uint16_t arrow_w;
    int16_t arrow_x;
    int16_t text_x;
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
    top_y = ctxmenu_entry_top_y(state, idx);
    row_h = (e->type == CTXMENU_SEPARATOR)
        ? WM_CTXMENU_SEP_HEIGHT : WM_CTXMENU_ROW_HEIGHT;
    is_sel = (idx == state->selected) && !e->is_disabled
        && (e->type == CTXMENU_COMMAND || e->type == CTXMENU_SUBMENU);
    draw_icon = state->config->theme.menu.show_pixmaps &&
        e->icon_window != XCB_WINDOW_NONE;


    if (e->type == CTXMENU_LABEL) {
        bg = state->config->theme.menu.label.color.background;
        fg = state->config->theme.menu.label.color.foreground;
        border_color = state->config->theme.menu.label.border.color;
        border_width = state->config->theme.menu.label.border.width;
    } else {
        bg = (is_sel)
            ? state->config->theme.menu.selected.color.background
            : state->config->theme.menu.unselected.color.background;
        fg = (e->is_disabled)
            ? state->config->theme.menu.disabled_foreground
            : (is_sel)
                ? state->config->theme.menu.selected.color.foreground
                : state->config->theme.menu.unselected.color.foreground;
        border_color = (is_sel)
            ? state->config->theme.menu.selected.border.color
            : state->config->theme.menu.unselected.border.color;
        border_width = (is_sel)
            ? state->config->theme.menu.selected.border.width
            : state->config->theme.menu.unselected.border.width;
    }

    menu_draw_row_bg(conn, state->window, bg,
            (int16_t) top_y, (uint16_t) row_h, state->width);

    /* Every style's own 'border' is drawn if 'border.width' is
     * greater than 0; the built-in default theme sets it to a subtle
     * 1px for 'unselected'/'selected' and to 0 for 'label', so
     * heading rows stay plain by default.  This is separate from
     * 'menu.border', the menu window's own outer frame, entries
     * aside; see that field's comment in 'config.h'. */
    if (e->type != CTXMENU_SEPARATOR && border_width > 0u) {
        xcb_gcontext_t border_gc = xcb_generate_id(conn);
        xcb_rectangle_t border_rect;

        xcb_create_gc(conn, border_gc, state->window,
                XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH,
                (const uint32_t[]) { border_color, border_width });
        border_rect.x = (int16_t) (border_width / 2u);
        border_rect.y = (int16_t) (top_y + (int) (border_width / 2u));
        border_rect.width = (uint16_t) ((uint32_t) state->width -
                border_width);
        border_rect.height = (uint16_t) ((uint32_t) row_h - border_width);
        xcb_poly_rectangle(conn, state->window, border_gc, 1,
                &border_rect);
        xcb_free_gc(conn, border_gc);
    }

    if (e->type == CTXMENU_SEPARATOR) {
        /* Draw a centered horizontal line for the separator */
        gc = xcb_generate_id(conn);
        gc_vals[0] = state->config->theme.menu.separator_color;
        xcb_create_gc(conn, gc, state->window,
                XCB_GC_FOREGROUND, gc_vals);
        rect.x = (int16_t) state->config->theme.menu.padding.horizontal;
        rect.y = (int16_t) (top_y + WM_CTXMENU_SEP_HEIGHT / 2);
        rect.width = (uint16_t) (state->width -
                (uint16_t) (state->config->theme.menu.padding
                        .horizontal * 2u));
        rect.height = 1;
        xcb_poly_fill_rectangle(conn, state->window, gc, 1, &rect);
        xcb_free_gc(conn, gc);
        return;
    }

    text_x = (int16_t) state->config->theme.menu.padding.horizontal;

    /* An entry with an associated client (see 'icon_window''s 
     * comment in 'ctxmenu.h') reserves this same square of space
     * whether or not a real icon is actually drawn into it.  A client
     * with no icon of its own to draw still leaves every row's text
     * aligned in the same column. */
    if (draw_icon) {
        /* '#if', not a runtime ternary: both operands are fixed
         * compile-time constants, so a ternary here left one branch
         * provably unreachable to the compiler (-Wunreachable-code).
         * Still guards the arithmetic against a future edit to either
         * constant that would otherwise underflow silently. */
#if WM_CTXMENU_ROW_HEIGHT > WM_MENU_ICON_INSET
        uint16_t icon_size = (uint16_t)
            (WM_CTXMENU_ROW_HEIGHT - WM_MENU_ICON_INSET);
#else
        uint16_t icon_size = 0u;
#endif
        int16_t icon_y = (int16_t)
            (top_y + (row_h - (int) icon_size) / 2);

        if (icon_size > 0u && state->surface != NULL) {
            struct position_s icon_pos;

            icon_pos.x = text_x;
            icon_pos.y = icon_y;
            wmicon_draw_at(conn, state->surface->ewmh, e->icon_window,
                    state->window, icon_pos, icon_size,
                    fg, bg, e->icon_cache);
        }
        text_x = (int16_t) (text_x + icon_size +
                (int16_t) state->config->theme.menu.padding.horizontal);
    }

    (void) snprintf(label_buf, sizeof(label_buf), "%s", e->label);

    (void) text_renderer_use_font(conn, (e->type == CTXMENU_LABEL)
            ? state->config->theme.menu.label.font
            : (is_sel)
                ? state->config->theme.menu.selected.font
                : state->config->theme.menu.unselected.font);
    text_renderer_set_color(fg, bg);
    menu_draw_label(conn, state->window,
            (struct position_s) { text_x,
                top_y + WM_CTXMENU_ROW_HEIGHT - 5 },
            label_buf);

    if (e->type == CTXMENU_SUBMENU) {
        arrow_w = menu_draw_measure(MENU_CONTEXT_CTXMENU_SUBMENU_ARROW);
        arrow_x = (int16_t) (state->width -
                (uint16_t) state->config->theme.menu.padding.horizontal -
                arrow_w);
        menu_draw_label(conn, state->window,
                (struct position_s) { arrow_x,
                    top_y + WM_CTXMENU_ROW_HEIGHT - 5 },
                MENU_CONTEXT_CTXMENU_SUBMENU_ARROW);
    }
}


/**
 * @brief Repaint only the given one or two entry indices, not the
 *        whole menu
 *
 * @c s_draw_entry already paints its own row's full background before
 * its label (see its own body), so redrawing just the row(s) that
 * actually changed selection is self-contained: no separate clear
 * step is needed first, and nothing else in the menu window is
 * touched.  A single deselect (e.g., the pointer leaving every entry)
 * passes @c -1 for @p idx_b.
 *
 * @param state Menu state the entries belong to
 * @param idx_a First index to redraw, or @c -1 for none
 * @param idx_b Second index to redraw, or @c -1 for none; skipped if
 *              equal to @p idx_a
 *
 * @note Complexity: @e O(1)
 */
void ctxmenu_redraw_entries(ctxmenu_state_td *state,
        int idx_a, int idx_b)
{
    if (state == NULL || state->connection == NULL ||
            state->window == XCB_WINDOW_NONE) {
        return;
    }

    if (idx_a >= 0 && idx_a < state->entry_count) {
        s_draw_entry(state, idx_a);
    }
    if (idx_b >= 0 && idx_b < state->entry_count && idx_b != idx_a) {
        s_draw_entry(state, idx_b);
    }

    xcb_flush(state->connection);
}


/* Repaint the context menu window */
void ctxmenu_redraw(ctxmenu_state_td *state)
{
    if (state == NULL || state->connection == NULL ||
            state->window == XCB_WINDOW_NONE) {
        return;
    }

    /* Clear background */
    menu_draw_row_bg(state->connection, state->window,
            state->config->theme.menu.unselected.color.background,
            0, state->height, state->width);

    for (int i = 0; i < state->entry_count; ++i) {
        s_draw_entry(state, i);
    }

    xcb_flush(state->connection);
}
