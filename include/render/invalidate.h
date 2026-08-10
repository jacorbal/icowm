/**
 * @file render/invalidate.h
 *
 * @brief Centralized redraw-invalidation helpers
 *
 * Provides thin inline helpers that mark a @c surface_td or
 * @c desktop_td as requiring a repaint on the next render cycle.
 * Centralizing the pattern here prevents ad-hoc @c is_outdated
 * assignments from spreading across unrelated modules.
 *
 * @ingroup render
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_INVALIDATE_H
#define RENDER_INVALIDATE_H


/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>


/**
 * @brief Mark a client as needing a full geometry configure and repaint
 *
 * Sets the per-client @p is_outdated flag so that the next
 * @c desktop_render_clients pass issues the heavyweight
 * @c xcb_configure_window, @c xcb_clear_area (with exposures), and
 * synthetic @c ConfigureNotify calls only for this client rather than
 * for every client on the desktop.  This avoids triggering spurious
 * redraws (and visible flicker) in other windows during a keyboard
 * resize or any operation that affects only a single client.
 *
 * @param c Client to invalidate, or @c NULL (no-op)
 *
 * @note Complexity: @e O(1)
 */
static inline void wm_invalidate_client(client_td *c)
{
    if (c != NULL) {
        c->is_outdated = true;
    }
}


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


#endif  /* ! RENDER_INVALIDATE_H */
