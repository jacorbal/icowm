/**
 * @file menu/cycledraw.c
 *
 * @brief Cycle menu visual rendering
 *
 * Implements the preview highlighting helpers and the @c cycle_draw
 * function that repaints the cycle-menu window.  State management,
 * input processing, and open/close logic stay in @c menu/cycle.c.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Render includes */
#include <render/icon.h>
#include <render/text.h>
#include <render/wmicon.h>

/* Default initial values */
#include <defs/ctxmenu.h>
#include <defs/cycle.h>
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <config.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/cycle.h>
#include <menu/internal.h>


/**
 * @brief Resolve the X window used as the visual target for cycle
 *        preview
 *
 * Determines which X window should be used to represent a client during
 * cycle preview operations.  The function accounts for icon menu mode,
 * hidden clients, and window decorations to select the appropriate
 * drawable target.
 *
 * @param client       Pointer to the client to evaluate
 * @param is_icon_menu Whether the cycle preview is operating in icon
 *                     menu mode
 *
 * @return The X window ID to use as preview target, or
 *         @c XCB_WINDOW_NONE if no valid target is available
 *
 * @note Returns @c XCB_WINDOW_NONE if @p client is null, hidden, or
 *       lacks a valid drawable target
 * @note Prefers @c client->icon_window in icon menu mode when available
 * @note Uses the frame window when the client is decorated
 * @note Complexity: @e O(1)
 */
xcb_window_t mi_cycle_preview_target(const client_td *client,
        bool is_icon_menu)
{
    if (client == NULL) {
        return XCB_WINDOW_NONE;
    }

    if (is_icon_menu) {
        return (client->icon_window != 0)
            ? client->icon_window
            : XCB_WINDOW_NONE;
    }

    if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
        return XCB_WINDOW_NONE;
    }

    if (client_is_decorated(client) && client->frame != 0) {
        return client->frame;
    }

    return client->window;
}


/**
 * @brief Return the border width for a cycle-preview target
 *
 * Computes the border width to use for a preview target in the cycle
 * interface, selecting the icon border width for icon previews, no
 * border for decorated client frames, and the normal window border
 * width for undecorated window targets.  When the target is highlighted,
 * an extra selection width is added.
 *
 * @param client         Pointer to the client associated with the target
 * @param config         Pointer to the active configuration
 * @param is_icon_menu   Whether the cycle menu is showing icon previews
 * @param is_highlighted Whether the target is currently highlighted
 *
 * @return Border width to apply to the preview target
 *
 * @note Complexity: @e O(1)
 */
uint32_t mi_cycle_preview_border_width(const client_td *client,
        const config_td *config, bool is_icon_menu, bool is_highlighted)
{
    uint32_t border_width;

    if (config == NULL) {
        return 0u;
    }

    if (is_icon_menu) {
        border_width = config->theme.icon.active.border.width;
    } else if (client != NULL &&
            client_is_decorated(client) &&
            client->frame != 0) {
        border_width = 0u;
    } else {
        border_width = config->theme.window.active.border.width;
    }

    if (is_highlighted) {
        border_width += WM_ICON_CYCLE_SEL_BORDER_EXTRA;
    }

    return border_width;
}


/**
 * @brief Apply preview border color and width to a target window
 *
 * Updates the border width and border color of the given cycle-preview
 * target.  For decorated client frames in window mode, the frame
 * background is also updated and cleared so the visual highlight is
 * redrawn consistently; otherwise only the border pixel is changed.
 *
 * @param connection     Active XCB connection used to update the window
 * @param target         Target window receiving the preview styling
 * @param client         Pointer to the client associated with @p target
 * @param config         Pointer to the active configuration
 * @param is_icon_menu   Whether the cycle menu is showing icon previews
 * @param border_color   Border color to apply
 * @param is_highlighted Whether the target is currently highlighted
 *
 * @note Complexity: @e O(1)
 */
