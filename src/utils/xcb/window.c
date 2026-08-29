/**
 * @file utils/xcb/window.c
 *
 * @brief What the window manager does to a window, implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/**
 * @brief Whether an operation on this window can be carried out
 *
 * @param window Window the caller named
 *
 * @return @c true when there is both a connection and a real window
 *
 * @note Complexity: @e O(1)
 */
static bool s_window_is_addressable(xcb_window_t window)
{
    return window != XCB_WINDOW_NONE && xcb_connection_get() != NULL;
}


/* Move a window, leaving its size alone */
void xcb_window_move(xcb_window_t window, int32_t x, int32_t y)
{
    if (!s_window_is_addressable(window)) {
        return;
    }

    xcb_configure_window(xcb_connection_get(), window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) { (uint32_t) x, (uint32_t) y });
}


/* Resize a window, leaving its position alone */
void xcb_window_resize(xcb_window_t window, uint32_t width,
        uint32_t height)
{
    if (!s_window_is_addressable(window) || width == 0u ||
            height == 0u) {
        return;
    }

    xcb_configure_window(xcb_connection_get(), window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) { width, height });
}


/* Move and resize a window at once */
void xcb_window_place(xcb_window_t window, int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    if (!s_window_is_addressable(window) || width == 0u ||
            height == 0u) {
        return;
    }

    xcb_configure_window(xcb_connection_get(), window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) x, (uint32_t) y, width, height });
}


/* Set the width of a window's own border */
void xcb_window_set_border(xcb_window_t window, uint32_t width)
{
    if (!s_window_is_addressable(window)) {
        return;
    }

    xcb_configure_window(xcb_connection_get(), window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH,
            (const uint32_t[]) { width });
}


/* Put a window above every other */
void xcb_window_raise(xcb_window_t window)
{
    if (!s_window_is_addressable(window)) {
        return;
    }

    xcb_configure_window(xcb_connection_get(), window,
            XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
}


/* Put a window below every other */
void xcb_window_lower(xcb_window_t window)
{
    if (!s_window_is_addressable(window)) {
        return;
    }

    xcb_configure_window(xcb_connection_get(), window,
            XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { XCB_STACK_MODE_BELOW });
}


/* Put a window directly above another */
void xcb_window_stack_above(xcb_window_t window, xcb_window_t sibling)
{
    if (!s_window_is_addressable(window) ||
            sibling == XCB_WINDOW_NONE) {
        return;
    }

    xcb_configure_window(xcb_connection_get(), window,
            XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { sibling, XCB_STACK_MODE_ABOVE });
}


/* Put a window directly below another */
void xcb_window_stack_below(xcb_window_t window, xcb_window_t sibling)
{
    if (!s_window_is_addressable(window) ||
            sibling == XCB_WINDOW_NONE) {
        return;
    }

    xcb_configure_window(xcb_connection_get(), window,
            XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { sibling, XCB_STACK_MODE_BELOW });
}


/* Show a window */
void xcb_window_show(xcb_window_t window)
{
    if (!s_window_is_addressable(window)) {
        return;
    }

    xcb_map_window(xcb_connection_get(), window);
}


/* Hide a window without destroying it */
void xcb_window_hide(xcb_window_t window)
{
    if (!s_window_is_addressable(window)) {
        return;
    }

    xcb_unmap_window(xcb_connection_get(), window);
}


/* Destroy a window */
void xcb_window_destroy(xcb_window_t window)
{
    if (!s_window_is_addressable(window)) {
        return;
    }

    xcb_destroy_window(xcb_connection_get(), window);
}


/* Give a window a new parent */
void xcb_window_reparent(xcb_window_t window, xcb_window_t parent,
        int16_t x, int16_t y)
{
    if (!s_window_is_addressable(window) ||
            parent == XCB_WINDOW_NONE) {
        return;
    }

    xcb_reparent_window(xcb_connection_get(), window, parent, x, y);
}
