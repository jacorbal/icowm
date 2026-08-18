/**
 * @file client/internal.h
 *
 * @brief Private helpers shared across client implementation modules
 *
 * Declares functions that were previously static inside the monolithic
 * @c client.c but are needed by more than one of its split translation
 * units (@c client_props.c, @c client_geom.c, @c client.c).
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CLIENT_INTERNAL_H
#define CLIENT_INTERNAL_H


/* System includes */
#include <stddef.h>     /* size_t */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Project includes */
#include <client.h>
#include <config.h>


/* X11 property readers (implemented in 'client/props.c') */
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
size_t ci_get_wm_name(xcb_connection_t *connection,
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
size_t ci_get_net_wm_name(xcb_ewmh_connection_t *ewmh,
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
int ci_get_wm_class(xcb_connection_t *connection,
        xcb_window_t window,
        char *restrict class_buf, size_t class_sz,
        char *restrict inst_buf, size_t inst_sz);

/* String/decoration helpers (implemented in 'client_geom.c') */
/**
 * @brief Allocate and zero all heap string buffers for a client
 *
 * @param client Client to populate
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 on allocation failure
 */
int ci_alloc_strings(client_td *client);

/**
 * @brief Apply decoration defaults from the loaded theme
 *
 * @param client Client to update
 * @param theme  Theme providing decoration settings
 */
void ci_set_decoration_defaults(client_td *client,
        struct config_theme_s *theme);

/**
 * @brief Create frame and titlebar windows for a decorated client
 *
 * @param client Client for which decorations are created
 *
 * @return Status of the operation
 * @retval     0 on success or when decoration creation is skipped
 * @retval non-0 on X11 failure
 */
int ci_create_decorations(client_td *client);


#endif  /* ! CLIENT_INTERNAL_H */
