/**
 * @file render/desktop.c
 *
 * @brief Desktop rendering implementation
 *
 * @note Decoration and icon constants are defined in @c defs/wm.h,
 *       pulled in via @c (render/desktop.h -> desktop.h -> defs/wm.h);
 *       button colors come from the theme passed to
 *       @c desktop_draw_titlebar_buttons
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */
#include <adt/ohtbl.h>  /* Hash table for clients */

/* Menu includes */
#include <menu/cycle.h>

/* Project includes */
#include <client.h>
#include <logger.h>
#include <render/text.h>

/* Local includes */
#include <render/desktop.h>
#include <render/internal.h>


/**
 * @brief Intern a custom atom and return @c XCB_ATOM_NONE on failure
 *
 * Performs an @c XCBInternAtom request for the given atom name and
 * returns the resulting atom identifier, or @c XCB_ATOM_NONE if the
 * request fails or the connection/name is invalid.
 *
 * @param connection XCB connection used to intern the atom
 * @param name       Atom name string to intern
 *
 * @return Interned atom identifier, or @c XCB_ATOM_NONE on failure
 *
 * @note Complexity: @e O(1)
 */
static xcb_atom_t s_intern_atom(xcb_connection_t *connection,
        const char *name)
{
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom = XCB_ATOM_NONE;

    if (connection == NULL || name == NULL) {
        return XCB_ATOM_NONE;
    }

    reply = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, 1,
                (uint16_t) strlen(name), name), NULL);

    if (reply != NULL) {
        atom = reply->atom;
        free(reply);
    }

    return atom;
}


/**
 * @brief Retrieve the root window background pixmap if set
 *
 * Queries the root window for one of the standard background pixmap
 * properties (@c _XROOTPMAP_ID, @c ESETROOT_PMAP_ID, @c _XSETROOT_ID)
 * and returns the first non-@c XCB_NONE pixmap found, or @c XCB_NONE if
 * no valid pixmap is present.
 *
 * @param connection XCB connection to the X server
 * @param root       Root window to query for background pixmap
 *
 * @return Root background pixmap, or @c XCB_NONE if not available
 *
 * @note Complexity: @e O(n) in the number of candidate properties
 */
static xcb_pixmap_t
    s_get_root_background_pixmap(xcb_connection_t *connection,
            xcb_window_t root)
{
    /* 'ESETROOT_PMAP_ID': legacy 'Esetroot' alias, also used by 'feh'.
     * '_XROOTPMAP_ID': set by 'Esetroot', 'feh', 'nitrogen', 'hsetroot', &c.
     * '_XSETROOT_ID': set by 'xsetroot' and 'xsetbg'. */
    const char *prop_names[] = {
        "_XROOTPMAP_ID", "ESETROOT_PMAP_ID", "_XSETROOT_ID"
    };
    const size_t prop_count = sizeof(prop_names) / sizeof(prop_names[0]);

    if (connection == NULL || root == XCB_WINDOW_NONE) {
        return XCB_NONE;
    }

    for (size_t i = 0; i < prop_count; ++i) {
        xcb_atom_t prop = s_intern_atom(connection, prop_names[i]);
        xcb_get_property_reply_t *reply;
        xcb_pixmap_t pixmap = XCB_NONE;
        if (prop == XCB_ATOM_NONE) {
            continue;
        }

        reply = xcb_get_property_reply(connection,
                xcb_get_property(connection, 0, root, prop,
                    XCB_ATOM_PIXMAP, 0, 1), NULL);
        if (reply == NULL) {
            continue;
        }

        if (reply->format == 32 && reply->value_len >= 1 &&
                xcb_get_property_value(reply) != NULL) {
            pixmap = *((xcb_pixmap_t *) xcb_get_property_value(reply));
        }
        free(reply);

        if (pixmap != XCB_NONE) {
            return pixmap;
        }
    }

    return XCB_NONE;
}


