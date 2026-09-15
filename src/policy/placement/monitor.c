/**
 * @file policy/placement/monitor.c
 *
 * @brief Resolving which monitor and workarea a client belongs to
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
#include <stdlib.h>     /* NULL, free */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/pair.h>

/* Command includes */
#include <cmds/client/transient.h>

/* Default initial values */
#include <defs/placement.h>

/* Surface includes */
#include <surface/desktop.h>
#include <surface/monitor.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <policy/placement/score.h>
#include <policy/placement/window.h>
#include <policy/placement/monitor.h>


/**
 * @brief Resolve the monitor a placement decision should target
 *
 * - Under @c CONFIG_PLACEMENT_MONITOR_PRIMARY, always returns
 *   @p surface's primary monitor.
 * - Under @c CONFIG_PLACEMENT_MONITOR_INDEX, returns @p monitor_index
 *   specifically (out of range falls back to monitor 0, logging
 *   a warning, the same as @c systray.monitor.index and @c rules.json's
 *   @c apply.monitor).
 * - Under @c CONFIG_PLACEMENT_MONITOR_ACTIVE, returns whichever monitor
 *   holds the current desktop's active client, falling through to the
 *   pointer-based resolution below when there is none or its monitor
 *   cannot be resolved.
 * - Under @c CONFIG_PLACEMENT_MONITOR_POINTER (the default), queries
 *   the pointer and returns whichever monitor it is currently over, or
 *   a degenerate (zero-area) geometry if the query fails; passing that
 *   on to @a placement_clip_to_monitor is safe, since its intersection
 *   against a zero-area rectangle is always empty, which is exactly
 *   what makes it leave its inputs unclipped.
 *
 * @param wm             Window manager state, for the pointer query
 * @param surface        Surface to resolve a monitor on
 * @param monitor_policy Which strategy to resolve with
 * @param monitor_index  Explicit monitor index, only consulted under
 *                       @c CONFIG_PLACEMENT_MONITOR_INDEX
 *
 * @return The resolved monitor's geometry
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors or
 *       clients on @p surface, whichever the resolved policy walks
 */
static monitor_td s_reference_monitor(const wm_td *wm,
        surface_td *surface,
        enum config_placement_monitor_e monitor_policy,
        uint32_t monitor_index)
{
    xcb_query_pointer_cookie_t cookie;
    xcb_query_pointer_reply_t *reply;
    monitor_td result = {.x = 0, .y = 0, .w = 0u, .h = 0u};

    if (monitor_policy == CONFIG_PLACEMENT_MONITOR_PRIMARY) {
        return surface_monitor_primary(surface);
    }

    if (monitor_policy == CONFIG_PLACEMENT_MONITOR_INDEX) {
        uint32_t idx = monitor_index;

        if (surface->monitor_count == 0u) {
            return result;
        }
        if (idx >= surface->monitor_count) {
            LOGGER_WARNING("Placement targets monitor %u, which does" \
                    " not exist on surface %u (%u monitor(s));" \
                    " falling back to monitor 0", monitor_index,
                    surface->id, surface->monitor_count);
            idx = 0u;
        }
        return surface->monitors[idx];
    }

    if (monitor_policy == CONFIG_PLACEMENT_MONITOR_ACTIVE) {
        desktop_td *desktop =
            surface_desktop_get(surface, surface->desktop_cur);

        if (desktop != NULL && desktop->client_active_id != 0u &&
                desktop->clients != NULL) {
            void *elem;

            ohtbl_foreach(desktop->clients, elem) {
                const client_td *const active = (client_td *) elem;

                if (active != NULL &&
                        active->id == desktop->client_active_id) {
                    monitor_td active_monitor =
                        surface_monitor_for_point(surface,
                                active->layout.geometry.cur.pos);

                    if (active_monitor.w > 0u && active_monitor.h > 0u) {
                        return active_monitor;
                    }
                    break;
                }
            }
        }
        /* No active client, or its monitor could not be resolved: falls
         * through to the pointer-based resolution below, the same
         * fallback CONFIG_PLACEMENT_MONITOR_POINTER itself uses */
    }

    cookie = xcb_query_pointer(wm_connection(wm), surface->screen->root);
    reply = xcb_query_pointer_reply(wm_connection(wm), cookie, NULL);
    if (reply != NULL) {
        result = surface_monitor_for_point(surface,
                (struct position_s) { reply->root_x, reply->root_y });
        free(reply);
    }

    return result;
}


