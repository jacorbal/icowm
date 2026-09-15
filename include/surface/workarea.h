/**
 * @file surface/workarea.h
 *
 * @brief Work area recomputation for a surface's desktops
 *
 * Split out of @c surface.h, alongside its sibling @c surface
 * headers, so a file that only needs work area recomputation does
 * not also pull in, and rebuild against, every other unrelated
 * surface concern (desktop, viewport, monitor, action, client)
 * declared in the same file.
 *
 * @see @p surface_s, in @c surface.h
 *
 * @defgroup surface_workarea Surface work area recomputation
 * @ingroup surface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SURFACE_WORKAREA_H
#define SURFACE_WORKAREA_H


/* Project includes */
#include <surface.h>


/**
 * @brief Recompute the work area for every desktop on a surface
 *
 * Iterates over all desktops belonging to the given surface and updates
 * each desktop work area using the surface's current width and height.
 * This keeps per-desktop usable geometry in sync after changes such as
 * screen resizing or strut updates.
 *
 * @param surface Pointer to the surface whose desktops will be
 *                refreshed
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
void surface_workarea_refresh_all(surface_td *surface);


#endif /* SURFACE_WORKAREA_H */
