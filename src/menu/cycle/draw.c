/**
 * @file menu/cycle/draw.c
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
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* Render includes */
#include <render/icon.h>
#include <render/outline.h>
#include <render/text.h>
#include <render/wmicon.h>

/* Default initial values */
#include <defs/ctxmenu.h>
#include <defs/cycle.h>
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <config.h>

#include <surface.h>
#include <desktop.h>

/* Local includes */
#include <cmds/client/icon.h>
#include <menu/draw.h>
#include <menu/cycle.h>
#include <menu/internal.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


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

    /* Space reserved for a row's client icon plus one more gap (the
     * same width as the menu's left padding) before its label; see
     * 'theme.menu.show-pixmaps''s comment in 'config.h' and
     * 'WM_MENU_ICON_INSET' in 'defs/ctxmenu.h' */
    if (config->theme.menu.show_pixmaps) {
        /* '#if' and not a ternary; see 'WM_MENU_ICON_INSET' in
         * 'defs/ctxmenu.h' for why */
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
 * repainting the whole viewport or just this one row on its own.
 *
 * @param connection XCB connection
 * @param i          Absolute entry index to draw, not one relative
 *                   to the viewport; must fall within the current
 *                   viewport
 * @param pad_y      Vertical padding, for this row's Y offset
 * @param style      Drawing constants from @a s_cycle_row_style
 *
 * @note Complexity: @e O(1)
 *
 * @see @a cycle_draw's comment for when repainting happens
 */
static void s_cycle_draw_row(xcb_connection_t *connection, int i,
        int16_t pad_y, const struct s_cycle_row_style_s *style)
{
    int16_t row_y = (int16_t) (pad_y +
            (i - g_cycle_menu.scroll_offset) * WM_CYCLE_MENU_ROW_HEIGHT);
    client_td *row_client = (i >= 0 && i < g_cycle_menu.count)
        ? g_cycle_menu.clients[i] : NULL;
    int16_t text_x = style->pad_x;
    char label_buf[WM_CYCLE_MENU_ENTRY_LENGTH];

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
        struct position_s icon_pos;

        icon_pos.x = style->pad_x;
        icon_pos.y = row_y +
                (WM_CYCLE_MENU_ROW_HEIGHT - (int) style->icon_size) / 2;

        wmicon_draw_at(connection, xcb_ewmh_connection_get(),
                row_client->window, g_cycle_menu.window,
                icon_pos, style->icon_size,
                (i == g_cycle_menu.selected) ? style->fg_sel
                    : style->fg_nor,
                (i == g_cycle_menu.selected) ? style->bg_sel
                    : style->bg_nor,
                &row_client->icon_pixmap_cache);
        text_x = (int16_t) (style->pad_x + style->icon_offset);
    }

    /* Truncate against the menu's width rather than the label's
     * own measured width, so a title long enough to have already
     * capped 'menu_w' at 'WM_CYCLE_MENU_LABEL_MAX_WIDTH' when the
     * menu opened (see 'cycle_init' in menu/cycle.c) is cut to match
     * instead of running past the window's right edge. */
    (void) snprintf(label_buf, sizeof(label_buf), "%s", g_cycle_menu.labels[i]);
    if (g_cycle_menu.width > text_x + style->pad_x) {
        menu_draw_truncate(label_buf,
                (uint16_t) (g_cycle_menu.width - text_x - style->pad_x));
    }

    menu_draw_label(connection, g_cycle_menu.window,
            (struct position_s) { text_x,
                row_y + WM_CYCLE_MENU_ROW_HEIGHT - 4 },
            label_buf);
}


