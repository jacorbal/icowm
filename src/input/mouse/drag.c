/**
 * @file input/mouse/drag.c
 *
 * @brief Mouse drag-operation state and implementation
 *
 * Manages the singleton drag state used by the move/resize and
 * icon-drag interactions.  All mutable drag state is @c static in
 * this translation unit; no other module accesses it directly.
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
#include <render/text.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Menu includes */
#include <menu/notify/desktop.h>

/* Local includes */
#include <input/mouse.h>
#include <input/mouse/drag.h>
#include <input/mouse/bounds.h>


/**
 * @brief Singleton drag state
 */
static struct {
    bool active;
    enum window_operation_e operation;
    client_td *client;
    desktop_td *desktop;
    xcb_window_t drag_window;   /**< Icon window moved, or
                                 *   'XCB_WINDOW_NONE' for normal drag */
    int16_t pointer_start_x;
    int16_t pointer_start_y;
    int32_t client_start_x;
    int32_t client_start_y;
    uint16_t client_start_w;
    uint16_t client_start_h;
    uint32_t screen_w;          /**< Screen width for edge snap */
    uint32_t screen_h;          /**< Screen height for edge snap */
    uint32_t snap;              /**< Snap distance in pixels */
    int32_t client_cur_x;       /**< Current X during drag (updated each
                                     motion notify event) */
    int32_t client_cur_y;       /**< Current Y during drag */
    bool anchor_right;          /**< Resize: right edge is fixed (resize
                                     from left) */
    bool anchor_bottom;         /**< Resize: bottom edge is fixed (resize
                                     from top) */
    bool resize_w;              /**< Resize: width is actively being
                                     changed in this drag */
    bool resize_h;              /**< Resize: height is actively being
                                     changed in this drag */
    xcb_window_t overlay_window;/**< Centered feedback overlay window */
    bool overlay_is_icon;       /**< Overlay belongs to icon drag */
    char overlay_text[32];      /**< Current overlay text */
    bool icon_was_mapped;       /**< Original icon mapped state before
                                     drag */
    int16_t last_root_x;        /**< Root-relative pointer position
                                     'drag_update' last actually acted
                                     on, so a duplicate 'MotionNotify'
                                     reporting the same position (the X
                                     server can deliver one right after
                                     a grab starts under an
                                     already-resting pointer) is
                                     skipped rather than repeating the
                                     same 'xcb_configure_window' and
                                     'xcb_flush' for no visible change;
                                     meaningless until 'has_last_pos' */
    int16_t last_root_y;        /**< See 'last_root_x' */
    bool has_last_pos;          /**< Whether 'last_root_x'/'last_root_y'
                                     hold a real prior position yet;
                                     false right after 'drag_start' so
                                     its first 'drag_update' always
                                     runs regardless of position */
    bool warp_pending;          /**< Whether the pointer is currently
                                     held against a warp-eligible
                                     screen edge, counting down to a
                                     desktop switch (see 'desktops.warp'
                                     in config.json, config_desktop_s) */
    bool warp_is_left;          /**< Which edge, only meaningful when
                                     'warp_pending' */
    struct timespec warp_due;   /**< When the held edge becomes due to
                                     warp, only meaningful when
                                     'warp_pending' */
} s_drag = {
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
    .warp_due = {0}
};


/**
 * @brief Synchronizes the active visual of the drag icon window.
 *
 * Thin wrapper over @c ri_render_client_icon_selected (render/icon.c),
 * the same "currently selected" render the icon cycle menu uses for
 * exactly this reason: active colors, caption, and hint indicators,
 * pixmap deliberately left out.  Kept as its own named function here
 * (rather than calling that one directly from every drag-start call
 * site) purely for the descriptive name at each call site; no logic
 * of its own remains to drift out of sync with the shared one now
 * that both need the exact same render.
 *
 * @param connection XCB connection
 */
static void s_drag_sync_icon_active_visual(xcb_connection_t *connection)
{
    ri_render_client_icon_selected(connection, s_drag.client);
}


