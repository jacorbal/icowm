/**
 * @file surface.c
 *
 * @brief Surface handling implementation
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
#include <stdlib.h>     /* NULL, free, malloc */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <surface.h>


/* Update the properties of the surface */
static void s_update_properties(surface_td *surface,
        xcb_screen_t *screen)
{
    xcb_depth_iterator_t depth_iter;
    int xx, yy;

    /* Update surface dimensions */
    xx = screen->width_in_pixels;  // XCB allows direct access to these
    yy = screen->height_in_pixels;

    surface->properties.dim.w = (xx > 0) ? (uint32_t) xx : 0;
    surface->properties.dim.h = (yy > 0) ? (uint32_t) yy : 0;

    /* Get dimensions in mm from the screen */
    xx = screen->width_in_millimeters;
    yy = screen->height_in_millimeters;

    surface->properties.dim_mm.w = (xx > 0) ? (uint32_t) xx : 0;
    surface->properties.dim_mm.h = (yy > 0) ? (uint32_t) yy : 0;

    /* Calculate DPI; dpi = px / (mm/25.4);  1 in ~= 25.4 mm */
    /* Calculate DPI for x-axis */
    if (surface->properties.dim_mm.w > 0) {
        surface->properties.dpi.x =
            (uint32_t) ((float) surface->properties.dim.w /
                    ((float) surface->properties.dim_mm.w / 25.4f));
    } else {
        /* Division by zero: DPI in 'x' set to 0 */
        surface->properties.dpi.x = 0;
    }

    /* Calculate DPI for y-axis */
    if (surface->properties.dim_mm.h > 0) {
        surface->properties.dpi.y =
            (uint32_t) ((float) surface->properties.dim.h /
                    ((float) surface->properties.dim_mm.h / 25.4f));
    } else {
        /* Division by zero: DPI in 'y' set to 0 */
        surface->properties.dpi.y = 0;
    }

    /* Set visual properties */
    /* Iterate through depths to find the appropriate visual */
    depth_iter = xcb_screen_allowed_depths_iterator(screen);

    // Assume we take the first depth available (modify as necessary)
    if (depth_iter.rem > 0) {
        xcb_depth_t *depth = depth_iter.data;
        xcb_visualtype_iterator_t visual_iter;

        surface->properties.visual_info.properties.depth = depth->depth;

        // Get the first visual ID from the first depth
        visual_iter =
            xcb_depth_visuals_iterator(depth);
        if (visual_iter.rem > 0) {
            surface->properties.visual_info.visual_id =
                visual_iter.data->visual_id;
        }

        /* NOTE: It may be needed to get the default colormap from the
         *       root window's configuration */
        surface->properties.visual_info.properties.colormap =
            screen->default_colormap;
    }
}


/* Initialize a new surface */
surface_td *surface_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        const uint32_t surface_id, uint32_t desktop_count,
        config_td *config)
{
    surface_td *surface;
    xcb_screen_iterator_t iter;

    LOGGER_DEBUG("Initializing surface %u", surface_id);
    surface = malloc(sizeof(surface_td));
    if (surface == NULL) {
        LOGGER_FATAL("Failed to allocate memory for surface %u",
                surface_id);
        return NULL;
    }

    LOGGER_TRACE("Retrieving surface information from X server", L_NARG);
    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    for (uint32_t i = 0; i < surface_id && iter.rem > 0; ++i) {
        xcb_screen_next(&iter);
    }
    surface->screen = iter.data;
    if (surface->screen == NULL) {
        LOGGER_FATAL("Failed to retrieve information for surface %u",
                surface_id);
        return NULL;
    }

    surface->id = surface_id;
    surface->connection = connection;
    surface->ewmh = ewmh;
    surface->config = config;

    /* Update surface properties */
    s_update_properties(surface, surface->screen);

    /* Handle desktops */
    LOGGER_TRACE("Setting up all %d desktops", desktop_count);

    LOGGER_TRACE("Initializing desktop list structure for surface %u",
            surface_id);
    surface->desktops = cdlist_init((void(*)(void *)) desktop_destroy);
    if (surface->desktops == NULL) {
        LOGGER_FATAL("Failed to allocate memory for desktops on" \
                " surface %u", surface_id);
        free(surface);
        return NULL;
    }

    /* Initialize desktops */
    surface->desktop_count = desktop_count;
    for (uint32_t i = 0; i < desktop_count; ++i) {
        desktop_td *desktop = desktop_init(surface->connection,
                surface->ewmh,
                surface_id, i,
                &(surface->config->base), &(surface->config->theme));
        if (desktop == NULL) {
            LOGGER_FATAL("Failed to initialize desktop %u on" \
                    " surface %u", i, surface_id);
            cdlist_destroy(surface->desktops);
            free(surface);
            return NULL;
        }

        LOGGER_TRACE("Inserting desktop %u ('%s') of " \
                "surface %u into desktop list",
                i, desktop->name, surface_id);

        if (surface_desktop_add(surface, desktop) != 0) {
            LOGGER_FATAL("Failed to insert desktop %u ('%s') on" \
                    " surface %u into desktop list",
                    i, desktop->name, surface_id);
            desktop_destroy(desktop);
            cdlist_destroy(surface->desktops);
            free(surface);
            return NULL;
        }
    }

    surface->is_outdated = true;

    return surface;
}