/* Draw the background of a desktop */
int desktop_render_background(desktop_td *desktop)
{
    xcb_screen_t *screen;
    xcb_screen_iterator_t iter;
    uint32_t values[2];
    xcb_pixmap_t root_pixmap;

    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_TRACE("Rendering background for desktop %u ('%s')" \
            " with color #%06x",
            desktop->id, desktop->name, desktop->background.bg.color);

    /* Get the screen */
    /* The 'screen_id' is taken by iterating all screens with the
     * current 'desktop->screen_id', and this should be by direct
     * access.  Maybe passing a pointer to the screen instead getting
     * the id on 'desktop_init' (vid. 'src/desktop.c').
     *
     * But, in general terms, the number of screens in any setup tends
     * to be low, so this loop has a complexity of O(n), where 'n' is
     * the number of screens, in most cases, 'n' approaches 1 or a small
     * constant. */
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

    root_pixmap = s_get_root_background_pixmap(desktop->connection,
            screen->root);
    if (root_pixmap != XCB_NONE) {
        /* An external tool ('xsetbg', 'feh', 'xsetroot', 'nitrogen',
         * painted the root window and recorded the pixmap ID in
         * a well-known atom.  Record that fact so that subsequent
         * repaints do not overwrite the wallpaper with our color.
         */
        /* NOTE: Deliberately do NOT set `XCB_CW_BACK_PIXMAP` on the
         *       root window to this pixmap.  Many setters free the
         *       pixmap after drawing (the pixels persist in the root
         *       drawable), so referencing it via `XCB_CW_BACK_PIXMAP`
         *       would cause X to use a freed resource on the next
         *       `xcb_clear_area`, and leading to a `BadPixmap` or
         *       `BadDrawable` X error and an abrupt crash. */
        desktop->background.use_root_pixmap = true;
        LOGGER_TRACE("External root pixmap 0x%x detected for" \
                " desktop %u ('%s'); skipping color fill",
                root_pixmap, desktop->id, desktop->name);
        return 0;
    }

    if (desktop->background.use_root_pixmap) {
        /* No pixmap atom found this time, but an external tool
         * previously painted the root window.  The pixels are still
         * there; leave the root window untouched so the wallpaper
         * remains visible. */
        LOGGER_TRACE("Preserving previous external background for" \
                " desktop %u ('%s')", desktop->id, desktop->name);
        return 0;
    }

    /* No external background detected and the window manager owns the
     * background: apply the configured color and clear the root window
     * to make it visible */
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


/* Draw the decoration button squares on a titlebar window */
void desktop_draw_titlebar_buttons(xcb_connection_t *connection,
        xcb_window_t titlebar, uint16_t frame_w, uint16_t frame_top,
        bool is_focused, bool is_sticky, bool is_layered,
        bool can_maximize, const struct config_theme_s *theme)
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
    uint32_t bg_fill;
    uint16_t step;
    int16_t right_edge;

    /* Button colors come from the theme: foreground contrasts against
     * the titlebar background so buttons are always visible. */
    uint32_t color_active = (theme != NULL)
        ? theme->window.active.foreground_color
        : 0x000000u;
    uint32_t color_inactive = (theme != NULL)
        ? theme->window.inactive.foreground_color
        : 0xFFFFFFu;

    /* Vertically center buttons in the titlebar */
    btn_y = (frame_top > btn) ? (int16_t) ((frame_top - btn) / 2u) : 0;

    step = (uint16_t) (btn + gap);
    gc = xcb_generate_id(connection);

    /* Left-aligned: pin/sticky */
    color = (is_sticky) ? color_active : color_inactive;
    xcb_create_gc(connection, gc, titlebar,
            XCB_GC_FOREGROUND, &color);
    rect = (xcb_rectangle_t) { (int16_t) pad, btn_y, btn, btn };
    xcb_poly_fill_rectangle(connection, titlebar, gc, 1, &rect);
    xcb_free_gc(connection, gc);

    /* Left-aligned button 1: Layer cycle (to the right of pin) */
    gc = xcb_generate_id(connection);
    color = (is_layered) ? color_active : color_inactive;
    xcb_create_gc(connection, gc, titlebar,
            XCB_GC_FOREGROUND, &color);
    x = (int16_t) ((int16_t) pad + (int16_t) step);
    rect = (xcb_rectangle_t) { x, btn_y, btn, btn };
    xcb_poly_fill_rectangle(connection, titlebar, gc, 1, &rect);
    xcb_free_gc(connection, gc);

    /* Right-aligned:
     * Iconify, Hide, Shade, Maximize, Fullscreen, Close (RTL alloc.) */
    /* Six buttons; compute starting 'x' from right edge */
    step = (uint16_t) (btn + gap);
    right_edge = (int16_t) (frame_w - pad);

    /* Button fill: active foreground for focused, inactive for
     * unfocused */
    fill = (is_focused) ? color_active : color_inactive;
    bg_fill = (theme != NULL)
        ? ((is_focused)
            ? theme->window.active.background_color
            : theme->window.inactive.background_color)
        : fill;

    for (int bi = 0; bi < 6; ++bi) {
        x = (int16_t) (right_edge - (int16_t) btn -
                (int16_t) ((uint16_t) bi * step));
        color = ((!can_maximize) && (bi == 1 || bi == 2))
            ? bg_fill : fill;
        gc = xcb_generate_id(connection);

        xcb_create_gc(connection, gc, titlebar,
                XCB_GC_FOREGROUND, &color);
        rect = (xcb_rectangle_t) { x, btn_y, btn, btn };
        xcb_poly_fill_rectangle(connection, titlebar, gc, 1, &rect);
        xcb_free_gc(connection, gc);
    }
}


