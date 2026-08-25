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
     * @note The @p old one is to save the position when the @p cur one
     *       is needed to be recovered later; as in saving the current
     *       geometry before maximizing, and restoring it with the
     *       @p old position and dimensions.
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
     * Set once, at 'client_init' ('client.c'), from this client's own
     * 'WM_NORMAL_HINTS' if it already declares 'win_gravity' there, or
     * from 'windows.gravity' in 'config.json' otherwise ('config.md'
     * §2.4): that config field is only ever a fallback for a client
     * that never states its own gravity, at any point in its life, not
     * a way to override one that does.  A later 'WM_NORMAL_HINTS'
     * update, in 'client_props_refresh_normal_hints'
     * ('client/props.c'), keeps this field in sync with whatever
     * 'win_gravity' that update
     * itself carries, per ICCCM's own "MUST honor" mandate; several
     * common toolkits (xterm's Xt shell, LibreOffice's VCL) only send
     * their real hints a moment after their first map, once fonts and
     * chrome are ready, which is when most real clients' own gravity
     * actually takes hold over the config default. */
    uint16_t gravity;
    struct sides_s frame_extents;   /**< [left, top, right, bottom] */
};


#endif  /* ! CLIENT_LAYOUT_H */
