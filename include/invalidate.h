/**
 * @file invalidate.h
 *
* @brief Centralized redraw-invalidation helpers
 *
 * Provides thin inline helpers that mark a @c surface_td or
 * @c desktop_td as requiring a repaint on the next render cycle.
 * Centralizing the pattern here prevents ad-hoc @c is_outdated
 * assignments from spreading across unrelated modules.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INVALIDATE_H
#define INVALIDATE_H


/* Project includes */
#include <desktop.h>
#include <surface.h>


/**
 * @brief Mark a surface as needing a repaint
 *
 * @param s Surface to invalidate, or @c NULL (no-op)
 *
 * @note Complexity: @e O(1)
 */
static inline void wm_invalidate_surface(surface_td *s)
{
    if (s != NULL) {
        s->is_outdated = true;
    }
}


/**
 * @brief Mark a desktop as needing a repaint
 *
 * @param d Desktop to invalidate, or @c NULL (no-op)
 *
 * @note Complexity: @e O(1)
 */
static inline void wm_invalidate_desktop(desktop_td *d)
{
    if (d != NULL) {
        d->is_outdated = true;
    }
}


#endif  /* ! INVALIDATE_H */