/**
 * @brief Clamp a 32-bit unsigned value to the 16-bit range
 *
 * Returns @p value converted to @c uint16_t, saturating to
 * @c UINT16_MAX if the input exceeds the maximum 16-bit unsigned value.
 *
 * @param value Unsigned 32-bit value to clamp
 *
 * @return Clamped 16-bit unsigned value
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_drag_u16_sat(uint32_t value)
{
    return (value > UINT16_MAX) ? UINT16_MAX : (uint16_t) value;
}


/**
 * @brief Return the full icon-window height for a dragged client
 *
 * Computes the icon height from the base icon square size and adds the
 * caption height when the client theme uses captioned icons.
 *
 * @param client Client whose icon height is requested
 *
 * @return Total icon-window height in pixels
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_drag_icon_height(const client_td *client)
{
    if (client == NULL || client->theme == NULL) {
        return (uint16_t) WM_ICON_SQUARE_SIZE;
    }

    return (uint16_t) (WM_ICON_SQUARE_SIZE +
            ((client->theme->icon.is_captioned)
                ? WM_ICON_CAPTION_HEIGHT
                : 0u));
}


/**
 * @brief Compute the centered overlay position for a target rectangle
 *
 * Centers an overlay of size @p overlay_w by @p overlay_h within the
 * target rectangle and stores the resulting top-left coordinates in
 * @p out_x and @p out_y.  Negative coordinates are clamped to zero
 * before conversion to @c int16_t.
 *
 * @param target_x  Left coordinate of the target rectangle
 * @param target_y  Top coordinate of the target rectangle
 * @param target_w  Width of the target rectangle
 * @param target_h  Height of the target rectangle
 * @param overlay_w Width of the overlay rectangle
 * @param overlay_h Height of the overlay rectangle
 * @param out_x     Computed overlay X coordinate
 * @param out_y     Computed overlay Y coordinate
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_overlay_rect(int32_t target_x, int32_t target_y,
        uint16_t target_w, uint16_t target_h,
        uint16_t overlay_w, uint16_t overlay_h,
        int16_t *out_x, int16_t *out_y)
{
    int32_t centered_x;
    int32_t centered_y;

    centered_x = target_x +
        ((int32_t) target_w - (int32_t) overlay_w) / 2;
    centered_y = target_y +
        ((int32_t) target_h - (int32_t) overlay_h) / 2;

    if (centered_x < 0) {
        centered_x = 0;
    }
    if (centered_y < 0) {
        centered_y = 0;
    }

    *out_x = (centered_x < INT16_MIN) ? INT16_MIN
        : (centered_x > INT16_MAX) ? INT16_MAX
        : (int16_t) centered_x;
    *out_y = (centered_y < INT16_MIN) ? INT16_MIN
        : (centered_y > INT16_MAX) ? INT16_MAX
        : (int16_t) centered_y;
}


/**
 * @brief Destroy and reset the active drag overlay window
 *
 * Destroys the overlay window if it exists and clears the associated
 * overlay state.
 *
 * @param connection XCB connection used to destroy the overlay window
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_overlay_hide(xcb_connection_t *connection)
{
    if (connection != NULL && s_drag.overlay_window != XCB_WINDOW_NONE) {
        xcb_destroy_window(connection, s_drag.overlay_window);
    }

    s_drag.overlay_window = XCB_WINDOW_NONE;
    s_drag.overlay_is_icon = false;
    s_drag.overlay_text[0] = '\0';
}


/**
 * @brief Show or reposition the drag overlay window
 *
 * Updates the overlay text and mode, computes a centered overlay
 * rectangle for the given target geometry, and either creates the
 * overlay window or moves and resizes the existing one before
 * repainting it.
 *
 * @param connection XCB connection used to manage the overlay window
 * @param is_icon    Whether the overlay should use the active icon theme
 * @param target_x   Left coordinate of the target rectangle
 * @param target_y   Top coordinate of the target rectangle
 * @param target_w   Width of the target rectangle
 * @param target_h   Height of the target rectangle
 * @param text       Overlay text to display
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_overlay_show(xcb_connection_t *connection,
        bool is_icon,
        int32_t target_x, int32_t target_y,
        uint16_t target_w, uint16_t target_h,
        const char *text)
{
    uint16_t text_w;
    uint16_t overlay_w;
    int16_t overlay_x;
    int16_t overlay_y;
    uint16_t create_mask;
    uint32_t create_values[4];

    if (connection == NULL || s_drag.client == NULL || text == NULL ||
            text[0] == '\0') {
        return;
    }

    (void) snprintf(s_drag.overlay_text, sizeof(s_drag.overlay_text),
            "%s", text);
    s_drag.overlay_is_icon = is_icon;

    (void) text_renderer_init(connection,
            (is_icon)
                ? s_drag.client->theme->icon.active.font
                : s_drag.client->theme->window.active.font);
    text_w = text_measure_string(s_drag.overlay_text);
    overlay_w = (uint16_t) (text_w + 2u * WM_DRAG_OVERLAY_PAD_X);
    if (overlay_w < WM_DRAG_OVERLAY_MIN_WIDTH) {
        overlay_w = WM_DRAG_OVERLAY_MIN_WIDTH;
    }

    s_drag_overlay_rect(target_x, target_y, target_w, target_h,
            overlay_w, WM_DRAG_OVERLAY_HEIGHT, &overlay_x, &overlay_y);

    if (s_drag.overlay_window == XCB_WINDOW_NONE) {
        s_drag.overlay_window = xcb_generate_id(connection);
        create_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
            XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
        create_values[0] = (is_icon)
            ? s_drag.client->theme->icon.active.color.background
            : s_drag.client->theme->window.active.color.background;
        create_values[1] = (is_icon)
            ? s_drag.client->theme->icon.active.border.color
            : s_drag.client->theme->window.active.border.color;
        create_values[2] = 1u;
        create_values[3] = XCB_EVENT_MASK_EXPOSURE;

        xcb_create_window(connection,
                XCB_COPY_FROM_PARENT,
                s_drag.overlay_window,
                s_drag.client->parent_id,
                overlay_x, overlay_y,
                overlay_w, WM_DRAG_OVERLAY_HEIGHT,
                1,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                XCB_COPY_FROM_PARENT,
                create_mask, create_values);
        xcb_map_window(connection, s_drag.overlay_window);
    } else {
        xcb_configure_window(connection, s_drag.overlay_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const uint32_t[]) {
                    (uint32_t) overlay_x,
                    (uint32_t) overlay_y,
                    overlay_w,
                    WM_DRAG_OVERLAY_HEIGHT,
                    XCB_STACK_MODE_ABOVE
                });
    }

    drag_repaint_overlay(connection);
    xcb_flush(connection);
}


/**
 * @brief Compute the absolute value of a 32-bit signed integer
 *
 * Returns the non-negative magnitude of the given value.
 *
 * @param value Input integer
 *
 * @return Absolute value of @p value
 *
 * @note Complexity: @e O(1)
 */
