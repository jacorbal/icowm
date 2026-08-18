/**
 * @file input/mouse/drag.c
 *
 * @brief Mouse drag-operation public state machine
 *
 * Manages the singleton drag state used by the move/resize and
 * icon-drag interactions.  Split by sub-concern into
 * @c drag/overlay.c, @c drag/snap.c, @c drag/outline.c, and
 * @c drag/warp.c; see @c drag/internal.h for the exact split and why.
 * All mutable drag state is defined here (@c s_drag, declared
 * @c extern to the rest of @c input/mouse/drag/ via that same
 * header) and, from outside this whole module, opaque: no other
 * module accesses it directly.
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
#include <stdio.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Windows & icons policy includes */
#include <policy/focus.h>
#include <policy/placement.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/desktop.h>
#include <defs/icon.h>
#include <defs/input.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <render/icon.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <input/mouse.h>
#include <input/mouse/drag.h>
#include <input/mouse/bounds.h>
#include <input/mouse/drag/internal.h>


drag_state_td s_drag = {
    .active = false,
    .operation = CLIENT_OPERATION_IDLE,
    .client = NULL,
    .desktop = NULL,
    .drag_window = XCB_WINDOW_NONE,
    .pointer_start_x = 0,
    .pointer_start_y = 0,
    .client_start_x = 0,
    .client_start_y = 0,
    .client_start_w = 0,
    .client_start_h = 0,
    .screen_w = 0,
    .screen_h = 0,
    .snap = 0,
    .client_cur_x = 0,
    .client_cur_y = 0,
    .anchor_right = false,
    .anchor_bottom = false,
    .resize_w = false,
    .resize_h = false,
    .overlay_window = XCB_WINDOW_NONE,
    .overlay_is_icon = false,
    .overlay_text = {'\0'},
    .icon_was_mapped = false,
    .last_root_x = 0,
    .last_root_y = 0,
    .has_last_pos = false,
    .warp_pending = false,
    .warp_is_left = false,
    .warp_due = {0},
    .root = XCB_WINDOW_NONE,
    .solid_drag = true,
    .outline_offscreened = false,
    .outline_windows = {
        XCB_WINDOW_NONE, XCB_WINDOW_NONE, XCB_WINDOW_NONE, XCB_WINDOW_NONE
    },
    .client_cur_w = 0,
    .client_cur_h = 0
};


