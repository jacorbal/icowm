/**
 * @file render/surface.c
 *
 * @brief Surface rendering implementation
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Project includes */
#include <desktop.h>
#include <logger.h>
#include <render/desktop.h>

/* Local includes */
#include <render/surface.h>


/* Render the current desktop on a surface */
int surface_render_current_desktop(surface_td *surface)
{
    desktop_td *desktop;
    cdlist_item_td *desktop_node;
    uint32_t counter = 0;

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
    for (counter = 0; counter < surface->desktop_cur; ++counter) {
        desktop_node = cdlist_next(desktop_node);
        if (desktop_node == NULL) {
            LOGGER_ERROR("Could not find desktop at index %u",
                    surface->desktop_cur);
            return 1;
        }
    }

    desktop = (desktop_td *) cdlist_data(desktop_node);
    if (desktop == NULL) {
        LOGGER_ERROR("NULL desktop pointer", L_NARG);
        return 1;
    }

    LOGGER_DEBUG("Rendering desktop '%s'", desktop->name);

    /* Render only this desktop */
    if (desktop_render_full(desktop) != 0) {
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
    desktop_td *desktop;
    uint32_t rendered_count = 0;

    if (surface == NULL || surface->desktops == NULL) {
        LOGGER_ERROR("Invalid surface or desktops list", L_NARG);
        return 1;
    }

    if (surface->desktop_count == 0) {
        LOGGER_WARNING("Surface has no desktops to render", L_NARG);
        return 0;
    }

    LOGGER_DEBUG("Full render of all %u desktops on surface %u",
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
        desktop = (desktop_td *) cdlist_data(desktop_node);

        if (desktop == NULL) {
            LOGGER_WARNING("NULL desktop in list at position %u",
                    rendered_count);
            desktop_node = cdlist_next(desktop_node);
            rendered_count++;
            continue;
        }

        LOGGER_DEBUG("Rendering desktop %u ('%s')",
                rendered_count, desktop->name);

        /* Only render if outdated */
        if (desktop->is_outdated) {
            if (desktop_render_full(desktop) != 0) {
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

    /* FLUSH ONCE at the end, not per-desktop */
    surface_render_flush(surface);
    surface->is_outdated = false;

    return 0;
}


/* Flush rendering operations */
void surface_render_flush(surface_td *surface)
{
    if (surface == NULL || surface->connection == NULL) {
        LOGGER_ERROR("Invalid surface or connection for flushing", L_NARG);
        return;
    }

    LOGGER_DEBUG("Flushing surface %u to X server", surface->id);
    xcb_flush(surface->connection);
}