void mi_cycle_preview_style_target(xcb_connection_t *connection,
        xcb_window_t target, const client_td *client,
        const config_td *config, bool is_icon_menu,
        uint32_t border_color, bool is_highlighted)
{
    uint32_t border_width;
    uint32_t frame_values[2];

    if (connection == NULL || target == XCB_WINDOW_NONE ||
            config == NULL) {
        return;
    }

    border_width = mi_cycle_preview_border_width(client, config,
            is_icon_menu, is_highlighted);
    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, &border_width);

    if (!is_icon_menu &&
            client != NULL &&
            client_is_decorated(client) &&
            client->frame == target) {
        frame_values[0] = border_color;
        frame_values[1] = border_color;
        xcb_change_window_attributes(connection, target,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                frame_values);
        xcb_clear_area(connection, 0, target, 0, 0, 0, 0);
    } else {
        xcb_change_window_attributes(connection, target,
                XCB_CW_BORDER_PIXEL, &border_color);
    }
}


/**
 * @brief Apply cycle preview highlighting and stacking for the selected
 *        client
 *
 * Updates the visual state of the currently selected client in the
 * cycle preview by adjusting its border color and ensuring it is
 * stacked above its peers.  Also restores the previous preview client's
 * border color according to its active or inactive state.
 *
 * @param connection Pointer to the XCB connection
 * @param config     Pointer to the configuration containing theme data
 *
 * @note No-op if required state (connection, config, menu, or
 *       selection) is invalid or incomplete, or if the selected
 *       client is the same one already previewed (nothing to change)
 * @note Restores the previous preview client's border color before
 *       applying the new selection highlight
 * @note Ensures the selected target window is raised above others
 * @note Updates @c g_cycle_menu.preview_client to track the current
 *       preview
 * @note Complexity: @e O(1)
 */
void mi_cycle_preview_apply(xcb_connection_t *connection,
        const config_td *config)
{
    client_td *selected;
    client_td *previous;
    xcb_window_t selected_target;
    xcb_window_t previous_target;
    uint32_t values[2];
    uint32_t selected_border;
    uint32_t previous_border;
    bool prev_is_active;

    if (connection == NULL || config == NULL ||
            g_cycle_menu.window == XCB_WINDOW_NONE ||
            g_cycle_menu.surface == NULL || g_cycle_menu.desktop == NULL ||
            g_cycle_menu.selected < 0 ||
            g_cycle_menu.selected >= g_cycle_menu.count) {
        return;
    }

    selected = g_cycle_menu.clients[g_cycle_menu.selected];
    selected_target = mi_cycle_preview_target(selected,
            g_cycle_menu.is_icon_menu);
    if (selected == NULL || selected_target == XCB_WINDOW_NONE) {
        return;
    }

    previous = g_cycle_menu.preview_client;

    /* Selection unchanged since this same client was last previewed
     * (e.g. re-called for an 'Expose' on the menu window itself, or
     * navigating with only one client in the cycle, which always
     * "changes" the index back to the same single entry): nothing
     * about the preview differs from what is already applied, so
     * skip repeating every border/background/stacking request below
     * for no visible change. */
    if (previous == selected) {
        return;
    }

    if (previous != NULL) {
        previous_target = mi_cycle_preview_target(previous,
                g_cycle_menu.is_icon_menu);

        if (previous_target != XCB_WINDOW_NONE) {
            prev_is_active =
                (g_cycle_menu.desktop->client_active_id == previous->id);

            if (g_cycle_menu.is_icon_menu) {
                previous_border = config->theme.icon.inactive.border.color;
            } else if (prev_is_active) {
                previous_border = config->theme.window.active.border.color;
            } else {
                previous_border = config->theme.window.inactive.border.color;
            }

            mi_cycle_preview_style_target(connection, previous_target,
                    previous, config, g_cycle_menu.is_icon_menu,
                    previous_border, false);

            if (g_cycle_menu.is_icon_menu) {
                /* Full render (stacking below the tray, colors,
                 * pixmap, caption, and hint indicators all included)
                 * via the same shared function every other place a
                 * deselected icon needs repainting already uses (see
                 * 's_cycle_repaint_icon' in menu/cycle.c), rather
                 * than this function's own separate, previously
                 * duplicated implementation of the same thing --
                 * duplication that is exactly how this and that
                 * other one drifted out of sync in the first place
                 * (this one never learned to omit the pixmap for a
                 * newly *selected* icon, below). */
                ri_render_client_icon(g_cycle_menu.desktop, previous,
                        true);
            } /* ! if (g_cycle_menu.is_icon_menu) */
        } /* ! if (previous_target) */
    }

    selected_border = (g_cycle_menu.is_icon_menu)
        ? config->theme.icon.active.border.color
        : config->theme.window.active.border.color;
    mi_cycle_preview_style_target(connection, selected_target,
            selected, config, g_cycle_menu.is_icon_menu,
            selected_border, true);

    if (g_cycle_menu.is_icon_menu) {
        /* Same "selected" render every other place a newly selected
         * icon needs it already uses (see 's_cycle_repaint_icon' in
         * menu/cycle.c): active colors, caption, and hint indicators
         * all included, deliberately just the pixmap left out,
         * rather than this function's own separate, previously
         * duplicated implementation, which (unlike that shared one)
         * never learned to omit the pixmap here at all. */
        ri_render_client_icon_selected(connection, selected);
    }

    values[0] = g_cycle_menu.window;
    values[1] = XCB_STACK_MODE_BELOW;
    xcb_configure_window(connection, selected_target,
            XCB_CONFIG_WINDOW_SIBLING |
            XCB_CONFIG_WINDOW_STACK_MODE,
            values);

    g_cycle_menu.preview_client = selected;
    xcb_flush(connection);
}


