/**
 * @file menu/confirm.c
 *
 * @brief Quit-confirmation dialog implementation
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
#include <defs/ewmh.h>
#include <defs/wm.h>

/* Project includes */
#include <config.h>
#include <render/text.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/confirm.h>


/** XCB window of the currently visible confirmation dialog */
static xcb_window_t s_confirm_window = XCB_WINDOW_NONE;


/** Currently highlighted button: 0="Cancel" (default), 1="Exit" */
static int s_confirm_selected = 0;


/**
 * @brief Resolved geometry and text for the confirmation dialog
 */
typedef struct {
    uint16_t w;
    uint16_t h;
    uint16_t btn_w;
    uint16_t btn_h;
    int16_t prompt_x;
    int16_t prompt_y;
    int16_t btn_y;
    int16_t cancel_x;
    int16_t exit_x;
    int16_t cancel_label_x;
    int16_t cancel_label_y;
    int16_t exit_label_x;
    int16_t exit_label_y;
    char prompt[CONFIRM_PROMPT_MAX_LEN];
} s_confirm_layout_td;


/** Cached layout used both for creation and repaint */
static s_confirm_layout_td s_confirm_layout;


/**
 * @brief Return the greater of two @c uint16_t values
 *
 * @param a First value
 * @param b Second value
 *
 * @return Maximum value between @p a and @p b
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_u16_max(uint16_t a, uint16_t b)
{
    return (a > b) ? a : b;
}


/**
 * @brief Compute dialog geometry from current text metrics
 *
 * Builds the prompt string using @c WM_EWMH_NAME, measures prompt and
 * button labels, enforces minimum dimensions, and stores all final
 * positions used both by window creation and repaint.
 *
 * @param layout Output layout descriptor to fill
 *
 * @note Complexity: @e O(n), where @e n is the prompt/label text size
 */
static void s_confirm_compute_layout(s_confirm_layout_td *layout)
{
    uint16_t prompt_w;
    uint16_t cancel_label_w;
    uint16_t exit_label_w;
    uint16_t btn_label_w;
    uint16_t btns_span_w;
    uint16_t btns_group_w;
    uint16_t prompt_span_w;

    if (layout == NULL) {
        return;
    }

    (void) snprintf(layout->prompt, sizeof(layout->prompt),
            CONFIRM_PROMPT_FMT, WM_EWMH_NAME);

    prompt_w = menu_draw_measure(layout->prompt);
    cancel_label_w = menu_draw_measure(CONFIRM_LABEL_CANCEL);
    exit_label_w = menu_draw_measure(CONFIRM_LABEL_EXIT);

    btn_label_w = s_u16_max(cancel_label_w, exit_label_w);
    layout->btn_w = s_u16_max(CONFIRM_BTN_MIN_W,
            (uint16_t) (btn_label_w + (CONFIRM_BTN_LABEL_PAD_X * 2u)));
    layout->btn_h = CONFIRM_BTN_H;

    btns_group_w = (uint16_t) ((layout->btn_w * 2u) + CONFIRM_BTN_GAP);
    btns_span_w = (uint16_t) (btns_group_w + (CONFIRM_PAD_X * 2u));
    prompt_span_w = (uint16_t) (prompt_w + (CONFIRM_PAD_X * 2u));

    layout->w = s_u16_max(CONFIRM_MIN_W, s_u16_max(btns_span_w,
                prompt_span_w));
    layout->h = s_u16_max(CONFIRM_MIN_H, (uint16_t)
            (CONFIRM_PROMPT_BASELINE_Y + CONFIRM_PROMPT_TO_BTN_GAP +
             layout->btn_h + CONFIRM_PAD_BOTTOM));
    layout->cancel_x = (int16_t) ((layout->w - btns_group_w) / 2u);
    layout->exit_x = (int16_t) (layout->cancel_x +
            (int16_t) layout->btn_w +
            (int16_t) CONFIRM_BTN_GAP);
    layout->btn_y = (int16_t) (layout->h - CONFIRM_PAD_BOTTOM -
            layout->btn_h);

    layout->prompt_x = (int16_t) ((layout->w > prompt_w)
            ? (layout->w - prompt_w) / 2u : CONFIRM_PAD_X);
    if (layout->prompt_x < (int16_t) CONFIRM_PAD_X) {
        layout->prompt_x = (int16_t) CONFIRM_PAD_X;
    }
    layout->prompt_y = (int16_t) CONFIRM_PROMPT_BASELINE_Y;

    layout->cancel_label_x = (int16_t) (layout->cancel_x +
            (int16_t) ((layout->btn_w - cancel_label_w) / 2u));
    layout->cancel_label_y = (int16_t) (layout->btn_y +
            (int16_t) CONFIRM_BTN_LABEL_BASELINE_OFFSET);
    layout->exit_label_x = (int16_t) (layout->exit_x +
            (int16_t) ((layout->btn_w - exit_label_w) / 2u));
    layout->exit_label_y = (int16_t) (layout->btn_y +
            (int16_t) CONFIRM_BTN_LABEL_BASELINE_OFFSET);
}