/* Free allocated memory for a surface */
void surface_destroy(surface_td *surface)
{
    if (surface == NULL) {
        return;
    }

    LOGGER_DEBUG("Deallocating structure for surface %u", surface->id);

    LOGGER_TRACE("Deallocating desktops on surface %u", surface->id);
    cdlist_destroy(surface->desktops);

    LOGGER_TRACE("Destroying surface %u", surface->id);
    free(surface);
}


/* Soft surface update */
void surface_update(surface_td *surface)
{
//    LOGGER_TRACE("Updating surface %u", surface->id);

    /* Establish that this surface is already updated */
    surface->is_outdated = false;
}


/* Full surface update */
void surface_update_full(surface_td *surface)
{
    cdlist_item_td *desktop_node = cdlist_head(surface->desktops);

    LOGGER_TRACE("Fully updating surface %u", surface->id);

    /* Soft update */
    surface_update(surface);

    /* Update all desktops */
    if (desktop_node != NULL) {
        /* Reference to the initial node not to end up an infinite loop
         * in this circular list */
        cdlist_item_td *desktop_initial = desktop_node;
        do {
            desktop_td *desktop_cur =
                (desktop_td *) cdlist_data(desktop_node);
                if (desktop_cur->is_outdated) {
                    desktop_update_full(desktop_cur);
                }
                desktop_node = cdlist_next(desktop_node);
        } while (desktop_node != desktop_initial);
    }

    LOGGER_TRACE("Updated surface %u", surface->id);
}


/* Resize the surface */
void surface_resize(surface_td *surface,
        uint32_t width, uint32_t height)
{
    if (surface->properties.dim.w != width) {
        surface->properties.dim.w = width;
    }

    if (surface->properties.dim.h != height) {
        surface->properties.dim.h = height;
    }

    /* TODO: More logic here to update connection, desktops, &c. */
}


/* Add a new desktop to the list */
int surface_desktop_add(surface_td *surface, desktop_td *desktop)
{
    if (surface == NULL|| desktop == NULL) {
        return -1;
    }

    /* Add to the tail of the circular linked list */
    if (cdlist_ins_next(surface->desktops,
                cdlist_tail(surface->desktops), desktop) != 0) {
        /* Failed to add to the list */
        return 1;
    }

    /* Update the count of desktops */
    surface->desktop_count++;

    return 0;
}


/* Remove a desktop from the list by its ID */
int surface_desktop_rem(surface_td *surface, uint32_t desktop_id)
{
    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    /* Iterate over each desktop in the circular list */
    for (cdlist_item_td *current_item = cdlist_head(surface->desktops);
         current_item != NULL;
         current_item = cdlist_next(current_item)) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            /* Remove the desktop */
            if (cdlist_rem_next(surface->desktops,
                        current_item, NULL) != 0) {
                /* Failed to remove from list */
                return 1;
            }
            desktop_destroy(desktop);

            /* Update the count of desktops */
            surface->desktop_count--;
            return 0;
        }
    }

    /* Desktop not found */
    return 2;
}


