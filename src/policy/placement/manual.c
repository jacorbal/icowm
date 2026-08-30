/**
 * @file policy/placement/manual.c
 *
 * @brief Manual placement: the position the person picks themselves
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* Local includes */
#include <policy/placement/manual.h>


/* Pick where a window goes, asking the person to point at it */
bool place_window_manual(const wm_td *wm, surface_td *surface,
        client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y)
{
    (void) wm;
    (void) surface;
    (void) client;
    (void) out_x;
    (void) out_y;

    /* Asking the person where the window goes means holding the
     * pointer and waiting for a click, which cannot happen inside the
     * map handler without stopping the event loop for every other
     * client too.  What that wait should look like is a decision of
     * its own, so until it is made this answers that it chose nothing
     * and the caller falls back, exactly as it does for a pointer it
     * could not query. */
    return false;
}