/**
 * @brief Per-call drawing constants shared by every row @c cycle_draw
 *        paints in one call, computed once up front rather than
 *        re-derived from @c config on each row
 */
struct s_cycle_row_style_s {
    uint32_t fg_sel;
    uint32_t bg_sel;
    uint32_t fg_nor;
    uint32_t bg_nor;
    int16_t pad_x;
    int16_t icon_offset;
    uint16_t icon_size;
};


/**
 * @brief Resolve the current theme's cycle-row drawing constants
 *
 * @param config Active configuration
 * @param style  Receives the resolved constants
 *
 * @note Complexity: @e O(1)
 */
static void s_cycle_row_style(const config_td *config,
        struct s_cycle_row_style_s *style)
{
    style->fg_sel = config->theme.menu.selected.color.foreground;
    style->bg_sel = config->theme.menu.selected.color.background;
    style->fg_nor = config->theme.menu.unselected.color.foreground;
    style->bg_nor = config->theme.menu.unselected.color.background;
    style->pad_x = (int16_t) config->theme.menu.padding.horizontal;
    style->icon_offset = 0;
    style->icon_size = 0u;

    /* Space reserved for a row's own client icon plus one more gap
     * (the same width as the menu's own left padding) before its
     * label; see 'theme.menu.show-pixmaps''s own doc comment in
     * config.h and 'WM_MENU_ICON_INSET' in defs/ctxmenu.h. */
    if (config->theme.menu.show_pixmaps) {
        /* '#if', not a runtime ternary: both operands are fixed
         * compile-time constants, so a ternary here left one branch
         * provably unreachable to the compiler (-Wunreachable-code).
         * Still guards the arithmetic against a future edit to either
         * constant that would otherwise underflow silently. */
#if WM_CYCLE_MENU_ROW_HEIGHT > WM_MENU_ICON_INSET
        style->icon_size = (uint16_t)
            (WM_CYCLE_MENU_ROW_HEIGHT - WM_MENU_ICON_INSET);
#else
        style->icon_size = 0u;
#endif
        style->icon_offset =
            (int16_t) (style->icon_size + style->pad_x);
    }
}


/**
 * @brief Paint one row of the cycle menu, background through label
 *
 * Self-contained: callers need no separate clear step first, whether
 * repainting the whole viewport or just this one row on its own (see
 * @c cycle_draw's own doc comment for when each happens).
 *
 * @param connection XCB connection
 * @param i          Absolute entry index to draw (not viewport-
 *                    relative); must fall within the current viewport
 * @param pad_y       Vertical padding, for this row's own Y offset
 * @param style      Drawing constants from @c s_cycle_row_style
 *
 * @note Complexity: @e O(1)
 */
