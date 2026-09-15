/**
 * @file surface/viewport.h
 *
 * @brief Viewport size and panning-room queries for a surface
 *
 * Split out of @c surface.h, alongside its sibling @c surface
 * headers, so a file that only needs viewport queries does not also
 * pull in, and rebuild against, every other unrelated surface concern
 * (desktop, monitor, workarea, action, client) declared in the same
 * file.
 *
 * @see @p surface_s, in @c surface.h
 *
 * @defgroup surface_viewport Surface viewport queries
 * @ingroup surface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SURFACE_VIEWPORT_H
#define SURFACE_VIEWPORT_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <surface.h>


/**
 * @brief Read the configured viewport size for @p surface's screen
 *
 * A null @p surface, one with no @c config, or one whose @c id sits
 * past @c CONFIG_MAX_SCREENS reports the physical screen size back
 * (a 1x1 viewport), the same fallback the configuration loader itself
 * applies whenever a @c viewport object is absent or fails validation.
 *
 * @param surface     Surface to read the viewport size for; may be
 *                    @c NULL
 * @param columns_out Where the configured viewport width, in whole
 *                    screens, is written; never null
 * @param rows_out    Where the configured viewport height, in whole
 *                    screens, is written; never null
 *
 * @note Implemented in @c surface/viewport.c
 * @note Complexity: @e O(1)
 */
void surface_viewport_dims(const surface_td *surface,
        uint32_t *columns_out, uint32_t *rows_out);

/**
 * @brief Whether @p surface's configured viewport spans more than a
 *        single screen along either axis
 *
 * The one question every affordance for panning, and for the sticky
 * flag panning is what gives meaning to, is gated on: a plain 1x1
 * desktop can never be panned by any amount, in any direction, from
 * any origin, so a pan drag cannot start, and a sticky toggle has
 * nothing to hold a client still against.
 *
 * @param surface Surface whose configured viewport size to check; may
 *                be @c NULL
 *
 * @retval  true if some drag or keybind could still pan @p surface's
 *               viewport by at least one screen in some direction
 * @retval false if it is a plain @c {1,1} desktop
 *
 * @note Unlike @a scmd_surface_viewport_pan_available, this ignores
 *       where the viewport origin currently sits
 * @note Implemented in @c surface/viewport.c
 * @note Complexity: @e O(1)
 */
bool surface_viewport_has_room(const surface_td *surface);


#endif /* SURFACE_VIEWPORT_H */
