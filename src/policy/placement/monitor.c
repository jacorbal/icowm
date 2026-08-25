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
#include <defs/placement.h>
#include <policy/placement/score.h>
#include <policy/placement/window.h>
#include <policy/placement/monitor.h>


/**
 * @brief Resolve the monitor a placement decision should target
 *
 * Under @c CONFIG_PLACEMENT_MONITOR_PRIMARY, always returns
 * @p surface's primary monitor.
 * Under @c CONFIG_PLACEMENT_MONITOR_POINTER (the default), queries the
 * pointer and returns whichever monitor it is currently over, or
 * a degenerate (zero-area) geometry if the query fails; passing that on
 * to @a placement_clip_to_monitor is safe, since its intersection
 * against
 * a zero-area rectangle is always empty, which is exactly what makes it
 * leave its inputs unclipped.
 *
 * @param wm             Window manager state, for the pointer query
 * @param surface        Surface to resolve a monitor on
 * @param monitor_policy Which strategy to resolve with
 *
 * @return The resolved monitor's geometry
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors on
 *       @p surface
 */
static monitor_td s_reference_monitor(const wm_td *wm,
        surface_td *surface,
        enum config_placement_monitor_e monitor_policy)
{
    xcb_query_pointer_cookie_t cookie;
    xcb_query_pointer_reply_t *reply;
    monitor_td result = {.x = 0, .y = 0, .w = 0u, .h = 0u};

    if (monitor_policy == CONFIG_PLACEMENT_MONITOR_PRIMARY) {
        return surface_primary_monitor(surface);
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


/**
 * @brief Clip a workarea rectangle down to whichever physical monitor
 *        it overlaps, on a surface with more than one
 *
 * Falls back to leaving @p out_wa and @p out_screen unclipped (copies
 * of @p wa and @p screen) on a single-monitor surface, or when the
 * intersection against @p monitor is empty (e.g., a monitor entirely
 * covered by a strut): in either case the caller's unclipped rectangle
 * is already the right answer, not an error.
 *
 * @param surface    Surface the clip is against
 * @param wa         Workarea rectangle to clip
 * @param screen     Screen dimensions to clip alongside @p wa
 * @param monitor    Physical monitor to clip against
 * @param out_wa     Receives the clipped workarea
 * @param out_screen Receives the clipped screen dimensions
 *
 * @note Complexity: @e O(1)
 */
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


/**
 * @brief Resolve the monitor a placement decision should target,
 *        preferring a related client's monitor over the configured
 *        policy when one is found
 *
 * A dialog should appear next to the window it belongs with, and
 * a fresh window from an application already running elsewhere should
 * appear next to that application, not wherever the pointer or the
 * primary monitor happens to be instead: checked in order, a specific
 * transient parent first (or, for a client transient for the whole
 * group per ICCCM §4.1.2.6, the resolved anchor, then any
 * currently-mapped sibling sharing the same group leader on this same
 * desktop.  Falls through to @a s_reference_monitor unchanged whenever
 * neither search finds a candidate, or the candidate found resolves to
 * a degenerate (zero-area) monitor.
 *
 * @param wm             Window manager state, for the pointer query
 *                       @a s_reference_monitor falls back to
 * @param surface        Surface to resolve a monitor on
 * @param client         Client being placed, or @c NULL to skip both
 *                       searches and go straight to the configured
 *                       policy
 * @param monitor_policy Fallback strategy when no related client is
 *                       found
 *
 * @return The resolved monitor's geometry
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       this desktop
 *
 * @see @a client_group_transient_anchor in @c cmds/client/transient.c
 */
monitor_td placement_reference_monitor(const wm_td *wm,
        surface_td *surface, const client_td *client,
        enum config_placement_monitor_e monitor_policy)
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
                            sibling->properties.state !=
                                (uint16_t) CLIENT_STATE_ICONIFIED) {
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

    return s_reference_monitor(wm, surface, monitor_policy);
}


/**
 * @brief Resolve the workarea and monitor-clipped bounds a placement
 *        calculation needs
 *
 * Shared by @a place_window_apply and @a place_window_apply_cascade so
 * both compute the exact same workarea and monitor bounds for a given
 * client.
 *
 * @param wm         Window manager instance
 * @param surface    Surface the client lives on
 * @param client     Client being placed
 * @param out_wa     Resolved workarea, unclipped to any single
 *                   monitor
 * @param out_mon_wa Workarea, clipped to the reference monitor
 * @param out_mon_sz Screen dimensions, clipped to the reference
 *                   monitor
 *
 * @note Complexity: @e O(1)
 */
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
                    wm_config(wm)->base.windows.monitor_policy),
            out_mon_wa, out_mon_sz);
}