static void s_cycle_draw_row(xcb_connection_t *connection, int i,
        int16_t pad_y, const struct s_cycle_row_style_s *style)
{
    int16_t row_y = (int16_t) (pad_y +
            (i - g_cycle_menu.scroll_offset) * WM_CYCLE_MENU_ROW_HEIGHT);
    client_td *row_client = (i >= 0 && i < g_cycle_menu.count)
        ? g_cycle_menu.clients[i] : NULL;
    int16_t text_x = style->pad_x;

    if (i == g_cycle_menu.selected) {
        menu_draw_row_bg(connection, g_cycle_menu.window, style->bg_sel,
                row_y, (uint16_t) WM_CYCLE_MENU_ROW_HEIGHT,
                g_cycle_menu.width);
        text_renderer_set_color(style->fg_sel, style->bg_sel);
    } else {
        menu_draw_row_bg(connection, g_cycle_menu.window, style->bg_nor,
                row_y, (uint16_t) WM_CYCLE_MENU_ROW_HEIGHT,
                g_cycle_menu.width);
        text_renderer_set_color(style->fg_nor, style->bg_nor);
    }

    /* A row with no client of its own (should not normally happen,
     * but 's_cycle_close_and_apply' and similar guard against it
     * elsewhere too) simply gets no icon and no reserved space, same
     * as 'icon_offset' being 0 when 'show-pixmaps' is off. */
    if (style->icon_size > 0u && row_client != NULL &&
            g_cycle_menu.surface != NULL) {
        int16_t icon_y = (int16_t) (row_y +
                (WM_CYCLE_MENU_ROW_HEIGHT - (int) style->icon_size) / 2);

        wmicon_draw_at(connection, g_cycle_menu.surface->ewmh,
                row_client->window, g_cycle_menu.window,
                style->pad_x, icon_y, style->icon_size,
                (i == g_cycle_menu.selected) ? style->fg_sel
                    : style->fg_nor,
                (i == g_cycle_menu.selected) ? style->bg_sel
                    : style->bg_nor,
                &row_client->icon_pixmap_cache);
        text_x = (int16_t) (style->pad_x + style->icon_offset);
    }

    menu_draw_label(connection, g_cycle_menu.window,
            text_x,
            (int16_t) (row_y + WM_CYCLE_MENU_ROW_HEIGHT - 4),
            g_cycle_menu.labels[i]);
}


