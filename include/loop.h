/**
 * @file loop.h
 *
 * @brief Main event loop, partial update, and full update
 *
 * Declares the three functions that run the window manager's main event
 * loop and maintain surface rendering state.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef LOOP_H
#define LOOP_H


/* Project includes */
#include <wm.h>


/* Public interface */
/**
 * @brief Run the main event loop until the window manageris stopped
 *
 * Installs signal handlers, allocates key symbols, grabs configured
 * bindings, scans pre-existing windows, performs an initial full
 * render, then blocks on @c poll() and drains XCB events until
 * @p wm->is_running becomes @c false.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1) per event; unbounded overall (loop runs
 *       until stopped)
 */
void loop_run(wm_td *wm);

/**
 * @brief Perform a partial (outdated-only) surface update
 *
 * Re-renders only the surfaces that have been marked as outdated.
 * Called on every iteration of the main event loop.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
void loop_update(wm_td *wm);

/**
 * @brief Force a full re-render of all surfaces
 *
 * Marks every surface as outdated and then delegates to @c loop_update.
 * Called once before entering the event loop so pre-existing windows
 * are drawn from scratch.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m is the number of desktops
 */
void loop_update_full(wm_td *wm);


#endif  /* ! LOOP_H */
