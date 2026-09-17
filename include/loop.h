/**
 * @file loop.h
 *
 * @brief Main event loop, partial update, and full update
 *
 * Declares the functions that run the window manager's main event loop
 * and maintain stage rendering state.
 *
 * IcoWM's operation flow, one turn of the event loop:
 *
 * @code{.unparsed}
 *
 *   +-----------+     +-------------+     +----------------+
 *   |  loop/    | --> |  handler/   | --> |  policy/       |
 *   |  dispatch |     |  input/     |     |  what to do    |
 *   +-----------+     +-------------+     +----------------+
 *                                                 |
 *                                                 v
 *   +----------------+    +-------------+    +--------------+
 *   |  is_outdated   | <- |  enact/     | <- |  cmds/       |
 *   |  is_focus_dirty|    |  and notify |    |  do it       |
 *   +----------------+    +-------------+    +--------------+
 *           |
 *           v
 *   +----------------+    +-------------+    +--------------+
 *   |  loop/refresh  | -> |  render/    | -> |  one flush   |
 *   |  end of turn   |    |  reads state|    |  loop.c      |
 *   +----------------+    +-------------+    +--------------+
 *
 * @endcode
 *
 * Nothing paints outside that pass, save one exception: an @c Expose
 * names the window and region the server wants back, so
 * @c handler/expose.c repaints it at once rather than marking the whole
 * desktop and waiting.
 *
 * @defgroup loop Main event loop and startup
 * @ingroup wm
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
#include <types/handles.h>


/* Public interface */
/**
 * @brief Run the main event loop until the window manager is stopped
 *
 * Installs signal handlers, allocates key symbols, grabs configured
 * bindings, scans pre-existing windows, performs an initial full
 * render, then blocks on @c poll() and drains XCB events until
 * @c wm->is_running becomes @c false.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1) per event; unbounded overall (loop runs
 *       until stopped)
 */
void loop_run(wm_td *wm);


#endif  /* ! LOOP_H */
