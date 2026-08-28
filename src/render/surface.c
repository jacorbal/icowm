/**
 * @file render/surface.c
 *
 * @brief Surface rendering implementation
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
#include <stddef.h>     /* NULL */

/* ADT includes */
#include <adt/cdlist.h>

/* Project includes */
#include <desktop.h>
#include <logger.h>
#include <render/desktop.h>

/* Local includes */
#include <render/surface.h>
#include <utils/xcb/connection.h>


/* Render the current desktop on a surface */
int surface_render_current_desktop(surface_td *surface)
{
    desktop_td *desktop;
    cdlist_item_td *desktop_node;

    if (surface == NULL || surface->desktops == NULL) {
        LOGGER_ERROR("Invalid surface or desktops list", L_NARG);
        return 1;
    }

    LOGGER_DEBUG("Rendering current desktop (index %u) on surface %u",
            surface->desktop_cur, surface->id);

    /* Find the current desktop */
    desktop_node = cdlist_head(surface->desktops);
    if (desktop_node == NULL) {
        LOGGER_ERROR("Surface has no desktops", L_NARG);
        return 1;
    }

    /* Iterate to the current desktop index */
    for (uint32_t counter = 0;
            counter < surface->desktop_cur;
            ++counter) {
        desktop_node = cdlist_next(desktop_node);
        if (desktop_node == NULL) {
            LOGGER_ERROR("Could not find desktop at index %u",
                    surface->desktop_cur);
            return 1;
        }
    }

    desktop = (desktop_td *) cdlist_data(desktop_node);
    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_DEBUG("Rendering desktop '%s'",
            (desktop->name[0] != '\0') ? desktop->name : "unnamed");

    /* Render only this desktop.  It is always the surface's current
     * desktop here, so clients must be (re-)mapped. */
    if (desktop_render_full(desktop, true) != 0) {
        LOGGER_ERROR("Failed to render desktop '%s'", desktop->name);
        return 1;
    }

    /* Flush ONCE at the end */
    surface_render_flush(surface);

    return 0;

}


/* Render all desktops on a surface */
int surface_render_all_desktops(surface_td *surface)
{
    cdlist_item_td *desktop_node;
    cdlist_item_td *desktop_initial;
    desktop_td *cur;
    uint32_t rendered_count = 0;

    if (surface == NULL || surface->desktops == NULL) {
        LOGGER_ERROR("Invalid surface or desktops list", L_NARG);
        return 1;
    }

    if (surface->desktop_count == 0) {
        LOGGER_WARNING("Surface has no desktops to render", L_NARG);
        return 0;
    }

    LOGGER_DEBUG("Fully rendering all %u desktops on surface %u",
            surface->desktop_count, surface->id);

    /* Get the first desktop */
    desktop_node = cdlist_head(surface->desktops);
    if (desktop_node == NULL) {
        LOGGER_ERROR("Surface desktops list is empty", L_NARG);
        return 1;
    }

    desktop_initial = desktop_node;

    /* Iterate through all desktops (circular list) */
    do {
        desktop_td *const desktop = (desktop_td *) cdlist_data(desktop_node);

        if (desktop == NULL) {
            LOGGER_WARNING("Desktop in list at position %u is null",
                    rendered_count);
            desktop_node = cdlist_next(desktop_node);
            rendered_count++;
            continue;
        }

        /* Checking is cheap and happens for every desktop regardless
         * of outcome, so this logs unconditionally; only the work
         * inside the 'is_outdated' branch below is actually expensive,
         * and 'desktop_render_full' logs its own specifics once that
         * runs. */
        LOGGER_DEBUG("Assessing whether desktop %u ('%s') needs" \
                " rendering", rendered_count,
                (desktop->name[0] != '\0') ? desktop->name : "unnamed");

        /* Only render if outdated */
        /* Pass wether this is the surface's currently displayed desktop
         * so that 'desktop_render_full()' never (re-)maps clients that
         * belong to a desktop the user is not currently looking at.
         * See 'desktop_render_clients' if you want. */
        if (desktop->is_outdated) {
            if (desktop_render_full(desktop,
                        rendered_count == surface->desktop_cur) != 0) {
                LOGGER_ERROR("Failed to render desktop '%s'",
                        desktop->name);
                return 1;
            }
        } else {
            LOGGER_TRACE("Desktop '%s' is up-to-date, skipping",
                    desktop->name);
        }
        rendered_count++;
        desktop_node = cdlist_next(desktop_node);
    } while (rendered_count < surface->desktop_count &&
             desktop_node != NULL &&
             desktop_node != desktop_initial);

    LOGGER_DEBUG("Rendered %u desktops on surface %u",
            rendered_count, surface->id);

    /* Re-apply the current desktop's background last so that it is the
     * one visible on the root window.  Earlier desktops in the list
     * would otherwise overwrite it. */
    cur = surface_desktop_get(surface, surface->desktop_cur);
    if (cur != NULL) {
        if (desktop_render_background(cur) != 0) {
            LOGGER_ERROR("Failed to re-apply background for current" \
                    " desktop '%s'", cur->name);
        }
    }

    /* FLUSH ONCE at the end, not per-desktop */
    surface_render_flush(surface);
    surface->is_outdated = false;

    return 0;
}


/* Mark current desktop outdated and repaint the surface */
void surface_render_current_desktop_repaint(surface_td *surface)
{
    desktop_td *cur;

    if (surface == NULL) {
        return;
    }

    cur = surface_desktop_get(surface, surface->desktop_cur);
    desktop_mark_outdated(cur);

    (void) surface_render_all_desktops(surface);
}


/* Flush rendering operations */
void surface_render_flush(surface_td *surface)
{
    if (surface == NULL || xcb_connection_get() == NULL) {
        LOGGER_ERROR("Invalid surface or connection for flushing",
                L_NARG);
        return;
    }

    LOGGER_DEBUG("Flushing surface %u to X server", surface->id);
}
