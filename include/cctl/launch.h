/**
 * @file cctl/launch.h
 *
 * @brief Program launch dispatch
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CCTL_LAUNCH_H
#define CCTL_LAUNCH_H


/* Project includes */
#include <surface.h>


/* Public interface */
/**
 * @brief Dispatch a program-launch event on the active desktop
 *
 * Locates the currently active desktop for @p surface and enqueues
 * a launch event for @p prog.  Does nothing if either @p surface or
 * @p prog is @c NULL or empty.
 *
 * @param surface    Active surface (screen); may be null
 * @param prog       Program command string to launch; may be null
 * @param class_name Optional @c WM_CLASS override string; may be null
 *
 * @note Complexity: @e O(1)
 */
void cctl_launch_dispatch(surface_td *surface, const char *restrict prog,
        const char *restrict class_name);


#endif  /* ! CCTL_LAUNCH_H */