static int32_t s_drag_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}


/**
 * @brief Select the delta with the smaller absolute magnitude
 *
 * Compares two deltas and returns the one whose absolute value
 * is smaller, preserving its original sign.
 *
 * @param current   Current best delta
 * @param candidate Candidate delta to compare
 *
 * @return The delta with the smaller absolute value
 *
 * @note Complexity: @e O(1)
 */
static int32_t s_drag_closer_delta(int32_t current, int32_t candidate)
{
    if (s_drag_abs_i32(candidate) < s_drag_abs_i32(current)) {
        return candidate;
    }

    return current;
}


/**
 * @brief Check whether two 1-D ranges overlap or are within snap
 *        distance
 *
 * Determines if two intervals either overlap or are closer than a given
 * snapping threshold, allowing near-alignment behavior.
 *
 * @param start_a Start of first range
 * @param end_a   End of first range
 * @param start_b Start of second range
 * @param end_b   End of second range
 * @param snap    Maximum allowed gap for ranges to be considered
 *                "close"
 *
 * @return @c true if ranges overlap or are within @p snap distance,
 *         otherwise @c false
 *
 * @note Complexity: @e O(1)
 */
static bool s_drag_ranges_close(int32_t start_a, int32_t end_a,
        int32_t start_b, int32_t end_b, int32_t snap)
{
    return !(end_a < start_b - snap || end_b < start_a - snap);
}


/**
 * @brief Apply snapping behavior during client movement
 *
 * Adjusts the proposed position of a moving client so it "snaps" to
 * nearby window edges or screen boundaries when within a configurable
 * threshold.  It compares the moving window against other visible,
 * non-iconified clients on the same desktop and computes the smallest
 * adjustment needed to align edges.
 *
 * Snapping is applied independently along both axes and also considers
 * screen edges if available.
 *
 * @param x      Pointer to the proposed X coordinate (updated in place)
 * @param y      Pointer to the proposed Y coordinate (updated in place)
 * @param width  Width of the moving client
 * @param height Height of the moving client
 *
 * @note Requires a valid global @c s_drag context
 * @note Complexity: @e O(n), where @e n is the number of clients in the
 *       stacking list
 */