/* Draw the dialog contents (prompt + two buttons) */
static void s_confirm_draw(xcb_connection_t *connection,
        const config_td *cfg)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;
    uint32_t gc_vals[1];
    uint32_t bg_win;
    uint32_t fg_sel;
    uint32_t bg_sel;
    uint32_t fg_nor;
    uint32_t bg_nor;
    const s_confirm_layout_td *layout = &s_confirm_layout;

    if (connection == NULL || cfg == NULL ||
            s_confirm_window == XCB_WINDOW_NONE) {
        return;
    }

    /* Set colors */
    bg_win = cfg->theme.window.inactive.background_color;
    fg_sel = cfg->theme.window.active.foreground_color;
    bg_sel = cfg->theme.window.active.background_color;
    fg_nor = cfg->theme.window.inactive.foreground_color;
    bg_nor = cfg->theme.window.inactive.background_color;

    gc = xcb_generate_id(connection);

    /* Clear window background */
    gc_vals[0] = bg_win;
    xcb_create_gc(connection, gc, s_confirm_window,
            XCB_GC_FOREGROUND, gc_vals);
    rect.x = 0;
    rect.y = 0;
    rect.width = layout->w;
    rect.height = layout->h;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);

    /* Cancel button background */
    gc_vals[0] = (s_confirm_selected == 0) ? bg_sel : bg_nor;
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, gc_vals);
    rect.x = layout->cancel_x;
    rect.y = layout->btn_y;
    rect.width = layout->btn_w;
    rect.height = layout->btn_h;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);

    /* Exit button background */
    gc_vals[0] = (s_confirm_selected == 1) ? bg_sel : bg_nor;
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, gc_vals);
    rect.x = layout->exit_x;
    rect.y = layout->btn_y;
    rect.width = layout->btn_w;
    rect.height = layout->btn_h;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);
    xcb_free_gc(connection, gc);

    /* Prompt text */
    text_renderer_set_color(fg_nor, bg_win);
    menu_draw_label(connection, s_confirm_window,
            layout->prompt_x, layout->prompt_y,
            layout->prompt);

    /* Cancel button label */
    text_renderer_set_color(
            (s_confirm_selected == 0) ? fg_sel : fg_nor,
            (s_confirm_selected == 0) ? bg_sel : bg_nor);
    menu_draw_label(connection, s_confirm_window,
            (int16_t) layout->cancel_label_x,
            (int16_t) layout->cancel_label_y,
            CONFIRM_LABEL_CANCEL);

    /* Exit button label */
    text_renderer_set_color(
            (s_confirm_selected == 1) ? fg_sel : fg_nor,
            (s_confirm_selected == 1) ? bg_sel : bg_nor);
    menu_draw_label(connection, s_confirm_window,
            (int16_t) layout->exit_label_x,
            (int16_t) layout->exit_label_y,
            CONFIRM_LABEL_EXIT);

    xcb_flush(connection);
}


