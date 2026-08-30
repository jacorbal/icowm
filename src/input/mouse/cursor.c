/**
 * @file input/mouse/cursor.c
 *
 * @brief Resize-border cursor loading and per-window application
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/cursor.h>

/* Default initial values */
#include <defs/cursor.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <input/mouse/bounds.h>
#include <input/mouse/cursor.h>
#include <input/mouse/internal.h>


/**
 * @brief Which border/corner zone of a client's bounding box a point
 *        falls within, if any
 */
enum s_resize_zone_e {
    S_RESIZE_ZONE_NONE = 0,
    S_RESIZE_ZONE_N,
    S_RESIZE_ZONE_S,
    S_RESIZE_ZONE_E,
    S_RESIZE_ZONE_W,
    S_RESIZE_ZONE_NE,
    S_RESIZE_ZONE_NW,
    S_RESIZE_ZONE_SE,
    S_RESIZE_ZONE_SW,
    S_RESIZE_ZONE_COUNT,    /**< Not a real zone; array size marker */
};

/**
 * One allocated cursor per @c s_resize_zone_e value; index 0 (@c NONE)
 * holds the plain left-pointer cursor
 *
 * Zero (@c XCB_CURSOR_NONE) until @c mouse_resize_cursors_init runs */
static xcb_cursor_t s_resize_cursors[S_RESIZE_ZONE_COUNT];

/**
 * The four-way move cursor; see @c mouse_cursor_move
 *
 * Zero (@c XCB_CURSOR_NONE) until @c mouse_resize_cursors_init runs */
static xcb_cursor_t s_move_cursor;


/**
 * @brief Determine which border/corner zone, if any, a point falls in
 *
 * Uses the same adaptive resize-grab margins as @a s_mouse_near_edge
 * (both call @a im_bounds_resize), so the cursor changes exactly where
 * a resize can actually start, including the same titlebar-row
 * exclusion for the left/right margins (see @a im_resize_bounds_td's
 * comment).  A titlebar button such as close, typically placed near the
 * frame's right edge, would otherwise still register as near that
 * edge, showing a resize cursor over it even though clicking it still
 * correctly closes the window rather than starting a resize (titlebar
 * buttons take priority over a border drag in the button-press handler
 * regardless of this function).
 *
 * @param client   Client whose geometry is used for the test
 * @param root_pos Pointer position in root-window coordinates
 *
 * @return The matching zone, or @c S_RESIZE_ZONE_NONE when @p root_pos
 *         falls outside every border zone (including entirely outside
 *         the client, its titlebar row, or its non-resizable interior)
 *
 * @note Complexity: @e O(1)
 */
static enum s_resize_zone_e s_mouse_resize_zone(const client_td *client,
        struct position_s root_pos)
{
    im_resize_bounds_td b;
    bool near_left;
    bool near_right;
    bool near_top;
    bool near_bottom;
    enum s_resize_zone_e zone;

    if (client == NULL) {
        return S_RESIZE_ZONE_NONE;
    }

    b = im_bounds_resize(client);

    /* Do not add bounds checks here.  The caller only invokes this
     * function for motion events already known to belong to this
     * client's frame or window, and an extra geometric check may
     * disagree with X11's actual border hit-testing by one or more
     * pixels.
     */
    near_left = root_pos.x < b.left + b.margin_left;
    near_right = root_pos.x >= b.right - b.margin_right;
    near_top = root_pos.y < b.top + b.margin_top;
    near_bottom = root_pos.y >= b.bottom - b.margin_bottom;

    if (b.has_titlebar_row &&
            root_pos.y >= b.titlebar_row_top &&
            root_pos.y < b.titlebar_row_bottom) {
        near_left = false;
        near_right = false;
    }

    if (near_top && near_left) { zone = S_RESIZE_ZONE_NW; }
    else if (near_top && near_right) { zone = S_RESIZE_ZONE_NE; }
    else if (near_bottom && near_left) { zone = S_RESIZE_ZONE_SW; }
    else if (near_bottom && near_right) { zone = S_RESIZE_ZONE_SE; }
    else if (near_top) { zone = S_RESIZE_ZONE_N; }
    else if (near_bottom) { zone = S_RESIZE_ZONE_S; }
    else if (near_left) { zone = S_RESIZE_ZONE_W; }
    else if (near_right) { zone = S_RESIZE_ZONE_E; }
    else { zone = S_RESIZE_ZONE_NONE; }

    return zone;
}