static void s_drag_snap_move(int32_t *x, int32_t *y,
        uint32_t width, uint32_t height)
{
    int32_t snap;
    int32_t right;
    int32_t bottom;

    if (x == NULL || y == NULL || s_drag.snap == 0) {
        return;
    }

    snap = (int32_t) s_drag.snap;
    right = *x + (int32_t) width;
    bottom = *y + (int32_t) height;

    if (s_drag.desktop != NULL && s_drag.desktop->stacking != NULL &&
            cdlist_size(s_drag.desktop->stacking) > 0) {
        cdlist_item_td *node;
        cdlist_item_td *initial;
        /* One past 'snap' itself, not 'snap' itself: 'abs(delta) <=
         * snap' below is what decides whether a candidate actually
         * applies, so starting exactly at 'snap' would make that
         * check pass on the untouched initial value alone whenever no
         * real candidate ever beat it, applying a spurious snap of
         * exactly the snap distance with no nearby window at all
         * responsible for it. */
        int32_t dx = snap + 1;
        int32_t dy = snap + 1;

        node = cdlist_head(s_drag.desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);

                if (other != NULL && other != s_drag.client &&
                        !client_is_hidden(other) &&
                        !client_is_iconified(other)) {
                    int32_t ox = other->layout.geometry.cur.pos.x;
                    int32_t oy = other->layout.geometry.cur.pos.y;
                    int32_t oright = ox +
                        (int32_t) other->layout.geometry.cur.dim.w;
                    int32_t obottom = oy +
                        (int32_t) other->layout.geometry.cur.dim.h;

                    if (s_drag_ranges_close(*y, bottom, oy,
                                obottom, snap)) {
                        dx = s_drag_closer_delta(dx, oright - *x);
                        dx = s_drag_closer_delta(dx, oright - right);
                        dx = s_drag_closer_delta(dx, ox - right);
                        dx = s_drag_closer_delta(dx, ox - *x);
                    }

                    if (s_drag_ranges_close(*x, right, ox,
                                oright, snap)) {
                        dy = s_drag_closer_delta(dy, obottom - *y);
                        dy = s_drag_closer_delta(dy, obottom - bottom);
                        dy = s_drag_closer_delta(dy, oy - bottom);
                        dy = s_drag_closer_delta(dy, oy - *y);
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }

        LOGGER_TRACE("Move snap candidates (x=%d, y=%d, right=%d," \
                " bottom=%d, dx=%d, dy=%d, snap=%d)",
                *x, *y, right, bottom, dx, dy, snap);

        if (s_drag_abs_i32(dx) <= snap) {
            *x += dx;
            right += dx;
        }

        if (s_drag_abs_i32(dy) <= snap) {
            *y += dy;
            bottom += dy;
        }
    }

    if (s_drag.screen_w > 0 &&
            s_drag_abs_i32(*x) <= snap) {
        right -= *x;
        *x = 0;
    }

    if (s_drag.screen_h > 0 &&
            s_drag_abs_i32(*y) <= snap) {
        bottom -= *y;
        *y = 0;
    }

    if (s_drag.screen_w > 0 &&
            s_drag_abs_i32(right -
                (int32_t) s_drag.screen_w) <= snap) {
        *x = (int32_t) s_drag.screen_w - (int32_t) width;
    }

    if (s_drag.screen_h > 0 &&
            s_drag_abs_i32(bottom -
                (int32_t) s_drag.screen_h) <= snap) {
        *y = (int32_t) s_drag.screen_h - (int32_t) height;
    }
}


/* Begin a drag operation for a managed client window */
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

    s_drag_overlay_hide(connection);
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
        s_drag.anchor_right  = false;
        s_drag.anchor_bottom = false;
        s_drag.resize_w = false;
        s_drag.resize_h = false;
    }

    client->properties.operation = (uint16_t) operation;

    /* A user-initiated move overrides any rule-assigned position */
    if (operation == CLIENT_OPERATION_MOVING) {
        client->rule_position_locked = false;
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

    s_drag_overlay_hide(connection);
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
    s_drag.screen_w = screen_w;
    s_drag.screen_h = screen_h;
    s_drag.icon_was_mapped = client->is_icon_mapped;
    s_drag.anchor_right  = false;
    s_drag.anchor_bottom = false;
    s_drag.resize_w = false;
    s_drag.resize_h = false;
    s_drag.has_last_pos = false;
    s_drag.warp_pending = false;

    client->properties.operation = CLIENT_OPERATION_MOVING;
    client->is_icon_mapped = true;
    s_drag_sync_icon_active_visual(connection);

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


/* Snap a resized client against peer windows and screen edges */
static void s_drag_snap_resize(int32_t *x, int32_t *y,
        uint32_t *width, uint32_t *height)
{
    int32_t snap;
    int32_t right;
    int32_t bottom;

    if (x == NULL || y == NULL || width == NULL || height == NULL ||
            s_drag.snap == 0) {
        return;
    }

    snap = (int32_t) s_drag.snap;
    right = *x + (int32_t) *width;
    bottom = *y + (int32_t) *height;

    /* Which edge actually moves as the pointer moves depends on which
     * corner or side the user grabbed: 'anchor_right' means the LEFT
     * edge is the one being dragged (the right edge stays put), and
     * symmetrically for 'anchor_bottom' and the top edge.  Every delta
     * and snap check below has to target whichever edge that is, not
     * always assume it is the right/bottom edge the way a
     * left-edge-fixed resize would. */
    if (s_drag.desktop != NULL && s_drag.desktop->stacking != NULL &&
            cdlist_size(s_drag.desktop->stacking) > 0) {
        cdlist_item_td *node;
        cdlist_item_td *initial;
        /* See the matching comment in 's_drag_snap_move' for why this
         * is 'snap + 1', not 'snap' itself. */
        int32_t d_horiz = snap + 1;
        int32_t d_vert = snap + 1;

        node = cdlist_head(s_drag.desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);

                if (other != NULL && other != s_drag.client &&
                        !client_is_hidden(other) &&
                        !client_is_iconified(other)) {
                    int32_t ox = other->layout.geometry.cur.pos.x;
                    int32_t oy = other->layout.geometry.cur.pos.y;
                    int32_t oright = ox +
                        (int32_t) other->layout.geometry.cur.dim.w;
                    int32_t obottom = oy +
                        (int32_t) other->layout.geometry.cur.dim.h;

                    if (s_drag_ranges_close(*y, bottom,
                                oy, obottom, snap)) {
                        if (s_drag.anchor_right) {
                            d_horiz = s_drag_closer_delta(d_horiz,
                                    oright - *x);
                            d_horiz = s_drag_closer_delta(d_horiz,
                                    ox - *x);
                        } else {
                            d_horiz = s_drag_closer_delta(d_horiz,
                                    oright - right);
                            d_horiz = s_drag_closer_delta(d_horiz,
                                    ox - right);
                        }
                    }

                    if (s_drag_ranges_close(*x, right,
                                ox, oright, snap)) {
                        if (s_drag.anchor_bottom) {
                            d_vert = s_drag_closer_delta(d_vert,
                                    obottom - *y);
                            d_vert = s_drag_closer_delta(d_vert,
                                    oy - *y);
                        } else {
                            d_vert = s_drag_closer_delta(d_vert,
                                    obottom - bottom);
                            d_vert = s_drag_closer_delta(d_vert,
                                    oy - bottom);
                        }
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }

        LOGGER_TRACE("Resize snap candidates (x=%d, y=%d, right=%d," \
                " bottom=%d, anchor-right=%d, anchor-bottom=%d," \
                " d-horiz=%d, d-vert=%d, snap=%d)",
                *x, *y, right, bottom,
                (int) s_drag.anchor_right, (int) s_drag.anchor_bottom,
                d_horiz, d_vert, snap);

        if (s_drag_abs_i32(d_horiz) <= snap) {
            if (s_drag.anchor_right) {
                *x += d_horiz;
                *width = geom_clamp_dim((int32_t) *width - d_horiz);
            } else {
                *width = geom_clamp_dim((int32_t) *width + d_horiz);
            }
            right = *x + (int32_t) *width;
        }

        if (s_drag_abs_i32(d_vert) <= snap) {
            if (s_drag.anchor_bottom) {
                *y += d_vert;
                *height = geom_clamp_dim((int32_t) *height - d_vert);
            } else {
                *height = geom_clamp_dim((int32_t) *height + d_vert);
            }
            bottom = *y + (int32_t) *height;
        }
    }

    if (s_drag.screen_w > 0) {
        if (s_drag.anchor_right) {
            /* Dragging the left edge: it can snap to the screen's own
             * left edge, which a resize never checked for before. */
            if (s_drag_abs_i32(*x) <= snap) {
                *width = geom_clamp_dim((int32_t) *width + *x);
                *x = 0;
            }
        } else if (s_drag_abs_i32(right -
                    (int32_t) s_drag.screen_w) <= snap) {
            *width = geom_clamp_dim((int32_t) s_drag.screen_w - *x);
        }
    }

    if (s_drag.screen_h > 0) {
        if (s_drag.anchor_bottom) {
            /* Dragging the top edge: same reasoning as the left edge
             * above, snapping to the screen's own top edge. */
            if (s_drag_abs_i32(*y) <= snap) {
                *height = geom_clamp_dim((int32_t) *height + *y);
                *y = 0;
            }
        } else if (s_drag_abs_i32(bottom -
                    (int32_t) s_drag.screen_h) <= snap) {
            *height = geom_clamp_dim((int32_t) s_drag.screen_h - *y);
        }
    }
}


/**
 * @brief Update the pending warp state from the pointer's current
 *        root-relative X position during a window or icon move
 *
 * Starts (or keeps running, without restarting it) a countdown to
 * switching desktops when the pointer is held against the left or
 * right screen edge, per @c desktops.warp in config.json (see @c
 * config_desktop_s and @c drag_warp_tick, which actually performs the
 * switch once the countdown elapses); cancels it the moment the
 * pointer leaves either edge, or when warping is disabled, there is
 * only one desktop, or no configuration can be resolved at all.
 *
 * @param root_x Pointer's current root-relative X position
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_check_warp_edge(int16_t root_x)
{
    surface_td *surface;
    bool at_left;
    bool at_right;

    if (s_drag.client == NULL) {
        s_drag.warp_pending = false;
        return;
    }

    surface = wm_get_surface_by_id(s_drag.client->screen_id);
    if (surface == NULL || surface->config == NULL ||
            !surface->config->desktops.warp ||
            surface->desktop_count <= 1u) {
        s_drag.warp_pending = false;
        return;
    }

    at_left = root_x <= 0;
    at_right = (int32_t) root_x >= (int32_t) s_drag.screen_w - 1;

    if (!at_left && !at_right) {
        s_drag.warp_pending = false;
        return;
    }

    if (s_drag.warp_pending && s_drag.warp_is_left == at_left) {
        /* Same edge still held: let the existing countdown keep
         * running rather than restarting it on every motion event. */
        return;
    }

    s_drag.warp_pending = true;
    s_drag.warp_is_left = at_left;
    if (clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due) == 0) {
        s_drag.warp_due.tv_nsec +=
            (long) WM_DESKTOP_WARP_DELAY_MS * 1000000L;
        if (s_drag.warp_due.tv_nsec >= 1000000000L) {
            s_drag.warp_due.tv_sec += 1;
            s_drag.warp_due.tv_nsec -= 1000000000L;
        }
    } else {
        /* Could not read the clock to schedule the countdown; safer
         * to not warp at all than to warp immediately on every edge
         * touch. */
        s_drag.warp_pending = false;
    }
}


