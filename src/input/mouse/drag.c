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

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf, NULL */
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Types includes */
#include <types/pair.h>

/* Windows & icons policy includes */
#include <policy/focus.h>
#include <policy/placement/icon.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/desktop.h>
#include <defs/icon.h>
#include <defs/input.h>

/* Project includes */
#include <client.h>
#include <cmds/surface.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <input/mouse/cursor.h>
#include <input/mouse/drag.h>
#include <input/mouse/bounds.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/overlay.h>
#include <input/mouse/drag/resist.h>
#include <input/mouse/drag/snap.h>
#include <input/mouse/drag/outline.h>
#include <input/mouse/drag/pan.h>
#include <input/mouse/drag/warp.h>
#include <utils/xcb/window.h>


drag_state_td s_drag = {
    .is_active = false,
    .operation = CLIENT_OPERATION_IDLE,
    .client = NULL,
    .desktop = NULL,
    .drag_window = XCB_WINDOW_NONE,
    .pointer_start_x = 0,
    .pointer_start_y = 0,
    .client_start = { { 0, 0 }, { 0u, 0u } },
    .screen_w = 0,
    .screen_h = 0,
    .snap_window = 0,
    .snap_screen = 0,
    .client_cur = { { 0, 0 }, { 0u, 0u } },
    .is_anchor_right = false,
    .is_anchor_bottom = false,
    .is_resize_w = false,
    .is_resize_h = false,
    .is_resist_axis_w = false,
    .is_resist_axis_h = false,
    .is_move_x_locked = false,
    .is_move_y_locked = false,
    .overlay_window = XCB_WINDOW_NONE,
    .is_overlay_icon = false,
    .overlay_text = {'\0'},
    .was_icon_mapped = false,
    .last_root_x = 0,
    .last_root_y = 0,
    .has_last_pos = false,
    .is_warp_pending = false,
    .warp_direction = COMPASS_NORTH,
    .warp_due = {0},
    .is_pan_pending = false,
    .pan_direction = COMPASS_NORTH,
    .pan_due = {0},
    .root = XCB_WINDOW_NONE,
    .is_solid_drag = true,
    .is_outline_offscreened = false,
    .outline_windows = {
        XCB_WINDOW_NONE, XCB_WINDOW_NONE, XCB_WINDOW_NONE, XCB_WINDOW_NONE
    }
};


/**
 * @brief Move the real window being dragged in outline mode off
 *        screen, for the duration of the drag
 *
 * See @c WM_DRAG_OFFSCREEN_POS itself (@c defs/input.h) for why this,
 * rather than unmapping it, is what keeps it out of sight without
 * ever disturbing real input focus, sloppy focus tracking, or
 * active-window rendering.  A plain @c xcb_configure_window, not
 * @a enact_client_move, since this is a purely visual, temporary
 * relocation with no logical meaning: unlike a real move,
 * it must never touch @p client's @c layout.geometry.cur.pos,
 * which every other part of the window manager still relies on to
 * reflect wherever the drag is logically taking it, not this
 * incidental physical parking spot.  Moving it back to its
 * genuine final position is left entirely to whichever one of
 * @a enact_client_move/@a enact_client_resize @a drag_end itself
 * already calls once the drag ends, rather than needing a
 * symmetrical function here.
 *
 * @param connection X connection
 * @param client Client to move off screen
 *
 * @note No-op if @p connection or @p client is null
 * @note Complexity: @e O(1)
 */
