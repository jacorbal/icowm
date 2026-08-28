/**
 * @file utils/xcb/window.h
 *
 * @brief What the window manager does to a window on the X server
 *
 * Named for what each operation means rather than for the request that
 * carries it.  Moving a window, raising it and giving it a border are
 * three different things to everything that asks for them, and one
 * @c ConfigureWindow with a different mask to the server; a caller
 * that has to know which bits go in that mask is a caller that knows
 * about X, whether or not that is any of its business.
 *
 * That is what this is for.  A command decides what should happen to a
 * window; how the server is told belongs here.  Fifty-odd
 * @c ConfigureWindow calls spread over twenty-six files each built
 * their own mask and value array, so any change to how the manager
 * talks to the server meant touching all of them.
 *
 * None of these flushes.  A request sits in XCB's own output buffer
 * until something sends it, and the caller is what knows whether more
 * are coming: flushing inside each operation would send half-finished
 * work down the socket one piece at a time.
 *
 * @defgroup xcbwindow Window operations
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_XCB_WINDOW_H
#define UTILS_XCB_WINDOW_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Move a window, leaving its size alone
 *
 * @param window Window to move
 * @param x      Where its left edge goes
 * @param y      Where its top edge goes
 *
 * @note No-op for @c XCB_WINDOW_NONE, so a caller need not check a
 *       window it may not have
 * @note Complexity: @e O(1)
 */
void xcb_window_move(xcb_window_t window, int32_t x, int32_t y);

/**
 * @brief Resize a window, leaving its position alone
 *
 * @param window Window to resize
 * @param width  Its new width
 * @param height Its new height
 *
 * @note No-op for @c XCB_WINDOW_NONE, or for a width or height of
 *       zero, which the server rejects outright
 * @note Complexity: @e O(1)
 */
void xcb_window_resize(xcb_window_t window, uint32_t width,
        uint32_t height);

/**
 * @brief Move and resize a window at once
 *
 * One request rather than two, which matters for more than the round
 * trip saved: a window moved and then resized is briefly at a size and
 * position it was never meant to hold, and a client watching its own
 * @c ConfigureNotify sees that intermediate state and may act on it.
 *
 * @param window Window to place
 * @param x      Where its left edge goes
 * @param y      Where its top edge goes
 * @param width  Its new width
 * @param height Its new height
 *
 * @note No-op for @c XCB_WINDOW_NONE, or for a width or height of
 *       zero
 * @note Complexity: @e O(1)
 */
void xcb_window_place(xcb_window_t window, int32_t x, int32_t y,
        uint32_t width, uint32_t height);

/**
 * @brief Set the width of a window's own border
 *
 * @param window Window to give a border to
 * @param width  Border width, in pixels; @c 0 removes it
 *
 * @note No-op for @c XCB_WINDOW_NONE
 * @note Complexity: @e O(1)
 */
void xcb_window_set_border(xcb_window_t window, uint32_t width);

/**
 * @brief Put a window above every other
 *
 * @param window Window to raise
 *
 * @note No-op for @c XCB_WINDOW_NONE
 * @note Complexity: @e O(1)
 */
void xcb_window_raise(xcb_window_t window);

/**
 * @brief Put a window below every other
 *
 * @param window Window to lower
 *
 * @note No-op for @c XCB_WINDOW_NONE
 * @note Complexity: @e O(1)
 */
void xcb_window_lower(xcb_window_t window);

/**
 * @brief Put a window directly above another
 *
 * Asked for against a named sibling rather than as an unqualified
 * raise, which claims the very top: restacking a row of windows one
 * after another with the latter would put each over everything else in
 * turn, seen as a flicker.
 *
 * @param window  Window to place
 * @param sibling Window it goes directly above
 *
 * @note No-op when either is @c XCB_WINDOW_NONE
 * @note Complexity: @e O(1)
 */
void xcb_window_stack_above(xcb_window_t window,
        xcb_window_t sibling);

/**
 * @brief Put a window directly below another
 *
 * @param window  Window to place
 * @param sibling Window it goes directly below
 *
 * @note No-op when either is @c XCB_WINDOW_NONE
 * @note Complexity: @e O(1)
 */
void xcb_window_stack_below(xcb_window_t window,
        xcb_window_t sibling);

/**
 * @brief Show a window
 *
 * @param window Window to map
 *
 * @note No-op for @c XCB_WINDOW_NONE
 * @note Complexity: @e O(1)
 */
void xcb_window_show(xcb_window_t window);

/**
 * @brief Hide a window without destroying it
 *
 * @param window Window to unmap
 *
 * @note No-op for @c XCB_WINDOW_NONE
 * @note Complexity: @e O(1)
 */
void xcb_window_hide(xcb_window_t window);


#endif /* !UTILS_XCB_WINDOW_H */