/**
 * @brief Resolve the on-screen rectangle a cycle-selection outline
 *        should surround for a given target
 *
 * For a decorated window target, @p client's tracked frame
 * geometry already reflects everything drawn on screen.  An
 * undecorated one is grown by its native border width, for the
 * reason given at that branch below.  For an icon target,
 * that geometry is not tracked anywhere on @p client itself, so it is
 * recomputed here the same way @c ccmd_client_ensure_icon_window
 * (cmds/client/icon.c) originally sized the icon window:
 * @c WM_ICON_SQUARE_SIZE alone when @p theme.icon.is-captioned is off,
 * plus @c WM_ICON_CAPTION_HEIGHT when it is on, since the icon
 * window's real height already includes room for that caption
 * text underneath the pixmap, not just the square icon area above
 * it.  Also grown by twice the icon's native border width, X11
 * drawing that border outside a window's core rectangle rather than
 * inside it: without this the outline would sit just inside the
 * icon's visible border rather than around the whole of it.  The
 * position is left alone, for the reason given at the window branch
 * below.
 *
 * @param client       Client currently selected
 * @param is_icon_menu Whether the cycle menu is showing icon previews
 *
 * @return The rectangle to outline, in root coordinates; an
 *         all-zero rectangle if @p client or @p client->config is
 *         null
 *
 * @note Complexity: @e O(1)
 */
static struct geometry_s s_mi_cycle_preview_outline_geom(
        const client_td *client, bool is_icon_menu)
{
    struct geometry_s geom = { { 0, 0 }, { 0u, 0u } };

    if (client == NULL) {
        return geom;
    }

    if (is_icon_menu) {
        uint16_t icon_h;
        uint32_t bw;

        if (client->config == NULL) {
            return geom;
        }

        icon_h = (uint16_t) (WM_ICON_SQUARE_SIZE +
                ((client->config->theme.icon.is_captioned)
                     ? WM_ICON_CAPTION_HEIGHT : 0u));
        bw = client->config->theme.icon.active.border.width;

        geom.pos.x = client->icon_pos.x;
        geom.pos.y = client->icon_pos.y;
        geom.dim.w = WM_ICON_SQUARE_SIZE + 2u * bw;
        geom.dim.h = (uint32_t) icon_h + 2u * bw;
        return geom;
    }

    /* A decorated client's 'layout.geometry.cur' is its frame
     * rectangle, which already covers everything drawn for it, so it
     * is outlined as it stands.
     *
     * An undecorated one has no frame at all.  The geometry is the
     * client window's core rectangle, and its border is an X11
     * native border, which the server draws entirely outside that
     * rectangle rather than inside it (see the render pass, which
     * has to compensate the position for exactly the
     * same reason whenever that width changes).  Outlining the core
     * rectangle alone therefore falls short by one border width on
     * every side.
     *
     * Only the size is adjusted, never the position.  A window's
     * x and y are already the upper-left corner of its outer
     * rectangle, border included, so the border grows a window
     * rightward and downward alone (X Consortium, 1994, "X Window
     * System Protocol", v11 R6, under CreateWindow).  Subtracting the
     * width from the position as well would start the outline one
     * border width above and to the left of the window. */
    if (!client_is_decorated(client) || client->frame == 0) {
        /* Asked with 'ignore_frame' set, since what is wanted is the
         * width actually drawn on this client's window, and with
         * the active style, which is what a client being cycled to is
         * wearing by the time the outline goes around it. */
        const uint32_t bw = client_border_width(client, true, true);

        geom = client->layout.geometry.cur;
        geom.dim.w += 2u * bw;
        geom.dim.h += 2u * bw;
        return geom;
    }

    return client->layout.geometry.cur;
}


/**
 * @brief Return the border width for a cycle-preview target
 *
 * Computes the border width to use for a preview target in the cycle
 * interface, selecting the icon border width for icon previews, no
 * border for decorated client frames, and the normal window border
 * width for undecorated window targets.
 *
 * @param client         Client the target belongs to
 * @param config         Active configuration
 * @param is_icon_menu   Whether the cycle menu is showing icon previews
 *
 * @return Border width to apply to the preview target
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_mi_cycle_preview_border_width(const client_td *client,
        const config_td *config, bool is_icon_menu)
{
    uint32_t border_width;

    if (config == NULL) {
        return 0u;
    }

    if (is_icon_menu) {
        border_width = config->theme.icon.active.border.width;
    } else if (client != NULL &&
            ((client_is_decorated(client) && client->frame != 0) ||
             client_is_fullscreen(client))) {
        /* No border for a decorated client's frame (it already
         * has its themed border painted elsewhere), and none for
         * a fullscreen client either: applying the normal window
         * border width here would paint a real, visible border over
         * fullscreen content (e.g., mpv, undecorated from the start),
         * the exact same reasoning 'ccmd_client_focus' in
         * 'cmds/client/focus.c' already applies for a plain focus
         * change. */
        border_width = 0u;
    } else {
        border_width = config->theme.window.active.border.width;
    }

    return border_width;
}


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
 * @note Prefers @p client->icon_window in icon menu mode when available
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


