/**
 * @file handler/randr.h
 *
 * @brief X RandR extension event handler
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

#ifndef HANDLER_RANDR_H
#define HANDLER_RANDR_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle XRandR extension events
 *
 * Processes monitor/screen-change notifications and schedules geometry
 * and repaint refresh work for affected surfaces.
 *
 * @param wm    Window manager state
 * @param event Raw XCB event from the main loop
 *
 * @note Complexity: @e O(b * s * k), where @e b is the number of key
 *       bindings, @e s is the number of surfaces, and @e k is the
 *       number of keycodes per keysym
 */
void handler_randr_event(wm_td *wm, xcb_generic_event_t *event);


#endif  /* ! HANDLER_RANDR_H */
