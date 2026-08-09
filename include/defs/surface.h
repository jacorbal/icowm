/**
 * @file defs/surface.h
 *
 * @brief Capacity limits for a surface's own RandR monitor list
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_SURFACE_H
#define DEFS_SURFACE_H


/** Maximum number of physical monitors tracked per surface
 *
 * Smaller under @c LOWMEM (see @c defs/lowmem.h): a
 * target that build is meant for is unlikely to drive many monitors
 * at once regardless. */
#ifdef LOWMEM
#define WM_SURFACE_MAX_MONITORS (2)
#else
#define WM_SURFACE_MAX_MONITORS (8)
#endif


#endif  /* ! DEFS_SURFACE_H */
