/**
 * @file enact/stage.h
 *
 * @brief Every action this window manager can carry out on a whole
 *        stage, one typed function per action
 *
 * Split out of @c enact.h, alongside @c enact/client.h and
 * @c enact/desktop.h, so a file that only needs stage actions does
 * not also pull in, and rebuild against, every client and desktop
 * action declared alongside it.
 *
 * Each @a enact_stage_* function below is the single place in the
 * whole project where its corresponding action actually happens.
 * A caller anywhere else (a keybinding handler, a menu callback, an
 * EWMH message handler, a rule) calls the matching @a enact_stage_*
 * function directly, with its typed parameters, instead of reaching
 * into @c cmds/stage.h itself.  Searching for an action's enum name
 * always leads back to exactly one function here.
 *
 * @see @c action.h
 * @see @c enact.h
 *
 * @defgroup enact_stage Stage action execution
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ENACT_STAGE_H
#define ENACT_STAGE_H


/* System includes */
#include <stdint.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Switch the stage to a specific desktop
 *
 * @param stage      Stage to switch
 * @param desktop_id Target desktop index
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_stage_desktop_switch(stage_td *stage,
        uint32_t desktop_id);

/**
 * @brief Switch the stage to the desktop north of the current
 *        one, in cyclic order
 *
 * @param stage Stage to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_stage_desktop_switch_north(stage_td *stage);

/**
 * @brief Switch the stage to the desktop south of the current
 *        one, in cyclic order
 *
 * @param stage Stage to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_stage_desktop_switch_south(stage_td *stage);

/**
 * @brief Switch the stage to the desktop east of the current
 *        one, in cyclic order
 *
 * @param stage Stage to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_stage_desktop_switch_east(stage_td *stage);

/**
 * @brief Switch the stage to the desktop west of the current
 *        one, in cyclic order
 *
 * @param stage Stage to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_stage_desktop_switch_west(stage_td *stage);

/**
 * @brief Move the stage's current desktop viewport a whole page
 *        north, clamped at the top of the pannable area
 *
 * The discrete counterpart to @a enact_stage_viewport_pan_north
 * below, which slides by @c viewport.pan-step pixels instead
 *
 * @param stage Stage whose viewport to move
 *
 * @note A no-op where there is no page north of the current one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_switch_north(stage_td *stage);

/**
 * @brief Move the stage's current desktop viewport a whole page
 *        south, clamped at the bottom of the pannable area
 *
 * @param stage Stage whose viewport to move
 *
 * @note A no-op where there is no page south of the current one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_switch_south(stage_td *stage);

/**
 * @brief Move the stage's current desktop viewport a whole page
 *        east, clamped at the right of the pannable area
 *
 * @param stage Stage whose viewport to move
 *
 * @note A no-op where there is no page east of the current one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_switch_east(stage_td *stage);

/**
 * @brief Move the stage's current desktop viewport a whole page
 *        west, clamped at the left of the pannable area
 *
 * @param stage Stage whose viewport to move
 *
 * @note A no-op where there is no page west of the current one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_switch_west(stage_td *stage);

/**
 * @brief Pan the stage's current desktop viewport one screen north,
 *        clamped at the top of the pannable area
 *
 * @param stage Stage to pan
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_pan_north(stage_td *stage);

/**
 * @brief Pan the stage's current desktop viewport one screen south,
 *        clamped at the bottom of the pannable area
 *
 * @param stage Stage to pan
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_pan_south(stage_td *stage);

/**
 * @brief Pan the stage's current desktop viewport one screen east,
 *        clamped at the right of the pannable area
 *
 * @param stage Stage to pan
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_pan_east(stage_td *stage);

/**
 * @brief Pan the stage's current desktop viewport one screen west,
 *        clamped at the left of the pannable area
 *
 * @param stage Stage to pan
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_pan_west(stage_td *stage);

/**
 * @brief Jump the stage's current desktop viewport straight to one
 *        of its configured pages, addressed by a single linear index
 *
 * @param stage Stage to reposition
 * @param page  Zero-based page index; see @a scmd_stage_
 *                viewport_goto (cmds/stage.h) for how it maps onto
 *                the configured viewport grid
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_stage_viewport_goto(stage_td *stage, uint32_t page);

/**
 * @brief Add a new, empty desktop to the end of the stage's
 *        desktop list
 *
 * @param stage Stage to add a desktop to
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_add(stage_td *stage);

/**
 * @brief Remove the stage's last desktop, moving any client
 *        still on it to the new last desktop first
 *
 * A no-op, silently, when only one desktop remains: see
 * @a stage_action_desktop_remove (in @c stage.h) for the exact
 * refusal conditions.
 *
 * @param stage Stage to remove the last desktop from
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop being removed
 */
void enact_stage_desktop_remove(stage_td *stage);

/**
 * @brief Toggle whether panel/tray struts are set aside when computing
 *        this stage's desktops' work areas
 *
 * @param stage Stage to toggle strutless-maximization mode on
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       @p stage
 */
void enact_stage_toggle_strutless_maximize(stage_td *stage);


#endif  /* ! ENACT_STAGE_H */
