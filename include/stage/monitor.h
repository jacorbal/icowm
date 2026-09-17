/**
 * @file stage/monitor.h
 *
 * @brief Physical monitor queries for a stage: refresh, locate by
 *        point, primary, and by compass direction
 *
 * Split out of @c stage.h, alongside its sibling @c stage
 * headers, so a file that only needs monitor queries does not also
 * pull in, and rebuild against, every other unrelated stage concern
 * (desktop, viewport, workarea, action, client) declared in the same
 * file.
 *
 * @see @p stage_s, in @c stage.h
 *
 * @defgroup stage_monitor Stage monitor queries
 * @ingroup stage
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef STAGE_MONITOR_H
#define STAGE_MONITOR_H


/* Type includes */
#include <types/direction.h>
#include <types/pair.h> /* position_s */

/* Project includes */
#include <stage.h>
#include <monitor.h>


/**
 * @brief Refresh the stage's list of physical monitors
 *
 * Queries the RandR 1.5 monitor list (@a xcb_randr_get_monitors) for
 * @p stage's root window and rebuilds @p stage->monitors from the
 * reply, clamped to @c WM_STAGE_MAX_MONITORS entries.  Falls back to
 * a single entry spanning @p stage->properties.dim (the whole
 * combined stage) when RandR is unavailable, the query fails, or the
 * reply lists no monitors, so @p stage->monitor_count is never left
 * at zero.
 *
 * @param stage Pointer to the stage whose monitor list to refresh
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors RandR
 *       reports
 */
void stage_monitor_refresh_all(stage_td *stage);

/**
 * @brief Find which of the stage's monitors contains a point
 *
 * @param stage Pointer to the stage to search
 * @param pos   Coordinate, in the stage's space
 *
 * @return The containing monitor, or, if the point falls outside every
 *         known monitor (e.g., a stale coordinate after a monitor was
 *         unplugged), the closest one by center-point distance.  Spans
 *         the whole stage if @p stage has no monitors of its own or
 *         @p stage is @c NULL
 *
 * @note Complexity: @e O(n), where @e n is @p stage->monitor_count
 */
monitor_td stage_monitor_for_point(const stage_td *stage,
        struct position_s pos);

/**
 * @brief Get the stage's primary monitor, if RandR flagged one
 *
 * @param stage Pointer to the stage to query
 *
 * @return The monitor RandR reports as primary.  Falls back to
 *         @p (stage->monitors[0]) if none was flagged as primary, and
 *         to a monitor spanning the whole stage if @p stage has no
 *         monitors of its own or @p stage is @c NULL
 *
 * @note Complexity: @e O(1)
 */
monitor_td stage_monitor_primary(const stage_td *stage);

/**
 * @brief Get the stage's monitor in a given compass direction from
 *        another one
 *
 * Unlike a desktop, a monitor already has a real, physical position
 * (@p current's @c x / @c y / @c w / @c h, as RandR reported it), so no
 * configured layout is needed to answer "which one is to the north" at
 * all.  Among every one of @p stage's monitors whose center genuinely
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
 * @param stage   Pointer to the stage structure
 * @param current The monitor to search from; need not itself be one
 *                  of @p stage's current monitors (an already-stale
 *                  caller-held copy is fine, since only its
 *                  @c x / @c y / @c w / @c h are read)
 * @param direction Compass direction to search in
 *
 * @return The neighboring monitor, or @p current itself, unchanged, if
 *         @p stage is @c NULL or no monitor lies in @p direction at
 *         all
 *
 * @note Complexity: @e O(n), where @e n is @p stage->monitor_count
 */
monitor_td stage_monitor_direction(const stage_td *stage,
        monitor_td current, enum compass_direction_e direction);


#endif /* STAGE_MONITOR_H */