/* Load every resize-border cursor, plus the plain pointer, once */
void mouse_resize_cursors_init(xcb_connection_t *connection)
{
    xcb_screen_t *screen;
    util_cursor_ctx_td *ctx;

    if (connection == NULL ||
            s_resize_cursors[S_RESIZE_ZONE_NONE] != 0) {
        return;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    ctx = util_cursor_ctx_new(connection, screen);

    s_resize_cursors[S_RESIZE_ZONE_NONE] = util_cursor_load(ctx,
            "left_ptr", WM_CURSOR_LEFT_PTR_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_N] = util_cursor_load(ctx,
            "top_side", WM_CURSOR_TOP_SIDE_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_S] = util_cursor_load(ctx,
            "bottom_side", WM_CURSOR_BOTTOM_SIDE_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_E] = util_cursor_load(ctx,
            "right_side", WM_CURSOR_RIGHT_SIDE_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_W] = util_cursor_load(ctx,
            "left_side", WM_CURSOR_LEFT_SIDE_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_NE] = util_cursor_load(ctx,
            "top_right_corner", WM_CURSOR_TOP_RIGHT_CORNER_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_NW] = util_cursor_load(ctx,
            "top_left_corner", WM_CURSOR_TOP_LEFT_CORNER_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_SE] = util_cursor_load(ctx,
            "bottom_right_corner", WM_CURSOR_BOTTOM_RIGHT_CORNER_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_SW] = util_cursor_load(ctx,
            "bottom_left_corner", WM_CURSOR_BOTTOM_LEFT_CORNER_GLYPH);
    s_move_cursor = util_cursor_load(ctx, "fleur", WM_CURSOR_FLEUR_GLYPH);

    util_cursor_ctx_free(ctx);
}


/* Free the cursors created by 'mouse_resize_cursors_init' */
void mouse_resize_cursors_destroy(xcb_connection_t *connection)
{
    if (connection == NULL) {
        return;
    }

    for (int i = 0; i < (int) S_RESIZE_ZONE_COUNT; ++i) {
        if (s_resize_cursors[i] != 0) {
            xcb_free_cursor(connection, s_resize_cursors[i]);
            s_resize_cursors[i] = 0;
        }
    }

    if (s_move_cursor != 0) {
        xcb_free_cursor(connection, s_move_cursor);
        s_move_cursor = 0;
    }
}


/* The plain pointer cursor, shown for 'S_RESIZE_ZONE_NONE' too */
xcb_cursor_t mouse_plain_cursor(void)
{
    return s_resize_cursors[S_RESIZE_ZONE_NONE];
}


/* The four-way move cursor; see this function's comment in 'mouse.h' */
xcb_cursor_t mouse_cursor_move(void)
{
    return s_move_cursor;
}


/* The border-resize cursor matching a given resize drag's
 * axis/anchor combination; see this function's comment in 'mouse.h' for
 * what each parameter means */
xcb_cursor_t mouse_resize_cursor_for_axes(bool resize_w, bool resize_h,
        bool anchor_right, bool anchor_bottom)
{
    enum s_resize_zone_e zone;

    if (resize_w && resize_h) {
        zone = (anchor_right)
            ? ((anchor_bottom) ? S_RESIZE_ZONE_NW : S_RESIZE_ZONE_SW)
            : ((anchor_bottom) ? S_RESIZE_ZONE_NE : S_RESIZE_ZONE_SE);
    } else if (resize_w) {
        zone = (anchor_right) ? S_RESIZE_ZONE_W : S_RESIZE_ZONE_E;
    } else if (resize_h) {
        zone = (anchor_bottom) ? S_RESIZE_ZONE_N : S_RESIZE_ZONE_S;
    } else {
        zone = S_RESIZE_ZONE_NONE;
    }

    return s_resize_cursors[zone];
}


/**
 * @brief Recompute and apply the resize-border cursor for a client
 *        window at a given pointer position
 *
 * Shared by @c mouse_handle_motion_hover (every pointer motion),
 * @c mouse_handle_enter (every time the pointer crosses into a new
 * window), and @c mouse_hover_poll_tick (a periodic fallback poll;
 * see its comment for why one is needed at all), since any one kind of
 * event or poll can be the only signal a given transition actually
 * produces: a client that selects @c PointerMotion for its purposes
 * (common in GTK/Qt applications tracking hover for their UI)
 * intercepts motion events before they propagate to whichever window
 * this logic is watching, leaving @c EnterNotify as the only remaining
 * signal for a decorated client (where hovering the frame's border
 * and then crossing into the client's child window is what needs
 * catching); an undecorated client has no separate frame to fall back
 * on at all, so moving from its border to its interior happens within
 * one single window with no crossing whatsoever, leaving periodic
 * polling as the only remaining option.
 *
 * @param connection XCB connection
 * @param surfaces   Every managed surface, to look up the client
 *                   @p window belongs to
 * @param window     Window the crossing, motion, or poll was
 *                   evaluated for
 * @param root_pos   Pointer position in root-window coordinates
 *
 * @return The resolved client @p window belongs to, or @c NULL if it
 *         does not belong to a resizable client
 *
 * @note Complexity: @e O(1)
 */
client_td *mouse_resize_cursor_update(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t window,
        struct position_s root_pos)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    enum s_resize_zone_e zone;

    if (connection == NULL || surfaces == NULL ||
            s_resize_cursors[S_RESIZE_ZONE_NONE] == 0) {
        return NULL;
    }

    client = lookup_find_client(surfaces, window, &surface, &desktop);
    if (client == NULL || !client_is_resizable(client)) {
        LOGGER_TRACE("No resizable client for resize cursor" \
                " (window=0x%x, root=%+d%+d, client=%p)",
                window, root_pos.x, root_pos.y, (void *) client);
        return NULL;
    }

    zone = s_mouse_resize_zone(client, root_pos);

    LOGGER_TRACE("Set resize cursor (window=0x%x, client-window=0x%x," \
            " frame=0x%x, root=%+d%+d, zone=%d, cursor=0x%x)",
            window, client->window, client->frame,
            root_pos.x, root_pos.y,
            (int) zone, s_resize_cursors[zone]);

    xcb_change_window_attributes(connection, window,
            XCB_CW_CURSOR,
            (const uint32_t[]) { s_resize_cursors[zone] });

    return client;
}
