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

/* XCB includes */
#include <xcb/xcb.h>

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
    const uint16_t w = 400;
    const uint16_t h = 90;
    const uint16_t btn_w = 100;
    const uint16_t btn_h = 26;
    const int16_t btn_y = (int16_t) (h - btn_h - 8);
    const int16_t cancel_x = 12;
    const int16_t exit_x = (int16_t) (w - btn_w - 12);

    if (connection == NULL || cfg == NULL ||
            s_confirm_window == XCB_WINDOW_NONE) {
        return;
    }

    bg_win = cfg->theme.window.active.background_color;
    fg_sel = cfg->theme.window.active.foreground_color;
    bg_sel = cfg->theme.window.active.border_color;
    fg_nor = cfg->theme.window.inactive.foreground_color;
    bg_nor = cfg->theme.window.active.background_color;

    gc = xcb_generate_id(connection);

    /* Clear window background */
    gc_vals[0] = bg_win;
    xcb_create_gc(connection, gc, s_confirm_window,
            XCB_GC_FOREGROUND, gc_vals);
    rect.x = 0;
    rect.y = 0;
    rect.width = w;
    rect.height = h;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);

    /* Cancel button background */
    gc_vals[0] = (s_confirm_selected == 0) ? bg_sel : bg_nor;
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, gc_vals);
    rect.x = cancel_x;
    rect.y = btn_y;
    rect.width = btn_w;
    rect.height = btn_h;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);

    /* Exit button background */
    gc_vals[0] = (s_confirm_selected == 1) ? bg_sel : bg_nor;
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND, gc_vals);
    rect.x = exit_x;
    rect.y = btn_y;
    rect.width = btn_w;
    rect.height = btn_h;
    xcb_poly_fill_rectangle(connection, s_confirm_window, gc, 1, &rect);
    xcb_free_gc(connection, gc);

    text_renderer_init(connection, cfg->theme.window.active.font);

    /* Prompt text */
    text_renderer_set_color(fg_nor, bg_win);
    menu_draw_label(connection, s_confirm_window, 12, 22,
            "Are you sure you want to exit IcoWM?");

    /* Cancel button label */
    text_renderer_set_color(
            (s_confirm_selected == 0) ? fg_sel : fg_nor,
            (s_confirm_selected == 0) ? bg_sel : bg_nor);
    menu_draw_label(connection, s_confirm_window,
            (int16_t) (cancel_x + 6),
            (int16_t) (btn_y + 17),
            "[ Cancel ]");

    /* Exit button label */
    text_renderer_set_color(
            (s_confirm_selected == 1) ? fg_sel : fg_nor,
            (s_confirm_selected == 1) ? bg_sel : bg_nor);
    menu_draw_label(connection, s_confirm_window,
            (int16_t) (exit_x + 14),
            (int16_t) (btn_y + 17),
            "[ Exit ]");

    xcb_flush(connection);
}


/* Open the quit-confirmation dialog centered on the screen */
void confirm_show(xcb_connection_t *connection,
        surface_td *surface,
        const config_td *cfg)
{
    const uint16_t w = 400;
    const uint16_t h = 90;
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[3];

    if (connection == NULL || surface == NULL || cfg == NULL ||
            surface->screen == NULL) {
        return;
    }

    confirm_close(connection);

    s_confirm_selected = 0;
    x = (int16_t) ((surface->properties.dim.w > w)
        ? (surface->properties.dim.w - w) / 2u : 0u);
    y = (int16_t) ((surface->properties.dim.h > h)
        ? (surface->properties.dim.h - h) / 2u : 0u);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.window.active.background_color;
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
            w, h,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_confirm_window);
    xcb_flush(connection);
}


/* Destroy the currently visible confirmation dialog */
void confirm_close(xcb_connection_t *connection)
{
    if (connection == NULL || s_confirm_window == XCB_WINDOW_NONE) {
        return;
    }
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
