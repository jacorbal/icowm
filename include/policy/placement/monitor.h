/**
 * @file policy/placement/monitor.h
 *
 * @brief Which monitor a client belongs to, and what of it is free
 *
 * Every placement decision starts by asking these two questions.
 * Struts, panels and the system tray are what make a workarea
 * smaller than the monitor holding it.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_MONITOR_H
#define POLICY_PLACEMENT_MONITOR_H


/* System includes */
#include <stdint.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Local includes */
#include <defs/placement.h>


/**
 * @brief Narrow a workarea and screen size to one monitor
 *
 * @param surface    Surface the monitor belongs to
 * @param wa         Workarea of the whole surface
 * @param screen     Dimensions of the whole surface
 * @param monitor    Monitor to narrow to
 * @param out_wa     Receives the monitor's workarea
 * @param out_screen Receives the monitor's dimensions
 *
 * @note Complexity: @e O(1)
 */
void placement_clip_to_monitor(const surface_td *surface,
        const struct geometry_s *wa, const struct dimensions_s *screen,
        monitor_td monitor,
        struct geometry_s *out_wa, struct dimensions_s *out_screen);


/**
 * @brief Resolve which monitor a client should be placed on
 *
 * @param surface        Surface to resolve within
 * @param wm             Window manager state
 * @param client         Client being placed
 * @param monitor_policy What the configuration asks for
 *
 * @return The monitor chosen
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors
 */
monitor_td placement_reference_monitor(const wm_td *wm,
        surface_td *surface, const client_td *client,
        enum config_placement_monitor_e monitor_policy);


/**
 * @brief Resolve the workarea a client is to be placed within
 *
 * @param wm          Window manager state
 * @param surface     Surface the client belongs to
 * @param client      Client being placed
 * @param out_wa      Receives the workarea to place within
 * @param out_mon_wa  Receives the chosen monitor's workarea
 * @param out_mon_sz  Receives the chosen monitor's dimensions
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors
 */
void placement_workarea(const wm_td *wm, surface_td *surface,
        const client_td *client,
        struct geometry_s *out_wa, struct geometry_s *out_mon_wa,
        struct dimensions_s *out_mon_sz);


#endif  /* ! POLICY_PLACEMENT_MONITOR_H */
