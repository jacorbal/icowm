/**
 * @file input/mouse/drag/pan.c
 *
 * @brief Viewport pan triggered by holding a drag against a screen
 *        edge
 *
 * One of the files @c input/mouse/drag/ is made of; see
 * @c drag/internal.h for why.  Sibling to @c drag/warp.c, which
 * switches whole desktops instead of panning within one: whichever of
 * the two a held edge actually means is decided by
 * @a scmd_surface_viewport_pan_available (@c cmds/surface.h), consulted
 * by both files independently rather than through a decision made once
 * here and handed to the other, so neither has to know the other
 * exists at all.  Like that sibling, a pan also warps the pointer
 * along with whatever it is dragging (@a s_pan_pointer_target below),
 * keeping the two visually glued together instead of leaving the
 * pointer resting at the screen edge while the window moves out from
 * under it.  Unlike that sibling, though, panning does not rely on
 * further motion to keep going: like @c input/mouse/viewport_edge.c's
 * hover-triggered version, this one re-arms its own countdown
 * unconditionally on every @a drag_pan_tick, at the same
 * @c WM_VIEWPORT_PAN_REPEAT_MS cadence that one already uses, so a
 * pointer held stationary against a physical screen edge (generating
 * no further @c MotionNotify events at all) still keeps panning for as
 * long as there is room to.
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
#include <stdio.h>      /* NULL, snprintf */
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/direction.h>
#include <types/pair.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <client/predicates.h>
#include <desktop.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Command includes */
#include <cmds/surface.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Local includes */
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/outline.h>
#include <input/mouse/drag/overlay.h>
#include <input/mouse/drag/pan.h>
#include <utils/xcb/connection.h>


/**
 * @brief Pan @p surface's current desktop one screen toward
 *        @p direction
 *
 * @param surface   Surface to pan
 * @param direction Compass direction to pan toward
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
static void s_pan_apply(surface_td *surface,
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


/**
 * @brief Keep the dragged window or icon under the pointer across a
 *        viewport pan, and the geometry overlay with it
 *
 * A no-op for a sticky dragged client: @a s_pan_apply just above
 * already left it untouched on screen (@a s_viewport_translate_visit,
 * @c cmds/surface.c, skips any client holding @c CLIENT_FLAG_STICKY),
 * so shifting the drag's own cached position here would only make it
 * visually snap on the next @c MotionNotify instead.  The dragged
 * client itself is also the one client that same translate walk
 * always skips regardless of stickiness (@a
 * scmd_surface_viewport_drag_exclude having named it for the whole
 * drag's duration), so every bit of its own repositioning below is
 * this function's job alone rather than shared with that walk.
 *
 * @param connection XCB connection
 * @param is_icon    Whether an icon window is being dragged
 * @param delta      Pixel delta the pan just applied to every
 *                   non-sticky client
 *
 * @note Complexity: @e O(1)
 */
