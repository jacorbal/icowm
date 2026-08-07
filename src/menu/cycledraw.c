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
#include <render/text.h>

/* Default initial values */
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
 * width for undecorated window targets. When the target is highlighted,
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
 *       selection) is invalid or incomplete
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
    if (previous != NULL && previous != selected) {
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
                xcb_configure_window(connection, previous_target,
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) { XCB_STACK_MODE_BELOW });

                values[0] = config->theme.icon.inactive.color.background;
                values[1] = config->theme.icon.inactive.border.color;

                xcb_change_window_attributes(connection, previous_target,
                        XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL, values);
                xcb_clear_area(connection, 0, previous_target, 0, 0, 0, 0);
                if (config->theme.icon.is_captioned &&
                        previous->info.name != NULL) {
                    const char *caption =
                        (previous->icon_info.visible_icon_name != NULL &&
                         previous->icon_info.visible_icon_name[0] != '\0')
                            ? previous->icon_info.visible_icon_name
                            : previous->info.name;

                    text_renderer_init(connection,
                            config->theme.icon.inactive.font);
                    text_renderer_set_color(
                            config->theme.icon.inactive.color.foreground,
                            config->theme.icon.inactive.color.background);
                    text_draw_string(connection, previous_target, XCB_NONE,
                            2,
                            (int16_t) (WM_ICON_SQUARE_SIZE +
                                WM_ICON_CAPTION_HEIGHT - 2u),
                            caption);
                }
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
        values[0] = config->theme.icon.active.color.background;
        values[1] = config->theme.icon.active.border.color;

        xcb_change_window_attributes(connection, selected_target,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL, values);
        xcb_clear_area(connection, 0, selected_target, 0, 0, 0, 0);
        if (config->theme.icon.is_captioned &&
                selected->info.name != NULL) {
            const char *caption =
                (selected->icon_info.visible_icon_name != NULL &&
                 selected->icon_info.visible_icon_name[0] != '\0')
                    ? selected->icon_info.visible_icon_name
                    : selected->info.name;

            text_renderer_init(connection,
                    config->theme.icon.active.font);
            text_renderer_set_color(
                    config->theme.icon.active.color.foreground,
                    config->theme.icon.active.color.background);
            text_draw_string(connection, selected_target, XCB_NONE,
                    2,
                    (int16_t) (WM_ICON_SQUARE_SIZE +
                        WM_ICON_CAPTION_HEIGHT - 2u),
                    caption);
        }
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


/* Repaint all menu entries */
void cycle_draw(xcb_connection_t *connection, const config_td *config)
{
    uint32_t fg_sel;
    uint32_t bg_sel;
    uint32_t fg_nor;
    uint32_t bg_nor;

    if (connection == NULL || config == NULL ||
            g_cycle_menu.window == XCB_WINDOW_NONE) {
        return;
    }

    fg_sel = config->theme.menu.selected.color.foreground;
    bg_sel = config->theme.menu.selected.color.background;
    fg_nor = config->theme.menu.unselected.color.foreground;
    bg_nor = config->theme.menu.unselected.color.background;

    text_renderer_init(connection, config->theme.menu.unselected.font);

    for (int i = g_cycle_menu.scroll_offset;
            i < g_cycle_menu.scroll_offset + g_cycle_menu.viewport_rows;
            ++i) {
        int16_t row_y = (int16_t) (WM_CYCLE_MENU_PAD_Y +
                (i - g_cycle_menu.scroll_offset) *
                WM_CYCLE_MENU_ROW_HEIGHT);

        if (i == g_cycle_menu.selected) {
            menu_draw_row_bg(connection, g_cycle_menu.window, bg_sel,
                    row_y, (uint16_t) WM_CYCLE_MENU_ROW_HEIGHT,
                    g_cycle_menu.width);
            text_renderer_set_color(fg_sel, bg_sel);
        } else {
            menu_draw_row_bg(connection, g_cycle_menu.window, bg_nor,
                    row_y, (uint16_t) WM_CYCLE_MENU_ROW_HEIGHT,
                    g_cycle_menu.width);
            text_renderer_set_color(fg_nor, bg_nor);
        }

        menu_draw_label(connection, g_cycle_menu.window,
                (int16_t) WM_CYCLE_MENU_PAD_X,
                (int16_t) (row_y + WM_CYCLE_MENU_ROW_HEIGHT - 4),
                g_cycle_menu.labels[i]);
    }

    /* Draw scroll-indicator arrows in the top/bottom padding areas when
     * there are hidden entries above or below the viewport */
    if (g_cycle_menu.count > g_cycle_menu.viewport_rows) {
        /* Up arrow: entries exist above the viewport */
        if (g_cycle_menu.scroll_offset > 0) {
            menu_draw_row_bg(connection, g_cycle_menu.window, bg_nor,
                    0, (int16_t) WM_CYCLE_MENU_PAD_Y,
                    g_cycle_menu.width);
            text_renderer_set_color(fg_sel, bg_nor);
            menu_draw_label(connection, g_cycle_menu.window,
                    (int16_t) (g_cycle_menu.width / 2u - 4u),
                    (int16_t) (WM_CYCLE_MENU_PAD_Y - 2),
                    "---");
        } else {
            /* Clear the top padding area when no arrow is needed */
            menu_draw_row_bg(connection, g_cycle_menu.window, bg_nor,
                    0, (int16_t) WM_CYCLE_MENU_PAD_Y,
                    g_cycle_menu.width);
        }

        /* Down arrow: entries exist below the viewport */
        if (g_cycle_menu.scroll_offset + g_cycle_menu.viewport_rows <
                g_cycle_menu.count) {
            int16_t bot_y = (int16_t) (WM_CYCLE_MENU_PAD_Y +
                    g_cycle_menu.viewport_rows *
                    WM_CYCLE_MENU_ROW_HEIGHT);
            menu_draw_row_bg(connection, g_cycle_menu.window, bg_nor,
                    bot_y, (int16_t) WM_CYCLE_MENU_PAD_Y,
                    g_cycle_menu.width);
            text_renderer_set_color(fg_sel, bg_nor);
            menu_draw_label(connection, g_cycle_menu.window,
                    (int16_t) (g_cycle_menu.width / 2u - 4u),
                    (int16_t) (bot_y + WM_CYCLE_MENU_PAD_Y - 2),
                    "---");
        } else {
            /* Clear the bottom padding area when no arrow is needed */
            int16_t bot_y = (int16_t) (WM_CYCLE_MENU_PAD_Y +
                    g_cycle_menu.viewport_rows *
                    WM_CYCLE_MENU_ROW_HEIGHT);
            menu_draw_row_bg(connection, g_cycle_menu.window, bg_nor,
                    bot_y, (int16_t) WM_CYCLE_MENU_PAD_Y,
                    g_cycle_menu.width);
        }
    }

    mi_cycle_preview_apply(connection, config);
    xcb_flush(connection);
}
