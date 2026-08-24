/**
 * @file surface.c
 *
 * @brief Surface lifecycle: init, resize, and destroy
 *
 * Desktop-list membership and navigation live in @c surface/
 * desktops.c, RandR monitor detection in @c surface/monitors.c, and
 * per-desktop work-area recomputation in @c surface/workareas.c.
 * The four concerns are kept apart on purpose: what a surface @e is,
 * its own monitors, its own desktop list, and the work area struts
 * and margins carve out of it are large enough that holding them
 * together would help nobody find anything.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
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
static void s_properties_update(surface_td *surface,
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
        xcb_depth_t *const depth = depth_iter.data;
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
    surface->is_showing_desktop = false;
    surface->strutless_maximize = false;
    surface->randr.is_known = false;
    surface->randr.output_id = 0u;
    surface->randr.crtc_id = 0u;
    surface->randr.mode_id = 0u;
    surface->randr.rotation = 0u;

    /* Update surface properties */
    s_properties_update(surface, surface->screen);

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
        desktop_td *const desktop = desktop_init(surface->connection,
                surface->ewmh,
                surface_id, i,
                surface->config);
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


/* Resize the surface */
void surface_resize(surface_td *surface,
        uint32_t width, uint32_t height)
{
    surface->properties.dim.w = width;
    surface->properties.dim.h = height;

    /* Dimensions are updated lazily by render/update paths. */
}
