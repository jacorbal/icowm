/**
 * @file render/desktop.h
 *
 * @brief Desktop rendering and drawing functions
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_DESKTOP_H
#define RENDER_DESKTOP_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <desktop.h>


/**
 * @brief Draw the background of a desktop
 *
 * @param desktop Pointer to the desktop to draw
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to draw background
 *
 * @note Complexity: @e O(1)
 */
int desktop_render_background(desktop_td *desktop);

/**
 * @brief Draw all clients on a desktop
 *
 * Iterates through all clients in the desktop's stacking list and
 * configures their geometry.  Windows are only mapped (made visible)
 * when @p is_current is @c true; for a desktop that is not the one
 * currently displayed on its surface, only geometry/stacking is updated
 * so that a stale full-render pass (triggered by an unrelated
 * 'is_outdated' flag, e.g., after moving/resizing a client) cannot undo
 * an explicit 'surface_clients_hide()' and make a client reappear on
 * top of the desktop the user actually switched to.
 *
 * @param desktop    Pointer to the desktop to draw
 * @param is_current Whether @p desktop is the surface's currently
 *                   displayed desktop; when @c false, clients are not
 *                   (re-)mapped, only their geometry is updated
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to draw clients
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int desktop_render_clients(desktop_td *desktop, bool is_current);

/**
 * @brief Full desktop render
 *
 * Clears the desktop and redraws everything: background and clients.
 * This is called when the desktop needs a complete refresh.
 *
 * @param desktop    Pointer to the desktop to render
 * @param is_current Whether @p desktop is the surface's currently
 *                   displayed desktop; forwarded to
 *                   'desktop_render_clients()' so that clients on
 *                   a desktop that is not currently shown are never
 *                   (re-)mapped by this general refresh path
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to render desktop
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int desktop_render_full(desktop_td *desktop, bool is_current);

/**
 * @brief Flush drawing operations to the X server
 *
 * Sends all accumulated drawing commands to the X server to make the
 * changes visible on screen.
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(1)
 */
void desktop_render_flush(desktop_td *desktop);


#endif  /* ! RENDER_DESKTOP_H */
