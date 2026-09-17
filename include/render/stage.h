/**
 * @file render/stage.h
 *
 * @brief Stage rendering and drawing functions
 *
 * @ingroup render
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_STAGE_H
#define RENDER_STAGE_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <stage.h>


/**
 * @brief Render the current desktop on a stage
 *
 * Renders only the currently active desktop on this stage.
 *
 * @param stage Pointer to the stage
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to render
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int stage_render_current_desktop(stage_td *stage);

/**
 * @brief Render all desktops on a stage (full update)
 *
 * Performs a complete refresh of the stage, updating all desktops.
 * Use this when major changes have occurred.
 *
 * @param stage Pointer to the stage
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to render
 *
 * @note Complexity: @e O(n * m), where @e n is the number of desktops
 *       and @e m is the number of clients per desktop
 */
int stage_render_all_desktops(stage_td *stage);

/**
 * @brief Mark current desktop outdated and repaint the stage
 *
 * Marks the stage's currently selected desktop as outdated and then
 * triggers @a stage_render_all_desktops so the update is applied
 * immediately.
 *
 * @param stage Pointer to the stage
 *
 * @note Complexity: @e O(n * m), where @e n is the number of desktops
 *       and @e m is the number of clients per desktop
 */
void stage_render_current_desktop_repaint(stage_td *stage);

/**
 * @brief Flush all rendering operations for the stage
 *
 * @param stage Pointer to the stage
 *
 * @note Complexity: @e O(1)
 */
void stage_render_flush(stage_td *stage);


#endif  /* ! RENDER_STAGE_H */
