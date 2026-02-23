/**
 * @file render/desktop.h
 *
 * @brief Desktop rendering and drawing functions
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_DESKTOP_H
#define RENDER_DESKTOP_H

/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

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
 *
 * @todo Currently, this is a no-op as X11 background management is
 * typically handled by separate tools or the EWMH-compliant desktop
 * environment.
 */
int desktop_render_background(desktop_td *desktop);

/**
 * @brief Draw all clients on a desktop
 *
 * Iterates through all clients in the desktop's stacking list and
 * configures them to be visible (maps windows and sets geometry).
 *
 * @param desktop Pointer to the desktop to draw
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to draw clients
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int desktop_render_clients(desktop_td *desktop);

/**
 * @brief Full desktop render
 *
 * Clears the desktop and redraws everything: background and clients.
 * This is called when the desktop needs a complete refresh.
 *
 * @param desktop Pointer to the desktop to render
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to render desktop
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int desktop_render_full(desktop_td *desktop);

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
