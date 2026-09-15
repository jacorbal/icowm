/**
 * @file surface/monitor.h
 *
 * @brief Physical monitor queries for a surface: refresh, locate by
 *        point, primary, and by compass direction
 *
 * Split out of @c surface.h, alongside its sibling @c surface
 * headers, so a file that only needs monitor queries does not also
 * pull in, and rebuild against, every other unrelated surface concern
 * (desktop, viewport, workarea, action, client) declared in the same
 * file.
 *
 * @see @p surface_s, in @c surface.h
 *
 * @defgroup surface_monitor Surface monitor queries
 * @ingroup surface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SURFACE_MONITOR_H
#define SURFACE_MONITOR_H


/* Type includes */
#include <types/direction.h>
#include <types/pair.h> /* position_s */

/* Project includes */
#include <surface.h>
#include <monitor.h>


/**
 * @brief Refresh the surface's list of physical monitors
 *
 * Queries the RandR 1.5 monitor list (@a xcb_randr_get_monitors) for
 * @p surface's root window and rebuilds @p surface->monitors from the
 * reply, clamped to @c WM_SURFACE_MAX_MONITORS entries.  Falls back to
 * a single entry spanning @p surface->properties.dim (the whole
 * combined surface) when RandR is unavailable, the query fails, or the
 * reply lists no monitors, so @p surface->monitor_count is never left
 * at zero.
 *
 * @param surface Pointer to the surface whose monitor list to refresh
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors RandR
 *       reports
 */
void surface_monitor_refresh_all(surface_td *surface);

/**
 * @brief Find which of the surface's monitors contains a point
 *
 * @param surface Pointer to the surface to search
 * @param pos     Coordinate, in the surface's space
 *
 * @return The containing monitor, or, if the point falls outside every
 *         known monitor (e.g., a stale coordinate after a monitor was
 *         unplugged), the closest one by center-point distance.  Spans
 *         the whole surface if @p surface has no monitors of its own or
 *         @p surface is @c NULL
 *
 * @note Complexity: @e O(n), where @e n is @p surface->monitor_count
 */
monitor_td surface_monitor_for_point(const surface_td *surface,
        struct position_s pos);

/**
 * @brief Get the surface's primary monitor, if RandR flagged one
 *
 * @param surface Pointer to the surface to query
 *
 * @return The monitor RandR reports as primary.  Falls back to
 *         @p (surface->monitors[0]) if none was flagged as primary, and
 *         to a monitor spanning the whole surface if @p surface has no
 *         monitors of its own or @p surface is @c NULL
 *
 * @note Complexity: @e O(1)
 */
monitor_td surface_monitor_primary(const surface_td *surface);

/**
 * @brief Get the surface's monitor in a given compass direction from
 *        another one
 *
 * Unlike a desktop, a monitor already has a real, physical position
 * (@p current's @c x / @c y / @c w / @c h, as RandR reported it), so no
 * configured layout is needed to answer "which one is to the north" at
 * all.  Among every one of @p surface's monitors whose center genuinely
 * lies in @p direction from @p current's center, whichever one is
 * nearest by that same measure is the answer, matching what a user
 * looking at the arrangement would call it even when the monitors
 * involved differ in size or are not perfectly aligned to one another.
 *
 * Deliberately never wraps around to the farthest monitor the opposite
 * way when none lies in @p direction at all, unlike a desktop's
 * equivalent, which optionally does when @c desktops.wrap-at-bounds is
 * enabled: wrapping a definite, ordered list (a desktop's circular one)
 * has one obviously correct meaning, but wrapping a genuinely 2-D
 * physical arrangement does not (does "east, wrapped" mean the westmost
 * monitor overall, or only the westmost one still on the same row?), so
 * no attempt is made to invent one here; a monitor has no equivalent of
 * @c wrap-at-bounds to make that choice configurable in the first
 * place, for the same reason.
 *
 * @param surface   Pointer to the surface structure
 * @param current   The monitor to search from; need not itself be one
 *                  of @p surface's current monitors (an already-stale
 *                  caller-held copy is fine, since only its
 *                  @c x / @c y / @c w / @c h are read)
 * @param direction Compass direction to search in
 *
 * @return The neighboring monitor, or @p current itself, unchanged, if
 *         @p surface is @c NULL or no monitor lies in @p direction at
 *         all
 *
 * @note Complexity: @e O(n), where @e n is @p surface->monitor_count
 */
monitor_td surface_monitor_direction(const surface_td *surface,
        monitor_td current, enum compass_direction_e direction);


#endif /* SURFACE_MONITOR_H */