static void s_pan_move_dragged(xcb_connection_t *connection,
        bool is_icon, struct position_s delta)
{
    int32_t new_window_x;
    int32_t new_window_y;
    bool show_geom;

    if (client_is_sticky(s_drag.client)) {
        return;
    }

    /* 'drag_pan_tick' warps the pointer by this same 'delta' right
     * after this returns, exactly like 's_warp_move_dragged' (drag/
     * warp.c) already does for a desktop warp, so 'pointer_start_x'/
     * 'pointer_start_y' and 'client_start.pos' are deliberately left
     * untouched here: the invariant linking them to 'client_cur.pos'
     * stays correct on its own once the pointer itself has moved, the
     * same reasoning that function's own comment lays out in full, and
     * shifting 'client_start.pos' here too, on top of that, would
     * double the effective delta the next real motion notify sees. */
    new_window_x = s_drag.client_cur.pos.x + delta.x;
    new_window_y = s_drag.client_cur.pos.y + delta.y;
    s_drag.client_cur.pos.x = new_window_x;
    s_drag.client_cur.pos.y = new_window_y;

    if (is_icon) {
        uint32_t vals[2];

        show_geom = s_drag.client->config != NULL &&
            s_drag.client->config->base.icons.show_geom;

        vals[0] = (uint32_t) new_window_x;
        vals[1] = (uint32_t) new_window_y;
        xcb_configure_window(connection, s_drag.client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
    } else {
        show_geom = s_drag.client->config != NULL &&
            s_drag.client->config->base.windows.show_geom;

        /* Solid drag: the real window moves live on every genuine
         * 'MotionNotify' too ('drag_update', drag.c), so a pan mid-
         * drag follows that exact same precedent instead of leaving
         * the dragged client to the generic per-client translate walk
         * (which now excludes it outright, see 's_viewport_pan_
         * excluded_client', cmds/surface.c).  A raw 'xcb_configure_
         * window' rather than 'ccmd_client_move', for the same reason
         * 's_viewport_translate_visit' itself avoids that wrapper: it
         * refuses to touch a maximized or fullscreen client at all,
         * while a pan still has to move one of those exactly like
         * every other client.  Only the outline stand-in
         * ('!is_solid_drag') needs its own manual move here instead,
         * since it is a set of separate, override-redirect windows
         * that walk never touched even before this exclusion existed.
         * The real window behind an outline drag stays exactly where
         * 'drag_client_move_offscreen' parked it, untouched by a pan,
         * precisely because that walk no longer reaches it either. */
        if (s_drag.is_solid_drag) {
            xcb_window_t target;
            uint32_t vals[2];

            s_drag.client->layout.geometry.cur.pos.x = new_window_x;
            s_drag.client->layout.geometry.cur.pos.y = new_window_y;
            target = (client_is_decorated(s_drag.client) &&
                    s_drag.client->frame != 0)
                ? s_drag.client->frame
                : s_drag.client->window;
            vals[0] = (uint32_t) new_window_x;
            vals[1] = (uint32_t) new_window_y;
            xcb_configure_window(connection, target,
                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
        } else {
            drag_outline_move(connection, (struct geometry_s) {
                        { new_window_x, new_window_y },
                        { s_drag.client_start.dim.w,
                            s_drag.client_start.dim.h } });
        }
    }

    /* Same geometry overlay 'drag_update' keeps current on every real
     * motion notify.  Without this, it would stay painted at the
     * position the window (or icon) had right before the pan until
     * whatever real pointer motion happens to come next, rather than
     * following it across immediately. */
    if (show_geom) {
        char geom_buf[24];

        (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                (int) new_window_x, (int) new_window_y);
        drag_overlay_show(connection, is_icon, (struct geometry_s) {
                    { new_window_x, new_window_y },
                    { is_icon
                        ? (uint16_t) WM_ICON_SQUARE_SIZE
                        : s_drag.client_start.dim.w,
                      is_icon
                        ? drag_icon_height(s_drag.client)
                        : s_drag.client_start.dim.h } },
                geom_buf);
    }
}


/* Track whether the pointer is held against a pan-eligible screen
 * edge, and schedule (or keep, or cancel) the pending viewport-pan
 * countdown accordingly */
void drag_pan_edge_check(int16_t root_x, int16_t root_y)
{
    surface_td *surface;
    bool at_left;
    bool at_right;
    bool at_top;
    bool at_bottom;
    enum compass_direction_e direction;

    if (s_drag.client == NULL) {
        s_drag.is_pan_pending = false;
        return;
    }

    surface = wm_get_surface_by_id(s_drag.client->screen_id);
    if (surface == NULL || surface->config == NULL ||
            !surface->config->desktops.pan_on_edge_drag) {
        s_drag.is_pan_pending = false;
        return;
    }

    at_left = root_x <= 0;
    at_right = (int32_t) root_x >= (int32_t) s_drag.screen_w - 1;
    at_top = root_y <= 0;
    at_bottom = (int32_t) root_y >= (int32_t) s_drag.screen_h - 1;

    /* A screen corner holds two edges at once; the horizontal one
     * wins, matching whichever edge 'drag_warp_edge_check' (drag/
     * warp.c) already preferred before this file existed. */
    if (at_left) {
        direction = COMPASS_WEST;
    } else if (at_right) {
        direction = COMPASS_EAST;
    } else if (at_top) {
        direction = COMPASS_NORTH;
    } else if (at_bottom) {
        direction = COMPASS_SOUTH;
    } else {
        s_drag.is_pan_pending = false;
        return;
    }

    if (!scmd_surface_viewport_pan_available(surface, direction)) {
        s_drag.is_pan_pending = false;
        return;
    }

    if (s_drag.is_pan_pending && s_drag.pan_direction == direction) {
        /* Same edge still held: let the existing countdown keep
         * running rather than restarting it on every motion event. */
        return;
    }

    s_drag.is_pan_pending = true;
    s_drag.pan_direction = direction;
    if (clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due) == 0) {
        clock_add_ms(&s_drag.pan_due, WM_VIEWPORT_PAN_DELAY_MS);
    } else {
        /* Could not read the clock to schedule the countdown; safer
         * to not pan at all than to pan immediately on every edge
         * touch. */
        s_drag.is_pan_pending = false;
    }
}


/* Milliseconds until a pointer held against a pan-eligible screen edge
 * is due to pan the viewport */
int drag_pan_ms_remaining(void)
{
    if (!s_drag.is_pan_pending) {
        return -1;
    }

    return (int) clock_ms_until(&s_drag.pan_due);
}


/**
 * @brief Work out where the pointer lands after a pan delta
 *
 * Keeps the pointer glued to whatever it is dragging across a
 * viewport pan, the same way @a s_warp_pointer_target (@c drag/
 * warp.c) keeps it glued across a desktop warp, except the pointer
 * here always moves by the exact same @p delta the dragged client
 * itself just moved by, rather than jumping to the opposite screen
 * edge: a pan stays on the one same desktop, so there is no
 * "opposite edge" of a different desktop to land near in the first
 * place.
 *
 * @param delta  Pixel delta the pan just applied to the dragged
 *               client
 * @param out_x  Receives the root X the pointer warps to
 * @param out_y  Receives the root Y the same
 *
 * @note Complexity: @e O(1)
 */
static void s_pan_pointer_target(struct position_s delta,
        int16_t *out_x, int16_t *out_y)
{
    int32_t max_x;
    int32_t max_y;
    int32_t new_x;
    int32_t new_y;

    /* Clamped to at most 'INT16_MAX', matching 's_warp_pointer_target'
     * (drag/warp.c): 'screen_w'/'screen_h' (uint32_t, no compile-time
     * bound) are not guaranteed to fit int16_t on an extreme
     * multi-monitor surface, and this pointer position is sent to the
     * X server as one, via 'xcb_warp_pointer' below. */
    max_x = ((int32_t) s_drag.screen_w - 1 > INT16_MAX)
        ? INT16_MAX : (int32_t) s_drag.screen_w - 1;
    max_y = ((int32_t) s_drag.screen_h - 1 > INT16_MAX)
        ? INT16_MAX : (int32_t) s_drag.screen_h - 1;

    new_x = (int32_t) s_drag.last_root_x + delta.x;
    new_y = (int32_t) s_drag.last_root_y + delta.y;

    if (new_x < 0) {
        new_x = 0;
    } else if (new_x > max_x) {
        new_x = max_x;
    }

    if (new_y < 0) {
        new_y = 0;
    } else if (new_y > max_y) {
        new_y = max_y;
    }

    *out_x = (int16_t) new_x;
    *out_y = (int16_t) new_y;
}


/* Perform the pending edge pan, if due */
void drag_pan_tick(xcb_connection_t *connection)
{
    surface_td *surface;
    desktop_td *desktop;
    struct position_s origin_before;
    struct position_s delta;
    int16_t new_root_x;
    int16_t new_root_y;
    bool is_icon;

    if (connection == NULL || !s_drag.is_pan_pending ||
            drag_pan_ms_remaining() > 0) {
        return;
    }

    s_drag.is_pan_pending = false;

    if (s_drag.client == NULL ||
            s_drag.operation != CLIENT_OPERATION_MOVING ||
            (s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window != s_drag.client->icon_window)) {
        /* Not (or no longer) a plain window move or icon move; nothing
         * to pan for, as a resize never sets 'is_pan_pending' in the
         * first place (see 'drag_pan_edge_check'), but this still
         * guards against it having somehow become stale. */
        return;
    }

    is_icon = s_drag.drag_window != XCB_WINDOW_NONE;

    surface = wm_get_surface_by_id(s_drag.client->screen_id);
    if (surface == NULL || surface->screen == NULL ||
            surface->config == NULL ||
            !surface->config->desktops.pan_on_edge_drag ||
            !scmd_surface_viewport_pan_available(surface,
                s_drag.pan_direction)) {
        /* Live re-check: the config, or the viewport's own room to
         * pan, may have changed since this was armed. */
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    /* Read the delta back from the origin itself, before and after,
     * rather than assuming a full, unclamped screen step: the
     * viewport can already sit at an unaligned origin coming from a
     * background pan drag ('input/mouse/drag/background.c') or an
     * EWMH '_NET_DESKTOP_VIEWPORT' request, in which case
     * 's_viewport_apply_origin' (cmds/surface.c) clamps the requested
     * step short of a whole screen.  Recomputing it this way instead
     * of assuming the step always lands exactly a whole screen away
     * keeps the dragged client in lock-step with every other client
     * on the desktop, which that same function already moved by
     * whatever the real, possibly-clamped delta turned out to be. */
    origin_before = desktop->viewport_origin;
    s_pan_apply(surface, s_drag.pan_direction);
    delta.x = origin_before.x - desktop->viewport_origin.x;
    delta.y = origin_before.y - desktop->viewport_origin.y;
    s_pan_move_dragged(connection, is_icon, delta);

    /* A sticky dragged client never actually moved just above (see
     * 's_pan_move_dragged''s own early return), so warping the
     * pointer here too would be the one thing that pulled it away
     * from the client instead of keeping it glued on, the exact
     * opposite of the point of this whole step. */
    if (!client_is_sticky(s_drag.client)) {
        s_pan_pointer_target(delta, &new_root_x, &new_root_y);
        xcb_warp_pointer(connection, XCB_NONE, surface->screen->root,
                0, 0, 0, 0, new_root_x, new_root_y);

        /* Matches 'last_root_x'/'last_root_y' up with the warp just
         * issued, exactly as 'drag_warp_tick' (drag/warp.c) does after
         * its own pointer warp: the synthetic 'MotionNotify' this
         * generates then reports the same position already recorded
         * here, so 'drag_update' (drag.c) drops it as a duplicate
         * rather than recomputing the very position this whole
         * function just set. */
        s_drag.last_root_x = new_root_x;
        s_drag.last_root_y = new_root_y;
    }

    /* Panning the viewport does not rely on a further 'MotionNotify'
     * to keep going, unlike a desktop warp: re-arming here at the
     * shorter repeat interval unconditionally, regardless of where the
     * pointer warp just above landed, is what keeps a single edge hold
     * panning repeatedly for as long as there is room to, exactly like
     * 'mouse_viewport_edge_tick' (input/mouse/viewport_edge.c), rather
     * than stalling as soon as a stationary pointer stops generating
     * fresh motion events of its own. */
    s_drag.is_pan_pending = true;
    if (clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due) == 0) {
        clock_add_ms(&s_drag.pan_due, WM_VIEWPORT_PAN_REPEAT_MS);
    } else {
        s_drag.is_pan_pending = false;
    }
}