/* Update the in-progress drag on a motion-notify event */
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
    dx = (int32_t) root_x - (int32_t) s_drag.pointer_start_x;
    dy = (int32_t) root_y - (int32_t) s_drag.pointer_start_y;

    if (s_drag.operation == CLIENT_OPERATION_MOVING &&
            s_drag.drag_window != XCB_WINDOW_NONE &&
            s_drag.drag_window == client->icon_window) {
        char geom_buf[24];
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
            (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                    (int) new_x, (int) new_y);
            s_drag_overlay_show(connection, true,
                    new_x, new_y,
                    (uint16_t) WM_ICON_SQUARE_SIZE,
                    s_drag_icon_height(client),
                    geom_buf);
        } else {
            s_drag_overlay_hide(connection);
        }
        s_drag_check_warp_edge(root_x);
        xcb_flush(connection);
    } else if (s_drag.operation == CLIENT_OPERATION_MOVING) {
        char geom_buf[24];
        bool show_geom = client->config_base != NULL &&
            client->config_base->windows.show_geom;
        int32_t new_x = s_drag.client_start_x + dx;
        int32_t new_y = s_drag.client_start_y + dy;

        s_drag_snap_move(&new_x, &new_y,
                s_drag.client_start_w, s_drag.client_start_h);

        s_drag.client_cur_x = new_x;
        s_drag.client_cur_y = new_y;
        enact_client_move(client, new_x, new_y);
        if (show_geom) {
            (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                    (int) new_x, (int) new_y);
            s_drag_overlay_show(connection, false,
                    new_x, new_y,
                    s_drag.client_start_w, s_drag.client_start_h,
                    geom_buf);
        } else {
            s_drag_overlay_hide(connection);
        }
        s_drag_check_warp_edge(root_x);
    } else if (s_drag.operation == CLIENT_OPERATION_RESIZING) {
        char geom_buf[24];
        bool show_geom = client->config_base != NULL &&
            client->config_base->windows.show_geom;
        int32_t new_x = s_drag.client_start_x;
        int32_t new_y = s_drag.client_start_y;
        uint32_t new_w;
        uint32_t new_h;

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

        s_drag_snap_resize(&new_x, &new_y, &new_w, &new_h);

        s_drag.client_cur_x = new_x;
        s_drag.client_cur_y = new_y;
        enact_client_resize(client, new_x, new_y, new_w, new_h);
        if (show_geom) {
            if (client->size_hints.inc_w > 1 &&
                    client->size_hints.inc_h > 1) {
                /* ICCCM 4.1.2.3: falls back to MIN_SIZE as the grid base */
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
                uint32_t cols = ((new_w > base_w)
                        ? (new_w - base_w) : 0u) / inc_w;
                uint32_t lines = ((new_h > base_h)
                        ? (new_h - base_h) : 0u) / inc_h;

                /* Cell count ('cols x lines') for a terminal-like client */
                (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                        cols, lines);
                /* Raw pixel dimensions */
                /*
                (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                        new_w, new_h);
                */
            } else {
                /* Raw pixel dimensions */
                (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                        new_w, new_h);
            }
            s_drag_overlay_show(connection, false,
                    new_x, new_y,
                    s_drag_u16_sat(new_w), s_drag_u16_sat(new_h),
                    geom_buf);
        } else {
            s_drag_overlay_hide(connection);
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
        uint32_t final_w = s_drag.client->layout.geometry.cur.dim.w;
        uint32_t final_h = s_drag.client->layout.geometry.cur.dim.h;
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
                            (desktop != NULL) ? &desktop->workarea : NULL);
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
                 * 'systray_layout_restack''s own doc comment), so an
                 * icon dropped over the tray's own area does not stay
                 * visually on top of it for long either way.  Forced
                 * here too, right as the icon settles into its final
                 * position, so there is no window at all, however
                 * brief, where it could still be showing over the
                 * tray. */
                systray_restack();
            }
        }

        s_drag.client->properties.operation = CLIENT_OPERATION_IDLE;
        s_drag.client->is_icon_mapped = s_drag.icon_was_mapped;

        if (connection != NULL && s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window == s_drag.client->icon_window) {
            xcb_clear_area(connection, 0, s_drag.client->icon_window,
                    0, 0, 0, 0);

            /* Request a full repaint so the icon returns to its normal
             * (inactive) appearance after being shown in active colors
             * during the drag */
            wm_request_client_redraw(s_drag.client);
        }

        if (finalize_resize) {
            enact_client_resize(s_drag.client,
                    s_drag.client->layout.geometry.cur.pos.x,
                    s_drag.client->layout.geometry.cur.pos.y,
                    final_w, final_h);
        }
    }

    s_drag_overlay_hide(connection);
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

    s_drag_overlay_hide(connection);
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


