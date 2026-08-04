/**
 * @file menu/notify.c
 *
 * @brief Desktop-switch notification popup implementation
 *
 * Implements @c notify_desktop_show, @c notify_desktop_close,
 * @c notify_desktop_repaint, @c notify_desktop_is_open,
 * @c notify_desktop_window, and @c notify_desktop_ms_remaining.
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
#include <stdio.h>      /* snprintf */
#include <stdint.h>
#include <time.h>       /* CLOCK_MONOTONIC, clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <render/text.h>
#include <surface.h>

/* Default initial values */
#include <defs/wm.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/notify.h>


/** XCB window of the currently visible desktop notification */
static xcb_window_t s_notify_window = XCB_WINDOW_NONE;

/** Cached label text for the notification */
static char s_notify_text[WM_DESKTOP_MAX_LENGTH_NAME + 16];

/** Monotonic timestamp when the notification was last shown */
static struct timespec s_notify_open_time = { 0, 0 };


/* Show the desktop-switch notification popup, centered on screen */
void notify_desktop_show(xcb_connection_t *connection,
        surface_td *surface, uint32_t desktop_idx,
        const char *desktop_name, const config_td *cfg)
{
    int16_t width;
    int16_t height;
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[3];
    uint16_t text_w;

    if (connection == NULL || surface == NULL || cfg == NULL ||
            surface->screen == NULL) {
        return;
    }

    if (!cfg->base.show_desktop_notify) {
        return;
    }

    if (desktop_name != NULL && desktop_name[0] != '\0') {
        snprintf(s_notify_text, sizeof(s_notify_text),
                "[%u] -- %s", desktop_idx, desktop_name);
    } else {
        snprintf(s_notify_text, sizeof(s_notify_text),
                "[%u]", desktop_idx);
    }

    notify_desktop_close(connection);

    text_renderer_init(connection, cfg->theme.window.active.font);
    text_w = menu_draw_measure(s_notify_text);

    /* Clamp minimum width and add horizontal padding */
    width = (int16_t) ((text_w > 40u) ? (text_w + 32u) : 72u);
    height = 36;

    x = (int16_t) (((int32_t) surface->properties.dim.w - width) / 2);
    y = (int16_t) (((int32_t) surface->properties.dim.h - height) / 2);
    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }

    s_notify_window = xcb_generate_id(connection);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.window.active.background_color;
    values[1] = cfg->theme.window.active.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE;
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_notify_window,
            surface->screen->root,
            x, y,
            (uint16_t) width, (uint16_t) height,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_notify_window);
    xcb_flush(connection);

    (void) clock_gettime(CLOCK_MONOTONIC, &s_notify_open_time);

    LOGGER_TRACE("Desktop notify shown: '%s'", s_notify_text);
}


/* Destroy the currently visible desktop-switch notification */
void notify_desktop_close(xcb_connection_t *connection)
{
    if (connection == NULL || s_notify_window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_destroy_window(connection, s_notify_window);
    xcb_flush(connection);
    s_notify_window = XCB_WINDOW_NONE;
    s_notify_open_time.tv_sec = 0;
    s_notify_open_time.tv_nsec = 0;
}


/* Repaint the desktop-switch notification from its cached text */
void notify_desktop_repaint(xcb_connection_t *connection,
        const config_td *cfg)
{
    int16_t width;
    int16_t height;
    uint16_t text_w;
    int16_t text_x;
    int16_t text_y;

    if (connection == NULL || cfg == NULL ||
            s_notify_window == XCB_WINDOW_NONE) {
        return;
    }

    text_renderer_init(connection, cfg->theme.window.active.font);
    text_renderer_set_color(cfg->theme.window.active.foreground_color,
            cfg->theme.window.active.background_color);

    text_w = menu_draw_measure(s_notify_text);
    width = (int16_t) ((text_w > 40u) ? (text_w + 32u) : 72u);
    height = 36;

    menu_draw_row_bg(connection, s_notify_window,
            cfg->theme.window.active.background_color,
            0, (uint16_t) height, (uint16_t) width);

    text_x = (int16_t) ((width - (int16_t) text_w) / 2);
    text_y = (int16_t) (height / 2 + 5);
    menu_draw_label(connection, s_notify_window, text_x, text_y,
            s_notify_text);

    xcb_flush(connection);
}


/* Query whether the desktop notification is currently visible */
bool notify_desktop_is_open(void)
{
    return s_notify_window != XCB_WINDOW_NONE;
}


/* Return the desktop notification window identifier */
xcb_window_t notify_desktop_window(void)
{
    return s_notify_window;
}


/* Return milliseconds remaining before the notification auto-closes */
int notify_desktop_ms_remaining(void)
{
    struct timespec now;
    long elapsed_ms;

    if (s_notify_window == XCB_WINDOW_NONE) {
        return -1;
    }

    if (s_notify_open_time.tv_sec == 0 && s_notify_open_time.tv_nsec == 0) {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }

    elapsed_ms = (long)
        ((now.tv_sec - s_notify_open_time.tv_sec) * 1000L +
         (now.tv_nsec - s_notify_open_time.tv_nsec) / 1000000L);

    if (elapsed_ms >= (long) WM_DESKTOP_NOTIFY_TIMEOUT_MS) {
        return 0;
    }

    return (int) ((long) WM_DESKTOP_NOTIFY_TIMEOUT_MS - elapsed_ms);
}