/* Clip a workarea rectangle down to whichever physical monitor it
 * overlaps, on a surface with more than one */
void placement_clip_to_monitor(const surface_td *surface,
        const struct geometry_s *wa, const struct dimensions_s *screen,
        monitor_td monitor,
        struct geometry_s *out_wa, struct dimensions_s *out_screen)
{
    struct geometry_s clipped;

    *out_wa = *wa;
    *out_screen = *screen;

    if (surface == NULL || surface->monitor_count <= 1u) {
        return;
    }

    clipped = geom_intersect_rect(wa->pos.x, wa->pos.y,
            wa->dim.w, wa->dim.h,
            monitor.x, monitor.y,
            monitor.w, monitor.h);
    if (clipped.dim.w == 0u || clipped.dim.h == 0u) {
        return;
    }

    *out_wa = clipped;
    out_screen->w = (uint32_t) monitor.x + monitor.w;
    out_screen->h = (uint32_t) monitor.y + monitor.h;
}


/* Resolve the monitor a placement decision should target, preferring
 * a related client's monitor over the configured policy when one is
 * found */
monitor_td placement_reference_monitor(const wm_td *wm,
        surface_td *surface, const client_td *client,
        enum config_placement_monitor_e monitor_policy,
        uint32_t monitor_index)
{
    const client_td *anchor = NULL;

    if (client != NULL) {
        anchor = client->transient_parent;
        if (anchor == NULL && client->is_transient_for_group) {
            anchor = client_group_transient_anchor(client);
        }
    }

    if (anchor == NULL && client != NULL) {
        xcb_window_t leader = client_group_leader(client);

        if (leader != XCB_WINDOW_NONE) {
            desktop_td *desktop =
                surface_desktop_get(surface, surface->desktop_cur);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const sibling = (client_td *) elem;

                    if (sibling != NULL && sibling != client &&
                            client_group_leader(sibling) == leader &&
                            !client_is_iconified(sibling)) {
                        anchor = sibling;
                        break;
                    }
                }
            } /* ! if (!desktop) */
        } /* ! if (leader) */
    }

    if (anchor != NULL) {
        monitor_td result = surface_monitor_for_point(surface,
                anchor->layout.geometry.cur.pos);

        if (result.w > 0u && result.h > 0u) {
            return result;
        }
    }

    return s_reference_monitor(wm, surface, monitor_policy, monitor_index);
}


/* Resolve the workarea and monitor-clipped bounds a placement
 * calculation needs */
void placement_workarea(const wm_td *wm, surface_td *surface,
        const client_td *client,
        struct geometry_s *out_wa, struct geometry_s *out_mon_wa,
        struct dimensions_s *out_mon_sz)
{
    struct dimensions_s screen;
    const desktop_td *desktop;

    screen.w = surface->properties.dim.w;
    screen.h = surface->properties.dim.h;

    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop != NULL && desktop->workarea.dim.w > 0u &&
            desktop->workarea.dim.h > 0u) {
        *out_wa = desktop->workarea;
    } else {
        out_wa->pos.x = 0;
        out_wa->pos.y = 0;
        out_wa->dim = screen;
    }

    placement_clip_to_monitor(surface, out_wa, &screen,
            placement_reference_monitor(wm, surface, client,
                    wm_config(wm)->base.windows.monitor_policy,
                    wm_config(wm)->base.windows.monitor_index),
            out_mon_wa, out_mon_sz);
}
