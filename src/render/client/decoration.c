/**
 * @file render/client/decoration.c
 *
 * @brief Client frame decoration (border) rendering
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

/* Project includes */
#include <client.h>
#include <config.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Local includes */
#include <render/client/decoration.h>


/* Repaint the frame background, border and corner resize grips */
void render_client_decoration_repaint_frame(xcb_connection_t *connection,
        client_td *client, bool use_active_style,
        const struct config_theme_s *theme)
{
    uint8_t opacity_percent;
    uint32_t frame_bg;
    bool bg_changed;

    if (connection == NULL || client == NULL || client->frame == 0 ||
            theme == NULL || !client_is_decorated(client)) {
        return;
    }

    frame_bg = (use_active_style)
        ? theme->window.active.border.color
        : theme->window.inactive.border.color;
    bg_changed = !client->layout.has_frame_bg ||
        client->layout.frame_bg != frame_bg;
    client->layout.frame_bg = frame_bg;
    client->layout.has_frame_bg = true;

    if (bg_changed) {
        xcb_change_window_attributes(connection, client->frame,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) { frame_bg, frame_bg });
    }

    if (use_active_style) {
        opacity_percent = (client->opacity_override.is_set_active)
            ? client->opacity_override.active
            : theme->window.active.opacity;
    } else {
        opacity_percent = (client->opacity_override.is_set_inactive)
            ? client->opacity_override.inactive
            : theme->window.inactive.opacity;
    }
    atom_set_window_opacity(connection, client->frame,
            config_theme_opacity_to_raw(opacity_percent));

    /* Only when the color just set is not the one already showing.
     * The frame is the content window's parent, so clearing it paints
     * over the content's own area until the client draws itself
     * again; doing that to show a color identical to the one already
     * there blanks the window for nothing, and this repaint runs for
     * any reason at all, not only a focus change. */
    if (bg_changed) {
        xcb_clear_area(connection, 0, client->frame, 0, 0, 0, 0);
    }
}


/* Repaint a client's frame decoration, unless it is currently forced
 * hidden */
void render_client_decoration_repaint_frame_unless_hidden(
        xcb_connection_t *connection, client_td *client,
        bool is_focused, bool hide_decoration,
        const struct config_theme_s *theme)
{
    if (!hide_decoration) {
        render_client_decoration_repaint_frame(connection, client,
                is_focused, theme);
    }
}
