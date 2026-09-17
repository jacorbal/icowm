/**
 * @file render/stage.c
 *
 * @brief Stage rendering implementation
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

/* Render includes */
#include <render/desktop.h>
#include <render/desktop/background.h>

/* Utils includes */
#include <utils/xcb/connection.h>

/* Project includes */
#include <desktop.h>
#include <logger.h>
#include <stage/desktop.h>

/* Local includes */
#include <render/outdate.h>
#include <render/stage.h>


/* Render the current desktop on a stage */
int stage_render_current_desktop(stage_td *stage)
{
    desktop_td *desktop;
    cdlist_item_td *desktop_node;

    if (stage == NULL || stage->desktops == NULL) {
        LOGGER_ERROR("Invalid stage or desktops list", L_NARG);
        return 1;
    }

    LOGGER_DEBUG("Rendering current desktop (index %u) on stage %u",
            stage->desktop_cur, stage->id);

    /* Find the current desktop */
    desktop_node = cdlist_head(stage->desktops);
    if (desktop_node == NULL) {
        LOGGER_ERROR("Stage has no desktops", L_NARG);
        return 1;
    }

    /* Iterate to the current desktop index */
    for (uint32_t counter = 0;
            counter < stage->desktop_cur;
            ++counter) {
        desktop_node = cdlist_next(desktop_node);
        if (desktop_node == NULL) {
            LOGGER_ERROR("Could not find desktop at index %u",
                    stage->desktop_cur);
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

    /* Render only this desktop.  It is always the stage's current
     * desktop here, so clients must be (re-)mapped. */
    if (desktop_render_full(desktop, true) != 0) {
        LOGGER_ERROR("Failed to render desktop '%s'", desktop->name);
        return 1;
    }

    /* Flush ONCE at the end */
    stage_render_flush(stage);

    return 0;

}


/* Render all desktops on a stage */
int stage_render_all_desktops(stage_td *stage)
{
    cdlist_item_td *desktop_node;
    cdlist_item_td *desktop_initial;
    desktop_td *cur;
    uint32_t rendered_count = 0;

    if (stage == NULL || stage->desktops == NULL) {
        LOGGER_ERROR("Invalid stage or desktops list", L_NARG);
        return 1;
    }

    if (stage->desktop_count == 0) {
        LOGGER_WARNING("Stage has no desktops to render", L_NARG);
        return 0;
    }

    LOGGER_DEBUG("Fully rendering all %u desktops on stage %u",
            stage->desktop_count, stage->id);

    /* Get the first desktop */
    desktop_node = cdlist_head(stage->desktops);
    if (desktop_node == NULL) {
        LOGGER_ERROR("Stage desktops list is empty", L_NARG);
        return 1;
    }

    desktop_initial = desktop_node;

    /* Iterate through all desktops (circular list) */
    do {
        desktop_td *const desktop =
            (desktop_td *) cdlist_data(desktop_node);

        if (desktop == NULL) {
            LOGGER_WARNING("Desktop in list at position %u is null",
                    rendered_count);
            desktop_node = cdlist_next(desktop_node);
            rendered_count++;
            continue;
        }

        /* Checking is cheap and happens for every desktop regardless of
         * outcome, so this logs unconditionally; only the work inside
         * the 'is_outdated' branch below is actually expensive, and
         * 'desktop_render_full' logs its specifics once that runs. */
        LOGGER_DEBUG("Assessing whether desktop %u ('%s') needs" \
                " rendering", rendered_count,
                (desktop->name[0] != '\0') ? desktop->name : "unnamed");

        /* Only render if outdated */
        /* Pass wether this is the stage's currently displayed desktop
         * so that 'desktop_render_full()' never (re-)maps clients that
         * belong to a desktop the user is not currently looking at.
         * See 'desktop_render_clients' if that's what you really want. */
        if (desktop->is_outdated) {
            if (desktop_render_full(desktop,
                        rendered_count == stage->desktop_cur) != 0) {
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
    } while (rendered_count < stage->desktop_count &&
             desktop_node != NULL &&
             desktop_node != desktop_initial);

    LOGGER_DEBUG("Rendered %u desktops on stage %u",
            rendered_count, stage->id);

    /* Re-apply the current desktop's background last so that it is the
     * one visible on the root window.  Earlier desktops in the list
     * would otherwise overwrite it. */
    cur = stage_desktop_get(stage, stage->desktop_cur);
    if (cur != NULL) {
        if (render_desktop_background_render(cur) != 0) {
            LOGGER_ERROR("Failed to re-apply background for current" \
                    " desktop '%s'", cur->name);
        }
    }

    /* FLUSH ONCE at the end, not per-desktop */
    stage_render_flush(stage);
    wm_validate_stage(stage);

    return 0;
}


/* Mark current desktop outdated and repaint the stage */
void stage_render_current_desktop_repaint(stage_td *stage)
{
    desktop_td *cur;

    if (stage == NULL) {
        return;
    }

    cur = stage_desktop_get(stage, stage->desktop_cur);
    desktop_mark_outdated(cur);

    (void) stage_render_all_desktops(stage);
}


/* Flush rendering operations */
void stage_render_flush(stage_td *stage)
{
    if (stage == NULL || xcb_connection_get() == NULL) {
        LOGGER_ERROR("Invalid stage or connection for flushing",
                L_NARG);
        return;
    }

    LOGGER_DEBUG("Flushing stage %u to X server", stage->id);
    xcb_flush(xcb_connection_get());
}
