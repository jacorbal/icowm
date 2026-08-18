/**
 * @file input/mouse/drag/overlay.c
 *
 * @brief Centered feedback overlay window shown during a drag
 *
 * Split out of what used to be a single, flat @c input/mouse/drag.c;
 * see @c drag/internal.h for why.
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

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <render/icon.h>
#include <render/text.h>

/* Local includes */
#include <input/mouse/drag.h>
#include <input/mouse/drag/internal.h>


void drag_sync_icon_active_visual(xcb_connection_t *connection)
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
uint16_t drag_u16_sat(uint32_t value)
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
uint16_t drag_icon_height(const client_td *client)
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
void drag_overlay_hide(xcb_connection_t *connection)
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
void drag_overlay_show(xcb_connection_t *connection,
        bool is_icon,
        int32_t target_x, int32_t target_y,
        uint16_t target_w, uint16_t target_h,
        const char *text)
{
    uint16_t text_w;
    uint16_t overlay_w;
    int16_t overlay_x;
    int16_t overlay_y;

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
        uint16_t create_mask;
        uint32_t create_values[4];

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
    uint16_t overlay_w;
    int16_t ascent;
    int16_t descent;
    int16_t text_y;

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
     * a separately hardcoded threshold that happened to only agree with
     * it for a wide-enough or narrow-enough string.
     *
     * Those two thresholds ('text_w + 2*PAD_X < MIN_WIDTH' here versus
     * 'text_w < MIN_WIDTH' in the box-sizing formula) disagreeing for
     * a string in between the two (long enough to push the box wider
     * than 'MIN_WIDTH', but still short enough of 'MIN_WIDTH' itself to
     * take the "narrow" branch here) is what left text looking pinned
     * to the left with a lopsided gap on the right (worst for a string
     * a few pixels short of exactly 'MIN_WIDTH', which could end up
     * with zero left margin at all).  Computing the box's own width the
     * same way here removes the mismatch entirely, for any string
     * length, not just the ones on either side of it that happened not
     * to expose the bug. */
    overlay_w = (uint16_t) (text_w + 2u * WM_DRAG_OVERLAY_PAD_X);

    if (overlay_w < WM_DRAG_OVERLAY_MIN_WIDTH) {
        overlay_w = WM_DRAG_OVERLAY_MIN_WIDTH;
    }
    text_x = (int16_t) ((overlay_w - text_w) / 2u);

    /* Vertically centered baseline for whatever font this theme
     * actually configures, rather than a single Y hardcoded for one
     * particular font size: see 'text_font_ascent's comment in
     * 'render/text.h' for the derivation (ascent placed 'top' pixels
     * below the box's own top edge, here with 'top' itself computed
     * from ascent/descent so half the leftover vertical space sits on
     * each side). */
    ascent = text_font_ascent();
    descent = text_font_descent();
    text_y = (int16_t)
        (((int32_t) WM_DRAG_OVERLAY_HEIGHT + ascent - descent) / 2);

    text_draw_string(connection, s_drag.overlay_window, XCB_NONE,
            text_x, text_y, s_drag.overlay_text);
}


