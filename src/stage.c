/**
 * @file stage.c
 *
 * @brief Stage lifecycle: init, resize, and destroy
 *
 * Desktop-list membership and navigation live in @c stage/desktops.c,
 * RandR monitor detection in @c stage/monitors.c, and per-desktop
 * work-area recomputation in @c stage/workareas.c.  The four concerns
 * are kept apart on purpose: what a stage @e is, its monitors, its
 * desktop list, and the work area struts and margins carve out of it
 * are large enough that holding them together would help nobody find
 * anything.
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

/* Utils includes */
#include <utils/xcb/connection.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <stage.h>
#include <stage/desktop.h>
#include <stage/monitor.h>


/**
 * @brief Update the cached properties of a stage from its X screen
 *
 * Refreshes the stage dimensions in pixels and millimeters, computes
 * horizontal and vertical DPI from those values, and updates visual
 * information using the first available screen depth and visual.  The
 * default colormap is copied from the screen when visual data is found.
 *
 * @param stage  Pointer to the stage to update
 * @param screen Pointer to the XCB screen providing the source
 *               properties
 *
 * @note DPI is set to @c 0 on an axis whose physical size is
 *       unavailable
 * @note Complexity: @e O(1)
 */
static void s_properties_update(stage_td *stage,
        xcb_screen_t *screen)
{
    xcb_depth_iterator_t depth_iter;
    int xx, yy;

    /* Update stage dimensions */
    /* XCB allows direct access to these */
    xx = screen->width_in_pixels;
    yy = screen->height_in_pixels;

    stage->properties.dim.w = (xx > 0) ? (uint32_t) xx : 0;
    stage->properties.dim.h = (yy > 0) ? (uint32_t) yy : 0;

    /* Get dimensions in mm from the screen */
    xx = screen->width_in_millimeters;
    yy = screen->height_in_millimeters;

    stage->properties.dim_mm.w = (xx > 0) ? (uint32_t) xx : 0;
    stage->properties.dim_mm.h = (yy > 0) ? (uint32_t) yy : 0;

    /* Calculate DPI; dpi = px / (mm/25.4);  1 in ~= 25.4 mm */
    /* Calculate DPI for x-axis */
    if (stage->properties.dim_mm.w > 0) {
        stage->properties.dpi.x =
            (uint32_t) ((float) stage->properties.dim.w /
                    ((float) stage->properties.dim_mm.w / 25.4f));
    } else {
        /* Division by zero: DPI in 'x' set to 0 */
        stage->properties.dpi.x = 0;
    }

    /* Calculate DPI for y-axis */
    if (stage->properties.dim_mm.h > 0) {
        stage->properties.dpi.y =
            (uint32_t) ((float) stage->properties.dim.h /
                    ((float) stage->properties.dim_mm.h / 25.4f));
    } else {
        /* Division by zero: DPI in 'y' set to 0 */
        stage->properties.dpi.y = 0;
    }

    /* Set visual properties */
    /* Iterate through depths to find the appropriate visual */
    depth_iter = xcb_screen_allowed_depths_iterator(screen);

    /* Assume we take the first depth available */
    if (depth_iter.rem > 0) {
        xcb_depth_t *const depth = depth_iter.data;
        xcb_visualtype_iterator_t visual_iter;

        stage->properties.visual_info.properties.depth = depth->depth;

        /* Get the first visual ID from the first depth */
        visual_iter =
            xcb_depth_visuals_iterator(depth);
        if (visual_iter.rem > 0) {
            stage->properties.visual_info.visual_id =
                visual_iter.data->visual_id;
        }

        /* It may be needed to get the default colormap from the root
         * window's configuration */
        stage->properties.visual_info.properties.colormap =
            screen->default_colormap;
    }
}


/* Initialize a new stage */
stage_td *stage_init(xcb_connection_t *connection,
        const uint32_t stage_id, uint32_t desktop_count,
        config_td *config)
{
    stage_td *stage;
    xcb_screen_iterator_t iter;

    LOGGER_DEBUG("Initializing stage %u", stage_id);
    stage = malloc(sizeof(stage_td));
    if (stage == NULL) {
        LOGGER_FATAL("Failed to allocate memory for stage %u",
                stage_id);
        return NULL;
    }

    LOGGER_DEBUG("Retrieving stage information from X server", L_NARG);
    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    for (uint32_t i = 0; i < stage_id && iter.rem > 0; ++i) {
        xcb_screen_next(&iter);
    }
    stage->screen = iter.data;
    if (stage->screen == NULL) {
        LOGGER_FATAL("Failed to retrieve information for stage %u",
                stage_id);
        free(stage);
        return NULL;
    }

    stage->id = stage_id;
    stage->config = config;
    stage->is_showing_desktop = false;
    stage->strutless_maximize = false;
    stage->randr.is_known = false;
    stage->randr.output_id = 0u;
    stage->randr.crtc_id = 0u;
    stage->randr.mode_id = 0u;
    stage->randr.rotation = 0u;

    /* Update stage properties */
    s_properties_update(stage, stage->screen);

    /* Discover this stage's physical monitors, now that its
     * combined dimensions (the RandR-unavailable fallback) are
     * known */
    stage_monitor_refresh_all(stage);

    /* Handle desktops */
    LOGGER_DEBUG("Setting up all %u desktops", desktop_count);

    LOGGER_TRACE("Initializing desktop list structure for stage %u",
            stage_id);
    stage->desktops = cdlist_init((void(*)(void *)) desktop_destroy);
    if (stage->desktops == NULL) {
        LOGGER_FATAL("Failed to allocate memory for desktops on" \
                " stage %u", stage_id);
        free(stage);
        return NULL;
    }

    /* Initialize desktops */
    stage->desktop_count = 0;
    for (uint32_t i = 0; i < desktop_count; ++i) {
        desktop_td *const desktop = desktop_init(xcb_connection_get(),
                stage_id, i,
                stage->config);
        if (desktop == NULL) {
            LOGGER_FATAL("Failed to initialize desktop %u on" \
                    " stage %u", i, stage_id);
            cdlist_destroy(stage->desktops);
            free(stage);
            return NULL;
        }

        LOGGER_TRACE("Inserting desktop %u ('%s') of " \
                "stage %u into desktop list",
                i, desktop->name, stage_id);

        if (stage_desktop_add(stage, desktop) != 0) {
            LOGGER_FATAL("Failed to insert desktop %u ('%s') on" \
                    " stage %u into desktop list",
                    i, desktop->name, stage_id);
            desktop_destroy(desktop);
            cdlist_destroy(stage->desktops);
            free(stage);
            return NULL;
        }
    }

    stage->is_outdated = true;

    return stage;
}


/* Free allocated memory for a stage */
void stage_destroy(stage_td *stage)
{
    if (stage == NULL) {
        return;
    }

    LOGGER_DEBUG("Deallocating structure for stage %u", stage->id);

    LOGGER_TRACE("Deallocating desktops on stage %u", stage->id);
    cdlist_destroy(stage->desktops);

    LOGGER_TRACE("Destroying stage %u", stage->id);
    free(stage);
}


/* Resize the stage */
void stage_resize(stage_td *stage,
        uint32_t width, uint32_t height)
{
    if (stage == NULL) {
        return;
    }

    stage->properties.dim.w = width;
    stage->properties.dim.h = height;

    /* Dimensions are updated lazily by render/update paths. */
}
