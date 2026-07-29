/**
 * @file render/surface.h
 *
 * @brief Surface rendering and drawing functions
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_SURFACE_H
#define RENDER_SURFACE_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <surface.h>


/**
 * @brief Render the current desktop on a surface
 *
 * Renders only the currently active desktop on this surface.
 *
 * @param surface Pointer to the surface
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to render
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int surface_render_current_desktop(surface_td *surface);

/**
 * @brief Render all desktops on a surface (full update)
 *
 * Performs a complete refresh of the surface, updating all desktops.
 * Use this when major changes have occurred.
 *
 * @param surface Pointer to the surface
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to render
 *
 * @note Complexity: @e O(n * m), where @e n is the number of desktops
 *       and @e m is the number of clients per desktop
 */
int surface_render_all_desktops(surface_td *surface);

/**
 * @brief Mark current desktop outdated and repaint the surface
 *
 * Marks the surface's currently selected desktop as outdated and then
 * triggers @c surface_render_all_desktops so the update is applied
 * immediately.
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n * m), where @e n is the number of desktops
 *       and @e m is the number of clients per desktop
 */
void surface_render_current_desktop_repaint(surface_td *surface);

/**
 * @brief Flush all rendering operations for the surface
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void surface_render_flush(surface_td *surface);


#endif  /* ! RENDER_SURFACE_H */
