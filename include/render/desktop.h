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

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <desktop.h>


/* Public interface */
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

/**
 * @brief Draw decoration button squares on a titlebar window
 *
 * Renders six right-aligned button squares (Iconify, Hide, Shade,
 * Maximize, Fullscreen, Close) and one left-aligned button (Pin/Sticky)
 * as filled rectangles.  The fill color is taken from @p theme:
 * @c window.active.foreground_color when @p is_focused is @c true,
 * @c window.inactive.foreground_color otherwise.  The pin button uses
 * the active foreground when sticky, and the inactive foreground when
 * not sticky.
 *
 * @param connection  Active XCB connection
 * @param titlebar    XCB window identifier of the titlebar
 * @param frame_w     Width of the titlebar in pixels
 * @param frame_top   Height of the titlebar in pixels
 * @param is_focused  Whether the owning client is currently focused
 * @param is_sticky   Whether the owning client has the sticky flag set
 * @param theme       Pointer to the theme providing button colors
 *
 * @note Complexity: @e O(1)
 */
void desktop_draw_titlebar_buttons(xcb_connection_t *connection,
        xcb_window_t titlebar, uint16_t frame_w, uint16_t frame_top,
        bool is_focused, bool is_sticky,
        const struct config_theme_s *theme);


#endif  /* ! RENDER_DESKTOP_H */