/* Get a desktop from the list by its ID */
desktop_td *surface_desktop_get(surface_td *surface,
        uint32_t desktop_id)
{
    if (surface == NULL) {
        return NULL;
    }

    /* Iterate over each desktop in the circular list */
    for (cdlist_item_td *current_item = cdlist_head(surface->desktops);
            current_item != NULL;
            current_item = cdlist_next(current_item)) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            return desktop;
        }
    }

    /* Desktop ID not found */
    return NULL;
}


/* Get the previous desktop in the list, optionally cycling */
desktop_td *surface_desktop_prev(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return NULL;
    }

    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            cdlist_item_td *prev_item = cdlist_prev(current_item);
            /* Wrapping is detected when the computed previous item is
             * the tail, that only happens when 'current_item' was the
             * head, since 'cdlist_prev' on a circular list wraps
             * 'head->tail'.  Comparing against the head here, as
             * a previous version of this function did, is wrong.  For
             * any desktop other than the first one, 'prev_item' can
             * legitimately equal the head (e.g. moving from the second
             * to the first desktop) which incorrectly looked like
             * a wraparound and jumped to the tail instead of stopping
             * at the head. */
            if (prev_item == cdlist_tail(surface->desktops)) {
                if (cycle) {
                    /* Circular behavior; wrap to the last desktop */
                    return (desktop_td *)
                        cdlist_data(cdlist_tail(surface->desktops));
                }
                /* No valid previous desktop */
                return NULL;
            }
            return (desktop_td *) cdlist_data(prev_item);
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop not found */
    return NULL;
}


/* Get the next desktop in the list, optionally cycling */
desktop_td *surface_desktop_next(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return NULL;
    }

    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            cdlist_item_td *next_item = cdlist_next(current_item);
            if (next_item == cdlist_head(surface->desktops)) {
                if (cycle) {
                    /* Circular behavior; wrap to the first desktop */
                    return (desktop_td *)
                        cdlist_data(cdlist_head(surface->desktops));
                }

                /* No valid next desktop */
                return NULL;
            }
            return (desktop_td *) cdlist_data(next_item);
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop not found */
    return NULL;
}


/* Select the previous desktop, optionally cycling */
int surface_desktop_select_prev(surface_td *surface, bool cycle)
{
    desktop_td *prev_desktop;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    prev_desktop =
        surface_desktop_prev(surface, surface->desktop_cur, cycle);
    if (prev_desktop) {
        /* Update ID of new current desktop */
        surface->desktop_cur = prev_desktop->id;
        return 0;
    }

    /* No previous desktop found */
    return 1;
}


/* Select the next desktop, optionally cycling */
int surface_desktop_select_next(surface_td *surface, bool cycle)
{
    desktop_td *next_desktop;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    next_desktop =
        surface_desktop_next(surface, surface->desktop_cur, cycle);
    if (next_desktop) {
        /* Update ID of new current desktop */
        surface->desktop_cur = next_desktop->id;
        return 0;
    }

    /* No next desktop found */
    return 1;
}


/* Select a specific desktop by ID */
int surface_desktop_select(surface_td *surface, uint32_t desktop_id)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    /* Iterate through the desktops list to check if ID is valid */
    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            /* Update ID of new current desktop */
            surface->desktop_cur = desktop_id;
            return 0;
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop ID not found */
    return 1;
}


/* Unmap all non-sticky clients on the specified desktop */
void surface_clients_hide(surface_td *surface, uint32_t desktop_id)
{
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;

    if (surface == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, desktop_id);
    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return;
    }

    initial = node;
    do {
        client_td *client = (client_td *) cdlist_data(node);
        if (client != NULL &&
                !(client->properties.flags & CLIENT_FLAG_STICKY)) {
            xcb_window_t target =
                (client_is_decorated(client) && client->frame != 0)
                ? client->frame
                : client->window;

            /* Track WM-initiated unmaps so handler_unmap_notify skips
             * them */
            client->ignore_unmap += 1u;
            if (target != client->window) {
                client->ignore_unmap += 1u;
            }

            if (client->titlebar != 0) {
                xcb_unmap_window(surface->connection, client->titlebar);
            }

            xcb_unmap_window(surface->connection, target);
            if (target != client->window) {
                xcb_unmap_window(surface->connection, client->window);
            }

            if (client->icon_window != 0 && client->is_icon_mapped) {
                xcb_unmap_window(surface->connection, client->icon_window);
            }
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);
}


