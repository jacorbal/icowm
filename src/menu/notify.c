/**
 * @file menu/notify.c
 *
 * @brief Generic notification helpers
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
#include <time.h>       /* CLOCK_MONOTONIC, clock_gettime, timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>
#include <utils/time/clock.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>

/* Project includes */
#include <config.h>
#include <render/text.h>
#include <stage.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/notify.h>
#include <utils/xcb/window.h>


/* Close the notification popup and reset its state */
void notify_popup_close(xcb_connection_t *connection,
        struct notify_popup_state_s *state)
{
    if (connection == NULL || state == NULL ||
            state->window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_window_destroy(state->window);
    state->window = XCB_WINDOW_NONE;
    state->open_time.tv_sec = 0;
    state->open_time.tv_nsec = 0;
}


/* Query whether the notification popup window exists */
bool notify_popup_is_open(const struct notify_popup_state_s *state)
{
    return state != NULL && state->window != XCB_WINDOW_NONE;
}


/* Return the popup window identifier, or XCB_WINDOW_NONE */
xcb_window_t notify_popup_window(const struct notify_popup_state_s *state)
{
    return (state != NULL) ? state->window : XCB_WINDOW_NONE;
}


/* Return milliseconds remaining before the popup reaches its timeout */
int notify_popup_ms_remaining(const struct notify_popup_state_s *state,
        int timeout_ms)
{
    long elapsed_ms;

    if (state == NULL || state->window == XCB_WINDOW_NONE) {
        return -1;
    }

    if (state->open_time.tv_sec == 0 && state->open_time.tv_nsec == 0) {
        return -1;
    }

    elapsed_ms = clock_ms_since(&state->open_time);
    if (elapsed_ms >= (long) timeout_ms) {
        return 0;
    }

    return (int) ((long) timeout_ms - elapsed_ms);
}


/* Create and show a notification popup centered on the screen */
void notify_popup_show_centered(xcb_connection_t *connection,
        stage_td *stage, struct notify_popup_state_s *state,
        const char *text, const config_td *cfg)
{
    int16_t width;
    int16_t height;
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[4];
    uint16_t text_w;

    if (connection == NULL || stage == NULL || state == NULL ||
            text == NULL || cfg == NULL || stage->screen == NULL) {
        return;
    }

    safe_strncpy(state->text, text, sizeof(state->text) - 1u);
    state->text[sizeof(state->text) - 1u] = '\0';

    notify_popup_close(connection, state);

    (void) text_renderer_use_font(connection, cfg->theme.overlay.font);
    text_w = menu_draw_measure(state->text);

    width = (int16_t) ((text_w > 40u) ? (text_w + 32u) : 72u);
    height = 36;
    x = (int16_t) (((int32_t) stage->properties.dim.w - width) / 2);
    y = (int16_t) (((int32_t) stage->properties.dim.h - height) / 2);
    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }

    state->window = xcb_generate_id(connection);
    mask = XCB_CW_BACK_PIXEL        |
           XCB_CW_BORDER_PIXEL      |
           XCB_CW_OVERRIDE_REDIRECT |
           XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.overlay.color.background;
    values[1] = cfg->theme.overlay.border.color;
    values[2] = 1;  /* override_redirect: prevent WM from managing it */
    values[3] = XCB_EVENT_MASK_EXPOSURE;

    xcb_create_window(connection, XCB_COPY_FROM_PARENT, state->window,
            stage->screen->root,
            x, y, (uint16_t) width, (uint16_t) height,
            (uint16_t) cfg->theme.overlay.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT, mask, values);

    /* Advertise this as a notification window, so a compositor that
     * inspects '_NET_WM_WINDOW_TYPE' recognizes it for what it is */
    if (xcb_ewmh_connection_get() != NULL) {
        xcb_atom_t window_type =
            xcb_ewmh_connection_get()->_NET_WM_WINDOW_TYPE_NOTIFICATION;

        xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_get(),
                state->window, 1, &window_type);
    }

    atom_set_window_opacity(connection, state->window,
            config_theme_opacity_to_raw(cfg->theme.overlay.opacity));
    xcb_map_window(connection, state->window);
    (void) clock_gettime(CLOCK_MONOTONIC, &state->open_time);
}


/* Repaint a centered notification popup using its cached text */
void notify_popup_repaint_centered(xcb_connection_t *connection,
        const struct notify_popup_state_s *state, const config_td *cfg)
{
    int16_t width;
    int16_t height;
    uint16_t text_w;
    int16_t text_x;
    int16_t text_y;

    if (connection == NULL || state == NULL || cfg == NULL ||
            state->window == XCB_WINDOW_NONE) {
        return;
    }

    (void) text_renderer_use_font(connection, cfg->theme.overlay.font);
    text_renderer_set_color(cfg->theme.overlay.color.foreground,
            cfg->theme.overlay.color.background);
    text_w = menu_draw_measure(state->text);
    width = (int16_t) ((text_w > 40u) ? (text_w + 32u) : 72u);
    height = 36;

    menu_draw_row_bg(connection, state->window,
            cfg->theme.overlay.color.background,
            0, (uint16_t) height, (uint16_t) width);
    text_x = (int16_t) ((width - (int16_t) text_w) / 2);
    text_y = (int16_t) (height / 2 + 5);
    menu_draw_label(connection, state->window,
            (struct position_s) { text_x, text_y }, state->text);
}