/* Open the quit-confirmation dialog centered on the screen */
void confirm_show(xcb_connection_t *connection,
        surface_td *surface,
        const config_td *cfg)
{
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[3];

    if (connection == NULL || surface == NULL || cfg == NULL ||
            surface->screen == NULL) {
        return;
    }

    /* Only one instance at a time */
    if (s_confirm_window != XCB_WINDOW_NONE) {
        return;
    }

    text_renderer_init(connection, cfg->theme.window.active.font);
    s_confirm_compute_layout(&s_confirm_layout);

    s_confirm_selected = 0;
    x = (int16_t) ((surface->properties.dim.w > s_confirm_layout.w)
        ? (surface->properties.dim.w - s_confirm_layout.w) / 2u : 0u);
    y = (int16_t) ((surface->properties.dim.h > s_confirm_layout.h)
        ? (surface->properties.dim.h - s_confirm_layout.h) / 2u : 0u);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.window.inactive.background_color;
    values[1] = cfg->theme.window.active.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE    |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_KEY_PRESS;

    s_confirm_window = xcb_generate_id(connection);
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_confirm_window,
            surface->screen->root,
            x, y,
            s_confirm_layout.w, s_confirm_layout.h,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_confirm_window);
    xcb_flush(connection);

    /* Grab the keyboard so all key events reach the window manager
     * regardless of which application window currently holds focus */
    xcb_grab_keyboard(connection,
            0,
            s_confirm_window,
            XCB_CURRENT_TIME,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC);
    xcb_set_input_focus(connection,
            XCB_INPUT_FOCUS_POINTER_ROOT,
            s_confirm_window,
            XCB_CURRENT_TIME);
    xcb_flush(connection);
}


/* Destroy the currently visible confirmation dialog */
void confirm_close(xcb_connection_t *connection)
{
    if (connection == NULL || s_confirm_window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_ungrab_keyboard(connection, XCB_CURRENT_TIME);
    xcb_destroy_window(connection, s_confirm_window);
    xcb_flush(connection);
    s_confirm_window = XCB_WINDOW_NONE;
    s_confirm_selected = 0;
}


/* Repaint the confirmation dialog from current state */
void confirm_repaint(xcb_connection_t *connection,
        const config_td *cfg)
{
    s_confirm_draw(connection, cfg);
}


/* Handle a mouse click inside the confirmation dialog */
bool confirm_handle_click(xcb_connection_t *connection,
        int x, int y)
{
    if (connection == NULL || s_confirm_window == XCB_WINDOW_NONE) {
        return false;
    }

    if (y >= s_confirm_layout.btn_y &&
            y < (s_confirm_layout.btn_y + (int) s_confirm_layout.btn_h)) {
        if (x >= s_confirm_layout.cancel_x &&
                x < (s_confirm_layout.cancel_x +
                    (int) s_confirm_layout.btn_w)) {
            s_confirm_selected = 0;
            confirm_close(connection);
            return true;
        }
        if (x >= s_confirm_layout.exit_x &&
                x < (s_confirm_layout.exit_x +
                    (int) s_confirm_layout.btn_w)) {
            s_confirm_selected = 1;
            confirm_accept(connection);
            return true;
        }
    }

    return false;
}


/* Move selection to the next button (wraps around) */
void confirm_toggle_selection(void)
{
    s_confirm_selected = (s_confirm_selected == 0) ? 1 : 0;
}


/* Activate the currently selected button */
void confirm_accept(xcb_connection_t *connection)
{
    if (s_confirm_selected == 1) {
        confirm_close(connection);
        (void) wm_request_stop();
    } else {
        confirm_close(connection);
    }
}


/* Query whether the confirmation dialog is currently visible */
bool confirm_is_open(void)
{
    return s_confirm_window != XCB_WINDOW_NONE;
}


/* Return the confirmation dialog window identifier */
xcb_window_t confirm_window(void)
{
    return s_confirm_window;
}