/**
 * @brief Draw corner-resize grips on a decorated frame window
 *
 * Paints small L-shaped marks at the four corners of the frame using
 * solid-color rectangles so that users can see that the window edges
 * are interactive resize grips.  The marks are drawn only for
 * resizable, decorated clients and use the active or inactive border
 * accent color.
 *
 * @param connection Active XCB connection
 * @param frame      Frame window to draw on
 * @param frame_w    Total frame width in pixels
 * @param frame_h    Total frame height in pixels
 * @param color      Fill color for the corner marks
 *
 * @note Complexity: @e O(1)
 */
static void s_draw_corner_grips(xcb_connection_t *connection,
        xcb_window_t frame, uint16_t frame_w, uint16_t frame_h,
        uint32_t color)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rects[8];
    uint32_t gc_vals[1];
    uint16_t arm = (uint16_t) WM_RESIZE_CORNER_SIZE;
    uint16_t thickness = 2u;

    if (connection == NULL || frame == XCB_WINDOW_NONE) {
        return;
    }

    if (frame_w < arm * 2u || frame_h < arm * 2u) {
        return;
    }

    gc = xcb_generate_id(connection);
    gc_vals[0] = color;
    xcb_create_gc(connection, gc, frame, XCB_GC_FOREGROUND, gc_vals);

    /* Top-left: horizontal arm */
    rects[0] = (xcb_rectangle_t) { 0, 0, arm, thickness };
    /* Top-left: vertical arm */
    rects[1] = (xcb_rectangle_t) { 0, 0, thickness, arm };
    /* Top-right: horizontal arm */
    rects[2] = (xcb_rectangle_t) {
        (int16_t)(frame_w - arm), 0, arm, thickness };
    /* Top-right: vertical arm */
    rects[3] = (xcb_rectangle_t) {
        (int16_t)(frame_w - thickness), 0, thickness, arm };
    /* Bottom-left: horizontal arm */
    rects[4] = (xcb_rectangle_t) {
        0, (int16_t)(frame_h - thickness), arm, thickness };
    /* Bottom-left: vertical arm */
    rects[5] = (xcb_rectangle_t) {
        0, (int16_t)(frame_h - arm), thickness, arm };
    /* Bottom-right: horizontal arm */
    rects[6] = (xcb_rectangle_t) {
        (int16_t)(frame_w - arm), (int16_t)(frame_h - thickness),
        arm, thickness };
    /* Bottom-right: vertical arm */
    rects[7] = (xcb_rectangle_t) {
        (int16_t)(frame_w - thickness), (int16_t)(frame_h - arm),
        thickness, arm };

    xcb_poly_fill_rectangle(connection, frame, gc, 8, rects);
    xcb_free_gc(connection, gc);
}