static void s_drag_client_move_offscreen(xcb_connection_t *connection,
        client_td *client)
{
    xcb_window_t target;

    if (connection == NULL || client == NULL) {
        return;
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;
    xcb_window_move(target, WM_DRAG_OFFSCREEN_POS,
            WM_DRAG_OFFSCREEN_POS);
}


/**
 * @brief Saturate an unsigned 32-bit value to the 16-bit range
 *
 * Returns @p value converted to @c uint16_t, saturating to
 * @c UINT16_MAX if the input exceeds the maximum 16-bit unsigned
 * value.  Unlike @a geom_dim_clamp, this applies no minimum floor at
 * all: a genuinely small or zero @p value passes through unchanged,
 * which matters here since the overlay shows the exact candidate size
 * a resize drag currently would apply, not a value about to become
 * a client's real, enforced geometry.
 *
 * @param value Unsigned 32-bit value to saturate
 *
 * @return @p value, saturated to @c UINT16_MAX
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_drag_dim_sat(uint32_t value)
{
    return (value > UINT16_MAX) ? UINT16_MAX : (uint16_t) value;
}


/**
 * @brief Follow the pointer while an iconified client's icon is
 *        dragged
 *
 * @param connection XCB connection
 * @param client     Client whose icon is being dragged
 * @param dx         Pointer displacement since the drag began, on X
 * @param dy         The same, on Y
 * @param root_pos   Pointer position in root coordinates
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_update_icon(xcb_connection_t *connection,
        client_td *client, int32_t dx, int32_t dy,
        struct position_s root_pos)
{
    bool show_geom = client->config != NULL &&
        client->config->base.icons.show_geom;
    int32_t new_x = s_drag.client_start.pos.x + dx;
    int32_t new_y = s_drag.client_start.pos.y + dy;

    s_drag.client_cur.pos.x = new_x;
    s_drag.client_cur.pos.y = new_y;

    xcb_window_move(client->icon_window, new_x, new_y);

    if (show_geom) {
        char geom_buf[24];

        (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                (int) new_x, (int) new_y);
        drag_overlay_show(connection, true, (struct geometry_s) {
                    { new_x, new_y },
                    { WM_ICON_SQUARE_SIZE,
                        drag_icon_height(client) } },
                geom_buf);
    } else {
        drag_overlay_hide(connection);
    }
    drag_warp_edge_check((int16_t) root_pos.x,
            (int16_t) root_pos.y);
    drag_pan_edge_check((int16_t) root_pos.x,
            (int16_t) root_pos.y);
    xcb_flush(connection);
}


/**
 * @brief Follow the pointer while a client window is moved
 *
 * @param connection XCB connection
 * @param client     Client being moved
 * @param dx         Pointer displacement since the drag began, on X
 * @param dy         The same, on Y
 * @param root_pos   Pointer position in root coordinates
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_update_move(xcb_connection_t *connection,
        client_td *client, int32_t dx, int32_t dy,
        struct position_s root_pos)
{
    bool show_geom = client->config != NULL &&
        client->config->base.windows.show_geom;
    int32_t new_x = s_drag.client_start.pos.x + dx;
    int32_t new_y = s_drag.client_start.pos.y + dy;

    drag_snap_move(&new_x, &new_y,
            s_drag.client_start.dim.w, s_drag.client_start.dim.h);

    /* A client maximized on just one axis has nothing valid to
     * move to on that axis at all: its width (horizontally
     * maximized) or height (vertically maximized) already fills
     * the whole workarea, so the one position that still fits is
     * the one it started this drag at.  Pinned after snapping,
     * not before, so nothing above can nudge it away from that
     * exact starting value regardless. */
    if (s_drag.is_move_x_locked) {
        new_x = s_drag.client_start.pos.x;
    }
    if (s_drag.is_move_y_locked) {
        new_y = s_drag.client_start.pos.y;
    }

    s_drag.client_cur.pos.x = new_x;
    s_drag.client_cur.pos.y = new_y;
    if (s_drag.is_solid_drag) {
        enact_client_move(client,
                (struct position_s) { new_x, new_y });
    } else {
        drag_outline_move(connection, (struct geometry_s) {
                    { new_x, new_y },
                    { s_drag.client_start.dim.w,
                        s_drag.client_start.dim.h } });
    }

    if (show_geom) {
        char geom_buf[24];

        (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                (int) new_x, (int) new_y);
        drag_overlay_show(connection, false, (struct geometry_s) {
                    { new_x, new_y },
                    { s_drag.client_start.dim.w,
                        s_drag.client_start.dim.h } },
                geom_buf);
    } else {
        drag_overlay_hide(connection);
    }
    drag_warp_edge_check((int16_t) root_pos.x,
            (int16_t) root_pos.y);
    drag_pan_edge_check((int16_t) root_pos.x,
            (int16_t) root_pos.y);
}


