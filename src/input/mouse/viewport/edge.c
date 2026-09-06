/**
 * @file input/mouse/viewport/edge.c
 *
 * @brief Viewport pan triggered by resting the pointer against a
 *        screen edge, no drag in progress
 *
 * Deliberately its own, self-contained module rather than folded into
 * @c input/mouse/drag/warp.c: that one only ever runs while a window
 * or icon drag is already active, moved along by the very
 * @c MotionNotify stream the drag itself consumes, and switches whole
 * desktops rather than panning within one.  This one instead has to
 * notice an edge hold with no drag (or any other pointer grab) present
 * at all, and once armed, keeps re-arming its own countdown on every
 * @c mouse_viewport_edge_tick rather than relying on further motion to
 * drive it, since panning the viewport never moves the pointer the way
 * a desktop warp does.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free, NULL */
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/direction.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/desktop.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <lookup.h>
#include <surface.h>

/* Command includes */
#include <cmds/surface.h>

/* Input includes */
#include <input/mouse/drag.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Local includes */
#include <input/mouse/viewport/edge.h>


/** Whether an edge hold is currently counting down toward a pan */
static bool s_pending = false;

/** Which edge @a s_pending counts down for, meaningful only while it
 *  is true */
static enum compass_direction_e s_direction;

/** Surface @a s_pending counts down for, meaningful only while it is
 *  true; tracked by pointer rather than id, exactly like the desktop
 *  warp's @c s_drag.client, since re-validated in full before ever
 *  being dereferenced again */
static surface_td *s_surface = NULL;

/** When the held edge becomes due to pan, only meaningful while
 *  @a s_pending is true */
static struct timespec s_due;


/**
 * @brief Whether @p surface has a pannable viewport configured at all
 *
 * @param surface Surface to check
 *
 * @return Whether @p surface's screen has a @c viewport wider or
 *         taller than one physical screen
 *
 * @note Complexity: @e O(1)
 */
static bool s_viewport_edge_is_active(const surface_td *surface)
{
    if (surface->config == NULL ||
            surface->id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return false;
    }

    return surface->config->base.screens[surface->id].viewport.columns >
            1u ||
        surface->config->base.screens[surface->id].viewport.rows > 1u;
}


/**
 * @brief Pan @p surface's current desktop one screen toward
 *        @p direction
 *
 * Shared by @a mouse_viewport_edge_tick's first pan and every repeat
 * after it, which only differ in which countdown re-arms them.
 *
 * @param surface   Surface to pan
 * @param direction Compass direction to pan toward
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
static void s_viewport_edge_pan(surface_td *surface,
        enum compass_direction_e direction)
{
    switch (direction) {
    case COMPASS_NORTH:
        scmd_surface_viewport_pan_north(surface);
        break;
    case COMPASS_SOUTH:
        scmd_surface_viewport_pan_south(surface);
        break;
    case COMPASS_EAST:
        scmd_surface_viewport_pan_east(surface);
        break;
    case COMPASS_WEST:
        scmd_surface_viewport_pan_west(surface);
        break;
    }
}


/* Track whether the pointer is held against a pan-eligible screen
 * edge, and schedule (or keep, or cancel) the pending viewport-pan
 * countdown accordingly; see the header's doc comment for the full
 * reasoning */
void mouse_viewport_edge_check(list_td *surfaces, xcb_window_t root,
        int16_t root_x, int16_t root_y)
{
    surface_td *surface;
    bool at_left;
    bool at_right;
    bool at_top;
    bool at_bottom;
    enum compass_direction_e direction;

    surface = lookup_surface_for_root(surfaces, root);
    if (surface == NULL || surface->config == NULL ||
            !surface->config->base.viewport.pan_on_edge_hover ||
            !s_viewport_edge_is_active(surface)) {
        s_pending = false;
        return;
    }

    at_left = root_x <= 0;
    at_right = (int32_t) root_x >=
        (int32_t) surface_width(surface) - 1;
    at_top = root_y <= 0;
    at_bottom = (int32_t) root_y >=
        (int32_t) surface_height(surface) - 1;

    /* A screen corner holds two edges at once; the horizontal one
     * wins, matching the same convention 'drag_warp_edge_check'
     * already uses. */
    if (at_left) {
        direction = COMPASS_WEST;
    } else if (at_right) {
        direction = COMPASS_EAST;
    } else if (at_top) {
        direction = COMPASS_NORTH;
    } else if (at_bottom) {
        direction = COMPASS_SOUTH;
    } else {
        s_pending = false;
        return;
    }

    if (s_pending && s_direction == direction && s_surface == surface) {
        /* Same edge still held: let the existing countdown keep
         * running rather than restarting it on every motion event. */
        return;
    }

    s_pending = true;
    s_direction = direction;
    s_surface = surface;
    if (clock_gettime(CLOCK_MONOTONIC, &s_due) == 0) {
        clock_add_ms(&s_due, WM_VIEWPORT_PAN_DELAY_MS);
    } else {
        /* Could not read the clock to schedule the countdown; safer
         * to not pan at all than to pan immediately on every edge
         * touch. */
        s_pending = false;
    }
}


/* Milliseconds until a pointer held against a pan-eligible screen edge
 * is due to pan the viewport */
int mouse_viewport_edge_ms_remaining(void)
{
    if (!s_pending) {
        return -1;
    }

    return (int) clock_ms_until(&s_due);
}


/* Perform the pending edge pan, if due */
void mouse_viewport_edge_tick(xcb_connection_t *connection)
{
    xcb_query_pointer_reply_t *reply;
    bool still_at_edge;

    if (connection == NULL || !s_pending ||
            mouse_viewport_edge_ms_remaining() > 0) {
        return;
    }

    s_pending = false;

    if (drag_is_active() || s_surface == NULL ||
            s_surface->screen == NULL || s_surface->config == NULL ||
            !s_surface->config->base.viewport.pan_on_edge_hover ||
            !s_viewport_edge_is_active(s_surface)) {
        return;
    }

    still_at_edge = false;
    reply = xcb_query_pointer_reply(connection,
            xcb_query_pointer(connection, s_surface->screen->root),
            NULL);
    if (reply != NULL) {
        if (reply->same_screen) {
            switch (s_direction) {
            case COMPASS_WEST:
                still_at_edge = reply->root_x <= 0;
                break;
            case COMPASS_EAST:
                still_at_edge = (int32_t) reply->root_x >=
                    (int32_t) surface_width(s_surface) - 1;
                break;
            case COMPASS_NORTH:
                still_at_edge = reply->root_y <= 0;
                break;
            case COMPASS_SOUTH:
                still_at_edge = (int32_t) reply->root_y >=
                    (int32_t) surface_height(s_surface) - 1;
                break;
            }
        }

        free(reply);
    }

    if (!still_at_edge) {
        return;
    }

    s_viewport_edge_pan(s_surface, s_direction);

    /* Panning the viewport never moves the pointer itself, unlike a
     * desktop warp, so no further 'MotionNotify' is coming to re-arm
     * this on its own; re-arming here at the shorter repeat interval
     * is what keeps a single edge hold panning repeatedly rather than
     * only once. */
    s_pending = true;
    if (clock_gettime(CLOCK_MONOTONIC, &s_due) == 0) {
        clock_add_ms(&s_due, WM_VIEWPORT_PAN_REPEAT_MS);
    } else {
        s_pending = false;
    }
}
