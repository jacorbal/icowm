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
#include <stdlib.h>     /* calloc, free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Default initial values */
#include <defs/ctxmenu.h>
#include <defs/kbd.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <lifecycle.h>
#include <render/text.h>
#include <render/wmicon.h>
#include <surface.h>

/* Menu includes */
#include <menu/draw.h>

/* Local includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Return the pixel height of a single menu row by entry type
 *
 * @param type Entry type
 *
 * @return @c WM_CTXMENU_SEP_HEIGHT for a separator, or
 *         @c WM_CTXMENU_ROW_HEIGHT for any other entry type
 *
 * @note Complexity: @e O(1)
 */
static int s_row_height(ctxmenu_entry_type_e type)
{
    return (type == CTXMENU_SEPARATOR)
        ? WM_CTXMENU_SEP_HEIGHT : WM_CTXMENU_ROW_HEIGHT;
}


/**
 * @brief Compute the pixel Y of the top edge of an entry by index
 *
 * Reads @p state->entry_top_y when the cache was successfully allocated
 * by @c ctxmenu_show, giving @e O(1) lookup; falls back to an @e O(n)
 * walk over @p state->entries otherwise (e.g., @c calloc failed at
 * menu-open time).
 *
 * @param state Menu state
 * @param idx   Entry index (0-based)
 *
 * @return Y coordinate (pixels) relative to the menu window top
 *
 * @note Complexity: @e O(1) with the cache, @e O(n) without it, where
 *       @e n is @p idx
 */
static int s_entry_top_y(const ctxmenu_state_td *state, int idx)
{
    int y;

    if (state->entry_top_y != NULL && idx >= 0 &&
            idx < state->entry_count) {
        return state->entry_top_y[idx];
    }

    y = (int) state->config->theme.menu.padding.vertical;
    for (int i = 0; i < idx && i < state->entry_count; ++i) {
        y += s_row_height(state->entries[i].type);
    }

    return y;
}


/**
 * @brief Compute per-row Y offsets and the total menu height
 *
 * Walks the entries once, filling @p state->entry_top_y (when
 * allocated) with the top-Y pixel offset of each row, so that
 * @c s_entry_top_y and @c s_entry_at_y can look rows up directly
 * afterwards instead of re-walking the entry array on every call.
 * Also returns the total height, replacing what used to be a separate
 * pass over the same entries.
 *
 * @param state Menu state; @p entries and @p entry_count must already
 *              be set
 *
 * @return Total menu height in pixels
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
static uint16_t s_build_layout(ctxmenu_state_td *state)
{
    int y = (int) state->config->theme.menu.padding.vertical;

    for (int i = 0; i < state->entry_count; ++i) {
        if (state->entry_top_y != NULL) {
            state->entry_top_y[i] = y;
        }
        y += s_row_height(state->entries[i].type);
    }

    return (uint16_t) (y + (int) state->config->theme.menu.padding.vertical);
}


/**
 * @brief Maximum measured width among non-label, non-separator
 *        entries, in whichever font is currently active
 *
 * Shared by @c s_compute_width's own second and third measuring
 * passes (@c unselected.font and @c selected.font respectively): the
 * two are otherwise identical, differing only in which font is active
 * when each is called; see @c s_compute_width's own doc comment for
 * why those stay two separate passes rather than one combined loop.
 *
 * @param entries     Entries to measure
 * @param entry_count Number of entries in @p entries
 * @param pad2        Horizontal padding to add on both sides
 * @param icon_offset Extra width to add for an entry with an
 *                    associated @c icon_window (0 when
 *                    @c theme.menu.show-pixmaps is off); see
 *                    @c s_compute_width
 * @param max_w       Current running maximum, carried in so the two
 *                    passes contribute to one shared result
 *
 * @return The greater of @p max_w and every measured entry's own width
 *
 * @note Complexity: @e O(n), where @e n is @p entry_count
 */
