/**
 * @file render/outdate.h
 *
 * @brief Centralized redraw-outdating helpers
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

#ifndef RENDER_OUTDATE_H
#define RENDER_OUTDATE_H


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
 * @param c Client to mark outdated, or @c NULL (no-op)
 *
 * @note Complexity: @e O(1)
 */
static inline void wm_outdate_client(client_td *c)
{
    if (c != NULL) {
        c->is_outdated = true;
    }
}


/**
 * @brief Mark a surface as needing a repaint
 *
 * @param s Surface to mark outdated, or @c NULL (no-op)
 *
 * @note Complexity: @e O(1)
 */
static inline void wm_outdate_surface(surface_td *s)
{
    if (s != NULL) {
        s->is_outdated = true;
    }
}


/**
 * @brief Mark a desktop as needing a repaint
 *
 * @param d Desktop to mark outdated, or @c NULL (no-op)
 *
 * @note Complexity: @e O(1)
 */
static inline void wm_outdate_desktop(desktop_td *d)
{
    if (d != NULL) {
        d->is_outdated = true;
    }
}


#endif  /* ! RENDER_OUTDATE_H */
