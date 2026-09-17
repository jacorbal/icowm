/**
 * @file handler/colormap.h
 *
 * @brief X @c ColormapNotify event handler
 *
 * Split out of @c handler.h, alongside its sibling @c handler
 * headers, so a file that only needs this one event's handler
 * does not also pull in, and rebuild against, every other
 * unrelated one declared alongside it.
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

#ifndef HANDLER_COLORMAP_H
#define HANDLER_COLORMAP_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle a @c COLORMAP_NOTIFY event
 *
 * ICCCM §4.1.8: a client's colormap attribute changed on one of the
 * windows named in its @c WM_COLORMAP_WINDOWS list (or ceased to be
 * installed at all).  Updates the matching cached entry in
 * @c colormap_windows.colormap_ids, and, when the owning client
 * currently holds real input focus, installs the updated colormap
 * immediately rather than waiting for the next focus change.
 *
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Colormap notify event
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       stages, @e d the number of desktops per stage, and @e c the
 *       number of clients per desktop
 */
void handler_colormap_notify(xcb_connection_t *connection,
        list_td *stages, const xcb_colormap_notify_event_t *event);


#endif  /* ! HANDLER_COLORMAP_H */