void drag_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        enum window_operation_e operation,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y,
        uint32_t screen_w, uint32_t screen_h,
        uint32_t snap)
{
    if (connection == NULL || client == NULL) {
        return;
    }

    drag_overlay_hide(connection);
    s_drag.active = true;
    s_drag.client = client;
    s_drag.desktop = desktop;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.operation = operation;
    s_drag.pointer_start_x = root_x;
    s_drag.pointer_start_y = root_y;
    s_drag.client_start_x = client->layout.geometry.cur.pos.x;
    s_drag.client_start_y = client->layout.geometry.cur.pos.y;
    s_drag.client_start_w =
        (uint16_t) client->layout.geometry.cur.dim.w;
    s_drag.client_start_h =
        (uint16_t) client->layout.geometry.cur.dim.h;
    s_drag.client_cur_x = s_drag.client_start_x;
    s_drag.client_cur_y = s_drag.client_start_y;
    s_drag.client_cur_w = s_drag.client_start_w;
    s_drag.client_cur_h = s_drag.client_start_h;
    s_drag.root = root;
    s_drag.solid_drag = (client->config_base == NULL) ||
        client->config_base->windows.solid_drag;
    s_drag.outline_offscreened = false;
    s_drag.screen_w = screen_w;
    s_drag.screen_h = screen_h;
    s_drag.snap = snap;
    s_drag.has_last_pos = false;
    s_drag.warp_pending = false;

    /* For resize operations, make the visible corner handles define the
     * corner hit zones.  Outside those adaptive-margin corner zones
     * (see 'im_resize_bounds' in input/mouse/bounds.h), keep the
     * existing center-based fallback so the rest of the border still
     * behaves as a resize handle. */
    if (operation == CLIENT_OPERATION_RESIZING) {
        im_resize_bounds_td b = im_resize_bounds(client);
        int32_t cx = s_drag.client_start_x +
            (int32_t) (s_drag.client_start_w / 2u);
        int32_t cy = s_drag.client_start_y +
            (int32_t) (s_drag.client_start_h / 2u);

        if ((int32_t) root_x < b.left + b.margin_left) {
            s_drag.anchor_right = true;
        } else if ((int32_t) root_x >= b.right - b.margin_right) {
            s_drag.anchor_right = false;
        } else {
            s_drag.anchor_right = ((int32_t) root_x < cx);
        }

        if ((int32_t) root_y < b.top + b.margin_top) {
            s_drag.anchor_bottom = true;
        } else if ((int32_t) root_y >= b.bottom - b.margin_bottom) {
            s_drag.anchor_bottom = false;
        } else {
            s_drag.anchor_bottom = ((int32_t) root_y < cy);
        }

        /* Track which axes are actively resized.  An axis is active
         * only when the grab point is near that edge.  Keeping the
         * other axis fixed at its start value prevents
         * client_constrain_size from snapping it down by a full
         * increment due to sub-increment pointer noise on the
         * orthogonal axis, which for size-hinted clients would produce
         * a 'ConfigureRequest' feedback loop */
        s_drag.resize_w = ((int32_t) root_x < b.left + b.margin_left ||
                (int32_t) root_x >= b.right - b.margin_right);
        s_drag.resize_h = ((int32_t) root_y < b.top + b.margin_top ||
                (int32_t) root_y >= b.bottom - b.margin_bottom);
        if (!s_drag.resize_w && !s_drag.resize_h) {
            s_drag.resize_w = true;
            s_drag.resize_h = true;
        }
    } else {
        s_drag.anchor_right = false;
        s_drag.anchor_bottom = false;
        s_drag.resize_w = false;
        s_drag.resize_h = false;
    }

    client->properties.operation = (uint16_t) operation;

    /* A user-initiated move overrides any rule-assigned position */
    if (operation == CLIENT_OPERATION_MOVING) {
        client->rule_position_locked = false;
    }

    /* An outline drag draws a stand-in rectangle from the very start,
     * rather than moving the real window live; see 's_drag.solid_drag'
     * itself for the config option this follows. */
    if (!s_drag.solid_drag) {
        drag_outline_start(connection, s_drag.client_start_x,
                s_drag.client_start_y, s_drag.client_start_w,
                s_drag.client_start_h);
    }

    xcb_grab_pointer(connection,
            0,
            root,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            (operation == CLIENT_OPERATION_MOVING)
                ? mouse_move_cursor()
                : mouse_resize_cursor_for_axes(s_drag.resize_w,
                        s_drag.resize_h, s_drag.anchor_right,
                        s_drag.anchor_bottom),
            event_time);
    xcb_flush(connection);
}


/* Begin a resize drag with an explicit anchor, rather than one
 * 'drag_start' would infer from 'root_x' / 'root_y' */
void drag_start_directed(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y,
        uint32_t screen_w, uint32_t screen_h,
        uint32_t snap,
        bool anchor_right, bool anchor_bottom,
        bool resize_w, bool resize_h)
{
    drag_start(connection, root, client, desktop,
            CLIENT_OPERATION_RESIZING, event_time, root_x, root_y,
            screen_w, screen_h, snap);

    if (!s_drag.active) {
        return;
    }

    s_drag.anchor_right = anchor_right;
    s_drag.anchor_bottom = anchor_bottom;
    s_drag.resize_w = resize_w;
    s_drag.resize_h = resize_h;

    /* The grab 'drag_start' already holds was given a cursor matching
     * its own inferred anchor/axes, which the overrides just above
     * may have replaced with a different direction entirely; update
     * the already-active grab's cursor to match rather than leave it
     * showing the wrong one for the rest of this drag. */
    xcb_change_active_pointer_grab(connection,
            mouse_resize_cursor_for_axes(resize_w, resize_h,
                    anchor_right, anchor_bottom),
            event_time,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION);
}


/* Begin a resize drag, locking out whichever axis (or axes)
 * 'axis_w_locked'/'axis_h_locked' mark as unavailable; see this
 * function's own Doxygen comment in drag.h for the ICCCM/traditional
 * WM reasoning and its bibliographic citation */
