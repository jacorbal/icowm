/**
 * @file monitor.h
 *
 * @brief Physical monitor rectangle type
 *
 * @p monitor_td is deliberately its own type, not a @c typedef or reuse
 * of @p (struct geometry_s) (in @c types/pair.h).  The two happen to
 * hold the same four numbers, but a monitor and an arbitrary rectangle
 * (a window's geometry, a work area, a clipped intersection) are not
 * the same thing, and sharing one type between them lets either be
 * passed where the other was meant without the compiler ever noticing.
 * Keeping them distinct also means the many far more common consumers
 * of @p geometry_s (windows, work areas, menus, ...) never need to pull
 * in this header at all, and this header never needs to pull in
 * @c types/pair.h.
 *
 * @note The duplication of @p ({ x, y, w, h }) this creates is
 *       intentional
 *
 * @see @c types/pair.h's own file comment for the project's established
 *      stance on repeating a small struct shape for clarity instead of
 *      typedef'ing one shared one
 *
 * @defgroup monitor Physical monitor geometry
 * @ingroup surface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MONITOR_H
#define MONITOR_H


/* System includes */
#include <stdint.h>


/**
 * @brief One physical monitor's rectangle within its surface
 *
 * Always in the same coordinate space as the surface it belongs to:
 * @p x / @p y are the monitor's own top-left corner relative to that
 * surface's own top-left corner, not relative to the monitor itself.
 */
typedef struct {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
} monitor_td;


#endif  /* ! MONITOR_H */
