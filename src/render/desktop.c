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

/* Local includes */
#include <render/desktop.h>


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


/* Draw all clients on a desktop */
int desktop_render_clients(desktop_td *desktop)
{
    cdlist_item_td *stacking_node;
    cdlist_item_td *stacking_initial;
    client_td *client;
    int client_count = 0;
    size_t stacking_size;
    uint16_t mask;
    int32_t values[4];

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

        /* Skip hidden clients */
        if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
            LOGGER_TRACE("Skipping hidden client 0x%08x", client->id);
            stacking_node = cdlist_next(stacking_node);
            continue;
        }

        /* Map the window to make it visible */
        xcb_map_window(desktop->connection, client->window);

        /* Configure position and size */
        mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
               XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[0] = client->layout.geometry.cur.pos.x;
        values[1] = client->layout.geometry.cur.pos.y;
        values[2] = (int32_t) client->layout.geometry.cur.dim.w;
        values[3] = (int32_t) client->layout.geometry.cur.dim.h;

        xcb_configure_window(desktop->connection, client->window,
                mask, (uint32_t *) values);

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
int desktop_render_full(desktop_td *desktop)
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
    if (desktop_render_clients(desktop) != 0) {
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
