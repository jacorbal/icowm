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

/* Type includes */
#include <types/handles.h>

/* Project includes */
#include <config.h>


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
 * Reads @p client->config's theme and a11y settings; a no-op if
 * @p client is @c NULL, and falls back to @c WM_TITLEBAR_DEFAULT_HEIGHT
 * undecorated when @p client->config is @c NULL.
 *
 * @param client Client to update
 */
void ci_set_decoration_defaults(client_td *client);

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