/* Add a new desktop to the surface */
int surface_action_desktop_add(surface_td *surface)
{
    desktop_td *desktop;

    LOGGER_DEBUG("Adding new desktop to surface %u", surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    desktop = desktop_init(surface->connection,
            surface->ewmh,
            surface->id,
            surface->desktop_count,
            &(surface->config->base),
            &(surface->config->theme));
    if (desktop == NULL) {
        LOGGER_ERROR("Failed to initialize new desktop on surface %u",
                surface->id);
        return 1;
    }

    if (surface_desktop_add(surface, desktop) != 0) {
        LOGGER_ERROR("Failed to add desktop to surface %u", surface->id);
        desktop_destroy(desktop);
        return 1;
    }

    surface->is_outdated = true;

    return 0;
}


/* Remove the last desktop from the surface */
int surface_action_desktop_remove(surface_td *surface)
{
    cdlist_item_td *tail_item;
    desktop_td *desktop;

    LOGGER_DEBUG("Removing desktop from surface %u", surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Need at least two desktops to remove one */
    if (surface->desktop_count <= 1) {
        LOGGER_NOTICE("Cannot remove the last desktop on surface %u",
                surface->id);
        return 1;
    }

    tail_item = cdlist_tail(surface->desktops);
    if (tail_item == NULL) {
        return 1;
    }

    desktop = (desktop_td *) cdlist_data(tail_item);
    if (desktop == NULL) {
        return 1;
    }

    /* If the desktop to be removed is the current one, switch first */
    if (desktop->id == surface->desktop_cur) {
        surface_clients_hide(surface, surface->desktop_cur);
        surface_desktop_select_prev(surface, false);
        surface_clients_show(surface, surface->desktop_cur);
    }

    if (surface_desktop_rem(surface, desktop->id) != 0) {
        LOGGER_ERROR("Failed to remove desktop from surface %u",
                surface->id);
        return 1;
    }

    surface->is_outdated = true;

    return 0;
}


/* Switch to a specific desktop by ID */
int surface_action_desktop_switch(surface_td *surface,
        uint32_t desktop_id)
{
    uint32_t old_id;

    LOGGER_DEBUG("Switching to desktop %u on surface %u",
            desktop_id, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    old_id = surface->desktop_cur;
    if (desktop_id == old_id) {
        return 0;
    }

    surface_clients_hide(surface, old_id);
    if (surface_desktop_select(surface, desktop_id) != 0) {
        /* Restore visibility on failure */
        surface_clients_show(surface, old_id);
        LOGGER_ERROR("Failed to switch to desktop %u on surface %u",
                desktop_id, surface->id);
        return 1;
    }

    surface_clients_show(surface, desktop_id);
    surface->is_outdated = true;
    xcb_flush(surface->connection);

    return 0;
}


/* Switch to the next desktop */
int surface_action_desktop_switch_next(surface_td *surface)
{
    uint32_t old_id;

    LOGGER_DEBUG("Switching to next desktop on surface %u", surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    old_id = surface->desktop_cur;
    surface_clients_hide(surface, old_id);
    surface_desktop_select_next(surface, true);

    if (surface->desktop_cur != old_id) {
        surface_clients_show(surface, surface->desktop_cur);
        surface->is_outdated = true;
        xcb_flush(surface->connection);
    } else {
        surface_clients_show(surface, old_id);
    }

    return 0;
}


/* Switch to the previous desktop */
int surface_action_desktop_switch_prev(surface_td *surface)
{
    uint32_t old_id;

    LOGGER_DEBUG("Switching to previous desktop on surface %u",
            surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    old_id = surface->desktop_cur;
    surface_clients_hide(surface, old_id);
    surface_desktop_select_prev(surface, true);

    if (surface->desktop_cur != old_id) {
        surface_clients_show(surface, surface->desktop_cur);
        surface->is_outdated = true;
        xcb_flush(surface->connection);
    } else {
        surface_clients_show(surface, old_id);
    }

    return 0;
}


/* Toggle full-surface mode */
int surface_action_toggle_fullsurface(surface_td *surface)
{
    LOGGER_DEBUG("Toggling full-surface mode on surface %u",
            surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    surface->fullsurface = !surface->fullsurface;
    surface->is_outdated = true;
    xcb_flush(surface->connection);

    return 0;
}


/* Set the surface resolution */
int surface_action_set_resolution(surface_td *surface,
        struct dimensions_s resolution)
{
    LOGGER_DEBUG("Setting resolution to %ux%u on surface %u",
            resolution.w, resolution.h, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    surface_resize(surface, resolution.w, resolution.h);
    surface->is_outdated = true;

    return 0;
}


/* Set the surface orientation */
int surface_action_set_orientation(surface_td *surface, int orientation)
{
    LOGGER_DEBUG("Setting orientation %d on surface %u",
            orientation, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Orientation changes require XRandR; not implemented */
    (void) orientation;

    return 0;
}


/* Set the surface brightness */
int surface_action_set_brightness(surface_td *surface,
        uint16_t brightness)
{
    LOGGER_DEBUG("Setting brightness %u on surface %u",
            (unsigned int) brightness, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Brightness control requires backlight interface; not implemented */
    (void) brightness;

    return 0;
}


/* Set the surface contrast */
int surface_action_set_contrast(surface_td *surface, uint16_t contrast)
{
    LOGGER_DEBUG("Setting contrast %u on surface %u",
            (unsigned int) contrast, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Contrast control requires display driver interface; not implemented */
    (void) contrast;

    return 0;
}


/* Apply current surface configuration settings */
int surface_action_configure_settings(surface_td *surface)
{
    LOGGER_DEBUG("Applying configuration settings on surface %u",
            surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    surface->is_outdated = true;
    xcb_flush(surface->connection);

    return 0;
}


/* Map all visible (non-hidden, non-iconified) clients on the specified
 * desktop */
void surface_clients_show(surface_td *surface, uint32_t desktop_id)
{
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;

    if (surface == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, desktop_id);
    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return;
    }

    initial = node;
    do {
        client_td *client = (client_td *) cdlist_data(node);
        if (client != NULL &&
                !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                client->properties.state !=
                    (uint16_t) CLIENT_STATE_ICONIFIED) {
            xcb_window_t target =
                (client_is_decorated(client) && client->frame != 0)
                ? client->frame
                : client->window;
            if (client->titlebar != 0) {
                xcb_map_window(surface->connection, client->titlebar);
            }
            xcb_map_window(surface->connection, target);
            if (target != client->window) {
                xcb_map_window(surface->connection, client->window);
            }
        } else if (client != NULL &&
                client->properties.state ==
                    (uint16_t) CLIENT_STATE_ICONIFIED &&
                client->icon_window != 0) {
            xcb_map_window(surface->connection, client->icon_window);
            client->is_icon_mapped = true;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    /* Restore Z-order: iterate from head (bottom) to tail (top),
     * raising each window to the top so the tail (topmost client) ends
     * up at the top of the X11 stacking order when all windows are
     * shown. */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        initial = node;
        do {
            client_td *c = (client_td *) cdlist_data(node);
            if (c != NULL &&
                    !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    c->properties.state !=
                    (uint16_t) CLIENT_STATE_ICONIFIED) {
                xcb_window_t tgt =
                    (client_is_decorated(c) && c->frame != 0)
                    ? c->frame : c->window;
                xcb_configure_window(surface->connection, tgt,
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    /* Restore input focus to the previously active client */
    if (desktop->client_active_id != 0) {
        node = cdlist_head(desktop->stacking);
        if (node != NULL) {
            initial = node;
            do {
                client_td *c = (client_td *) cdlist_data(node);
                if (c != NULL && c->id == desktop->client_active_id &&
                        c->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                    xcb_set_input_focus(surface->connection,
                            XCB_INPUT_FOCUS_PARENT,
                            c->window, XCB_CURRENT_TIME);
                    break;
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }
    }

    desktop->is_outdated = true;

}