/* Apply preview border color and width to a target window */
void mi_cycle_preview_style_target(xcb_connection_t *connection,
        xcb_window_t target, const client_td *client,
        const config_td *config, bool is_icon_menu,
        uint32_t border_color)
{
    uint32_t border_width;

    if (connection == NULL || target == XCB_WINDOW_NONE ||
            config == NULL) {
        return;
    }

    border_width = s_mi_cycle_preview_border_width(client, config,
            is_icon_menu);
    xcb_window_set_border(target, border_width);

    if (!is_icon_menu &&
            client != NULL &&
            client_is_decorated(client) &&
            client->frame == target) {
        uint32_t frame_values[2] = { border_color, border_color };

        xcb_change_window_attributes(connection, target,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                frame_values);
        xcb_clear_area(connection, 0, target, 0, 0, 0, 0);
    } else {
        xcb_change_window_attributes(connection, target,
                XCB_CW_BORDER_PIXEL, &border_color);
    }
}


/* Apply the cycle preview highlight and stacking to the selection */
void mi_cycle_preview_apply(xcb_connection_t *connection,
        const config_td *config)
{
    client_td *selected;
    client_td *previous;
    xcb_window_t selected_target;
    xcb_window_t previous_target;
    uint32_t selected_border;

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
     * (e.g., re-called for an 'Expose' on the menu window itself, or
     * navigating with only one client in the cycle, which always
     * "changes" the index back to the same single entry).  Nothing
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
            uint32_t previous_border;
            bool prev_is_active =
                (g_cycle_menu.desktop->client_active_id == previous->id);

            if (g_cycle_menu.is_icon_menu) {
                previous_border =
                    config->theme.icon.inactive.border.color;
            } else if (prev_is_active) {
                previous_border =
                    config->theme.window.active.border.color;
            } else {
                previous_border =
                    config->theme.window.inactive.border.color;
            }

            mi_cycle_preview_style_target(connection, previous_target,
                    previous, config, g_cycle_menu.is_icon_menu,
                    previous_border);

            if (g_cycle_menu.is_icon_menu) {
                /* Full render (stacking below the tray, colors,
                 * pixmap, caption, and hint indicators all included)
                 * via the same shared function every other place a
                 * deselected icon needs repainting already uses (see
                 * 's_cycle_repaint_icon' in menu/cycle.c), rather
                 * than a separate, duplicated implementation of the
                 * same thing.  That duplication is exactly how this
                 * one and that other one drifted out of sync in the
                 * first place
                 * (this one never learned to omit the pixmap for a
                 * newly *selected* icon, below). */
                ri_render_client_icon(previous, true, true);
            } /* ! if (g_cycle_menu.is_icon_menu) */
        } /* ! if (previous_target) */
    }

    selected_border = (g_cycle_menu.is_icon_menu)
        ? config->theme.icon.active.border.color
        : config->theme.window.active.border.color;
    mi_cycle_preview_style_target(connection, selected_target,
            selected, config, g_cycle_menu.is_icon_menu,
            selected_border);

    if (g_cycle_menu.is_icon_menu) {
        /* Same "selected" render every other place a newly selected
         * icon needs it already uses (see 's_cycle_repaint_icon' in
         * menu/cycle.c): active colors, caption, and hint indicators
         * all included, deliberately just the pixmap left out,
         * rather than this function's separate, previously
         * duplicated implementation, which (unlike that shared one)
         * never learned to omit the pixmap here at all. */
        ri_render_client_icon(selected, true, true);
    }

    xcb_window_stack_below(selected_target, g_cycle_menu.window);

    /* The cycle-selection outline itself: a separate overlay (see
     * render/outline.h), never the target's native border width,
     * so switching selection never shifts the target by however many
     * pixels 'theme.cycle.border.width' happens to be.  Created once,
     * the first time a selection is applied after 'cycle_init', then
     * simply moved to each new selection's rectangle afterward;
     * 'cycle_destroy' is the one place these 4 windows are ever
     * destroyed.  Deliberately placed here, after 'selected_target'
     * has already been stacked below the menu just above: 'stack
     * below sibling' inserts immediately below that sibling, pushing
     * whatever was already immediately below it one step further
     * away, so whichever of these two calls runs last ends up on
     * top of the other.  Outlining a target only to have that same
     * target's stacking request immediately bury the outline
     * behind it again defeats the whole point of drawing one. */
    if (g_cycle_menu.outline_windows[0] == XCB_WINDOW_NONE) {
        render_outline_show(connection,
                g_cycle_menu.surface->screen->root,
                s_mi_cycle_preview_outline_geom(selected,
                    g_cycle_menu.is_icon_menu),
                config->theme.cycle.border.width,
                config->theme.cycle.border.color,
                g_cycle_menu.window,
                g_cycle_menu.outline_windows);
    } else {
        render_outline_move(connection,
                s_mi_cycle_preview_outline_geom(selected,
                    g_cycle_menu.is_icon_menu),
                config->theme.cycle.border.width,
                g_cycle_menu.window,
                g_cycle_menu.outline_windows);
    }

    g_cycle_menu.preview_client = selected;
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

    (void) text_renderer_use_font(connection,
            config->theme.menu.unselected.font);

    /* A viewport shift (scrolling) changes every row actually shown, so
     * it still needs the full loop below.  Otherwise selection moved
     * between two rows already on screen, and only those two actually
     * changed which color/text they show, for repainting the rest would
     * be identical to what is already there. */
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
        if (g_cycle_menu.last_drawn_selected >=
                        g_cycle_menu.scroll_offset
                && g_cycle_menu.last_drawn_selected <
                        g_cycle_menu.scroll_offset +
                    g_cycle_menu.viewport_rows) {
            s_cycle_draw_row(connection,
                    g_cycle_menu.last_drawn_selected, pad_y, &style);
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
     * meaningful as part of a full repaint.  Their content depends
     * solely on 'scroll_offset' and 'count', neither of which changes
     * on a same-viewport selection move.
     *
     * 'menu_draw_label' positions text by its baseline, and the
     * top/bottom padding strips are each only 'pad_y' pixels tall (the
     * default theme's 4px is smaller than most fonts' ascent), so
     * a naive baseline offset clips the glyph against whichever window
     * edge is closer: the top indicator's baseline sits at the font's
     * own ascent from Y=0 (see 'text_font_ascent'), keeping it below
     * the window's top edge; the bottom indicator's sits 'descent'
     * pixels above the window's bottom edge (see 'text_font_descent'),
     * keeping it above that edge instead. */
    if (need_full_repaint && g_cycle_menu.count >
                g_cycle_menu.viewport_rows) {
        int16_t top_baseline_y = text_font_ascent();

        /* Up arrow: entries exist above the viewport */
        if (g_cycle_menu.scroll_offset > 0) {
            menu_draw_row_bg(connection, g_cycle_menu.window,
                    style.bg_nor, 0, (uint16_t) pad_y,
                    g_cycle_menu.width);
            text_renderer_set_color(style.fg_sel, style.bg_nor);
            menu_draw_label(connection, g_cycle_menu.window,
                    (struct position_s) {
                        g_cycle_menu.width / 2 - 4, top_baseline_y },
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
                    (struct position_s) {
                        g_cycle_menu.width / 2 - 4,
                        bot_y + pad_y - text_font_descent() },
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
}
