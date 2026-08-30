/**
 * @file client/layout.h
 *
 * @brief Client geometry and decoration layout
 *
 * Where a client's frame is, how large it is, what its size hints
 * allow, and how much of the screen edge it reserves.
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

#ifndef CLIENT_LAYOUT_H
#define CLIENT_LAYOUT_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/client.h>

/**
 * @brief Window layout, position, dimensions and strut
 */
struct client_layout_s {
    /**
     * @brief Position and dimensions of the client
     *
     * @note @p old holds what @p cur is to be restored to later, as
     *       when the geometry is put aside before maximizing and
     *       given back on unmaximizing
     */
    struct {
        struct geometry_s cur;
        struct geometry_s old;
    } geometry;

    /**
     * @brief Area where the client exist on the screen, plus the area
     *        are marked off-bounds for client placement
     *
     * @note Traditional strut property will be @p strut_partial.sides
     *        when @p .start and @p .end are zero
     */
    struct strut_partial_s strut_partial;

    /**
     * @brief Window gravity
     *
     * Set once, at @a client_init (@c client.c), from this client's
     * @c WM_NORMAL_HINTS if it already declares @c win_gravity there,
     * or from @c windows.gravity in @c config.json otherwise (see
     * @c config.md §2.4).  That config field is only ever a fallback
     * for a client that never states its gravity, at any point in its
     * life, not a way to override one that does.
     *
     * A later @c WM_NORMAL_HINTS update, in
     * @a client_props_refresh_normal_hints (@c client/props.c), keeps
     * this field in sync with whatever @c win_gravity that update
     * itself carries, per ICCCM's "MUST honor" mandate; several common
     * toolkits (XTerm's Xt shell, LibreOffice's VCL) only send their
     * real hints a moment after their first map, once fonts and chrome
     * are ready, which is when most real clients' gravity actually
     * takes hold over the config default. */
    uint16_t gravity;
    struct sides_s frame_extents;   /**< [left, top, right, bottom] */
};


#endif  /* ! CLIENT_LAYOUT_H */