/* Repaint the frame background, border, and corner resize grips */
void desktop_repaint_frame_decoration(xcb_connection_t *connection,
        const client_td *client, bool use_active_style,
        const struct config_theme_s *theme)
{
    if (connection == NULL || client == NULL || client->frame == 0 ||
            theme == NULL || !client_is_decorated(client)) {
        return;
    }

    xcb_change_window_attributes(connection, client->frame,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
                (use_active_style)
                    ? theme->window.active.border_color
                    : theme->window.inactive.border_color,
                (use_active_style)
                    ? theme->window.active.border_color
                    : theme->window.inactive.border_color
            });
    xcb_clear_area(connection, 0, client->frame, 0, 0, 0, 0);

    if (((client->config_base == NULL) ||
                client->config_base->windows.has_grips) &&
            client_is_resizable(client) &&
            !client_is_fullscreen(client) &&
            !client_is_maximized(client)) {
        s_draw_corner_grips(connection, client->frame,
                (uint16_t) client->layout.geometry.cur.dim.w,
                (uint16_t) client->layout.geometry.cur.dim.h,
                (use_active_style)
                    ? theme->window.active.grip_color
                    : theme->window.inactive.grip_color);
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
    bool hide_decoration;
    bool has_extra_window_border;
    uint32_t border_width;

    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    if (desktop->stacking == NULL) {
        LOGGER_ERROR("Desktop stacking list is null", L_NARG);
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
        LOGGER_ERROR("Stacking list head is null despite size > 0",
                L_NARG);
        return 1;
    }

    stacking_initial = stacking_node;

    /* Iterate through stacking list (back to front) */
    do {
        client = (client_td *) cdlist_data(stacking_node);

        if (client == NULL) {
            LOGGER_ERROR("Null client found in stacking list at" \
                    " position %d", client_count);
            stacking_node = cdlist_next(stacking_node);
            continue;
        }
        client_count++;

        /* Keep icon windows visible only for iconified clients.
         * Plain hidden windows must stay fully unmapped. */
        if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
            if (client->properties.state ==
                    (uint16_t) CLIENT_STATE_ICONIFIED) {
                ri_render_client_icon(desktop, client, is_current);
            }
            stacking_node = cdlist_next(stacking_node);
            continue;
        }

        is_focused = (desktop->client_active_id == client->id);

        hide_decoration =
            (client->properties.state ==
                 (uint16_t) CLIENT_STATE_FULLSCREEN &&
             client->was_decorated_fullscreen);

        target = (client_is_decorated(client) && client->frame != 0)
            ? client->frame
            : client->window;

        has_extra_window_border =
            cycle_client_has_extra_border(client, false);
        if (client_is_decorated(client) && client->frame != 0) {
            border_width = (has_extra_window_border)
                ? WM_ICON_CYCLE_SEL_BORDER_EXTRA : 0u;
        } else {
            border_width = client->theme->window.general.border_width;
            if (has_extra_window_border) {
                border_width += WM_ICON_CYCLE_SEL_BORDER_EXTRA;
            }
        }

        xcb_configure_window(desktop->connection, target,
                XCB_CONFIG_WINDOW_BORDER_WIDTH, &border_width);

        /* Map the window to make it visible */
        /* Only do this when 'desktop' is the surface's currently
         * displayed desktop.
         *
         * Its also invoked as part of a general
         * 'surface_render_all_desktops()' refresh pass whenever ANY
         * desktop's 'is_outdated' flag is set (e.g., after moving or
         * resizing a client, which marks its own desktop outdated).
         *
         * If that pass unconditionally mapped clients on a desktop that
         * is not currently shown, it could race with (and undo) an
         * explicit 'surface_clients_hide()' issued by a desktop switch,
         * making a client reappear on top of the desktop the user just
         * switched to.
         *
         * Visibility of non-current desktops must be governed solely by
         * 'surface_clients_hide'/'surface_clients_show' */
        if (is_current) {
            if (client->icon_window != 0 && client->is_icon_mapped) {
                xcb_unmap_window(desktop->connection,
                        client->icon_window);
                client->is_icon_mapped = false;
            }
            if (client->titlebar != 0 && !hide_decoration) {
                xcb_map_window(desktop->connection, client->titlebar);
            } else if (client->titlebar != 0) {
                xcb_unmap_window(desktop->connection, client->titlebar);
            }
            xcb_map_window(desktop->connection, target);

            /* Do not re-map the content window for shaded clients: the
             * shade operation explicitly unmaps it, and mapping it here
             * would undo the shade and prevent the titlebar-only view
             * from being painted correctly, especially for inactive
             * windows that receive no 'FocusOut'-triggered repaint */
            if (target != client->window && !client_is_shaded(client)) {
                xcb_map_window(desktop->connection, client->window);
            }
        }

        if (client->is_outdated) {
            /* Configure position and size; only when the client's
             * geometry or decoration changed.  Skipping this for
             * up-to-date clients prevents the server from generating
             * spurious 'ConfigureNotify' and 'Expose' events that cause
             * other windows to unnecessarily redraw, which appears as
             * flicker during keyboard resize of an unrelated client. */
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
                    ? (uint16_t) (client->layout.geometry.cur.dim.w -
                            left - right)
                    : 1;
                inner_h = (client->layout.geometry.cur.dim.h > top + bottom)
                    ? (uint16_t) (client->layout.geometry.cur.dim.h -
                            top - bottom)
                    : 1;

                xcb_configure_window(desktop->connection, client->window,
                        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                        (const uint32_t[]) {
                            left, top, inner_w, inner_h
                        });

                /* ICCCM §4.2.3: the xcb_configure_window above positions
                 * the inner window relative to the frame (x=left, y=top),
                 * so the X server delivers a 'ConfigureNotify' to the
                 * client with those frame-relative coordinates.  Override
                 * it immediately with a synthetic 'ConfigureNotify'
                 * carrying the true screen-relative position so the
                 * client's last geometry notification is always correct.
                 * Without this a decorated client sees a frame-relative
                 * 'ConfigureNotify' as its final event on every render
                 * pass, causing misaligned popups and a content area
                 * that appears not to fill the frame until the next
                 * user-triggered repaint. */
                client_send_synthetic_configure_notify(desktop->connection,
                        client);

                /* Force a repaint AFTER the synthetic 'ConfigureNotify'
                 * so the client always redraws at its correct
                 * screen-relative geometry.  Some programs do not
                 * redraw on 'ConfigureNotify' alone; this 'Expose'
                 * ensures the drawing happens at the right size and
                 * position after every render pass, including the
                 * initial map and post-resize redraws.  Setting
                 * 'exposures=1' causes the X server to generate an
                 * 'Expose' event, which arrives in the client's queue
                 * after both the 'xcb_configure_window' and the
                 * synthetic 'ConfigureNotify' above. */
                xcb_clear_area(desktop->connection, 1,
                        client->window, 0, 0, 0, 0);
                desktop_repaint_frame_decoration(desktop->connection,
                        client, is_focused, desktop->config_theme);

                if (client->titlebar != 0 && !hide_decoration) {
                    xcb_configure_window(desktop->connection,
                            client->titlebar,
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
                            client->titlebar, XCB_CW_BACK_PIXEL,
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

                    /* Use theme foreground color so text contrasts
                     * against the titlebar background (active or
                     * inactive) */
                    text_renderer_set_color(
                    (is_focused)
                    ? desktop->config_theme->window.active.foreground_color
                    : desktop->config_theme->window.inactive.foreground_color,
                    (is_focused)
                    ? desktop->config_theme->window.active.background_color
                    : desktop->config_theme->window.inactive.background_color);

                    text_draw_string(desktop->connection,
                            client->titlebar, XCB_NONE,
                            (int16_t) (WM_DECOR_BTN_PAD +
                                2u * (WM_DECOR_BTN_SIZE +
                                    WM_DECOR_BTN_GAP) +
                                WM_DECOR_BTN_GAP),
                            (int16_t) ((title_h >
                                (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD)
                            ? title_h -
                                (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD
                            : title_h),
                            client->info.name);

                    desktop_draw_titlebar_buttons(desktop->connection,
                            client->titlebar,
                            inner_w,
                            title_h,
                            is_focused,
                            (bool) client_is_sticky(client),
                            (client->properties.layer != CLIENT_LAYER_NORMAL),
                            (!client_is_fullscreen(client) &&
                             (bool) client_is_resizable(client)),
                            desktop->config_theme);
                } else if (client->titlebar != 0) {
                    xcb_unmap_window(desktop->connection, client->titlebar);
                }
            }

            client->is_outdated = false;
        } else if (target != client->window && desktop->focus_dirty) {
            /* The client geometry has not changed; only refresh the
             * focus-sensitive decoration colors (border and titlebar
             * background/text) when the active client actually changed.
             * Skipping this repaint when focus is unchanged avoids
             * spurious 'xcb_clear_area + text-draw' calls on every
             * render pass during resize, which was the source of the
             * desktop-wide flickering visible on all non-resized
             * windows. */
            left = (uint16_t) client->layout.frame_extents.left;
            right = (uint16_t) client->layout.frame_extents.right;
            top = (uint16_t) client->layout.frame_extents.top;
            bottom = (uint16_t) client->layout.frame_extents.bottom;
            title_h = client->title_height;
            inner_w = (client->layout.geometry.cur.dim.w > left + right)
                ? (uint16_t) (client->layout.geometry.cur.dim.w -
                        left - right)
                : 1;

            desktop_repaint_frame_decoration(desktop->connection, client,
                    is_focused, desktop->config_theme);

            if (client->titlebar != 0 && !hide_decoration) {
                xcb_change_window_attributes(desktop->connection,
                        client->titlebar, XCB_CW_BACK_PIXEL,
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

                text_renderer_set_color(
                (is_focused)
                    ? desktop->config_theme->window.active.foreground_color
                    : desktop->config_theme->window.inactive.foreground_color,
                (is_focused)
                    ? desktop->config_theme->window.active.background_color
                    : desktop->config_theme->window.inactive.background_color);

                text_draw_string(desktop->connection,
                        client->titlebar, XCB_NONE,
                        (int16_t) (WM_DECOR_BTN_PAD +
                            2u * (WM_DECOR_BTN_SIZE +
                                WM_DECOR_BTN_GAP) +
                            WM_DECOR_BTN_GAP),
                        (int16_t) ((title_h >
                            (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD)
                        ? title_h -
                            (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD
                        : title_h),
                        client->info.name);

                desktop_draw_titlebar_buttons(desktop->connection,
                        client->titlebar,
                        inner_w,
                        title_h,
                        is_focused,
                        (bool) client_is_sticky(client),
                        (client->properties.layer != CLIENT_LAYER_NORMAL),
                        (!client_is_fullscreen(client) &&
                         (bool) client_is_resizable(client)),
                        desktop->config_theme);
            } else if (client->titlebar != 0) {
                xcb_unmap_window(desktop->connection, client->titlebar);
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
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
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
    desktop->focus_dirty = true;

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
