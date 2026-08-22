/**
 * @file types/direction.h
 *
 * @brief A single, shared compass-direction enumeration
 *
 * @defgroup types Generic reusable types
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef TYPE_DIRECTION_H
#define TYPE_DIRECTION_H


/**
 * @brief One of the four compass directions
 *
 * Shared by every subsystem that steps to a neighboring desktop or
 * monitor in a given direction, rather than each defining its own
 * private, differently-named copy of the same four values: @c
 * enact_client_send_to_desktop_north/@c _south/@c _east/@c _west
 * (enact.h), carrying the focused client to another desktop; the
 * desktop-cycle keybind's own equivalent, switching the view itself
 * without moving any client along; and a directional monitor-move,
 * replacing the flat array-order @c next/@c prev @c ccmd_client_
 * move_to_next_monitor/@c _prev_monitor still use today (cmds/
 * client/geom.c), unlike a desktop, a monitor's own real, physical
 * position already makes "the one to the north" a meaningful
 * question with no configured layout needed to answer it at all.
 */
enum compass_direction_e {
    COMPASS_NORTH,
    COMPASS_SOUTH,
    COMPASS_EAST,
    COMPASS_WEST
};


#endif  /* ! TYPE_DIRECTION_H */