static uint16_t s_max_width_for_selectable(const ctxmenu_entry_td *entries,
        int entry_count, uint16_t pad2, uint16_t icon_offset,
        uint16_t max_w)
{
    for (int i = 0; i < entry_count; ++i) {
        uint16_t w;

        if (entries[i].type == CTXMENU_SEPARATOR ||
                entries[i].type == CTXMENU_LABEL) {
            continue;
        }

        w = (uint16_t) (menu_draw_measure(entries[i].label) + pad2);
        if (entries[i].icon_window != XCB_WINDOW_NONE) {
            w = (uint16_t) (w + icon_offset);
        }
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
 * @brief Compute the pixel width required to display all menu entries
 *
 * Iterates over all entries and measures each label, adding space for
 * the left padding, the submenu indicator, and, for an entry with an
 * associated @c icon_window, its own application icon.
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
    uint16_t pad2 = (uint16_t) (config->theme.menu.padding.horizontal * 2u);
    uint16_t icon_offset = 0u;

    /* Space reserved for an entry's own icon plus one more gap (the
     * same width as the menu's own left padding) before its label;
     * see 'theme.menu.show-pixmaps''s own doc comment in config.h. */
    if (config->theme.menu.show_pixmaps) {
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
        icon_offset = (uint16_t)
            (icon_size + config->theme.menu.padding.horizontal);
    }

    /* Measured in three passes, one font each, rather than switching
     * between 'unselected.font' and 'selected.font' on every regular
     * entry as a single combined pass would: since
     * max(max(a, b)) == max(max(a), max(b)), computing each font's
     * contribution to the overall maximum in its own pass gives the
     * identical result while letting 'text_renderer_init's
     * already-active check actually skip a redundant reopen of the
     * same font (on the X11 backend, a full round trip) for every
     * single entry, which is what interleaving the two fonts would
     * otherwise force on every call. */

    text_renderer_init(connection, config->theme.menu.label.font);
    for (int i = 0; i < entry_count; ++i) {
        uint16_t w;

        if (entries[i].type != CTXMENU_LABEL) {
            continue;
        }

        w = (uint16_t) (menu_draw_measure(entries[i].label) + pad2);
        if (w > max_w) {
            max_w = w;
        }
    }

    text_renderer_init(connection, config->theme.menu.unselected.font);
    max_w = s_max_width_for_selectable(entries, entry_count, pad2,
            icon_offset, max_w);

    text_renderer_init(connection, config->theme.menu.selected.font);
    max_w = s_max_width_for_selectable(entries, entry_count, pad2,
            icon_offset, max_w);

    return max_w;
}


/**
 * @brief Return the row index at the given pixel Y, or -1 if none
 *
 * Binary-searches @p state->entry_top_y for the last row whose top
 * offset is @c <= @p y, then checks @p y still falls within that row's
 * height, giving @e O(log n) lookup when the cache is available.
 * Falls back to an @e O(n) linear walk over @p state->entries when it
 * is not (e.g., @c calloc failed at menu-open time).
 *
 * @param state Menu state
 * @param y     Pixel Y relative to menu window
 *
 * @return Entry index, or -1 if @p y is outside all rows
 *
 * @note Complexity: @e O(log n) with the cache, @e O(n) without it,
 *       where @e n is @p state->entry_count
 */
static int s_entry_at_y(const ctxmenu_state_td *state, int y)
{
    int lo;
    int hi;
    int mid;
    int found;
    int cur_y;
    int row_h;

    if (state->entry_top_y != NULL) {
        lo = 0;
        hi = state->entry_count - 1;
        found = -1;

        while (lo <= hi) {
            mid = lo + (hi - lo) / 2;
            if (state->entry_top_y[mid] <= y) {
                found = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }

        if (found < 0) {
            return -1;
        }

        row_h = s_row_height(state->entries[found].type);
        return (y < state->entry_top_y[found] + row_h) ? found : -1;
    }

    /* Fallback linear scan when no cache is available */
    cur_y = (int) state->config->theme.menu.padding.vertical;
    for (int i = 0; i < state->entry_count; ++i) {
        row_h = s_row_height(state->entries[i].type);
        if (y >= cur_y && y < cur_y + row_h) {
            return i;
        }
        cur_y += row_h;
    }

    return -1;
}



/**
 * @brief Records whether the most recently activated entry was
 *        triggered by the keyboard (@c Return / @c KP_Enter or a
 *        printable-character shortcut) or by a mouse click
 *
 * Set immediately before @c s_ctxmenu_activate_entry is called from
 * either @c ctxmenu_handle_keypress (@c true) or @c ctxmenu_handle_click
 * (@c false), so that an entry's @c on_activate callback can query
 * @c ctxmenu_last_activation_was_keyboard to decide between a
 * keyboard-driven and a pointer-driven interaction (e.g., window move
 * or resize).
 */
static bool s_activated_by_keyboard = false;


/**
 * @brief Activate the entry at the given index in a context menu
 *
 * Closes the entire menu hierarchy first (releasing keyboard and pointer
 * grabs), then invokes the entry's @p on_activate callback or calls
 * @c lifecycle_dispatch_launch for command entries.  Closing before the
 * callback allows the callback to establish its own grabs (e.g., for
 * interactive keyboard move or resize) without conflicting with the
 * menu's active grab.  For separator, label, or disabled entries no
 * action is taken but @c true is returned to consume the event.
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
    ctxmenu_entry_td e;
    ctxmenu_state_td *root;
    xcb_connection_t *conn;
    surface_td *surf;

    if (state == NULL || idx < 0 || idx >= state->entry_count) {
        return false;
    }

    e = state->entries[idx];
    if (e.type == CTXMENU_SEPARATOR || e.type == CTXMENU_LABEL ||
            e.is_disabled) {
        return true;
    }

    root = state;
    while (root->parent != NULL) {
        root = root->parent;
    }

    /* Save connection and surface before close clears them */
    conn = root->connection;
    surf = root->surface;

    /* Close first so keyboard and pointer grabs are released before the
     * callback runs; this lets callbacks establish their own grabs */
    ctxmenu_close(root);

    if (e.on_activate != NULL) {
        e.on_activate(conn, e.userdata);
    } else if (e.command[0] != '\0') {
        lifecycle_dispatch_launch(surf, e.command, e.class_name);
    }

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
    top_y = s_entry_top_y(state, idx);
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
     * aside; see that field's own doc comment in config.h. */
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
                (uint16_t) (state->config->theme.menu.padding.horizontal * 2u));
        rect.height = 1;
        xcb_poly_fill_rectangle(conn, state->window, gc, 1, &rect);
        xcb_free_gc(conn, gc);
        return;
    }

    text_x = (int16_t) state->config->theme.menu.padding.horizontal;

    /* An entry with an associated client (see 'icon_window''s own doc
     * comment in ctxmenu.h) reserves this same square of space
     * whether or not a real icon is actually drawn into it: a client
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
            wmicon_draw_at(conn, state->surface->ewmh, e->icon_window,
                    state->window, text_x, icon_y, icon_size,
                    fg, bg, e->icon_cache);
        }
        text_x = (int16_t) (text_x + icon_size +
                (int16_t) state->config->theme.menu.padding.horizontal);
    }

    (void) snprintf(label_buf, sizeof(label_buf), "%s", e->label);

    text_renderer_init(conn, (e->type == CTXMENU_LABEL)
            ? state->config->theme.menu.label.font
            : (is_sel)
                ? state->config->theme.menu.selected.font
                : state->config->theme.menu.unselected.font);
    text_renderer_set_color(fg, bg);
    menu_draw_label(conn, state->window,
            text_x,
            (int16_t) (top_y + WM_CTXMENU_ROW_HEIGHT - 5),
            label_buf);

    if (e->type == CTXMENU_SUBMENU) {
        arrow_w = menu_draw_measure(MENU_CONTEXT_CTXMENU_SUBMENU_ARROW);
        arrow_x = (int16_t) (state->width -
                (uint16_t) state->config->theme.menu.padding.horizontal -
                arrow_w);
        menu_draw_label(conn, state->window,
                arrow_x,
                (int16_t) (top_y + WM_CTXMENU_ROW_HEIGHT - 5),
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
 * touched.  A single deselect (e.g. the pointer leaving every entry)
 * passes @c -1 for @p idx_b.
 *
 * @param state Menu state the entries belong to
 * @param idx_a First index to redraw, or @c -1 for none
 * @param idx_b Second index to redraw, or @c -1 for none; skipped if
 *              equal to @p idx_a
 *
 * @note Complexity: @e O(1)
 */