void drag_start_resize_axis_locked(xcb_connection_t *connection,
        xcb_window_t root, client_td *client, desktop_td *desktop,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y,
        uint32_t screen_w, uint32_t screen_h,
        uint32_t snap,
        bool axis_w_locked, bool axis_h_locked)
{
    drag_start(connection, root, client, desktop,
            CLIENT_OPERATION_RESIZING, event_time, root_x, root_y,
            screen_w, screen_h, snap);

    if (!s_drag.active) {
        return;
    }

    if (axis_w_locked) {
        s_drag.resize_w = false;
    }
    if (axis_h_locked) {
        s_drag.resize_h = false;
    }

    if (!s_drag.resize_w && !s_drag.resize_h) {
        /* The only edge the grab point was near belongs to the axis
         * this maximize state has locked: cancel outright rather than
         * leave an inert resize drag running that visibly does
         * nothing while held. */
        drag_cancel(connection, client);
        return;
    }

    /* One of the two axes above may have just been locked out of a
     * grab that started as a corner (both axes); update the
     * already-active grab's cursor to match whichever single-axis
     * shape is left, the same reasoning as 'drag_start_directed'. */
    xcb_change_active_pointer_grab(connection,
            mouse_resize_cursor_for_axes(s_drag.resize_w, s_drag.resize_h,
                    s_drag.anchor_right, s_drag.anchor_bottom),
            event_time,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION);
}


/* Begin a drag operation for an icon window */
void drag_start_icon(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        int32_t icon_x, int32_t icon_y,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y,
        uint32_t screen_w, uint32_t screen_h)
{
    if (connection == NULL || client == NULL) {
        return;
    }

    drag_overlay_hide(connection);
    s_drag.active = true;
    s_drag.client = client;
    s_drag.desktop = desktop;
    s_drag.drag_window = client->icon_window;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.pointer_start_x = root_x;
    s_drag.pointer_start_y = root_y;
    s_drag.client_start_x = icon_x;
    s_drag.client_start_y = icon_y;
    s_drag.client_start_w = 0;
    s_drag.client_start_h = 0;
    s_drag.client_cur_x = icon_x;
    s_drag.client_cur_y = icon_y;
    /* Icon drags are always solid, regardless of 'windows.solid-drag':
     * moving just the small icon window live is cheap enough on its
     * own that the outline machinery would add complexity for no
     * real benefit here; see 'drag_end''s own comment on this same
     * exclusion. */
    s_drag.solid_drag = true;
    s_drag.screen_w = screen_w;
    s_drag.screen_h = screen_h;
    s_drag.icon_was_mapped = client->is_icon_mapped;
    s_drag.anchor_right = false;
    s_drag.anchor_bottom = false;
    s_drag.resize_w = false;
    s_drag.resize_h = false;
    s_drag.has_last_pos = false;
    s_drag.warp_pending = false;

    client->properties.operation = CLIENT_OPERATION_MOVING;
    client->is_icon_mapped = true;
    drag_sync_icon_active_visual(connection);

    xcb_grab_pointer(connection,
            0,
            root,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            XCB_NONE,
            event_time);
    xcb_flush(connection);
}