/**
 * @brief Work out the geometry a resize drag has reached
 *
 * Applies the anchor the drag started from, the snapping, and every
 * size hint the client declared, in that order.
 *
 * @param client Client being resized
 * @param dx     Pointer displacement since the drag began, on X
 * @param dy     The same, on Y
 * @param out    Receives the position and dimensions reached
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_resize_geometry(client_td *client, int32_t dx,
        int32_t dy, struct geometry_s *out)
{
    uint32_t drag_dist_w = (uint32_t) ((dx < 0) ? -dx : dx);
    uint32_t drag_dist_h = (uint32_t) ((dy < 0) ? -dy : dy);
    int32_t new_x = s_drag.client_start.pos.x;
    int32_t new_y = s_drag.client_start.pos.y;
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
    uint32_t resistance = (client->config != NULL)
        ? client->config->base.windows.edges.resistance : 0u;

    /* A maximize-locked axis is not fixed for the whole drag the way
     * it is for every other client, so the flags are recomputed on
     * every update rather than read once at the start */
    drag_resist_axis_update(drag_dist_w, drag_dist_h, resistance);

    /* Determine resize direction from the anchor computed at drag
     * start.  When 'is_anchor_right' is set the right edge is fixed
     * and we resize from the left: the window moves and
     * shrinks/grows as the pointer moves right/left.  Similarly for
     * 'is_anchor_bottom' and the top edge.  When an axis is not
     * actively resized its dimension is frozen at the start value
     * so that client_size_constrain cannot floor it due to
     * sub-increment pointer noise, which would cause size-hinted
     * clients to lose a row or column and enter
     * a 'ConfigureRequest' loop. */
    if (!s_drag.is_resize_w) {
        new_w = s_drag.client_start.dim.w;
    } else if (s_drag.is_anchor_right) {
        int32_t clamped_dx = dx;
        int32_t min_w = (int32_t) WM_MIN_WINDOW_DIMENSION;

        if ((int32_t) s_drag.client_start.dim.w - clamped_dx < min_w) {
            clamped_dx = (int32_t) s_drag.client_start.dim.w - min_w;
        }
        new_x = s_drag.client_start.pos.x + clamped_dx;
        new_w = geom_dim_clamp(
                (int32_t) s_drag.client_start.dim.w - clamped_dx);
    } else {
        new_w = geom_dim_clamp(
                (int32_t) s_drag.client_start.dim.w + dx);
    }

    if (!s_drag.is_resize_h) {
        new_h = s_drag.client_start.dim.h;
    } else if (s_drag.is_anchor_bottom) {
        int32_t clamped_dy = dy;
        int32_t min_h = (int32_t) WM_MIN_WINDOW_DIMENSION;

        if ((int32_t) s_drag.client_start.dim.h - clamped_dy < min_h) {
            clamped_dy = (int32_t) s_drag.client_start.dim.h - min_h;
        }
        new_y = s_drag.client_start.pos.y + clamped_dy;
        new_h = geom_dim_clamp(
                (int32_t) s_drag.client_start.dim.h - clamped_dy);
    } else {
        new_h = geom_dim_clamp(
                (int32_t) s_drag.client_start.dim.h + dy);
    }

    drag_snap_resize(&new_x, &new_y, &new_w, &new_h);

    /* 'client_size_constrain' (client/geom.c) expects its
     * width/height in terms of the client's content window
     * (what its 'WM_NORMAL_HINTS' actually describe, per
     * ICCCM), not 'new_w'/'new_h' here, which are frame-relative
     * (this whole function's 'client_start.dim.w'/'.h', what
     * they were seeded from, already store 'geometry.cur.dim.w'/
     * '.h', established elsewhere ('ci_create_decorations', in
     * 'client/geom.c') as the frame's total, decoration
     * included), converted here to content space, constrained, then
     * back, the same round trip 's_kb_resize_axis_target'
     * ('input/kbd/interact.c') already makes for the keyboard
     * resize path.
     *
     * Only ever grows either dimension past what the drag alone
     * would have left it at, never shrinks one back down, since
     * that would fight the user's drag instead of merely
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

    client_size_constrain(client,
            &resize_content_w, &resize_content_h);
    resize_constrained_w = resize_content_w + resize_ext_w;
    resize_constrained_h = resize_content_h + resize_ext_h;
    if (resize_constrained_w < resize_floor_w) {
        resize_constrained_w = resize_floor_w;
    }
    if (resize_constrained_h < resize_floor_h) {
        resize_constrained_h = resize_floor_h;
    }

    if (s_drag.is_anchor_right &&
            resize_constrained_w > (uint32_t) new_w) {
        new_x -= (int32_t) (resize_constrained_w - (uint32_t) new_w);
    }
    if (s_drag.is_anchor_bottom &&
            resize_constrained_h > (uint32_t) new_h) {
        new_y -= (int32_t) (resize_constrained_h - (uint32_t) new_h);
    }
    new_w = geom_dim_clamp((int32_t) resize_constrained_w);
    new_h = geom_dim_clamp((int32_t) resize_constrained_h);

    out->pos.x = new_x;
    out->pos.y = new_y;
    out->dim.w = new_w;
    out->dim.h = new_h;
}


/**
 * @brief Show the size overlay while a resize drag is under way
 *
 * @param connection XCB connection
 * @param client     Client being resized
 * @param reached    Geometry the drag has reached
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_resize_show_geometry(xcb_connection_t *connection,
        client_td *client, struct geometry_s reached)
{
    /* 'reached.dim.w'/'reached.dim.h' are the decorated frame's total
     * (border and titlebar included, established elsewhere; see
     * 'ci_create_decorations', in client/geom.c), the same as
     * 'client->layout.geometry.cur.dim' itself, but both the
     * size hints below (ICCCM, always about a client's
     * content, decoration notwithstanding) and the geometry
     * text shown here are about that content alone, so convert
     * to content space first, the same round trip the
     * resize-floor block above already makes. */
    uint32_t ext_w = (uint32_t)
    client->layout.frame_extents.left +
        (uint32_t) client->layout.frame_extents.right;
    uint32_t ext_h = (uint32_t) client->layout.frame_extents.top +
        (uint32_t) client->layout.frame_extents.bottom;
    uint32_t content_w = (reached.dim.w > ext_w)
        ? reached.dim.w - ext_w : 0u;
    uint32_t content_h = (reached.dim.h > ext_h)
        ? reached.dim.h - ext_h : 0u;
    char geom_buf[24];

    if (client->hints_icccm.size.inc.w > 1 &&
            client->hints_icccm.size.inc.h > 1) {
        /* ICCCM §4.1.2.3: falls back to 'MIN_SIZE' as the grid
         * base */
        uint32_t base_w = (client->hints_icccm.size.base.w > 0)
            ? client->hints_icccm.size.base.w
            : ((client->hints_icccm.size.min.w > 0)
                    ? client->hints_icccm.size.min.w : 0u);
        uint32_t base_h = (client->hints_icccm.size.base.h > 0)
            ? client->hints_icccm.size.base.h
            : ((client->hints_icccm.size.min.h > 0)
                    ? client->hints_icccm.size.min.h : 0u);
        uint32_t inc_w = client->hints_icccm.size.inc.w;
        uint32_t inc_h = client->hints_icccm.size.inc.h;
        uint32_t cols = ((content_w > base_w)
                ? (content_w - base_w) : 0u) / inc_w;
        uint32_t lines = ((content_h > base_h)
                ? (content_h - base_h) : 0u) / inc_h;

        /* Cell count, columns by lines, for a terminal */
        (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                cols, lines);
        /* Raw pixel dimensions */
        /*
        (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                reached.dim.w, reached.dim.h);
        */
    } else {
        /* Raw pixel dimensions, content only, not the decorated
         * frame's total; see this block's comment above */
        (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                content_w, content_h);
    }
    drag_overlay_show(connection, false, (struct geometry_s) {
                { reached.pos.x, reached.pos.y },
                { s_drag_dim_sat(reached.dim.w),
                  s_drag_dim_sat(reached.dim.h) } },
            geom_buf);
}


