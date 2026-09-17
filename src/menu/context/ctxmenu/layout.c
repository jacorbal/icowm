/**
 * @file menu/context/ctxmenu/layout.c
 *
 * @brief Row geometry and hit-testing for a context menu
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* Render includes */
#include <render/text.h>

/* Project includes */
#include <config.h>
#include <client.h>

/* Menu includes */
#include <menu/draw.h>

/* Local includes */
#include <menu/context/ctxmenu/layout.h>


/**
 * @brief Row height in pixels for a given context menu entry type
 *
 * @param type Entry type to measure
 *
 * @return @c WM_CTXMENU_SEP_HEIGHT for a separator,
 *         @c WM_CTXMENU_ROW_HEIGHT for anything else
 *
 * @note Complexity: @e O(1)
 */
static int s_row_height(ctxmenu_entry_type_e type)
{
    return (type == CTXMENU_SEPARATOR)
        ? WM_CTXMENU_SEP_HEIGHT : WM_CTXMENU_ROW_HEIGHT;
}


/**
 * @brief Maximum measured width among non-label, non-separator entries,
 *        in whichever font is currently active
 *
 * Shared by @a ctxmenu_width_compute's second and third measuring
 * passes, for @c unselected.font and @c selected.font respectively.
 * The two are otherwise identical, differing only in which font is
 * active when each is called.  The comment on @a ctxmenu_width_compute
 * explains why those stay two separate passes rather than one combined
 * loop.
 *
 * @param entries     Entries to measure
 * @param entry_count Number of entries in @p entries
 * @param pad2        Horizontal padding to add on both sides
 * @param icon_offset Extra width to add for an entry with an associated
 *                    @c icon_window (0 when @c theme.menu.show-pixmaps
 *                    is off); see @c ctxmenu_width_compute
 * @param max_w Current running maximum, carried in so the two
 *                    passes contribute to one shared result
 *
 * @return The greater of @p max_w and every measured entry's width
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


/* Compute the pixel Y of the top edge of an entry by index */
int ctxmenu_entry_top_y(const ctxmenu_state_td *state, int idx)
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


/* Compute per-row Y offsets and the total menu height */
uint16_t ctxmenu_layout_build(ctxmenu_state_td *state)
{
    int y = (int) state->config->theme.menu.padding.vertical;

    for (int i = 0; i < state->entry_count; ++i) {
        if (state->entry_top_y != NULL) {
            state->entry_top_y[i] = y;
        }
        y += s_row_height(state->entries[i].type);
    }

    return (uint16_t) (y +
            (int) state->config->theme.menu.padding.vertical);
}


/* Compute the pixel width required to display all menu entries */
uint16_t ctxmenu_width_compute(xcb_connection_t *connection,
        const ctxmenu_entry_td *entries, int entry_count,
        const config_td *config)
{
    uint16_t max_w = WM_CTXMENU_MIN_WIDTH;
    uint16_t pad2 =
        (uint16_t) (config->theme.menu.padding.horizontal * 2u);
    uint16_t icon_offset = 0u;

    /* Space reserved for an entry's icon plus one more gap (the same
     * width as the menu's left padding) before its label; see
     * 'theme.menu.show-pixmaps''s comment in 'config.h' */
    if (config->theme.menu.show_pixmaps) {
        /* '#if' and not a ternary; see 'WM_MENU_ICON_INSET' in
         * 'defs/ctxmenu.h' for why */
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
     * entry as a single combined pass would.
     *
     * Since max(max(a, b)) == max(max(a), max(b)), computing each
     * font's contribution to the overall maximum in its pass gives the
     * identical result while asking 'text_renderer_use_font' for each
     * font once instead of alternating between the two on every single
     * entry. */

    (void) text_renderer_use_font(connection,
            config->theme.menu.label.font);
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

    (void) text_renderer_use_font(connection,
            config->theme.menu.unselected.font);
    max_w = s_max_width_for_selectable(entries, entry_count, pad2,
            icon_offset, max_w);

    (void) text_renderer_use_font(connection,
            config->theme.menu.selected.font);
    max_w = s_max_width_for_selectable(entries, entry_count, pad2,
            icon_offset, max_w);

    return max_w;
}


/* Return the row index at the given pixel Y, or -1 if none */
int ctxmenu_entry_at_y(const ctxmenu_state_td *state, int y)
{
    int cur_y;
    int row_h;

    if (state->entry_top_y != NULL) {
        int found = -1;
        unsigned int lo = 0u;
        unsigned int hi = (unsigned int) state->entry_count;

        /* Half-open range, so that the upper bound never has to sit one
         * below zero: a signed index walked down past zero is what lets
         * the optimizer assume its arithmetic cannot overflow */
        while (lo < hi) {
            unsigned int mid = lo + (hi - lo) / 2u;

            if (state->entry_top_y[mid] <= y) {
                found = (int) mid;
                lo = mid + 1u;
            } else {
                hi = mid;
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
