/**
 * @file handler/sync.h
 *
 * @brief X Sync extension event handler
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

#ifndef HANDLER_SYNC_H
#define HANDLER_SYNC_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle XSync extension events
 *
 * Recognizes @c AlarmNotify events for @c _NET_WM_SYNC_REQUEST alarms
 * and applies the owning client's pending throttled resize, if any.
 * Events on alarms owned by no managed client, or arriving while XSync
 * is unavailable, are ignored.
 *
 * @param wm    Window manager state
 * @param event Raw XCB event from the main loop
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across every desktop and surface
 *
 * @see @c ccmd_client_resize_flush_pending
 */
void handler_sync_event(const wm_td *wm, xcb_generic_event_t *event);


#endif  /* ! HANDLER_SYNC_H */
