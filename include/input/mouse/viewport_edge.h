/**
 * @file input/mouse/viewport_edge.h
 *
 * @brief Edge-triggered viewport pan while the pointer merely rests at
 *        a screen edge, no drag in progress
 *
 * @ingroup input_mouse
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_VIEWPORT_EDGE_H
#define INPUT_MOUSE_VIEWPORT_EDGE_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>


/**
 * @brief Track whether the pointer is currently held against a
 *        pan-eligible screen edge, and schedule (or keep, or cancel)
 *        the pending viewport-pan countdown accordingly
 *
 * Meant to be called on every plain hover @c MotionNotify (never while
 * a drag, menu, or other pointer grab already owns the event; see
 * @c loop/event/motion.c's dispatch), so a window or icon drag's own
 * edge-triggered desktop warp (@c input/mouse/drag/warp.h) is never
 * second-guessed by this, entirely separate, feature.  Restarting the
 * countdown on every single motion notify while the same edge stays
 * held is deliberately avoided, since that would leave the countdown
 * permanently reset and never actually elapse.  When the pointer sits
 * against two edges at once (a screen corner), the horizontal edge
 * wins, the same convention @a drag_warp_edge_check already uses.
 *
 * @param surfaces Every managed surface, to resolve @p root to the one
 *                 whose configuration and viewport size govern whether
 *                 this may fire
 * @param root     Root window the motion happened on
 * @param root_x   Pointer X position in root-window coordinates
 * @param root_y   Pointer Y position in root-window coordinates
 *
 * @note A no-op, clearing any pending pan, unless @p root resolves to a
 *       surface with @c desktops.pan_on_edge_hover enabled and a
 *       @c viewport wider or taller than one physical screen
 * @note Complexity: @e O(1)
 */
void mouse_viewport_edge_check(list_td *surfaces, xcb_window_t root,
        int16_t root_x, int16_t root_y);

/**
 * @brief Milliseconds until a pointer held against a pan-eligible
 *        screen edge is due to pan the viewport
 *
 * Tracked by @a mouse_viewport_edge_check as the pointer moves (see
 * @p desktops.pan_on_edge_hover in @c config.json,
 * @c config_desktop_s); serviced by @a mouse_viewport_edge_tick.
 *
 * @return Milliseconds remaining (never negative), or @c -1 if the
 *         pointer is not currently held against an eligible edge
 *
 * @note Complexity: @e O(1)
 */
int mouse_viewport_edge_ms_remaining(void);

/**
 * @brief Perform the pending edge pan, if its countdown has elapsed
 *
 * Meant to be called on every main-loop iteration, the same way
 * @a drag_warp_tick is, so a pointer left resting against a screen
 * edge still keeps panning even with no further @c MotionNotify
 * arriving to drive it: the viewport pan never moves the pointer
 * itself, unlike a desktop warp, so nothing else would otherwise wake
 * this back up.  Re-queries the live pointer position first, rather
 * than trusting whichever coordinates the countdown was originally
 * armed with in @a mouse_viewport_edge_check, so a pointer that
 * already moved away from the edge (or a surface whose configuration
 * changed) is never acted on stale; a drag that started in the
 * meantime (@a drag_is_active) is likewise deferred to entirely, so
 * this never second-guesses that drag's own edge-triggered desktop
 * warp.  Once fired, immediately re-arms its own countdown at the
 * shorter @c WM_VIEWPORT_PAN_REPEAT_MS interval (@c defs/desktop.h)
 * for as long as the pointer keeps resting there, so a single edge
 * hold pans repeatedly rather than only once.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop (from @a scmd_surface_viewport_pan_north
 *       and its three siblings)
 */
void mouse_viewport_edge_tick(xcb_connection_t *connection);


#endif  /* ! INPUT_MOUSE_VIEWPORT_EDGE_H */
