/**
 * @file cmds/client/screen.h
 *
 * @brief Functions resolving a client's decoration target window and
 *        its current monitor
 *
 * @defgroup cmds Client, desktop, and stage commands
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_CCMD_SCREEN_H
#define CMDS_CCMD_SCREEN_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>

/* Project includes */
#include <monitor.h>


/**
 * @brief Return the frame window when decorated, otherwise the client
 *        window
 *
 * @param client Pointer to the client to inspect
 *
 * @return Frame window when available, client window otherwise;
 *         @c XCB_WINDOW_NONE if @p client is @c NULL
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client);

/**
 * @brief Find which monitor a client is currently on
 *
 * Resolves @p client's stage from the global @c wm singleton, then
 * finds whichever of that stage's monitors @p client's center point
 * currently falls on.
 *
 * @param client      Client to resolve a monitor for
 * @param out_stage   Receives the resolved stage (may be null)
 * @param out_monitor Receives the resolved monitor's raw geometry
 *                    (screen edges, not adjusted for panel/dock struts)
 *
 * @retval  true on success
 * @retval false if the client's stage could not be found; callers
 *               fall back to @c ccmd_screen_dim's raw screen size in
 *               that case
 *
 * @note Complexity: @e O(n), where @e n is the number of stages
 */
bool ccmd_client_monitor(client_td *client, stage_td **out_stage,
        monitor_td *out_monitor);

/**
 * @brief Retrieve the pixel dimensions of the client's current screen
 *
 * Either @p out_w or @p out_h may be null but not both.
 *
 * @param client Pointer to the client whose screen is queried
 * @param out_w  Destination for the screen width in pixels, or null
 * @param out_h  Destination for the screen height in pixels, or null
 *
 * @return @c true on success
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
bool ccmd_screen_dim(const client_td *client,
        uint16_t *restrict out_w, uint16_t *restrict out_h);


#endif  /* ! CMDS_CCMD_SCREEN_H */