/* Query whether the active drag window matches the overlay window */
bool drag_is_overlay_window(xcb_window_t window)
{
    return s_drag.overlay_window != XCB_WINDOW_NONE &&
        window == s_drag.overlay_window;
}


/* Return the client currently being dragged, or NULL */
client_td *drag_client(void)
{
    return s_drag.client;
}


/* Repaint the active drag overlay window */
void drag_repaint_overlay(xcb_connection_t *connection)
{
    uint32_t bg;
    uint32_t fg;
    uint32_t border;
    const char *font_name;
    uint16_t text_w;
    int16_t text_x;

    if (connection == NULL ||
            s_drag.overlay_window == XCB_WINDOW_NONE ||
            s_drag.client == NULL || s_drag.client->theme == NULL ||
            s_drag.overlay_text[0] == '\0') {
        return;
    }

    if (s_drag.overlay_is_icon) {
        bg = s_drag.client->theme->icon.active.color.background;
        fg = s_drag.client->theme->icon.active.color.foreground;
        border = s_drag.client->theme->icon.active.border.color;
        font_name = s_drag.client->theme->icon.active.font;
    } else {
        bg = s_drag.client->theme->window.active.color.background;
        fg = s_drag.client->theme->window.active.color.foreground;
        border = s_drag.client->theme->window.active.border.color;
        font_name = s_drag.client->theme->window.active.font;
    }

    xcb_change_window_attributes(connection, s_drag.overlay_window,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) { bg, border });
    xcb_clear_area(connection, 0, s_drag.overlay_window, 0, 0, 0, 0);

    (void) text_renderer_init(connection, font_name);
    text_renderer_set_color(fg, bg);

    text_w = text_measure_string(s_drag.overlay_text);
    /* Horizontally centered within the overlay window's own actual
     * width, computed with the exact same formula 's_drag_overlay_
     * show' used to size that window in the first place, rather than
     * a separately hardcoded threshold that happened to only agree
     * with it for a wide-enough or narrow-enough string.  Those two
     * thresholds ('text_w + 2*PAD_X < MIN_WIDTH' here versus 'text_w
     * < MIN_WIDTH' in the box-sizing formula) disagreeing for a
     * string in between the two -- long enough to push the box wider
     * than 'MIN_WIDTH', but still short enough of 'MIN_WIDTH' itself
     * to take the "narrow" branch here -- is what left text looking
     * pinned to the left with a lopsided gap on the right (worst for
     * a string a few pixels short of exactly 'MIN_WIDTH', which could
     * end up with zero left margin at all): computing the box's own
     * width the same way here removes the mismatch entirely, for any
     * string length, not just the ones on either side of it that
     * happened not to expose the bug. */
    {
        uint16_t overlay_w = (uint16_t) (text_w + 2u * WM_DRAG_OVERLAY_PAD_X);

        if (overlay_w < WM_DRAG_OVERLAY_MIN_WIDTH) {
            overlay_w = WM_DRAG_OVERLAY_MIN_WIDTH;
        }
        text_x = (int16_t) ((overlay_w - text_w) / 2u);
    }

    /* Vertically centered baseline for whatever font this theme
     * actually configures, rather than a single Y hardcoded for one
     * particular font size: see 'text_font_ascent's own doc comment
     * in render/text.h for the derivation (ascent placed 'top' pixels
     * below the box's own top edge, here with 'top' itself computed
     * from ascent/descent so half the leftover vertical space sits on
     * each side). */
    {
        int16_t ascent = text_font_ascent();
        int16_t descent = text_font_descent();
        int16_t text_y = (int16_t)
            (((int32_t) WM_DRAG_OVERLAY_HEIGHT + ascent - descent) / 2);

        text_draw_string(connection, s_drag.overlay_window, XCB_NONE,
                text_x, text_y, s_drag.overlay_text);
    }
}