/* Repaint all menu entries */
void cycle_draw(xcb_connection_t *connection, const config_td *config)
{
    struct s_cycle_row_style_s style;
    int16_t pad_y;
    bool need_full_repaint;

    if (connection == NULL || config == NULL ||
            g_cycle_menu.window == XCB_WINDOW_NONE) {
        return;
    }

    s_cycle_row_style(config, &style);
    pad_y = (int16_t) config->theme.menu.padding.vertical;

    text_renderer_init(connection, config->theme.menu.unselected.font);

    /* A viewport shift (scrolling) changes every row actually shown,
     * so it still needs the full loop below; otherwise selection
     * moved between two rows already on screen, and only those two
     * actually changed which color/text they show -- repainting the
     * rest would be identical to what is already there. */
    need_full_repaint = !g_cycle_menu.has_drawn_once ||
        g_cycle_menu.last_drawn_scroll_offset != g_cycle_menu.scroll_offset;

    if (need_full_repaint) {
        for (int i = g_cycle_menu.scroll_offset;
                i < g_cycle_menu.scroll_offset +
                    g_cycle_menu.viewport_rows;
                ++i) {
            s_cycle_draw_row(connection, i, pad_y, &style);
        }
    } else if (g_cycle_menu.last_drawn_selected != g_cycle_menu.selected) {
        if (g_cycle_menu.last_drawn_selected >= g_cycle_menu.scroll_offset &&
                g_cycle_menu.last_drawn_selected < g_cycle_menu.scroll_offset +
                    g_cycle_menu.viewport_rows) {
            s_cycle_draw_row(connection, g_cycle_menu.last_drawn_selected,
                    pad_y, &style);
        }
        if (g_cycle_menu.selected >= g_cycle_menu.scroll_offset &&
                g_cycle_menu.selected < g_cycle_menu.scroll_offset +
                    g_cycle_menu.viewport_rows) {
            s_cycle_draw_row(connection, g_cycle_menu.selected,
                    pad_y, &style);
        }
    }

    g_cycle_menu.last_drawn_selected = g_cycle_menu.selected;
    g_cycle_menu.last_drawn_scroll_offset = g_cycle_menu.scroll_offset;
    g_cycle_menu.has_drawn_once = true;

    /* Draw scroll-indicator arrows in the top/bottom padding areas when
     * there are hidden entries above or below the viewport.  Only
     * meaningful as part of a full repaint: their content depends
     * solely on 'scroll_offset' and 'count', neither of which changes
     * on a same-viewport selection move.
     * 'menu_draw_label' positions text by its baseline, and the
     * top/bottom padding strips are each only 'pad_y' pixels tall
     * (the default theme's 4px is smaller than most fonts' own
     * ascent), so a naive baseline offset clips the glyph against
     * whichever window edge is closer: the top indicator's baseline
     * sits at the font's own ascent from Y=0 (see 'text_font_ascent'),
     * keeping it below the window's top edge; the bottom indicator's
     * sits 'descent' pixels above the window's bottom edge (see
     * 'text_font_descent'), keeping it above that edge instead. */
    if (need_full_repaint && g_cycle_menu.count > g_cycle_menu.viewport_rows) {
        int16_t top_baseline_y = text_font_ascent();

        /* Up arrow: entries exist above the viewport */
        if (g_cycle_menu.scroll_offset > 0) {
            menu_draw_row_bg(connection, g_cycle_menu.window,
                    style.bg_nor, 0, (uint16_t) pad_y,
                    g_cycle_menu.width);
            text_renderer_set_color(style.fg_sel, style.bg_nor);
            menu_draw_label(connection, g_cycle_menu.window,
                    (int16_t) (g_cycle_menu.width / 2u - 4u),
                    top_baseline_y,
                    WM_CYCLE_MENU_SCROLL_UP_INDICATOR);
        } else {
            /* Clear the top padding area when no arrow is needed */
            menu_draw_row_bg(connection, g_cycle_menu.window,
                    style.bg_nor, 0, (uint16_t) pad_y,
                    g_cycle_menu.width);
        }

        /* Down arrow: entries exist below the viewport */
        if (g_cycle_menu.scroll_offset + g_cycle_menu.viewport_rows <
                g_cycle_menu.count) {
            int16_t bot_y = (int16_t) (pad_y +
                    g_cycle_menu.viewport_rows *
                    WM_CYCLE_MENU_ROW_HEIGHT);
            menu_draw_row_bg(connection, g_cycle_menu.window,
                    style.bg_nor, bot_y, (uint16_t) pad_y,
                    g_cycle_menu.width);
            text_renderer_set_color(style.fg_sel, style.bg_nor);
            menu_draw_label(connection, g_cycle_menu.window,
                    (int16_t) (g_cycle_menu.width / 2u - 4u),
                    (int16_t) (bot_y + pad_y - text_font_descent()),
                    WM_CYCLE_MENU_SCROLL_DOWN_INDICATOR);
        } else {
            /* Clear the bottom padding area when no arrow is needed */
            int16_t bot_y = (int16_t) (pad_y +
                    g_cycle_menu.viewport_rows *
                    WM_CYCLE_MENU_ROW_HEIGHT);
            menu_draw_row_bg(connection, g_cycle_menu.window,
                    style.bg_nor, bot_y, (uint16_t) pad_y,
                    g_cycle_menu.width);
        }
    }

    mi_cycle_preview_apply(connection, config);
    xcb_flush(connection);
}
