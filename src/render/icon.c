/**
 * @file render/icon.c
 *
 * @brief Icon window rendering for iconified clients
 *
 * Implements @c ri_render_client_icon, which handles the visual
 * representation of iconified clients.  Extracted from
 * @c render/desktop.c to separate icon rendering from the broader
 * desktop rendering pipeline.
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

/* Menu includes */
#include <menu/cycle.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <render/text.h>

/* Local includes */
#include <render/internal.h>


/**
 * @brief Render the icon window for an iconified client
 *
 * Applies icon window attributes (background, border color and width,
 * stacking) and optionally draws a caption label.  Called from
 * @c desktop_render_clients for clients that are both hidden and
 * iconified.
 *
 * @param desktop    Desktop whose rendering context and theme are used
 * @param client     The iconified client to render
 * @param is_current @c true when @p desktop is the currently visible one
 *
 * @note No-op when @p client has no icon window or is not icon-mapped
 * @note Complexity: @e O(1)
 */
void ri_render_client_icon(desktop_td *desktop, client_td *client,
        bool is_current)
{
    bool is_cycle_sel;
    bool has_extra_icon_border;
    uint32_t border_width;

    if (desktop == NULL || client == NULL) {
        return;
    }

    if (!is_current || !client->is_icon_mapped ||
            client->icon_window == 0) {
        return;
    }

    is_cycle_sel = cycle_is_open() &&
        cycle_get_selected_client() == client;

    has_extra_icon_border =
        cycle_client_has_extra_border(client, true);
    xcb_change_window_attributes(desktop->connection,
            client->icon_window,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
        (is_cycle_sel)
            ? desktop->config_theme->icon.active.color.background
            : desktop->config_theme->icon.inactive.color.background,
        (is_cycle_sel)
            ? desktop->config_theme->icon.active.border.color
            : desktop->config_theme->icon.inactive.border.color
            });

    border_width = (is_cycle_sel)
        ? client->theme->icon.active.border.width
        : client->theme->icon.inactive.border.width;
    if (has_extra_icon_border) {
        border_width += WM_ICON_CYCLE_SEL_BORDER_EXTRA;
    }
    xcb_configure_window(desktop->connection,
            client->icon_window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH,
            &border_width);

    xcb_clear_area(desktop->connection, 0,
            client->icon_window, 0, 0, 0, 0);
    xcb_map_window(desktop->connection, client->icon_window);
    xcb_configure_window(desktop->connection,
            client->icon_window,
            XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { XCB_STACK_MODE_BELOW });

    if (desktop->config_theme->icon.is_captioned &&
            client->info.name != NULL) {
        const char *caption =
            (client->icon_info.visible_icon_name != NULL &&
             client->icon_info.visible_icon_name[0] != '\0')
                ? client->icon_info.visible_icon_name
                : client->info.name;

        text_renderer_init(desktop->connection,
                desktop->config_theme->icon.inactive.font);
        text_renderer_set_color(
        (is_cycle_sel)
            ? desktop->config_theme->icon.active.color.foreground
            : desktop->config_theme->icon.inactive.color.foreground,
        (is_cycle_sel)
            ? desktop->config_theme->icon.active.color.background
            : desktop->config_theme->icon.inactive.color.background);

        text_draw_string(desktop->connection,
                client->icon_window, XCB_NONE,
                2,
                (int16_t) (WM_ICON_SQUARE_SIZE +
                    WM_ICON_CAPTION_HEIGHT - 2u),
                caption);
    }
}