/* Return the current drag position */
void drag_current_pos(int32_t *x, int32_t *y)
{
    if (x != NULL) {
        *x = s_drag.client_cur_x;
    }
    if (y != NULL) {
        *y = s_drag.client_cur_y;
    }
}


/* Milliseconds until the pending warp is due */
int drag_warp_ms_remaining(void)
{
    struct timespec now;
    long remaining_ms;

    if (!s_drag.warp_pending) {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    remaining_ms =
        (long) (s_drag.warp_due.tv_sec - now.tv_sec) * 1000L +
        (s_drag.warp_due.tv_nsec - now.tv_nsec) / 1000000L;

    return (remaining_ms < 0) ? 0 : (int) remaining_ms;
}


/* Perform the pending warp, if due */
void drag_warp_tick(xcb_connection_t *connection)
{
    surface_td *surface;
    desktop_td *old_desktop;
    desktop_td *new_desktop;
    uint32_t old_desktop_id;
    int16_t new_root_x;
    bool cycle;
    bool is_icon;

    if (connection == NULL || !s_drag.warp_pending ||
            drag_warp_ms_remaining() > 0) {
        return;
    }

    s_drag.warp_pending = false;

    if (s_drag.client == NULL ||
            s_drag.operation != CLIENT_OPERATION_MOVING ||
            (s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window != s_drag.client->icon_window)) {
        /* Not (or no longer) a plain window move or icon move;
         * nothing to warp for -- a resize never sets 'warp_pending'
         * in the first place (see 's_drag_check_warp_edge'), but this
         * still guards against it having somehow become stale. */
        return;
    }

    is_icon = s_drag.drag_window != XCB_WINDOW_NONE;

    surface = wm_get_surface_by_id(s_drag.client->screen_id);
    if (surface == NULL || surface->screen == NULL ||
            surface->config == NULL ||
            !surface->config->desktops.warp ||
            surface->desktop_count <= 1u) {
        return;
    }

    old_desktop_id = surface->desktop_cur;
    old_desktop = surface_desktop_get(surface, old_desktop_id);
    cycle = surface->config->desktops.cycle;

    new_desktop = s_drag.warp_is_left
        ? surface_desktop_prev(surface, old_desktop_id, cycle)
        : surface_desktop_next(surface, old_desktop_id, cycle);
    if (new_desktop == NULL || new_desktop->id == old_desktop_id) {
        /* Already at the end and 'cycle' is off: nothing to warp to. */
        return;
    }

    /* Move the dragged client itself to the new desktop without
     * touching its mapped state at all: unlike a normal desktop
     * switch, it must stay visible and uninterrupted throughout the
     * whole warp, not hidden with the rest of the old desktop's
     * clients below. */
    if (old_desktop != NULL) {
        (void) desktop_action_client_rem(old_desktop, s_drag.client);
    }
    (void) desktop_action_client_add(new_desktop, s_drag.client);

    /* 'desktop_action_client_rem'/'_add' above only move the client
     * between each desktop's own stacking list and lookup table;
     * neither one touches the client's own recorded 'desktop_id'
     * (unlike 'desktop_action_send_client', the normal "send to
     * another desktop" path, which does).  Left stale here, anything
     * that reads a client's desktop from that field directly instead
     * of from whichever desktop's stacking list it is actually in --
     * the window list menu's own per-desktop grouping foremost among
     * them (see 'winlist.c') -- would keep showing the just-warped
     * client under the desktop it left, or drop it from view
     * entirely, even though the warp itself already moved it
     * correctly everywhere else. */
    s_drag.client->desktop_id = new_desktop->id;

    surface->desktop_cur = new_desktop->id;
    surface_clients_hide(surface, old_desktop_id);
    surface_clients_show(surface, new_desktop->id);
    surface->is_outdated = true;

    /* Same desktop-switch notification a normal (non-warp) switch
     * shows (see 's_show_desktop_overlay' in cmds/surface.c, whose own
     * thin wrapper over this same call this mirrors): without it, a
     * warp is the one way to switch desktops that never shows which
     * one just became active. */
    notify_desktop_show(surface->connection, surface,
            surface->desktop_cur, new_desktop->name, surface->config);

    /* Reposition the pointer to the opposite edge, one pixel in from
     * it rather than exactly on it, so the very next motion notify
     * does not immediately re-arm another warp back the way it just
     * came from. */
    new_root_x = s_drag.warp_is_left
        ? (int16_t) ((s_drag.screen_w > 1u)
                ? (s_drag.screen_w - 2u) : 0u)
        : (int16_t) 1;

    /* Move the dragged window or icon by the exact same delta the
     * pointer itself is about to jump, so it stays under the cursor
     * across the warp instead of being left behind on the old
     * desktop's own edge.  Shifting 'client_cur_x' (the position
     * 'drag_update' last actually applied, which already folds in
     * any edge-snapping) is what 'pointer_start_x'/'client_start_x'
     * being left untouched below relies on: with both of those
     * unchanged, the very next real motion notify's own 'new_x =
     * client_start_x + (root_x - pointer_start_x)' is a plain linear
     * function of 'root_x', so it naturally reflects the same shift
     * automatically -- adjusting either baseline here instead would
     * cancel that shift back out (the bug an earlier version of this
     * function actually had: shifting 'pointer_start_x' to
     * compensate for the pointer jump made the computed position
     * identical before and after the warp, keeping the dragged
     * window or icon pinned at its old spot rather than following
     * the pointer to the new one). */
    {
        int32_t new_window_x = s_drag.client_cur_x +
            ((int32_t) new_root_x - (int32_t) s_drag.last_root_x);
        bool show_geom;

        s_drag.client_cur_x = new_window_x;

        if (is_icon) {
            uint32_t vals[2];

            show_geom = s_drag.client->config_base != NULL &&
                s_drag.client->config_base->icons.show_geom;

            vals[0] = (uint32_t) new_window_x;
            vals[1] = (uint32_t) s_drag.client_cur_y;
            xcb_configure_window(connection, s_drag.client->icon_window,
                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
        } else {
            show_geom = s_drag.client->config_base != NULL &&
                s_drag.client->config_base->windows.show_geom;

            enact_client_move(s_drag.client, new_window_x,
                    s_drag.client_cur_y);
        }

        /* Same geometry overlay 'drag_update' keeps current on every
         * real motion notify: without this, it would stay painted at
         * the position the window (or icon) had right before the
         * warp -- on the old desktop's own edge -- until whatever
         * real pointer motion happens to come next, rather than
         * following it across immediately. */
        if (show_geom) {
            char geom_buf[24];

            (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                    (int) new_window_x, (int) s_drag.client_cur_y);
            s_drag_overlay_show(connection, is_icon,
                    new_window_x, s_drag.client_cur_y,
                    is_icon
                        ? (uint16_t) WM_ICON_SQUARE_SIZE
                        : s_drag.client_start_w,
                    is_icon
                        ? s_drag_icon_height(s_drag.client)
                        : s_drag.client_start_h,
                    geom_buf);
        }
    }

    xcb_warp_pointer(connection, XCB_NONE, surface->screen->root,
            0, 0, 0, 0, new_root_x, s_drag.last_root_y);

    s_drag.last_root_x = new_root_x;
    s_drag.desktop = new_desktop;

    xcb_flush(connection);
}