void drag_update(xcb_connection_t *connection,
        int16_t root_x, int16_t root_y)
{
    client_td *client;
    int32_t dx;
    int32_t dy;

    if (connection == NULL || !s_drag.active || s_drag.client == NULL) {
        return;
    }

    /* A duplicate 'MotionNotify' reporting the exact same root
     * position as the one already acted on is a real occurrence, not
     * just theoretical: the X server can deliver one right after the
     * pointer grab starts under an already-resting pointer (the same
     * kind of spurious repeat already handled for menu selection in
     * 'ctxmenu_handle_motion', menu/context/ctxmenu.c).  Skipping it
     * here avoids repeating the same 'xcb_configure_window' and
     * 'xcb_flush' for a position that produces no visible change. */
    if (s_drag.has_last_pos && root_x == s_drag.last_root_x &&
            root_y == s_drag.last_root_y) {
        return;
    }
    s_drag.last_root_x = root_x;
    s_drag.last_root_y = root_y;
    s_drag.has_last_pos = true;

    client = s_drag.client;

    /* The pointer has now genuinely moved since 'drag_start', which
     * fires on every plain click with no way yet to tell a click
     * apart from a real drag; only now, confirmed a real drag rather
     * than a click that never moved, does the real window actually
     * move off screen (see 'drag_move_client_offscreen''s own doc
     * comment).  'outline_offscreened' guards this so it only ever
     * happens once per drag.  Icon drags are always solid (see
     * 'drag_start_icon''s own comment), so this never applies to
     * them at all. */
    if (!s_drag.solid_drag && !s_drag.outline_offscreened) {
        drag_move_client_offscreen(connection, client);
        s_drag.outline_offscreened = true;
    }

    dx = (int32_t) root_x - (int32_t) s_drag.pointer_start_x;
    dy = (int32_t) root_y - (int32_t) s_drag.pointer_start_y;

    if (s_drag.operation == CLIENT_OPERATION_MOVING &&
            s_drag.drag_window != XCB_WINDOW_NONE &&
            s_drag.drag_window == client->icon_window) {
        bool show_geom = client->config_base != NULL &&
            client->config_base->icons.show_geom;
        int32_t new_x = s_drag.client_start_x + dx;
        int32_t new_y = s_drag.client_start_y + dy;
        uint32_t vals[2];

        s_drag.client_cur_x = new_x;
        s_drag.client_cur_y = new_y;

        vals[0] = (uint32_t) new_x;
        vals[1] = (uint32_t) new_y;
        xcb_configure_window(connection, client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);

        if (show_geom) {
            char geom_buf[24];

            (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                    (int) new_x, (int) new_y);
            drag_overlay_show(connection, true,
                    new_x, new_y,
                    (uint16_t) WM_ICON_SQUARE_SIZE,
                    drag_icon_height(client),
                    geom_buf);
        } else {
            drag_overlay_hide(connection);
        }
        drag_check_warp_edge(root_x);
        xcb_flush(connection);
    } else if (s_drag.operation == CLIENT_OPERATION_MOVING) {
        bool show_geom = client->config_base != NULL &&
            client->config_base->windows.show_geom;
        int32_t new_x = s_drag.client_start_x + dx;
        int32_t new_y = s_drag.client_start_y + dy;

        drag_snap_move(&new_x, &new_y,
                s_drag.client_start_w, s_drag.client_start_h);

        s_drag.client_cur_x = new_x;
        s_drag.client_cur_y = new_y;
        if (s_drag.solid_drag) {
            enact_client_move(client, new_x, new_y);
        } else {
            drag_outline_move(connection, new_x, new_y,
                    s_drag.client_start_w, s_drag.client_start_h);
        }

        if (show_geom) {
            char geom_buf[24];

            (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                    (int) new_x, (int) new_y);
            drag_overlay_show(connection, false,
                    new_x, new_y,
                    s_drag.client_start_w, s_drag.client_start_h,
                    geom_buf);
        } else {
            drag_overlay_hide(connection);
        }
        drag_check_warp_edge(root_x);
    } else if (s_drag.operation == CLIENT_OPERATION_RESIZING) {
        bool show_geom = client->config_base != NULL &&
            client->config_base->windows.show_geom;
        int32_t new_x = s_drag.client_start_x;
        int32_t new_y = s_drag.client_start_y;
        uint32_t new_w;
        uint32_t new_h;
        uint32_t resize_ext_w;
        uint32_t resize_ext_h;
        uint32_t resize_content_w;
        uint32_t resize_content_h;
        uint32_t resize_floor_w;
        uint32_t resize_floor_h;
        uint32_t resize_constrained_w;
        uint32_t resize_constrained_h;

        /* Determine resize direction from the anchor computed at drag
         * start.  When 'anchor_right' is set the right edge is fixed
         * and we resize from the left: the window moves and
         * shrinks/grows as the pointer moves right/left.  Similarly for
         * 'anchor_bottom' and the top edge.  When an axis is not
         * actively resized its dimension is frozen at the start value
         * so that client_constrain_size cannot floor it due to
         * sub-increment pointer noise, which would cause size-hinted
         * clients to lose a row or column and enter
         * a 'ConfigureRequest' loop. */
        if (!s_drag.resize_w) {
            new_w = s_drag.client_start_w;
        } else if (s_drag.anchor_right) {
            int32_t clamped_dx = dx;
            int32_t min_w = (int32_t) WM_MIN_WINDOW_DIMENSION;

            if ((int32_t) s_drag.client_start_w - clamped_dx < min_w) {
                clamped_dx = (int32_t) s_drag.client_start_w - min_w;
            }
            new_x = s_drag.client_start_x + clamped_dx;
            new_w = geom_clamp_dim(
                    (int32_t) s_drag.client_start_w - clamped_dx);
        } else {
            new_w = geom_clamp_dim(
                    (int32_t) s_drag.client_start_w + dx);
        }

        if (!s_drag.resize_h) {
            new_h = s_drag.client_start_h;
        } else if (s_drag.anchor_bottom) {
            int32_t clamped_dy = dy;
            int32_t min_h = (int32_t) WM_MIN_WINDOW_DIMENSION;

            if ((int32_t) s_drag.client_start_h - clamped_dy < min_h) {
                clamped_dy = (int32_t) s_drag.client_start_h - min_h;
            }
            new_y = s_drag.client_start_y + clamped_dy;
            new_h = geom_clamp_dim(
                    (int32_t) s_drag.client_start_h - clamped_dy);
        } else {
            new_h = geom_clamp_dim(
                    (int32_t) s_drag.client_start_h + dy);
        }

        drag_snap_resize(&new_x, &new_y, &new_w, &new_h);

        /* 'client_constrain_size' (client/geom.c) expects its own
         * width/height in terms of the client's own content window
         * (what its own 'WM_NORMAL_HINTS' actually describe, per
         * ICCCM), not 'new_w'/'new_h' here, which are frame-relative
         * (this whole function's own 'client_start_w'/'_h', what they
         * were seeded from, already store 'geometry.cur.dim.w'/'.h',
         * established elsewhere ('ci_create_decorations', in
         * 'client/geom.c') as the frame's own total, decoration
         * included), converted here to content space, constrained, then
         * back, the same round trip 's_kb_resize_axis_target'
         * ('input/kbd/interact.c') already makes for the keyboard
         * resize path.
         *
         * Only ever grows either dimension past what the drag alone
         * would have left it at, never shrinks one back down, since
         * that would fight the user's own drag instead of merely
         * flooring it. */
        resize_ext_w = (uint32_t) client->layout.frame_extents.left +
            (uint32_t) client->layout.frame_extents.right;
        resize_ext_h = (uint32_t) client->layout.frame_extents.top +
            (uint32_t) client->layout.frame_extents.bottom;
        resize_content_w = (new_w > resize_ext_w)
            ? (uint32_t) new_w - resize_ext_w : 0u;
        resize_content_h = (new_h > resize_ext_h)
            ? (uint32_t) new_h - resize_ext_h : 0u;
        resize_floor_w = resize_ext_w + WM_MIN_WINDOW_DIMENSION;
        resize_floor_h = resize_ext_h + WM_MIN_WINDOW_DIMENSION;

        client_constrain_size(client,
                &resize_content_w, &resize_content_h);
        resize_constrained_w = resize_content_w + resize_ext_w;
        resize_constrained_h = resize_content_h + resize_ext_h;
        if (resize_constrained_w < resize_floor_w) {
            resize_constrained_w = resize_floor_w;
        }
        if (resize_constrained_h < resize_floor_h) {
            resize_constrained_h = resize_floor_h;
        }

        if (s_drag.anchor_right &&
                resize_constrained_w > (uint32_t) new_w) {
            new_x -= (int32_t) (resize_constrained_w - (uint32_t) new_w);
        }
        if (s_drag.anchor_bottom &&
                resize_constrained_h > (uint32_t) new_h) {
            new_y -= (int32_t) (resize_constrained_h - (uint32_t) new_h);
        }
        new_w = geom_clamp_dim((int32_t) resize_constrained_w);
        new_h = geom_clamp_dim((int32_t) resize_constrained_h);

        s_drag.client_cur_x = new_x;
        s_drag.client_cur_y = new_y;
        s_drag.client_cur_w = (int32_t) new_w;
        s_drag.client_cur_h = (int32_t) new_h;
        if (s_drag.solid_drag) {
            enact_client_resize(client, new_x, new_y, new_w, new_h);
        } else {
            drag_outline_move(connection, new_x, new_y, new_w, new_h);
        }
        if (show_geom) {
            /* 'new_w'/'new_h' are the decorated frame's own total
             * (border and titlebar included, established elsewhere; see
             * 'ci_create_decorations', in client/geom.c), the same as
             * 'client->layout.geometry.cur.dim' itself, but both the
             * size hints below (ICCCM, always about a client's own
             * content, decoration notwithstanding) and the geometry
             * text shown here are about that content alone, so convert
             * to content space first, the same round trip the
             * resize-floor block above already makes. */
            uint32_t ext_w = (uint32_t)
            client->layout.frame_extents.left +
                (uint32_t) client->layout.frame_extents.right;
            uint32_t ext_h = (uint32_t) client->layout.frame_extents.top +
                (uint32_t) client->layout.frame_extents.bottom;
            uint32_t content_w = (new_w > ext_w) ? new_w - ext_w : 0u;
            uint32_t content_h = (new_h > ext_h) ? new_h - ext_h : 0u;
            char geom_buf[24];

            if (client->size_hints.inc_w > 1 &&
                    client->size_hints.inc_h > 1) {
                /* ICCCM §4.1.2.3: falls back to 'MIN_SIZE' as the grid
                 * base */
                uint32_t base_w = (client->size_hints.base_w > 0)
                    ? (uint32_t) client->size_hints.base_w
                    : ((client->size_hints.min_w > 0)
                            ? (uint32_t) client->size_hints.min_w : 0u);
                uint32_t base_h = (client->size_hints.base_h > 0)
                    ? (uint32_t) client->size_hints.base_h
                    : ((client->size_hints.min_h > 0)
                            ? (uint32_t) client->size_hints.min_h : 0u);
                uint32_t inc_w = (uint32_t) client->size_hints.inc_w;
                uint32_t inc_h = (uint32_t) client->size_hints.inc_h;
                uint32_t cols = ((content_w > base_w)
                        ? (content_w - base_w) : 0u) / inc_w;
                uint32_t lines = ((content_h > base_h)
                        ? (content_h - base_h) : 0u) / inc_h;

                /* Cell count ('cols x lines') for a terminal-like client */
                (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                        cols, lines);
                /* Raw pixel dimensions */
                /*
                (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                        new_w, new_h);
                */
            } else {
                /* Raw pixel dimensions, content only, not the decorated
                 * frame's own total; see this block's comment above */
                (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                        content_w, content_h);
            }
            drag_overlay_show(connection, false,
                    new_x, new_y,
                    drag_u16_sat(new_w), drag_u16_sat(new_h),
                    geom_buf);
        } else {
            drag_overlay_hide(connection);
        }
    }
}


