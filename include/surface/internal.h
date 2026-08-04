/**
 * @file surface/internal.h
 *
 * @brief Private helpers shared across surface implementation modules
 *
 * Declares helper functions that are used by more than one of the
 * surface translation units (@c surface/surface.c,
 * @c surface/actions.c) but must not be exposed as part of the public
 * surface API declared in @c surface.h.
 *
 * @note This header is private to the surface subsystem and must not be
 *       included outside of @c src/surface/
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SURFACE_INTERNAL_H
#define SURFACE_INTERNAL_H


/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <surface.h>


/**
 * @brief Update cached visual and screen properties from an X screen
 *
 * Reads geometry, depth, visual, root window and colormap from
 * @p screen and stores the result in @p surface->properties.
 *
 * @param surface Target surface to update
 * @param screen  X screen from which to read the properties
 *
 * @note Implemented in @c surface/surface.c
 * @note Complexity: @e O(n), where @e n is the number of visuals on the
 *       screen
 */
void si_update_properties(surface_td *surface,
        xcb_screen_t *screen);


#endif  /* ! SURFACE_INTERNAL_H */
