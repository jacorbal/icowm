/**
 * @file handler/property.h
 *
 * @brief X @c PropertyNotify event handler
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

#ifndef HANDLER_PROPERTY_H
#define HANDLER_PROPERTY_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle a @c PROPERTY_NOTIFY event
 *
 * Refreshes the client name when @c WM_NAME or @c _NET_WM_NAME changes.
 * When @c _NET_WM_STRUT_PARTIAL or @c _NET_WM_STRUT changes, re-reads
 * the strut values into @c client->layout.strut_partial and calls
 * @c desktop_update_workarea on all desktops of the owning stage so
 * that maximize and placement policies use the updated work area.
 *
 * @param wm         Pointer to the window manager itself
 * @param connection XCB connection
 * @param stages     All managed stages
 * @param event      Property notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       stages
 */
void handler_property_notify(const wm_td *wm,
        xcb_connection_t *connection,
        list_td *stages, xcb_property_notify_event_t *event);


#endif  /* ! HANDLER_PROPERTY_H */
