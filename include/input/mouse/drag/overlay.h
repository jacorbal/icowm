/**
 * @file input/mouse/drag/overlay.h
 *
 * @brief Centered feedback overlay window shown during a drag
 *
 * @ingroup input_mouse
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_DRAG_OVERLAY_H
#define INPUT_MOUSE_DRAG_OVERLAY_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Types includes */
#include <types/pair.h>


/** Horizontal padding, in pixels, inside the drag-position overlay */
#define WM_DRAG_OVERLAY_PAD_X (8u)

/** Height, in pixels, of the drag-position overlay window */
#define WM_DRAG_OVERLAY_HEIGHT (22u)

/** Minimum width, in pixels, of the drag-position overlay window */
#define WM_DRAG_OVERLAY_MIN_WIDTH (40u)


/**
 * @brief Destroy and reset the active drag overlay window
 *
 * Destroys the overlay window if it exists and clears the associated
 * overlay state.
 *
 * @param connection XCB connection used to destroy the overlay window
 *
 * @note Complexity: @e O(1)
 */
void drag_overlay_hide(xcb_connection_t *connection);

/**
 * @brief Show or reposition the drag overlay window
 *
 * Updates the overlay text and mode, computes a centered overlay
 * rectangle for the given target geometry, and either creates the
 * overlay window or moves and resizes the existing one before
 * repainting it.
 *
 * @param connection XCB connection used to manage the overlay window
 * @param is_icon    Whether the overlay should use the active icon
 *                   theme
 * @param target Target rectangle to center the overlay within
 * @param text   Overlay text to display
 *
 * @note Complexity: @e O(1)
 */
void drag_overlay_show(xcb_connection_t *connection,
        bool is_icon,
        struct geometry_s target,
        const char *text);

/**
 * @brief Query whether a window is the active drag overlay window
 *
 * @param window X window identifier to compare
 *
 * @return @c true when @p window is the drag overlay window
 *
 * @note Complexity: @e O(1)
 */
bool drag_is_overlay_window(xcb_window_t window);

/**
 * @brief Repaint the active drag overlay window
 *
 * Redraws the current geometry text into the overlay window created for
 * interactive move/resize feedback.  Called both when the overlay's
 * content changes mid-drag, and from @c handler_expose
 * (@c handler/expose.c) when the overlay window itself receives an
 * @c Expose event (e.g., after another window that had been covering it
 * is removed or moved away).
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void drag_overlay_repaint(xcb_connection_t *connection);


#endif  /* ! INPUT_MOUSE_DRAG_OVERLAY_H */
