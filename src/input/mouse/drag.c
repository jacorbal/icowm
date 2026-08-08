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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Windows & icons policy includes */
#include <policy/focus.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/icon.h>
#include <defs/input.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <lookup.h>
#include <render/text.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <input/mouse/drag.h>


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
    .icon_was_mapped = false
};


#define WM_DRAG_OVERLAY_PAD_X (8u)
#define WM_DRAG_OVERLAY_HEIGHT (22u)
#define WM_DRAG_OVERLAY_MIN_WIDTH (40u)


/**
 * @brief Synchronizes the active visual of the drag icon window.
 *
 * Updates the background and border colors of the drag icon window so
 * that they match the active icon theme, and then clears the window to
 * force a visual refresh.  If the connection, client, theme, or icon
 * window are not valid, the function returns without doing anything.
 *
 * @param connection XCB connection used to issue window attribute and
 *                   clear-area requests
 */
static void s_drag_sync_icon_active_visual(xcb_connection_t *connection)
{
    const char *caption;

    if (connection == NULL || s_drag.client == NULL ||
            s_drag.client->theme == NULL ||
            s_drag.client->icon_window == 0) {
        return;
    }

    xcb_change_window_attributes(connection,
            s_drag.client->icon_window,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
                s_drag.client->theme->icon.active.color.background,
                s_drag.client->theme->icon.active.border.color
            });
    xcb_clear_area(connection, 0,
            s_drag.client->icon_window, 0, 0, 0, 0);

    if (!s_drag.client->theme->icon.is_captioned ||
            s_drag.client->info.name == NULL) {
        return;
    }

    caption = (s_drag.client->icon_info.visible_icon_name != NULL &&
            s_drag.client->icon_info.visible_icon_name[0] != '\0')
        ? s_drag.client->icon_info.visible_icon_name
        : s_drag.client->info.name;

    text_renderer_init(connection,
            s_drag.client->theme->icon.active.font);
    text_renderer_set_color(
            s_drag.client->theme->icon.active.color.foreground,
            s_drag.client->theme->icon.active.color.background);
    text_draw_string(connection, s_drag.client->icon_window, XCB_NONE,
            2,
            (int16_t) (WM_ICON_SQUARE_SIZE + WM_ICON_CAPTION_HEIGHT - 2u),
            caption);
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
            (client->theme->icon.is_captioned
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
        int32_t dx = snap;
        int32_t dy = snap;

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

    /* For resize operations, make the visible corner handles define the
     * corner hit zones.  Outside those 12 px corner zones, keep the
     * existing center-based fallback so the rest of the border still
     * behaves as a resize handle. */
    if (operation == CLIENT_OPERATION_RESIZING) {
        int32_t left = s_drag.client_start_x;
        int32_t top = s_drag.client_start_y;
        int32_t right = left + (int32_t) s_drag.client_start_w;
        int32_t bottom = top + (int32_t) s_drag.client_start_h;
        int32_t cx = s_drag.client_start_x +
            (int32_t) (s_drag.client_start_w / 2u);
        int32_t cy = s_drag.client_start_y +
            (int32_t) (s_drag.client_start_h / 2u);
        int32_t corner = WM_RESIZE_CORNER_SIZE;

        if ((int32_t) root_x < left + corner) {
            s_drag.anchor_right = true;
        } else if ((int32_t) root_x >= right - corner) {
            s_drag.anchor_right = false;
        } else {
            s_drag.anchor_right = ((int32_t) root_x < cx);
        }

        if ((int32_t) root_y < top + corner) {
            s_drag.anchor_bottom = true;
        } else if ((int32_t) root_y >= bottom - corner) {
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
        s_drag.resize_w = ((int32_t) root_x < left + corner ||
                (int32_t) root_x >= right - corner);
        s_drag.resize_h = ((int32_t) root_y < top + corner ||
                (int32_t) root_y >= bottom - corner);
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
            XCB_NONE,
            event_time);
    xcb_flush(connection);
}


/* Begin a drag operation for an icon window */
void drag_start_icon(xcb_connection_t *connection, xcb_window_t root,
        client_td *client,
        int32_t icon_x, int32_t icon_y,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y)
{
    if (connection == NULL || client == NULL) {
        return;
    }

    s_drag_overlay_hide(connection);
    s_drag.active = true;
    s_drag.client = client;
    s_drag.desktop = NULL;
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
    s_drag.icon_was_mapped = client->is_icon_mapped;
    s_drag.anchor_right  = false;
    s_drag.anchor_bottom = false;
    s_drag.resize_w = false;
    s_drag.resize_h = false;

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
        int32_t d_horiz = snap;
        int32_t d_vert = snap;

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
        (void) client_send_event_move(client, new_x, new_y);
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
        (void) client_send_event_resize(client, new_x, new_y,
                new_w, new_h);
        if (show_geom) {
            (void) snprintf(geom_buf, sizeof(geom_buf), "%ux%u",
                    new_w, new_h);
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
                (void) client_send_event_restore(ic);
                if (surface != NULL && desktop != NULL) {
                    focus_apply(NULL, surface, desktop, ic, true, NULL);
                }
            } else {
                s_drag.client->icon_x =
                    (int16_t) (s_drag.client_start_x + dx);
                s_drag.client->icon_y =
                    (int16_t) (s_drag.client_start_y + dy);
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
            (void) client_send_event_resize(s_drag.client,
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
    text_x = (text_w < WM_DRAG_OVERLAY_MIN_WIDTH)
        ? (int16_t) ((WM_DRAG_OVERLAY_MIN_WIDTH - text_w) / 2u)
        : (int16_t) WM_DRAG_OVERLAY_PAD_X;

    text_draw_string(connection, s_drag.overlay_window, XCB_NONE,
            text_x, 15, s_drag.overlay_text);
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
