/**
 * @file render/desktop.c
 *
 * @brief Desktop rendering implementation
 */
/*
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
    uint32_t mask;
    uint32_t values[1];

    if (desktop == NULL) {
        LOGGER_ERROR("Received NULL desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_DEBUG("Rendering background for desktop '%s' with color #%06x",
            desktop->name, desktop->background.bg.color);

    /* Get the screen */
    iter = xcb_setup_roots_iterator(xcb_get_setup(desktop->connection));
    screen = NULL;
    
    for (uint32_t i = 0; i < desktop->screen_id && iter.rem > 0; i++) {
        xcb_screen_next(&iter);
    }
    
    if (iter.rem == 0 || iter.data == NULL) {
        LOGGER_ERROR("Could not get screen for background rendering",
                L_NARG);
        return 1;
    }
    
    screen = iter.data;

    /* Change the root window background color */
    mask = XCB_CW_BACK_PIXEL;
    values[0] = desktop->background.bg.color;

    xcb_change_window_attributes(desktop->connection,
            screen->root, mask, values);

    /* Clear/expose the window to show the new background */
    xcb_clear_area(desktop->connection, 1,
            screen->root, 0, 0,
            screen->width_in_pixels,
            screen->height_in_pixels);

    LOGGER_DEBUG("Background rendered for desktop '%s'", desktop->name);

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
        LOGGER_ERROR("Received NULL desktop pointer", L_NARG);
        return 1;
    }

    if (desktop->stacking == NULL) {
        LOGGER_ERROR("Desktop stacking list is NULL!", L_NARG);
        return 1;
    }

    stacking_size = cdlist_size(desktop->stacking);
    LOGGER_DEBUG("Rendering %zu client(s) from stacking list on desktop '%s'",
            stacking_size, desktop->name);

    /* If no clients, return early */
    if (stacking_size == 0) {
        LOGGER_DEBUG("No clients to render on desktop '%s'", desktop->name);
        return 0;
    }

    stacking_node = cdlist_head(desktop->stacking);
    if (stacking_node == NULL) {
        LOGGER_ERROR("Stacking list head is NULL despite size > 0", L_NARG);
        return 1;
    }

    stacking_initial = stacking_node;

    /* Iterate through stacking list (back to front) */
    do {
        client = (client_td *) cdlist_data(stacking_node);
        
        if (client == NULL) {
            LOGGER_ERROR("NULL client found in stacking list at position %d",
                    client_count);
            stacking_node = cdlist_next(stacking_node);
            continue;
        }
        client_count++;

        /* Skip hidden clients */
        if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
            LOGGER_TRACE("Skipping hidden client %#x", client->id);
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

        LOGGER_TRACE("Rendered client %#x at (%u, %u) size (%u x %u)",
                client->id,
                client->layout.geometry.cur.pos.x,
                client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h);

        stacking_node = cdlist_next(stacking_node);
    } while (stacking_node != NULL && 
             stacking_node != stacking_initial &&
             client_count < (int)stacking_size);

    LOGGER_DEBUG("Successfully rendered %d clients on desktop '%s'",
            client_count, desktop->name);

    return 0;
}


/* Full desktop render */
int desktop_render_full(desktop_td *desktop)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Received NULL desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_DEBUG("Full render of desktop '%s'", desktop->name);

    /* Draw background (currently a no-op in X11) */
    if (desktop_render_background(desktop) != 0) {
        LOGGER_ERROR("Failed to render background", L_NARG);
        return 1;
    }

    /* Draw all clients */
    if (desktop_render_clients(desktop) != 0) {
        LOGGER_ERROR("Failed to render clients", L_NARG);
        return 1;
    }

    /* Mark desktop as up-to-date */
    desktop->is_outdated = false;

    /* NOTE: Do NOT flush here! Let the caller (surface) handle flushing */

    return 0;
}


/* Flush drawing operations */
void desktop_render_flush(desktop_td *desktop)
{
    if (desktop == NULL || desktop->connection == NULL) {
        LOGGER_ERROR("Invalid desktop or connection for flushing", L_NARG);
        return;
    }

    xcb_flush(desktop->connection);
}
