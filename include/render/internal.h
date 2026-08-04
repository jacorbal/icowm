/**
 * @file render/internal.h
 *
 * @brief Private helpers shared across render implementation modules
 *
 * Declares helper functions used by more than one render translation
 * unit (@c render/desktop.c, @c render/icon.c, @c render/surface.c)
 * that must not be exposed as part of the public render API.
 *
 * @note This header is private to the render subsystem and must not be
 *       included outside of @c src/render/
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_INTERNAL_H
#define RENDER_INTERNAL_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <client.h>
#include <desktop.h>


/**
 * @brief Render the icon window for a hidden (iconified) client
 *
 * Applies icon window attributes (background, border color and width,
 * stacking) and optionally draws a caption label.  Called from
 * @c desktop_render_clients for clients with @c CLIENT_FLAG_HIDDEN set.
 *
 * @param desktop    Desktop whose rendering context and theme are used
 * @param client     The iconified client to render
 * @param is_current @c true when @p desktop is the currently visible one
 *
 * @note No-op when @p client has no icon window or is not icon-mapped
 * @note Implemented in @c render/icon.c
 * @note Complexity: @e O(1)
 */
void ri_render_client_icon(desktop_td *desktop, client_td *client,
        bool is_current);


#endif  /* ! RENDER_INTERNAL_H */
