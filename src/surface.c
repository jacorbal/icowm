/**
 * @file surface.c
 *
 * @brief Surface lifecycle, i.e., init, update, resize, and desktop
 *        list implementation
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
#include <xcb/randr.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <surface.h>


/**
 * @brief Update the cached properties of a surface from its X screen
 *
 * Refreshes the surface dimensions in pixels and millimeters, computes
 * horizontal and vertical DPI from those values, and updates visual
 * information using the first available screen depth and visual.  The
 * default colormap is copied from the screen when visual data is found.
 *
 * @param surface Pointer to the surface to update
 * @param screen  Pointer to the XCB screen providing the source
 *                properties
 *
 * @note DPI is set to @c 0 on an axis whose physical size is
 *       unavailable
 * @note Complexity: @e O(1)
 */
static void s_update_properties(surface_td *surface,
        xcb_screen_t *screen)
{
    xcb_depth_iterator_t depth_iter;
    int xx, yy;

    /* Update surface dimensions */
    xx = screen->width_in_pixels;   /* XCB allows direct access to these */
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

        /* It may be needed to get the default colormap from the root
         * window's configuration */
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

    LOGGER_DEBUG("Retrieving surface information from X server", L_NARG);
    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    for (uint32_t i = 0; i < surface_id && iter.rem > 0; ++i) {
        xcb_screen_next(&iter);
    }
    surface->screen = iter.data;
    if (surface->screen == NULL) {
        LOGGER_FATAL("Failed to retrieve information for surface %u",
                surface_id);
        free(surface);
        return NULL;
    }

    surface->id = surface_id;
    surface->connection = connection;
    surface->ewmh = ewmh;
    surface->config = config;
    surface->showing_desktop = false;
    surface->randr.is_known = false;
    surface->randr.output_id = 0u;
    surface->randr.crtc_id = 0u;
    surface->randr.mode_id = 0u;
    surface->randr.rotation = 0u;

    /* Update surface properties */
    s_update_properties(surface, surface->screen);

    /* Discover this surface's own physical monitors, now that its
     * combined dimensions (the RandR-unavailable fallback) are
     * known */
    surface_refresh_monitors(surface);

    /* Handle desktops */
    LOGGER_DEBUG("Setting up all %u desktops", desktop_count);

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
    surface->desktop_count = 0;
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

    /* Dimensions are updated lazily by render/update paths. */
}


/**
 * @brief Whole-surface fallback for the monitor list
 *
 * Fills @p surface->monitors with a single entry spanning @p
 * surface->properties.dim, used whenever RandR cannot supply a real
 * monitor list.
 *
 * @param surface Pointer to the surface to fall back
 */
static void s_surface_monitors_fallback(surface_td *surface)
{
    surface->monitors[0].x = 0;
    surface->monitors[0].y = 0;
    surface->monitors[0].w = surface->properties.dim.w;
    surface->monitors[0].h = surface->properties.dim.h;
    surface->monitor_count = 1u;
    surface->primary_monitor_index = 0u;
}


/* Refresh the surface's own list of physical monitors */
void surface_refresh_monitors(surface_td *surface)
{
    xcb_randr_get_monitors_cookie_t cookie;
    xcb_randr_get_monitors_reply_t *reply;
    xcb_randr_monitor_info_iterator_t it;

    if (surface == NULL || surface->connection == NULL ||
            surface->screen == NULL) {
        return;
    }

    cookie = xcb_randr_get_monitors(surface->connection,
            surface->screen->root, 1u);
    reply = xcb_randr_get_monitors_reply(surface->connection,
            cookie, NULL);
    if (reply == NULL) {
        LOGGER_NOTICE("Failed to query RandR monitors for surface" \
                " %u; treating it as one monitor", surface->id);
        s_surface_monitors_fallback(surface);
        return;
    }

    surface->monitor_count = 0u;
    surface->primary_monitor_index = 0u;
    it = xcb_randr_get_monitors_monitors_iterator(reply);
    while (it.rem > 0 &&
            surface->monitor_count < WM_SURFACE_MAX_MONITORS) {
        xcb_randr_monitor_info_t *info = it.data;
        monitor_td *slot =
            &surface->monitors[surface->monitor_count];

        slot->x = info->x;
        slot->y = info->y;
        slot->w = info->width;
        slot->h = info->height;
        if (info->primary) {
            surface->primary_monitor_index = surface->monitor_count;
        }
        ++surface->monitor_count;

        xcb_randr_monitor_info_next(&it);
    }
    free(reply);

    if (surface->monitor_count == 0u) {
        LOGGER_NOTICE("RandR reported no monitors for surface %u;" \
                " treating it as one monitor", surface->id);
        s_surface_monitors_fallback(surface);
        return;
    }

    LOGGER_DEBUG("Surface %u has %u monitor(s)",
            surface->id, surface->monitor_count);
}


