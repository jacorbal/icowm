/**
 * @file stage/monitors.c
 *
 * @brief RandR physical-monitor detection and lookup for a stage
 *
 * One of the files @c stage/ is made of; see
 * @c stage.c's comment for why.
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
#include <stdint.h>
#include <stdlib.h>     /* free */
#include <strings.h>    /* strcasecmp */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/randr.h>

/* Utils includes */
#include <utils/xcb/atom.h>
#include <utils/xcb/reply.h>
#include <utils/xcb/connection.h>

/* Type includes */
#include <types/direction.h>

/* Project includes */
#include <config.h>
#include <logger.h>

/* Local includes */
#include <stage.h>
#include <stage/monitor.h>


/**
 * @brief Whole-stage fallback for the monitor list
 *
 * Fills @p stage->monitors with a single entry spanning
 * @p stage->properties.dim, used whenever RandR cannot supply a real
 * monitor list.
 *
 * @param stage Pointer to the stage to fall back
 */
static void s_stage_monitor_fallback(stage_td *stage)
{
    stage->monitors[0].x = 0;
    stage->monitors[0].y = 0;
    stage->monitors[0].w = stage->properties.dim.w;
    stage->monitors[0].h = stage->properties.dim.h;
    stage->monitor_count = 1u;
    stage->primary_monitor_index = 0u;
}


/**
 * @brief Whether a RandR output should be recognized as a monitor
 *        IcoWM manages windows on
 *
 * Looks up @p name among the configured RandR output profiles (see
 * @c config_randr_s, loaded from @c randr.json); a matching profile
 * that is explicitly disabled excludes that output from
 * @c stage->monitors entirely, as if it were not connected at all,
 * letting a user with more physical outputs than they want IcoWM
 * to place windows on limit it to specific ones by name (e.g., an
 * always-connected "HDMI-1" projector meant only for mirroring, never
 * for managing windows).  With RandR profile management off
 * altogether, or with no profile configured for this particular
 * output name, every detected output is used, unchanged from before
 * this existed.
 *
 * @param config Active configuration, or @c NULL to always allow
 * @param name   Resolved RandR output name (e.g., @c "HDMI-1")
 *
 * @return @c false only when a matching profile exists and is
 *         explicitly disabled; @c true otherwise
 *
 * @note Complexity: @e O(n), where @e n is the number of configured
 *       output profiles
 */
static bool s_stage_output_is_used(const config_td *config,
        const char *name)
{
    if (config == NULL || !config->randr.is_enabled) {
        return true;
    }

    for (uint32_t i = 0u; i < config->randr.output_count; ++i) {
        if (strcasecmp(config->randr.outputs[i].name, name) == 0) {
            return config->randr.outputs[i].is_enabled;
        }
    }

    return true;
}


/* Refresh the stage's list of physical monitors */
void stage_monitor_refresh_all(stage_td *stage)
{
    xcb_randr_get_monitors_cookie_t cookie;
    xcb_randr_get_monitors_reply_t *reply;
    xcb_generic_error_t *error = NULL;
    xcb_randr_monitor_info_iterator_t it;

    if (stage == NULL || xcb_connection_get() == NULL ||
            stage->screen == NULL) {
        return;
    }

    cookie = xcb_randr_get_monitors(xcb_connection_get(),
            stage->screen->root, 1u);
    reply = xcb_randr_get_monitors_reply(xcb_connection_get(),
            cookie, &error);
    if (reply == NULL) {
        xcb_reply_log_error(error, "the XRandR monitor list");
        LOGGER_NOTICE("Failed to query RandR monitors for stage" \
                " %u; treating it as one monitor", stage->id);
        s_stage_monitor_fallback(stage);
        return;
    }

    stage->monitor_count = 0u;
    stage->primary_monitor_index = 0u;
    it = xcb_randr_get_monitors_monitors_iterator(reply);
    while (it.rem > 0 &&
            stage->monitor_count < WM_STAGE_MAX_MONITORS) {
        xcb_randr_monitor_info_t *const info = it.data;
        char output_name[CONFIG_RANDR_OUTPUT_NAME_LENGTH];
        monitor_td *slot;
        bool name_resolved;

        name_resolved = atom_name(xcb_connection_get(), info->name,
                output_name, sizeof(output_name));
        if (name_resolved && !s_stage_output_is_used(stage->config,
                    output_name)) {
            LOGGER_DEBUG("Excluding RandR output '%s' from stage" \
                    " %u: disabled by its configured profile",
                    output_name, stage->id);
            xcb_randr_monitor_info_next(&it);
            continue;
        }

        slot = &stage->monitors[stage->monitor_count];
        slot->x = info->x;
        slot->y = info->y;
        slot->w = info->width;
        slot->h = info->height;
        if (info->primary) {
            stage->primary_monitor_index = stage->monitor_count;
        }
        ++stage->monitor_count;

        xcb_randr_monitor_info_next(&it);
    }
    free(reply);

    if (stage->monitor_count == 0u) {
        LOGGER_NOTICE("RandR reported no monitors for stage %u;" \
                " treating it as one monitor", stage->id);
        s_stage_monitor_fallback(stage);
        return;
    }

    LOGGER_DEBUG("Stage %u has %u monitor(s)",
            stage->id, stage->monitor_count);
}


