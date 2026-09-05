/**
 * @file client/props.h
 *
 * @brief Client property block, and the readers for a window's
 *        identity strings
 *
 * The state, layer, flags, type, operation and focusing words a
 * client carries, all of them read through the predicates in
 * @c client/predicates.h rather than directly.  Those six are this
 * window manager's, not properties of any X window.
 *
 * Alongside them, the three readers that fetch what a window calls
 * itself: @c WM_NAME, @c _NET_WM_NAME and @c WM_CLASS.  Kept together
 * although two are ICCCM and one is EWMH, since they are one job
 * rather than two: @a client_init calls all three in turn to fill in
 * a client's name, class and instance.  The hints each specification
 * defines, which really are stored per protocol, live in
 * @c client/icccm.h and @c client/ewmh.h instead.
 *
 * @ingroup client
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CLIENT_PROPS_H
#define CLIENT_PROPS_H


/* System includes */
#include <stddef.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <client/state.h>

/**
 * @brief Window properties
 *
 * Encapsulates various properties of a client: state, layering
 * behavior, and any applicable flags.
 */
struct client_properties_s {
    uint16_t state;      /**< State (maximized, iconified,...) */
    uint16_t layer;      /**< Layer (above, normal, below) */
    uint16_t flags;      /**< Flags (hidden, pinned, focusable,...) */
    uint16_t type;       /**< Type (normal, notification...) */
    uint16_t operation;  /**< Operation (moving, resizing...) */
    uint16_t focusing;   /**< Focusing (focused, unfocused) */
};


/* Public interface */
/**
 * @brief Retrieve the @c WM_NAME property of a window
 *
 * @param connection Pointer to the XCB connection
 * @param window     Window ID to query
 * @param buffer     Destination buffer
 * @param buffer_sz  Size of destination buffer
 *
 * @return Length of name on success, 0 otherwise
 */
size_t client_props_get_wm_name(xcb_connection_t *connection,
        xcb_window_t window, char *buffer, size_t buffer_sz);

/**
 * @brief Retrieve the @c _NET_WM_NAME property of a window (UTF-8)
 *
 * @param ewmh      Pointer to the EWMH connection
 * @param window    Window ID to query
 * @param buffer    Destination buffer
 * @param buffer_sz Size of destination buffer
 *
 * @return Length of name on success, 0 otherwise
 */
size_t client_props_get_net_wm_name(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, char *buffer, size_t buffer_sz);

/**
 * @brief Retrieve the @c WM_CLASS property of a window
 *
 * @param connection Pointer to the XCB connection
 * @param window     Window ID to query
 * @param class_buf  Buffer for the class name
 * @param class_sz   Size of @p class_buf
 * @param inst_buf   Buffer for the instance name (may be null)
 * @param inst_sz    Size of @p inst_buf (ignored when null)
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 on failure
 */
int client_props_get_wm_class(xcb_connection_t *connection,
        xcb_window_t window,
        char *restrict class_buf, size_t class_sz,
        char *restrict inst_buf, size_t inst_sz);


#endif  /* ! CLIENT_PROPS_H */