/**
 * @brief Follow the pointer while a client window is resized
 *
 * @param connection XCB connection
 * @param client     Client being resized
 * @param dx         Pointer displacement since the drag began, on X
 * @param dy         The same, on Y
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_update_resize(xcb_connection_t *connection,
        client_td *client, int32_t dx, int32_t dy)
{
    const bool show_geom = client->config != NULL &&
        client->config->base.windows.show_geom;
    struct geometry_s reached;

    s_drag_resize_geometry(client, dx, dy, &reached);
    s_drag.client_cur = reached;

    if (s_drag.is_solid_drag) {
        enact_client_resize(client, reached);
    } else {
        drag_outline_move(connection, reached);
    }

    if (show_geom) {
        s_drag_resize_show_geometry(connection, client, reached);
    } else {
        drag_overlay_hide(connection);
    }
}


void drag_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        enum window_operation_e operation,
        xcb_timestamp_t event_time,
        struct position_s root_pos,
        struct dimensions_s screen_dim)
{
    xcb_grab_pointer_cookie_t grab_cookie;
    xcb_grab_pointer_reply_t *grab_reply;

    if (connection == NULL || client == NULL) {
        return;
    }

    drag_overlay_hide(connection);
    s_drag.is_active = true;
    s_drag.client = client;
    scmd_surface_viewport_drag_exclude(client);
    s_drag.desktop = desktop;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.operation = operation;
    s_drag.pointer_start_x = (int16_t) root_pos.x;
    s_drag.pointer_start_y = (int16_t) root_pos.y;
    s_drag.client_start.pos.x = client->layout.geometry.cur.pos.x;
    s_drag.client_start.pos.y = client->layout.geometry.cur.pos.y;
    s_drag.client_start.dim.w =
        (uint16_t) client->layout.geometry.cur.dim.w;
    s_drag.client_start.dim.h =
        (uint16_t) client->layout.geometry.cur.dim.h;
    s_drag.client_cur.pos.x = s_drag.client_start.pos.x;
    s_drag.client_cur.pos.y = s_drag.client_start.pos.y;
    s_drag.client_cur.dim.w = s_drag.client_start.dim.w;
    s_drag.client_cur.dim.h = s_drag.client_start.dim.h;
    s_drag.root = root;
    s_drag.is_solid_drag = (client->config == NULL) ||
        client->config->base.windows.solid_drag;
    s_drag.is_outline_offscreened = false;
    s_drag.screen_w = screen_dim.w;
    s_drag.screen_h = screen_dim.h;
    s_drag.snap_window = (client->config == NULL)
        ? 0u : client->config->base.windows.edges.snap.window;
    s_drag.snap_screen = (client->config == NULL)
        ? 0u : client->config->base.windows.edges.snap.screen;
    s_drag.has_last_pos = false;
    s_drag.is_warp_pending = false;
    s_drag.is_pan_pending = false;

    /* For resize operations, make the visible corner handles define the
     * corner hit zones.  Outside those adaptive-margin corner zones
     * (see 'im_bounds_resize' in input/mouse/bounds.h), keep the
     * existing center-based fallback so the rest of the border still
     * behaves as a resize handle. */
    if (operation == CLIENT_OPERATION_RESIZING) {
        im_resize_bounds_td b = im_bounds_resize(client);
        int32_t cx = s_drag.client_start.pos.x +
            (int32_t) (s_drag.client_start.dim.w / 2u);
        int32_t cy = s_drag.client_start.pos.y +
            (int32_t) (s_drag.client_start.dim.h / 2u);

        if (root_pos.x < b.left + b.margin_left) {
            s_drag.is_anchor_right = true;
        } else if (root_pos.x >= b.right - b.margin_right) {
            s_drag.is_anchor_right = false;
        } else {
            s_drag.is_anchor_right = (root_pos.x < cx);
        }

        if (root_pos.y < b.top + b.margin_top) {
            s_drag.is_anchor_bottom = true;
        } else if (root_pos.y >= b.bottom - b.margin_bottom) {
            s_drag.is_anchor_bottom = false;
        } else {
            s_drag.is_anchor_bottom = (root_pos.y < cy);
        }

        /* Track which axes are actively resized.  An axis is active
         * only when the grab point is near that edge.  Keeping the
         * other axis fixed at its start value prevents
         * client_size_constrain from snapping it down by a full
         * increment due to sub-increment pointer noise on the
         * orthogonal axis, which for size-hinted clients would produce
         * a 'ConfigureRequest' feedback loop */
        s_drag.is_resize_w = (root_pos.x < b.left + b.margin_left ||
                root_pos.x >= b.right - b.margin_right);
        s_drag.is_resize_h = (root_pos.y < b.top + b.margin_top ||
                root_pos.y >= b.bottom - b.margin_bottom);
        if (!s_drag.is_resize_w && !s_drag.is_resize_h) {
            s_drag.is_resize_w = true;
            s_drag.is_resize_h = true;
        }
    } else {
        s_drag.is_anchor_right = false;
        s_drag.is_anchor_bottom = false;
        s_drag.is_resize_w = false;
        s_drag.is_resize_h = false;
    }

    /* A client maximized on just one axis has nothing valid to move
     * to on that axis: its width (horizontally maximized) or
     * height (vertically maximized) already fills the whole
     * workarea, the same reasoning 'drag_start_resize_axis_locked'
     * already applies to resizing that same axis.  Set here,
     * unconditionally, rather than via a second wrapper function
     * mirroring that one: every move-start call site already passes
     * 'client' itself, so its current maximize state can be read
     * directly right here instead of requiring each one to resolve
     * and pass it through explicitly. */
    s_drag.is_move_x_locked = operation == CLIENT_OPERATION_MOVING &&
        client_is_maximized_horz(client);
    s_drag.is_move_y_locked = operation == CLIENT_OPERATION_MOVING &&
        client_is_maximized_vert(client);

    client->properties.operation = (uint16_t) operation;

    /* A user-initiated move overrides any rule-assigned position */
    if (operation == CLIENT_OPERATION_MOVING) {
        client->has_rule_position_locked = false;
    }

    /* An outline drag draws a stand-in rectangle from the very start,
     * rather than moving the real window live; see
     * 's_drag.is_solid_drag' itself for the configuration option
     * this follows. */
    if (!s_drag.is_solid_drag) {
        drag_outline_start(connection, s_drag.client_start);
    }

    grab_cookie = xcb_grab_pointer(connection,
            0,
            root,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            (operation == CLIENT_OPERATION_MOVING)
                ? mouse_cursor_move()
                : mouse_resize_cursor_for_axes(s_drag.is_resize_w,
                        s_drag.is_resize_h, s_drag.is_anchor_right,
                        s_drag.is_anchor_bottom),
            event_time);
    grab_reply = xcb_grab_pointer_reply(connection, grab_cookie, NULL);

    if (grab_reply == NULL ||
            grab_reply->status != XCB_GRAB_STATUS_SUCCESS) {
        LOGGER_WARNING("xcb_grab_pointer failed for drag start," \
                " status=%d",
                (grab_reply != NULL) ? (int) grab_reply->status : -1);
        free(grab_reply);
        if (!s_drag.is_solid_drag) {
            drag_outline_end(connection);
        }
        s_drag.is_active = false;
        return;
    }
    free(grab_reply);
    xcb_flush(connection);
}