/* Finish the drag on a button-release event */
void drag_end(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        int16_t root_x, int16_t root_y)
{
    if (!s_drag.active) {
        return;
    }

    if (s_drag.client != NULL) {
        /* In a solid drag, the real window already tracks
         * 'layout.geometry.cur' live, updated by every 'drag_update'
         * along the way; an outline drag never touches it until now,
         * so 'client_cur_w'/'_h', kept live throughout instead (see
         * their own doc comment above), are what actually hold the
         * final size here. */
        uint32_t final_w = s_drag.solid_drag
            ? s_drag.client->layout.geometry.cur.dim.w
            : (uint32_t) s_drag.client_cur_w;
        uint32_t final_h = s_drag.solid_drag
            ? s_drag.client->layout.geometry.cur.dim.h
            : (uint32_t) s_drag.client_cur_h;
        bool finalize_resize =
            s_drag.operation == CLIENT_OPERATION_RESIZING;

        if (s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window == s_drag.client->icon_window) {
            int32_t dx = (int32_t) root_x -
                (int32_t) s_drag.pointer_start_x;
            int32_t dy = (int32_t) root_y -
                (int32_t) s_drag.pointer_start_y;

            if (dx * dx + dy * dy < WM_ICON_DRAG_THRESHOLD) {
                /* Treat as a click: restore and focus */
                client_td *ic = s_drag.client;
                enact_client_restore(ic);
                if (surface != NULL && desktop != NULL) {
                    focus_apply(NULL, surface, desktop, ic, true, NULL);
                }
            } else {
                int16_t new_icon_x =
                    (int16_t) (s_drag.client_start_x + dx);
                int16_t new_icon_y =
                    (int16_t) (s_drag.client_start_y + dy);
                int32_t tray_x;
                int32_t tray_y;
                uint16_t tray_w;
                uint16_t tray_h;
                bool pushed_out_of_tray = false;

                /* Kept off the tray's own rectangle outright, rather
                 * than left there and relying on stacking alone to
                 * hide it: an icon dragged over the tray still left
                 * the tray's own text missing in that exact span,
                 * even though the icon itself stayed correctly
                 * stacked below it throughout. */
                if (surface != NULL &&
                        systray_get_geometry(surface, &tray_x, &tray_y,
                            &tray_w, &tray_h)) {
                    pushed_out_of_tray = icon_avoid_systray_overlap(
                            &new_icon_x, &new_icon_y,
                            (uint16_t) WM_ICON_SQUARE_SIZE,
                            (uint16_t) WM_ICON_SQUARE_SIZE,
                            tray_x, tray_y, tray_w, tray_h,
                            (desktop != NULL)
                                ? &desktop->workarea : NULL);
                }

                s_drag.client->icon_x = new_icon_x;
                s_drag.client->icon_y = new_icon_y;

                /* The drag itself only ever moved the icon window as
                 * far as the pointer's own last position (see
                 * 'drag_update' above); an adjustment made here, after
                 * that already stopped, needs its own explicit request
                 * to actually reach the window, or the icon would stay
                 * showing wherever the pointer dropped it while
                 * 'icon_x'/'icon_y' above already disagree with what
                 * is on screen. */
                if (pushed_out_of_tray && connection != NULL) {
                    uint32_t vals[2];

                    vals[0] = (uint32_t) new_icon_x;
                    vals[1] = (uint32_t) new_icon_y;
                    xcb_configure_window(connection,
                            s_drag.client->icon_window,
                            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                            vals);
                }

                /* Restacking already happens on its own every second
                 * or so, driven by the systray's own clock tick (see
                 * 'systray_layout_restack''s comment), so an icon
                 * dropped over the tray's own area does not stay
                 * visually on top of it for long either way.  Forced
                 * here too, right as the icon settles into its final
                 * position, so there is no window at all, however
                 * brief, where it could still be showing over the tray. */
                systray_restack();

                /* Restore whatever mapped state the icon window had
                 * right before this drag began, the same value
                 * 'drag_sync_icon_active_visual' (drag_start_icon)
                 * forced to 'true' for the duration of the drag to
                 * keep it visible while being moved.  Scoped to this
                 * branch alone, rather than run unconditionally for
                 * every kind of drag this function ends: the click
                 * branch above already leaves 'is_icon_mapped' at
                 * the correct 'false' 'enact_client_restore' set,
                 * alongside 'icon_window' itself at zero; applying
                 * this same assignment there too would stamp 'true'
                 * straight back over it, since 'icon_was_mapped' was
                 * necessarily 'true' to begin dragging a mapped icon
                 * in the first place, leaving 'is_icon_mapped' true
                 * while 'icon_window' is already destroyed. */
                s_drag.client->is_icon_mapped = s_drag.icon_was_mapped;
            }
        }

        s_drag.client->properties.operation = CLIENT_OPERATION_IDLE;

        if (connection != NULL &&
                s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window == s_drag.client->icon_window) {
            xcb_clear_area(connection, 0, s_drag.client->icon_window,
                    0, 0, 0, 0);

            /* Request a full repaint so the icon returns to its normal
             * (inactive) appearance after being shown in active colors
             * during the drag */
            wm_request_client_redraw(s_drag.client);
        }

        /* Icon drags never go through 'solid_drag'/the outline
         * machinery at all (see 'drag_start_icon''s own comment,
         * intentionally always solid given how cheap moving just an
         * icon already is); their own final position is already
         * fully settled by the icon-specific block above, so this
         * whole thing only applies to an actual client window drag. */
        if (s_drag.drag_window == XCB_WINDOW_NONE) {
            if (s_drag.solid_drag) {
                /* Already fully applied live, one 'enact_client_move'/
                 * 'enact_client_resize' per 'drag_update' along the
                 * way; a resize alone gets one more here, to finalize
                 * whatever that last live call left off at (e.g.,
                 * snapping fully onto the size-hint grid a client
                 * with 'WM_NORMAL_HINTS' increments declares, which
                 * the live calls only approach step by step as the
                 * pointer moves). */
                if (finalize_resize) {
                    enact_client_resize(s_drag.client,
                            s_drag.client->layout.geometry.cur.pos.x,
                            s_drag.client->layout.geometry.cur.pos.y,
                            final_w, final_h);
                }
            } else {
                /* An outline drag never touched the real window until
                 * now; apply the final position (and size, for a
                 * resize) in one single call, then destroy the 4
                 * strip windows that made up the outline stand-in. */
                if (finalize_resize) {
                    enact_client_resize(s_drag.client,
                            s_drag.client_cur_x, s_drag.client_cur_y,
                            final_w, final_h);
                } else {
                    enact_client_move(s_drag.client,
                            s_drag.client_cur_x, s_drag.client_cur_y);
                }
                drag_outline_end(connection);
            }
        }
    }

    drag_overlay_hide(connection);
    s_drag.active = false;
    s_drag.operation = CLIENT_OPERATION_IDLE;
    s_drag.client = NULL;
    s_drag.desktop = NULL;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.icon_was_mapped = false;
    s_drag.warp_pending = false;

    if (connection != NULL) {
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        xcb_flush(connection);
    }
}