/* Find which of the stage's monitors contains a point */
monitor_td stage_monitor_for_point(const stage_td *stage,
        struct position_s pos)
{
    monitor_td fallback = {.x = 0, .y = 0, .w = 0u, .h = 0u};
    uint32_t closest = 0u;
    int64_t closest_dist = -1;

    if (stage == NULL) {
        return fallback;
    }
    if (stage->monitor_count == 0u) {
        fallback.w = stage->properties.dim.w;
        fallback.h = stage->properties.dim.h;
        return fallback;
    }

    for (uint32_t i = 0; i < stage->monitor_count; ++i) {
        const monitor_td *m = &stage->monitors[i];
        int32_t mright = m->x + (int32_t) m->w;
        int32_t mbottom = m->y + (int32_t) m->h;
        int64_t cx;
        int64_t cy;
        int64_t dist;

        if (pos.x >= m->x && pos.x < mright &&
                pos.y >= m->y && pos.y < mbottom) {
            return *m;
        }

        cx = m->x + (int32_t) (m->w / 2u) - pos.x;
        cy = m->y + (int32_t) (m->h / 2u) - pos.y;
        dist = cx * cx + cy * cy;
        if (closest_dist < 0 || dist < closest_dist) {
            closest_dist = dist;
            closest = i;
        }
    }

    return stage->monitors[closest];
}


/* Get the stage's primary monitor, if RandR flagged one */
monitor_td stage_monitor_primary(const stage_td *stage)
{
    monitor_td fallback = {.x = 0, .y = 0, .w = 0u, .h = 0u};

    if (stage == NULL) {
        return fallback;
    }
    if (stage->monitor_count == 0u) {
        fallback.w = stage->properties.dim.w;
        fallback.h = stage->properties.dim.h;
        return fallback;
    }

    return stage->monitors[stage->primary_monitor_index];
}


/* Find the stage's monitor in a given compass direction from
 * another one */
monitor_td stage_monitor_direction(const stage_td *stage,
        monitor_td current, enum compass_direction_e direction)
{
    int64_t cur_cx;
    int64_t cur_cy;
    int64_t best_dist = -1;
    monitor_td best = current;

    if (stage == NULL) {
        return current;
    }

    cur_cx = (int64_t) current.x + (int64_t) (current.w / 2u);
    cur_cy = (int64_t) current.y + (int64_t) (current.h / 2u);

    for (uint32_t i = 0u; i < stage->monitor_count; ++i) {
        const monitor_td *const m = &stage->monitors[i];
        int64_t mcx;
        int64_t mcy;
        int64_t dx;
        int64_t dy;
        int64_t dist;

        if (m->x == current.x && m->y == current.y) {
            /* This is 'current' itself, found again by coordinate
             * (see 'ccmd_client_move_to_monitor_north', cmds/client/
             * geom.c, for the same identify-by-coordinate approach,
             * since a 'monitor_td' carries no ID or index of its own
             * to compare against instead); never its candidate
             * neighbor. */
            continue;
        }

        mcx = (int64_t) m->x + (int64_t) (m->w / 2u);
        mcy = (int64_t) m->y + (int64_t) (m->h / 2u);
        dx = mcx - cur_cx;
        dy = mcy - cur_cy;

        switch (direction) {
        case COMPASS_NORTH:
            if (dy >= 0) { continue; }
            break;
        case COMPASS_SOUTH:
            if (dy <= 0) { continue; }
            break;
        case COMPASS_EAST:
            if (dx <= 0) { continue; }
            break;
        case COMPASS_WEST:
            if (dx >= 0) { continue; }
            break;
        }

        /* Center-to-center squared distance, the same "closest wins"
         * principle 'stage_monitor_for_point' above already uses
         * for its off-monitor fallback: among every monitor that
         * genuinely lies in the requested direction at all (the
         * switch above), whichever one is nearest by that measure is
         * the one a user would call "the monitor to the north"
         * (or south, east, west), even when the monitors involved
         * are not all the same size or perfectly aligned. */
        dist = dx * dx + dy * dy;
        if (best_dist < 0 || dist < best_dist) {
            best_dist = dist;
            best = *m;
        }
    }

    return best;
}
