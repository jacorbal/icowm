/**
 * @file render/desktop.c
 *
 * @brief Desktop rendering implementation
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
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */
#include <adt/ohtbl.h>  /* Hash table for clients */

/* Project includes */
#include <client.h>
#include <logger.h>
#include <render/text.h>

/* Local includes */
#include <render/desktop.h>


/** Size of square icon window, in pixels */
#define WM_ICON_SQUARE_SIZE         (48u)

/** Height of caption area below the icon square, in pixels */
#define WM_ICON_CAPTION_HEIGHT      (14u)

/** Size of a single decoration button square, in pixels */
#define WM_DECOR_BTN_SIZE           (12u)

/** Gap between adjacent decoration buttons, in pixels */
#define WM_DECOR_BTN_GAP            (2u)

/** Horizontal padding from the frame edge to the outermost button */
#define WM_DECOR_BTN_PAD            (4u)

/** Button fill color when active (focused window, normal state) */
#define WM_DECOR_COLOR_ACTIVE       (0x000000u)

/** Button fill color when inactive (unfocused window) */
#define WM_DECOR_COLOR_INACTIVE     (0xFFFFFFu)

/** Button fill color when disabled (action not available) */
#define WM_DECOR_COLOR_DISABLED     (0x808080u)


/* Draw the background of a desktop */
int desktop_render_background(desktop_td *desktop)
{
    xcb_screen_t *screen;
    xcb_screen_iterator_t iter;
    uint32_t values[2];

    if (desktop == NULL) {
        LOGGER_ERROR("Received 'NULL' desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_TRACE("Rendering background for desktop %u ('%s')" \
            " with color #%06x",
            desktop->id, desktop->name, desktop->background.bg.color);

    /* Get the screen */
    /* TODO/FIXME: The 'screen_id' is taken by iterating all screens
     *             with the current 'desktop->screen_id', and this
     *             should be by direct access.  Maybe passing a pointer
     *             to the screen instead getting the id on
     *             'desktop_init' (vid. 'src/desktop.c').
     *
     *  But, in general terms, the number of screens in any setup tends
     *  to be low, so this loop has a complexity of O(n), where 'n' is
     *  the number of screens, in most cases, 'n' approaches 1 or
     *  a small constant.
     */
    iter = xcb_setup_roots_iterator(xcb_get_setup(desktop->connection));
    screen = NULL;
    for (uint32_t i = 0; i < desktop->screen_id && iter.rem > 0; ++i) {
        xcb_screen_next(&iter);
    }

    if (iter.rem == 0 || iter.data == NULL) {
        LOGGER_ERROR("Could not get screen for background rendering",
                L_NARG);
        return 1;
    }

    screen = iter.data;

    /* NOTE: Clear any existing background pixmap, then set the
     *       background pixel and repaint the root window.  Values are
     *       ordered by ascending bit position: 'XCB_CW_BACK_PIXMAP'
     *       (bit 0) comes before 'XCB_CW_BACK_PIXEL' (bit 1).
     *       Unsetting the background pixmap ensures that xcb_clear_area
     *       fills with the pixel color rather than the previous
     *       pixmap. */
    values[0] = XCB_BACK_PIXMAP_NONE;
    values[1] = desktop->background.bg.color;
    xcb_change_window_attributes(desktop->connection, screen->root,
            XCB_CW_BACK_PIXMAP | XCB_CW_BACK_PIXEL, values);
    xcb_clear_area(desktop->connection, 0, screen->root, 0, 0,
            screen->width_in_pixels, screen->height_in_pixels);

    LOGGER_TRACE("Background rendered for desktop %u ('%s')",
            desktop->id, desktop->name);

    return 0;
}


/**
 * @brief Draw the decoration button squares on a titlebar window
 *
 * Renders six right-aligned button squares (Iconify, Hide, Shade,
 * Maximize, Fullscreen, Close) and one left-aligned button (Pin/Sticky)
 * using filled rectangles.  The fill color for the right-aligned
 * buttons and the pin button is taken from the @p theme:
 * @c window.active.foreground_color when focused, and
 * @c window.inactive.foreground_color when unfocused.
 *
 * @param connection  XCB connection
 * @param titlebar    XCB window id of the titlebar
 * @param frame_w     Total width of the titlebar in pixels
 * @param frame_top   Total height of the titlebar in pixels
 * @param is_focused  Whether the owning client is focused
 * @param is_sticky   Whether the owning client is sticky (pin active)
 * @param theme       Pointer to the theme providing button colors
 *
 * @note Complexity: @e O(1)
 */
void desktop_draw_titlebar_buttons(xcb_connection_t *connection,
        xcb_window_t titlebar, uint16_t frame_w, uint16_t frame_top,
        bool is_focused, bool is_sticky,
        const struct config_theme_s *theme)
{
    xcb_gcontext_t gc;
    uint32_t color;
    xcb_rectangle_t rect;
    uint16_t btn = (uint16_t) WM_DECOR_BTN_SIZE;
    uint16_t gap = (uint16_t) WM_DECOR_BTN_GAP;
    uint16_t pad = (uint16_t) WM_DECOR_BTN_PAD;
    int16_t btn_y;
    int16_t x;
    uint32_t fill;
    uint16_t step;
    int16_t right_edge;

    /* Button colors come from the theme: foreground contrasts against
     * the titlebar background so buttons are always visible */
    uint32_t color_active = (theme != NULL)
        ? theme->window.active.foreground_color
        : 0x000000u;
    uint32_t color_inactive = (theme != NULL)
        ? theme->window.inactive.foreground_color
        : 0xFFFFFFu;

    /* Vertically center buttons in the titlebar */
    btn_y = (frame_top > btn)
        ? (int16_t) ((frame_top - btn) / 2u)
        : 0;

    gc = xcb_generate_id(connection);
    /* Left-aligned: Pin button */
    color = (is_sticky) ? color_active: color_inactive;
    xcb_create_gc(connection, gc, titlebar,
            XCB_GC_FOREGROUND, &color);
    rect = (xcb_rectangle_t) { (int16_t) pad, btn_y, btn, btn };
    xcb_poly_fill_rectangle(connection, titlebar, gc, 1, &rect);
    xcb_free_gc(connection, gc);

    /* Right-aligned:
     * Iconify, Hide, Shade, Maximize, Fullscreen, Close (RTL alloc.) */
    /* Six buttons; compute starting 'x' from right edge */
    step = (uint16_t) (btn + gap);
    right_edge = (int16_t) (frame_w - pad);

    /* Button fill: black for focused window (active state),
     * white for unfocused window (inactive state). */
    fill = (is_focused) ? color_active : color_inactive;

    for (int bi = 0; bi < 6; ++bi) {
        x = (int16_t) (right_edge - (int16_t) btn -
                (int16_t) ((uint16_t) bi * step));
        color = fill;
        gc = xcb_generate_id(connection);

        xcb_create_gc(connection, gc, titlebar,
                XCB_GC_FOREGROUND, &color);
        rect = (xcb_rectangle_t) { x, btn_y, btn, btn };
        xcb_poly_fill_rectangle(connection, titlebar, gc, 1, &rect);
        xcb_free_gc(connection, gc);
    }
}


/* Draw all clients on a desktop */
int desktop_render_clients(desktop_td *desktop, bool is_current)
{
    cdlist_item_td *stacking_node;
    cdlist_item_td *stacking_initial;
    client_td *client;
    int client_count = 0;
    size_t stacking_size;
    uint16_t mask;
    int32_t values[4];
    xcb_window_t target;
    bool is_focused;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
    uint16_t title_h;
    uint16_t inner_w;
    uint16_t inner_h;

    if (desktop == NULL) {
        LOGGER_ERROR("Received 'NULL' desktop pointer", L_NARG);
        return 1;
    }

    if (desktop->stacking == NULL) {
        LOGGER_ERROR("Desktop stacking list is NULL!", L_NARG);
        return 1;
    }

    stacking_size = cdlist_size(desktop->stacking);
    LOGGER_DEBUG("Rendering %zu client(s) from stacking list" \
            " on desktop %u ('%s')",
            stacking_size, desktop->id, desktop->name);

    /* If no clients, return early */
    if (stacking_size == 0) {
        LOGGER_TRACE("No clients to render on desktop %u ('%s')",
                desktop->id, desktop->name);
        return 0;
    }

    stacking_node = cdlist_head(desktop->stacking);
    if (stacking_node == NULL) {
        LOGGER_ERROR("Stacking list head is 'NULL' despite size > 0",
                L_NARG);
        return 1;
    }

    stacking_initial = stacking_node;

    /* Iterate through stacking list (back to front) */
    do {
        client = (client_td *) cdlist_data(stacking_node);

        if (client == NULL) {
            LOGGER_ERROR("NULL client found in stacking list at" \
                    " position %d", client_count);
            stacking_node = cdlist_next(stacking_node);
            continue;
        }
        client_count++;

        /* Keep icon windows visible for iconified clients */
        if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
            if (is_current && client->is_icon_mapped &&
                    client->icon_window != 0) {
                xcb_change_window_attributes(desktop->connection,
                        client->icon_window,
                        XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                        (const uint32_t[]) {
                            desktop->config_theme->icon.background_color,
                            desktop->config_theme->icon.border_color
                        });
                xcb_clear_area(desktop->connection, 0,
                        client->icon_window, 0, 0, 0, 0);
                xcb_map_window(desktop->connection, client->icon_window);

                if (desktop->config_theme->icon.is_captioned &&
                        client->info.name != NULL) {
                    text_renderer_init(desktop->connection,
                            desktop->config_theme->icon.font);
                    text_renderer_set_color(
                            desktop->config_theme->icon.foreground_color,
                            desktop->config_theme->icon.background_color);
                    text_draw_string(desktop->connection,
                            client->icon_window, XCB_NONE,
                            2,
                            (int16_t) (WM_ICON_SQUARE_SIZE +
                                WM_ICON_CAPTION_HEIGHT - 2u),
                            client->info.name);
                }
            }
            stacking_node = cdlist_next(stacking_node);
            continue;
        }

        is_focused = (desktop->client_active_id == client->id);
        target = (client_is_decorated(client) && client->frame != 0)
            ? client->frame
            : client->window;

        /* Map the window to make it visible */
        /* NOTE: Only do this when 'desktop' is the surface's currently
         *       displayed desktop.  This function is also invoked as
         *       part of a general 'surface_render_all_desktops()'
         *       refresh pass whenever ANY desktop's 'is_outdated' flag
         *       is set (e.g., after moving or resizing a client, which
         *       marks its own desktop outdated).  If that pass
         *       unconditionally mapped clients on a desktop that is not
         *       currently shown, it could race with (and undo) an
         *       explicit 'surface_clients_hide()' issued by a desktop
         *       switch, making a client reappear on top of the desktop
         *       the user just switched to.  Visibility of non-current
         *       desktops must be governed solely by
         *       'surface_clients_hide()'/'surface_clients_show()'. */
        if (is_current) {
            if (client->icon_window != 0 && client->is_icon_mapped) {
                xcb_unmap_window(desktop->connection, client->icon_window);
                client->is_icon_mapped = false;
            }
            if (client->titlebar != 0) {
                xcb_map_window(desktop->connection, client->titlebar);
            }
            xcb_map_window(desktop->connection, target);
            if (target != client->window) {
                xcb_map_window(desktop->connection, client->window);
            }
        }

        /* Configure position and size */
        mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
               XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[0] = client->layout.geometry.cur.pos.x;
        values[1] = client->layout.geometry.cur.pos.y;
        values[2] = (int32_t) client->layout.geometry.cur.dim.w;
        values[3] = (int32_t) client->layout.geometry.cur.dim.h;

        xcb_configure_window(desktop->connection, target, mask,
                (uint32_t *) values);

        if (target != client->window) {
            left = (uint16_t) client->layout.frame_extents.left;
            right = (uint16_t) client->layout.frame_extents.right;
            top = (uint16_t) client->layout.frame_extents.top;
            bottom = (uint16_t) client->layout.frame_extents.bottom;
            title_h = client->title_height;
            inner_w = (client->layout.geometry.cur.dim.w > left + right)
                ? (uint16_t)
                    (client->layout.geometry.cur.dim.w - left - right)
                : 1;
            inner_h = (client->layout.geometry.cur.dim.h > top + bottom)
                ? (uint16_t)
                    (client->layout.geometry.cur.dim.h - top - bottom)
                : 1;

            xcb_configure_window(desktop->connection, client->window,
                    XCB_CONFIG_WINDOW_X     |
                    XCB_CONFIG_WINDOW_Y     |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT,
                    (const uint32_t[]) {
                        left, top, inner_w, inner_h
                    });

            xcb_change_window_attributes(desktop->connection, client->frame,
                    XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                    (const uint32_t[]) {
                (is_focused)
                    ? desktop->config_theme->window.active.border_color
                    : desktop->config_theme->window.inactive.border_color,
                (is_focused)
                    ? desktop->config_theme->window.active.border_color
                    : desktop->config_theme->window.inactive.border_color
                    });
            xcb_clear_area(desktop->connection, 0, client->frame,
                    0, 0, 0, 0);

            if (client->titlebar != 0) {
                xcb_configure_window(desktop->connection, client->titlebar,
                        XCB_CONFIG_WINDOW_X     |
                        XCB_CONFIG_WINDOW_Y     |
                        XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT,
                        (const uint32_t[]) {
                            left,
                            (top > title_h) ? top - title_h : 0,
                            inner_w, title_h
                        });

                xcb_change_window_attributes(desktop->connection,
                        client->titlebar,
                        XCB_CW_BACK_PIXEL,
                        (const uint32_t[]) {
                    (is_focused)
                    ? desktop->config_theme->window.active.background_color
                    : desktop->config_theme->window.inactive.background_color
                        });

                xcb_clear_area(desktop->connection, 0,
                        client->titlebar, 0, 0, 0, 0);
                text_renderer_init(desktop->connection,
                        (is_focused)
                            ? desktop->config_theme->window.active.font
                            : desktop->config_theme->window.inactive.font);

                /* Use theme foreground color so text contrasts against
                 * the titlebar background (active or inactive) */
                text_renderer_set_color(
                        (is_focused)
                ? desktop->config_theme->window.active.foreground_color
                : desktop->config_theme->window.inactive.foreground_color,
                        (is_focused)
                ? desktop->config_theme->window.active.background_color
                : desktop->config_theme->window.inactive.background_color);

                text_draw_string(desktop->connection,
                        client->titlebar, XCB_NONE,
                        (int16_t) (WM_DECOR_BTN_PAD + WM_DECOR_BTN_SIZE +
                            WM_DECOR_BTN_PAD),
                        (int16_t) ((top > WM_TITLEBAR_TEXT_BOTTOM_PAD)
                            ? top - WM_TITLEBAR_TEXT_BOTTOM_PAD
                            : top),
                        client->info.name);

                desktop_draw_titlebar_buttons(desktop->connection,
                        client->titlebar,
                        inner_w,
                        title_h,
                        is_focused,
                        (bool) client_is_sticky(client),
                        desktop->config_theme);
            }
        }

        LOGGER_TRACE("Rendered client 0x%08x with" \
                     " geometry (%ux%u%+u%+u)",
                client->id,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h,
                client->layout.geometry.cur.pos.x,
                client->layout.geometry.cur.pos.y);

        stacking_node = cdlist_next(stacking_node);
    } while (stacking_node != NULL &&
             stacking_node != stacking_initial &&
             client_count < (int)stacking_size);

    LOGGER_DEBUG("Successfully rendered %d clients" \
            " on desktop %u ('%s')",
            client_count, desktop->id, desktop->name);

    return 0;
}


/* Full desktop render */
int desktop_render_full(desktop_td *desktop, bool is_current)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Received 'NULL' desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_TRACE("Fully rendering desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Draw background */
    if (desktop_render_background(desktop) != 0) {
        LOGGER_ERROR("Failed to render background on" \
                 " desktop %u ('%s')", desktop->id, desktop->name);
        return 1;
    }

    /* Draw all clients */
    if (desktop_render_clients(desktop, is_current) != 0) {
        LOGGER_ERROR("Failed to render clients", L_NARG);
        return 1;
    }

    /* Mark desktop as up-to-date */
    desktop->is_outdated = false;

    /* NOTE: Do NOT flush here!  Let the surface handle the flushing */

    return 0;
}


/* Flush drawing operations */
void desktop_render_flush(desktop_td *desktop)
{
    if (desktop == NULL || desktop->connection == NULL) {
        LOGGER_ERROR("Invalid desktop or connection for flushing",
                L_NARG);
        return;
    }

    xcb_flush(desktop->connection);
}
