/**
 * @file handler/selection.h
 *
 * @brief X @c SelectionClear event handler
 *
 * Split out of @c handler.h, alongside its sibling @c handler headers,
 * so a file that only needs this one event's handler does not also pull
 * in, and rebuild against, every other unrelated one declared alongside
 * it.
 *
 * @see @c handler.h
 *
 * @ingroup handler
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef HANDLER_SELECTION_H
#define HANDLER_SELECTION_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle a @c SELECTION_CLEAR event
 *
 * Setting a new owner for an @c ICCCM manager selection (@c WM_S<n>)
 * always sends this to whichever window owned it right before, per the
 * core X11 protocol itself, regardless of whether that prior owner ever
 * asked to be told; @a wm_startup_acquire_selection
 * (@c wm/startup/selection.c) relies on exactly this when @c -r asks it
 * to replace a still-running window manager.
 *
 * A no-op unless @p event names this window manager's own support
 * window (the same one @c wm/startup/selection.c originally acquired
 * every managed screen's own selection with, reused since as the
 * @c _NET_SUPPORTING_ WM_CHECK window too), in which case another
 * window manager (or another instance of this one, started with @c -r)
 * has just taken @c WM_S<n> away, and the same coordinated shutdown
 * a quit request already starts is started here too, so this instance
 * relinquishes the rest of what it owns instead of continuing to run
 * underneath whatever just replaced it.
 *
 * @param wm    Window manager state
 * @param event Selection clear event
 *
 * @note Complexity: @e O(1), aside from @a wm_shutdown_begin's own cost
 *       when it is reached
 */
void handler_selection_clear(wm_td *wm,
        const xcb_selection_clear_event_t *event);


#endif  /* ! HANDLER_SELECTION_H */