/* Cancel an in-progress drag when the dragged client disappears */
void drag_cancel(xcb_connection_t *connection, const client_td *client)
{
    if (!s_drag.active || s_drag.client != client) {
        return;
    }

    drag_overlay_hide(connection);
    /* Moved back from its own off-screen parking spot first (see
     * 'drag_move_client_offscreen''s own doc comment), to its own
     * genuine, never-actually-changed logical position, for a client
     * that survives the cancel (see the comment on
     * 'properties.operation' just below, for exactly this same
     * distinction): without this, it would stay stuck off screen
     * forever, with nothing left to ever move it back. */
    if (s_drag.outline_offscreened && s_drag.client != NULL) {
        xcb_window_t target =
            (client_is_decorated(s_drag.client) &&
                s_drag.client->frame != 0)
            ? s_drag.client->frame
            : s_drag.client->window;
        const uint32_t vals[2] = {
            (uint32_t) s_drag.client->layout.geometry.cur.pos.x,
            (uint32_t) s_drag.client->layout.geometry.cur.pos.y
        };

        xcb_configure_window(connection, target,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
    }
    /* A no-op whenever no outline drag was in progress (solid drag,
     * or an icon drag, which is always solid regardless); otherwise
     * destroys the 4 strip windows, so a client that disappears
     * mid-drag never leaves them stuck on screen with nothing left
     * to ever remove them. */
    drag_outline_end(connection);
    s_drag.active = false;
    s_drag.operation = CLIENT_OPERATION_IDLE;
    s_drag.warp_pending = false;
    if (s_drag.client != NULL) {
        /* The module-level 's_drag' bookkeeping above is reset either
         * way, but without this the client's OWN operation flag stays
         * stuck at 'MOVING'/'RESIZING' forever whenever a future caller
         * passes a client that survives the cancel. */
        s_drag.client->properties.operation = CLIENT_OPERATION_IDLE;
    }
    s_drag.client = NULL;
    s_drag.desktop = NULL;
    s_drag.drag_window = XCB_WINDOW_NONE;

    if (connection != NULL) {
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        xcb_flush(connection);
    }
}


/* Query whether a drag operation is currently active */
bool drag_is_active(void)
{
    return s_drag.active;
}


/* Query whether the active drag is on an icon window */
bool drag_is_icon_drag(void)
{
    return s_drag.active &&
        s_drag.drag_window != XCB_WINDOW_NONE &&
        s_drag.client != NULL &&
        s_drag.drag_window == s_drag.client->icon_window;
}

void drag_current_pos(int32_t *restrict x, int32_t *restrict y)
{
    if (x != NULL) {
        *x = s_drag.client_cur_x;
    }
    if (y != NULL) {
        *y = s_drag.client_cur_y;
    }
}