/* Begin a resize drag with an explicit anchor, rather than one
 * 'drag_start' would infer from 'root_pos' */
void drag_start_directed(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        xcb_timestamp_t event_time,
        struct position_s root_pos,
        struct dimensions_s screen_dim,
        bool anchor_right, bool anchor_bottom,
        bool resize_w, bool resize_h)
{
    drag_start(connection, root, client, desktop,
            CLIENT_OPERATION_RESIZING, event_time, root_pos,
            screen_dim);

    if (!s_drag.is_active) {
        return;
    }

    s_drag.is_anchor_right = anchor_right;
    s_drag.is_anchor_bottom = anchor_bottom;
    s_drag.is_resize_w = resize_w;
    s_drag.is_resize_h = resize_h;

    /* The grab 'drag_start' already holds was given a cursor matching
     * its inferred anchor/axes, which the overrides just above
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
 * function's Doxygen comment in drag.h for the ICCCM/traditional
 * WM reasoning and its bibliographic citation */
void drag_start_resize_axis_locked(xcb_connection_t *connection,
        xcb_window_t root, client_td *client, desktop_td *desktop,
        xcb_timestamp_t event_time,
        struct position_s root_pos,
        struct dimensions_s screen_dim,
        bool axis_w_locked, bool axis_h_locked)
{
    drag_start(connection, root, client, desktop,
            CLIENT_OPERATION_RESIZING, event_time, root_pos,
            screen_dim);

    if (!s_drag.is_active) {
        return;
    }

    /* A locked axis is not a dead end the way it was before the
     * resistance threshold existed: dragging it past 'windows.edges.
     * resistance' (see 'drag_update''s handling) reversibly
     * un-maximizes it mid-drag, matching Openbox's identical
     * behavior (moveresize.c).  Recorded here, once, for
     * 'drag_update' to consult on every motion event; 'is_resize_w'/
     * 'is_resize_h' themselves start false below for a locked axis, the
     * same as before the threshold existed, and only 'drag_update'
     * itself ever flips them back on, never this function again. */
    s_drag.is_resist_axis_w = axis_w_locked;
    s_drag.is_resist_axis_h = axis_h_locked;
    if (axis_w_locked) {
        s_drag.is_resize_w = false;
    }
    if (axis_h_locked) {
        s_drag.is_resize_h = false;
    }

    if (!s_drag.is_resize_w && !s_drag.is_resize_h &&
            !s_drag.is_resist_axis_w && !s_drag.is_resist_axis_h) {
        /* Neither axis has any way to ever become active, now or
         * later: the grab point was near neither edge to begin with,
         * and neither axis is a maximize-locked one the resistance
         * threshold could still activate.  Cancel outright rather
         * than leave a genuinely inert resize drag running that
         * visibly does nothing while held. */
        drag_cancel(connection, client);
        return;
    }

    /* One of the two axes above may have just been locked out of a
     * grab that started as a corner (both axes); update the
     * already-active grab's cursor to match whichever single-axis
     * shape is left, the same reasoning as 'drag_start_directed'. */
    xcb_change_active_pointer_grab(connection,
            mouse_resize_cursor_for_axes(s_drag.is_resize_w,
                    s_drag.is_resize_h, s_drag.is_anchor_right,
                    s_drag.is_anchor_bottom),
            event_time,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION);
}


/* Update the in-progress drag on a motion-notify event */
void drag_update(xcb_connection_t *connection,
        struct position_s root_pos)
{
    client_td *client;
    int32_t dx;
    int32_t dy;

    if (connection == NULL || !s_drag.is_active || s_drag.client == NULL) {
        return;
    }

    /* A duplicate 'MotionNotify' reporting the exact same root
     * position as the one already acted on is a real occurrence, not
     * just theoretical: the X server can deliver one right after the
     * pointer grab starts under an already-resting pointer (the same
     * kind of spurious repeat already handled for menu selection in
     * 'ctxmenu_handle_motion', menu/context/ctxmenu/handle.c).
     * Skipping it
     * here avoids repeating the same 'xcb_configure_window' and
     * 'xcb_flush' for a position that produces no visible change. */
    if (s_drag.has_last_pos && root_pos.x == s_drag.last_root_x &&
            root_pos.y == s_drag.last_root_y) {
        return;
    }
    s_drag.last_root_x = (int16_t) root_pos.x;
    s_drag.last_root_y = (int16_t) root_pos.y;
    s_drag.has_last_pos = true;

    client = s_drag.client;

    /* The pointer has now genuinely moved since 'drag_start', which
     * fires on every plain click with no way yet to tell a click
     * apart from a real drag; only now, confirmed a real drag rather
     * than a click that never moved, does the real window actually
     * move off screen (see 'drag_client_move_offscreen''s doc
     * comment).  'is_outline_offscreened' guards this so it only ever
     * happens once per drag.  Icon drags are always solid (see
     * 'drag_icon_start''s comment), so this never applies to
     * them at all. */
    if (!s_drag.is_solid_drag && !s_drag.is_outline_offscreened) {
        s_drag_client_move_offscreen(connection, client);
        s_drag.is_outline_offscreened = true;
    }

    dx = root_pos.x - (int32_t) s_drag.pointer_start_x;
    dy = root_pos.y - (int32_t) s_drag.pointer_start_y;

    if (s_drag.operation == CLIENT_OPERATION_MOVING &&
            s_drag.drag_window != XCB_WINDOW_NONE &&
            s_drag.drag_window == client->icon_window) {
        s_drag_update_icon(connection, client, dx, dy, root_pos);
    } else if (s_drag.operation == CLIENT_OPERATION_MOVING) {
        s_drag_update_move(connection, client, dx, dy, root_pos);
    } else if (s_drag.operation == CLIENT_OPERATION_RESIZING) {
        s_drag_update_resize(connection, client, dx, dy);
    }
}


/* Finish the drag on a button-release event */
void drag_end(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        struct position_s root_pos)
{
    if (!s_drag.is_active) {
        return;
    }

    if (s_drag.client != NULL) {
        /* In a solid drag, the real window already tracks
         * 'layout.geometry.cur' live, updated by every 'drag_update'
         * along the way; an outline drag never touches it until now,
         * so 'client_cur.dim.w'/'.h', kept live throughout instead
         * (see 'client_cur''s doc comment, drag/internal.h), are
         * what actually hold the final size here. */
        uint32_t final_w = s_drag.is_solid_drag
            ? s_drag.client->layout.geometry.cur.dim.w
            : (uint32_t) s_drag.client_cur.dim.w;
        uint32_t final_h = s_drag.is_solid_drag
            ? s_drag.client->layout.geometry.cur.dim.h
            : (uint32_t) s_drag.client_cur.dim.h;
        bool finalize_resize =
            s_drag.operation == CLIENT_OPERATION_RESIZING;

        if (s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window == s_drag.client->icon_window) {
            int32_t dx = root_pos.x -
                (int32_t) s_drag.pointer_start_x;
            int32_t dy = root_pos.y -
                (int32_t) s_drag.pointer_start_y;

            if (dx * dx + dy * dy < WM_ICON_DRAG_THRESHOLD) {
                /* Treat as a click: restore and focus */
                client_td *const ic = s_drag.client;
                enact_client_restore(ic);
                if (surface != NULL && desktop != NULL) {
                    focus_apply(NULL, surface, desktop, ic, true, NULL);
                }
            } else {
                int16_t new_icon_x =
                    (int16_t) (s_drag.client_start.pos.x + dx);
                int16_t new_icon_y =
                    (int16_t) (s_drag.client_start.pos.y + dy);
                struct geometry_s tray;
                bool pushed_out_of_tray = false;

                /* Kept off the tray's rectangle outright, rather
                 * than left there and relying on stacking alone to
                 * hide it: an icon dragged over the tray still left
                 * the tray's text missing in that exact span,
                 * even though the icon itself stayed correctly
                 * stacked below it throughout. */
                if (surface != NULL &&
                        systray_get_geometry(surface, &tray)) {
                    pushed_out_of_tray =
                        place_icon_avoid_systray_overlap(
                                &new_icon_x, &new_icon_y,
                                (struct dimensions_s) {
                                    WM_ICON_SQUARE_SIZE,
                                    WM_ICON_SQUARE_SIZE },
                                tray,
                                (desktop != NULL)
                                ? &desktop->workarea : NULL);
                }

                s_drag.client->icon_pos.x = new_icon_x;
                s_drag.client->icon_pos.y = new_icon_y;

                /* The drag itself only ever moved the icon window as
                 * far as the pointer's last position (see
                 * 'drag_update' above); an adjustment made here, after
                 * that already stopped, needs its explicit request
                 * to actually reach the window, or the icon would stay
                 * showing wherever the pointer dropped it while
                 * 'icon_pos' above already disagrees with what
                 * is on screen. */
                if (pushed_out_of_tray && connection != NULL) {

                    xcb_window_move(s_drag.client->icon_window,
                            new_icon_x, new_icon_y);
                }

                /* Restacking already happens on its own every second
                 * or so, driven by the systray's clock tick (see
                 * 'systray_layout_restack''s comment), so an icon
                 * dropped over the tray's area does not stay
                 * visually on top of it for long either way.  Forced
                 * here too, right as the icon settles into its final
                 * position, so there is no window at all, however
                 * brief, where it could still be showing over the
                 * tray. */
                systray_restack();

                /* Restore whatever mapped state the icon window had
                 * right before this drag began, the same value
                 * 'drag_icon_sync_active_visual' (drag_icon_start)
                 * forced to 'true' for the duration of the drag to
                 * keep it visible while being moved.  Scoped to this
                 * branch alone, rather than run unconditionally for
                 * every kind of drag this function ends: the click
                 * branch above already leaves 'is_icon_mapped' at
                 * the correct 'false' 'enact_client_restore' set,
                 * alongside 'icon_window' itself at zero; applying
                 * this same assignment there too would stamp 'true'
                 * straight back over it, since 'was_icon_mapped' was
                 * necessarily 'true' to begin dragging a mapped icon
                 * in the first place, leaving 'is_icon_mapped' true
                 * while 'icon_window' is already destroyed. */
                s_drag.client->is_icon_mapped = s_drag.was_icon_mapped;
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

        /* Icon drags never go through 'is_solid_drag'/the outline
         * machinery at all (see 'drag_icon_start''s comment,
         * intentionally always solid given how cheap moving just an
         * icon already is); their final position is already
         * fully settled by the icon-specific block above, so this
         * whole thing only applies to an actual client window drag. */
        if (s_drag.drag_window == XCB_WINDOW_NONE) {
            if (s_drag.is_solid_drag) {
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
                            (struct geometry_s) {
                                s_drag.client->layout.geometry.cur.pos,
                                { final_w, final_h } });
                }
            } else {
                /* An outline drag never touched the real window until
                 * now; apply the final position (and size, for a
                 * resize) in one single call, then destroy the 4
                 * strip windows that made up the outline stand-in.
                 * The resize path forces this call through rather
                 * than going via the normal 'enact_client_resize'
                 * (see 'ccmd_client_resize_force''s doc comment):
                 * with the real window left parked off screen for
                 * the whole drag (see 'drag_client_move_offscreen'),
                 * a client already mid-exchange from some earlier,
                 * unrelated resize would otherwise have this one
                 * single, final call silently queued behind that
                 * exchange's 'AlarmNotify' instead of applied,
                 * leaving it stuck off screen with no further call
                 * ever coming to retry it, unlike a solid drag's
                 * live sequence of many resize calls along the way. */
                if (finalize_resize) {
                    enact_client_resize_force(s_drag.client,
                            (struct geometry_s) {
                                s_drag.client_cur.pos,
                                { final_w, final_h } });
                } else {
                    enact_client_move(s_drag.client,
                            s_drag.client_cur.pos);
                }
                drag_outline_end(connection);
            }

            /* See 'drag_resist_axis_finalize''s doc comment,
             * drag/resist.h, for why this is only ever still needed
             * under '!is_solid_drag': 'drag_update' above already
             * handled the 'is_solid_drag' case fully, live, on every
             * single threshold crossing along the way.  Geometry
             * itself is already correctly settled by now, from the
             * exact same finalize calls just above; this only ever
             * updates 'properties.state' (and its EWMH atoms) to
             * match, never geometry a second time. */
            drag_resist_axis_finalize(finalize_resize);
        }
    }

    drag_overlay_hide(connection);
    s_drag.is_active = false;
    s_drag.operation = CLIENT_OPERATION_IDLE;
    s_drag.client = NULL;
    scmd_surface_viewport_drag_exclude(NULL);
    s_drag.desktop = NULL;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.was_icon_mapped = false;
    s_drag.is_warp_pending = false;
    s_drag.is_pan_pending = false;

    if (connection != NULL) {
        xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
        xcb_flush(connection);
    }
}


/* Cancel an in-progress drag when the dragged client disappears */
void drag_cancel(xcb_connection_t *connection, const client_td *client)
{
    if (!s_drag.is_active || s_drag.client != client) {
        return;
    }

    drag_overlay_hide(connection);
    /* Moved back from its off-screen parking spot first (see
     * 'drag_client_move_offscreen''s doc comment), to its
     * genuine, never-actually-changed logical position, for a client
     * that survives the cancel (see the comment on
     * 'properties.operation' just below, for exactly this same
     * distinction): without this, it would stay stuck off screen
     * forever, with nothing left to ever move it back. */
    if (s_drag.is_outline_offscreened && s_drag.client != NULL) {
        xcb_window_t target =
            (client_is_decorated(s_drag.client) &&
                s_drag.client->frame != 0)
            ? s_drag.client->frame
            : s_drag.client->window;
        xcb_window_move(target,
                s_drag.client->layout.geometry.cur.pos.x,
                s_drag.client->layout.geometry.cur.pos.y);
    }
    /* A no-op whenever no outline drag was in progress (solid drag,
     * or an icon drag, which is always solid regardless); otherwise
     * destroys the 4 strip windows, so a client that disappears
     * mid-drag never leaves them stuck on screen with nothing left
     * to ever remove them. */
    drag_outline_end(connection);
    s_drag.is_active = false;
    s_drag.operation = CLIENT_OPERATION_IDLE;
    s_drag.is_warp_pending = false;
    s_drag.is_pan_pending = false;
    if (s_drag.client != NULL) {
        /* The module-level 's_drag' bookkeeping above is reset either
         * way, but without this the client's OWN operation flag stays
         * stuck at 'MOVING'/'RESIZING' forever whenever a future caller
         * passes a client that survives the cancel. */
        s_drag.client->properties.operation = CLIENT_OPERATION_IDLE;
    }
    s_drag.client = NULL;
    scmd_surface_viewport_drag_exclude(NULL);
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
    return s_drag.is_active;
}


/* Move the real window being dragged in outline mode off screen, for
 * the duration of the drag; see the header's doc comment for the
 * full reasoning */
/* Return the client currently being dragged, or NULL */
client_td *drag_client(void)
{
    return s_drag.client;
}