static void s_ctxmenu_repaint_entries(ctxmenu_state_td *state,
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


/**
 * @brief Move a menu's selection one step in a direction, skipping
 *        separators, labels, and disabled entries
 *
 * Shared by the Up and Down arrow handling in @c ctxmenu_handle_
 * keypress, which only differ in @p step's sign and where an
 * initially-unselected state (@c selected @c < @c 0) starts scanning
 * from; everything else (wrapping around either end of the entry
 * list, skipping unselectable entries, repainting once a valid one is
 * found) is identical between the two.
 *
 * @param state Menu state whose selection to move
 * @param step  @c +1 to move down/forward, @c -1 to move up/backward
 *
 * @note Complexity: @e O(n), where @e n is @c state->entry_count
 */
static void s_ctxmenu_move_selection(ctxmenu_state_td *state, int step)
{
    int prev_sel = state->selected;
    int next = (prev_sel < 0)
        ? ((step > 0) ? 0 : state->entry_count - 1)
        : prev_sel + step;
    int i;

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
        s_ctxmenu_repaint_entries(state, prev_sel, next);
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
    ctxmenu_state_td *root;
    ctxmenu_state_td *child_state;
    int16_t sub_x;
    int16_t sub_y;

    if (state == NULL || state->window == XCB_WINDOW_NONE) {
        return false;
    }

    /* Up arrow: move selection to previous selectable entry */
    if (keysym == KS_UP) {
        s_ctxmenu_move_selection(state, -1);
        return true;
    }

    /* Down arrow: move selection to next selectable entry */
    if (keysym == KS_DOWN) {
        s_ctxmenu_move_selection(state, 1);
        return true;
    }

    /* Right arrow: open submenu for the selected entry */
    if (keysym == KS_RIGHT) {
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
                sub_y = (int16_t) (state->origin_y +
                        (int16_t) s_entry_top_y(state, sel));
                ctxmenu_show(connection, surface, child_state,
                        sub_x, sub_y, config);
                state->child = child_state;
            }
        }
        return true;
    }

    /* Left arrow: close this submenu and return to parent */
    if (keysym == KS_LEFT) {
        if (state->parent != NULL) {
            /* No explicit repaint needed here: submenus open clear of
             * the parent's own area (see 'sub_x' above), and on the
             * rare occasion one gets clamped close enough to overlap
             * it anyway, destroying it (just below) already makes the
             * X server generate its own 'Expose' for whatever area of
             * the parent that uncovers, which 'ctxmenu_repaint_window'
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
                return ctxmenu_handle_keypress(connection, surface,
                        state, KS_RIGHT, config);
            }
            s_activated_by_keyboard = true;
            return s_ctxmenu_activate_entry(state, sel);
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

    /* Printable character: jump to first matching entry */
    if (keysym > 0xFFu || !isprint((int) keysym)) {
        return false;
    }

    target = (char) tolower((int) ((unsigned char) keysym));
    match_count = 0;
    match_idx = -1;
    for (int i = 0; i < state->entry_count; ++i) {
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

    sel = state->selected;
    state->selected = match_idx;
    s_ctxmenu_repaint_entries(state, sel, match_idx);
    if (match_count == 1) {
        s_activated_by_keyboard = true;
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
    uint32_t values[4];
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
    size_t entry_count;

    if (connection == NULL || surface == NULL || state == NULL ||
            config == NULL || state->entries == NULL ||
            state->entry_count <= 0) {
        return;
    }

    /* Copied into a 'size_t' local right after the guard above proved
     * it positive: GCC's allocation-size analysis cannot otherwise
     * see past the 'ctxmenu_close'/'s_compute_width' calls between
     * here and the 'calloc' below to know 'state->entry_count' is
     * still positive at that point, since either call could in
     * principle modify the struct through the same pointer, so
     * without this it falls back to assuming the field's entire
     * signed range is possible there. */
    entry_count = (size_t) state->entry_count;

    ctxmenu_close(state);

    state->connection = connection;
    state->surface = surface;
    state->config = config;
    state->selected = -1;
    state->last_motion_y = -1;

    state->width = s_compute_width(connection, state->entries,
            state->entry_count, config);

    /* Cache each row's top-Y offset so 's_entry_top_y' and
     * 's_entry_at_y' need not re-walk 'entries' on every repaint,
     * click, or motion event; 'entry_top_y' is left 'NULL' (and both
     * helpers fall back to an 'O(n)' walk) if 'calloc' fails */
    state->entry_top_y = (int32_t *) calloc(entry_count,
            sizeof(int32_t));
    state->height = s_build_layout(state);

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
           XCB_CW_BORDER_PIXEL      |
           XCB_CW_OVERRIDE_REDIRECT |
           XCB_CW_EVENT_MASK;
    values[0] = config->theme.menu.unselected.color.background;
    values[1] = config->theme.menu.border.color;
    values[2] = 1;  /* override_redirect: prevent WM from managing it */
    values[3] = XCB_EVENT_MASK_EXPOSURE     |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_POINTER_MOTION;

    xcb_create_window(connection, XCB_COPY_FROM_PARENT,
            state->window, surface->screen->root,
            clamped_x, clamped_y,
            state->width, state->height,
            (uint16_t) config->theme.menu.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* Raise to the top */
    stk[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, state->window,
            XCB_CONFIG_WINDOW_STACK_MODE, stk);

    xcb_map_window(connection, state->window);

    /* Grab keyboard and pointer for the root menu only (not submenus).
     * The keyboard grab redirects all key events (including 'Escape')
     * to the window manager so the menu can be dismissed without the
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
                1,                      /* owner_events: report events
                                           normally to whichever window
                                           in the menu hierarchy the
                                           pointer is actually over
                                           (needed so 'MotionNotify'
                                           events are delivered with the
                                           correct 'event' window and
                                           window-relative coordinates,
                                           enabling hover highlight) */
                surface->screen->root,
                XCB_EVENT_MASK_BUTTON_PRESS   |
                XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_POINTER_MOTION,
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
    }

    /* Always release keyboard and pointer grabs when the root menu
     * closes, even if the window was already gone.  This prevents stale
     * grabs from blocking further input when a race condition or early
     * destroy leaves 'window' as 'XCB_WINDOW_NONE' before close. */
    if (state->parent == NULL && state->connection != NULL) {
        xcb_ungrab_keyboard(state->connection, XCB_CURRENT_TIME);
        xcb_ungrab_pointer(state->connection, XCB_CURRENT_TIME);
    }

    if (state->connection != NULL) {
        xcb_flush(state->connection);
    }

    state->window = XCB_WINDOW_NONE;
    state->selected = -1;
    state->width = 0;
    state->height = 0;
    state->connection = NULL;
    state->surface = NULL;
    state->config = NULL;

    free(state->entry_top_y);
    state->entry_top_y = NULL;
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
            state->config->theme.menu.unselected.color.background,
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

    idx = s_entry_at_y(state, y);
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

    s_activated_by_keyboard = false;

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

        /* Mark this entry selected (a click may land here with no
         * prior hover over this exact row, e.g. the pointer already
         * resting here when the menu first mapped) so it stays
         * visibly highlighted for as long as its own submenu is
         * open, the same as the keyboard path already shows via
         * whatever row 'state->selected' was left on by prior
         * up/down navigation. */
        if (state->selected != idx) {
            int prev_sel = state->selected;

            state->selected = idx;
            s_ctxmenu_repaint_entries(state, prev_sel, idx);
        }

        child_state->entries = state->entries[idx].items;
        child_state->entry_count = state->entries[idx].item_count;
        child_state->parent = state;
        child_state->child = NULL;

        sub_x = (int16_t) (state->origin_x + (int16_t) state->width);
        sub_y = (int16_t) (state->origin_y +
                (int16_t) s_entry_top_y(state, idx));

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


/* Query whether the last activated entry was triggered by the keyboard */
bool ctxmenu_last_activation_was_keyboard(void)
{
    return s_activated_by_keyboard;
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

    idx = s_entry_at_y(state, y);

    /* Clear selection when pointer leaves all entries */
    if (idx < 0 || idx >= state->entry_count ||
            state->entries[idx].type == CTXMENU_SEPARATOR ||
            state->entries[idx].type == CTXMENU_LABEL ||
            state->entries[idx].is_disabled) {
        if (state->selected >= 0) {
            prev_sel = state->selected;
            state->selected = -1;
            s_ctxmenu_repaint_entries(state, prev_sel, -1);
        }
        return;
    }

    if (idx == state->selected) {
        return;
    }

    prev_sel = state->selected;
    state->selected = idx;
    s_ctxmenu_repaint_entries(state, prev_sel, idx);
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


/**
 * @brief Repaint whichever submenu under @p root currently owns @p win
 *
 * Shared by every concrete menu's own @c X_repaint (root menu, window
 * menu, window list): each one only differs in which @c root state it
 * passes, so this one function replaces an identical lookup-then-
 * repaint sequence that used to be copied into each of them.
 *
 * @param root Top-level state of the concrete menu's own submenu tree
 * @param win  Window the repaint request arrived for
 *
 * @note No-op if @p win does not belong to any submenu under @p root
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void ctxmenu_repaint_window(ctxmenu_state_td *root, xcb_window_t win)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(root, win);
    if (state != NULL) {
        ctxmenu_repaint(state);
    }
}


/**
 * @brief Forward a pointer-motion event to whichever submenu under
 *        @p root currently owns @p win
 *
 * Shared by every concrete menu's own @c X_handle_motion; see
 * @c ctxmenu_repaint_window's own doc comment for the general
 * reasoning.
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
        xcb_window_t win, int x, int y)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(root, win);
    if (state != NULL) {
        ctxmenu_handle_motion(state, x, y);
    }
}


/**
 * @brief Forward a click, translated to menu-local coordinates, to
 *        whichever submenu under @p root currently owns @p win
 *
 * Shared by every concrete menu's own @c X_handle_click; see
 * @c ctxmenu_repaint_window's own doc comment for the general
 * reasoning.
 *
 * @param connection XCB connection
 * @param surface    Surface the click occurred on
 * @param root       Top-level state of the concrete menu's own
 *                   submenu tree
 * @param win        Window the click event arrived for
 * @param x          Pointer X position, in @p win's own coordinates
 * @param y          Pointer Y position, in @p win's own coordinates
 * @param config     Active configuration
 *
 * @return @c true if @p win belonged to a submenu under @p root and
 *         the click was forwarded, @c false otherwise
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
bool ctxmenu_handle_click_window(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root, xcb_window_t win,
        int x, int y, const config_td *config)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(root, win);
    if (state == NULL) {
        return false;
    }

    x -= state->origin_x;
    y -= state->origin_y;
    return ctxmenu_handle_click(connection, surface, state, x, y,
            config);
}


/**
 * @brief Forward a keypress to the deepest currently open submenu
 *        under @p root
 *
 * Applies the keypress to the deepest open submenu, not always
 * @p root itself: without this, arrow keys would keep moving the
 * selection in a top-level list even while a nested submenu was open
 * in front of it, making that submenu look unresponsive to the
 * keyboard.  Shared by every concrete menu's own
 * @c X_handle_keypress.
 *
 * @param connection XCB connection
 * @param surface    Surface the key press occurred on
 * @param root       Top-level state of the concrete menu's own
 *                   submenu tree
 * @param keysym     Keysym of the pressed key
 * @param config     Active configuration
 *
 * @return @c true if the key was consumed, @c false otherwise
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
bool ctxmenu_handle_keypress_deepest(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root,
        xcb_keysym_t keysym, const config_td *config)
{
    ctxmenu_state_td *deepest;

    deepest = ctxmenu_find_state_for_window(root,
            ctxmenu_deepest_window(root));
    if (deepest == NULL) {
        deepest = root;
    }

    return ctxmenu_handle_keypress(connection, surface, deepest,
            keysym, config);
}