/* Find which of the surface's monitors contains a point */
monitor_td surface_monitor_for_point(const surface_td *surface,
        int32_t x, int32_t y)
{
    monitor_td fallback = {.x = 0, .y = 0, .w = 0u, .h = 0u};
    uint32_t closest = 0u;
    int64_t closest_dist = -1;

    if (surface == NULL) {
        return fallback;
    }
    if (surface->monitor_count == 0u) {
        fallback.w = surface->properties.dim.w;
        fallback.h = surface->properties.dim.h;
        return fallback;
    }

    for (uint32_t i = 0; i < surface->monitor_count; ++i) {
        const monitor_td *m = &surface->monitors[i];
        int32_t mright = m->x + (int32_t) m->w;
        int32_t mbottom = m->y + (int32_t) m->h;
        int64_t cx;
        int64_t cy;
        int64_t dist;

        if (x >= m->x && x < mright &&
                y >= m->y && y < mbottom) {
            return *m;
        }

        cx = m->x + (int32_t) (m->w / 2u) - x;
        cy = m->y + (int32_t) (m->h / 2u) - y;
        dist = cx * cx + cy * cy;
        if (closest_dist < 0 || dist < closest_dist) {
            closest_dist = dist;
            closest = i;
        }
    }

    return surface->monitors[closest];
}


/* Get the surface's primary monitor, if RandR flagged one */
monitor_td surface_primary_monitor(const surface_td *surface)
{
    monitor_td fallback = {.x = 0, .y = 0, .w = 0u, .h = 0u};

    if (surface == NULL) {
        return fallback;
    }
    if (surface->monitor_count == 0u) {
        fallback.w = surface->properties.dim.w;
        fallback.h = surface->properties.dim.h;
        return fallback;
    }

    return surface->monitors[surface->primary_monitor_index];
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
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return -1;
    }

    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count && current_item != NULL;
            ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            void *removed_desktop = NULL;
            /* Remove the desktop */
            if (cdlist_rem_next(surface->desktops,
                        cdlist_prev(current_item),
                        &removed_desktop) != 0) {
                /* Failed to remove from list */
                return 1;
            }
            desktop_destroy((desktop_td *) ((removed_desktop != NULL)
                        ? removed_desktop : (void *) desktop));

            /* Update the count of desktops */
            surface->desktop_count--;
            return 0;
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop not found */
    return 2;
}


/* Get a desktop from the list by its ID */
desktop_td *surface_desktop_get(surface_td *surface,
        uint32_t desktop_id)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return NULL;
    }

    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count && current_item != NULL;
            ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            return desktop;
        }
        current_item = cdlist_next(current_item);
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
             * legitimately equal the head (e.g., moving from the second
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


/* Recompute the work area for every desktop on a surface */
void surface_refresh_workareas(surface_td *surface)
{
    if (surface == NULL) {
        return;
    }

    for (uint32_t did = 0u; did < surface->desktop_count; ++did) {
        desktop_td *d = surface_desktop_get(surface, did);

        if (d != NULL) {
            desktop_update_workarea(d,
                    surface->properties.dim.w,
                    surface->properties.dim.h);
        }
    }
}
