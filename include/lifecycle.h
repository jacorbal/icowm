/**
 * @file lifecycle.h
 *
 * @brief Client and window lifecycle management
 *
 * Declares functions for the three lifecycle operations performed at
 * window-manager startup and during normal event processing: adopting
 * pre-existing windows, refreshing a client's name from the X server,
 * and dispatching program-launch events to the event queue.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef LIFECYCLE_H
#define LIFECYCLE_H


/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>


/* Public interface */
/**
 * @brief Adopt all pre-existing mapped windows at window manager
 *        startup
 *
 * Queries the window tree for each screen in @p wm and calls
 * @c client_manage on any already-mapped, non-override-redirect child.
 *
 * @param wm Pointer to the window manager singleton
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       pre-existing windows across all screens
 */
void lifecycle_scan_existing(wm_td *wm);

/**
 * @brief Dispatch a program-launch event on the active desktop
 *
 * Locates the currently active desktop for @p surface and enqueues
 * a launch event for @p prog.  Does nothing if either @p surface or
 * @p prog is @c NULL or empty.
 *
 * @param surface    Active surface (screen); may be null
 * @param prog       Program command string to launch; may be null
 * @param class_name Program class, may be null
 *
 * @note Complexity: @e O(1)
 */
void lifecycle_dispatch_launch(surface_td *surface, const char *prog,
        const char *class_name);

/**
 * @brief Build and enqueue a launch event for a desktop
 *
 * Creates the action-data and event structures for a desktop
 * command-launch action and adds them to the event queue.
 *
 * @param desktop Target desktop; must not be null
 * @param command Command string; must not be null or empty
 *
 * @return 0 on success, non-zero on allocation or queue error
 *
 * @note Complexity: @e O(1)
 */
int lifecycle_send_desktop_launch(desktop_td *desktop,
        const char *command);


#endif  /* ! LIFECYCLE_H */
