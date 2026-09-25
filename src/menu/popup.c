/**
 * @file menu/popup.c
 *
 * @brief Informational client popup window implementation
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
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <cmds/client/screen.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <render/text.h>
#include <stage.h>

/* Default initial values */
#include <defs/popup.h>

/* Utils includes */
#include <utils/time/clock.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/popup.h>


/** XCB window of the currently visible info popup */
static xcb_window_t s_popup_window = XCB_WINDOW_NONE;

/** Modifier mask that opened the popup (locking bits stripped) */
static uint16_t s_popup_modifier = 0;

/** Keycode that opened the popup */
static xcb_keycode_t s_popup_keycode = 0;

/** Cached text lines; reused when the popup receives an expose event */
static char s_popup_lines[4][WM_INFO_POPUP_LINE_MAX_LENGTH];

/** Monotonic timestamp when the popup was last shown */
static struct timespec s_popup_open_time = { 0, 0 };


/* Show a popup by the client window, with its information */
void popup_show(xcb_connection_t *connection,
        stage_td *stage, const desktop_td *desktop,
        client_td *client,
        uint16_t modifier, xcb_keycode_t keycode, const config_td *cfg)
{
    const char *name;
    const char *class_name;
    const char *instance_name;
    /** Left/right margin around the text, matching the @c x=8 origin
     *  'popup_repaint' already draws every line at */
    const int16_t text_margin = 8;
    /** Floor under the computed width, so a popup for a client with
     *  very short property values never ends up uncomfortably
     *  narrow */
    const int16_t min_width = 260;
    int16_t width;
    const int16_t height = 96;
    int16_t x;
    int16_t y;
    int32_t max_x;
    int32_t max_y;
    uint32_t mask;
    uint32_t values[4];
    stage_td *client_stage = NULL;
    monitor_td client_monitor;
    uint32_t monitor_id = 0u;
    uint16_t widest_line = 0u;

    if (connection == NULL || stage == NULL || desktop == NULL ||
            client == NULL || cfg == NULL ||
            stage->screen == NULL) {
        return;
    }

    name = (client->info.name != NULL) ? client->info.name : "";
    class_name = (client->info.class_name[1] != NULL)
        ? client->info.class_name[1] : "";
    instance_name = (client->info.class_name[0] != NULL)
        ? client->info.class_name[0] : "";

    /* Resolve which physical monitor the client's center point
     * currently falls on, the same way
     * 'ccmd_client_move_to_next_monitor' in 'cmds/client/geom.c' does,
     * to display alongside its desktop/stage; a client on
     * a single-monitor stage always resolves to monitor 0. */
    if (ccmd_client_monitor(client, &client_stage, &client_monitor) &&
            client_stage != NULL) {
        for (uint32_t i = 0u; i < client_stage->monitor_count; ++i) {
            if (client_stage->monitors[i].x == client_monitor.x &&
                    client_stage->monitors[i].y == client_monitor.y) {
                monitor_id = i;
                break;
            }
        }
    }

    /* Build every line, and size the window to its widest one,
     * before creating anything: sizing the window first and fitting
     * the text into whatever that left (the previous approach, a
     * fixed guess wide enough for the longest line this popup could
     * ever show) reliably wastes space for every shorter one, since
     * most lines never come close to the longest possible. */
    (void) snprintf(s_popup_lines[0], sizeof(s_popup_lines[0]),
            "name=%s class=%s instance=%s",
            name, class_name, instance_name);
    (void) snprintf(s_popup_lines[1], sizeof(s_popup_lines[1]),
            "frame_id=%#x client_id=%#x desktop_id=%u stage_id=%u" \
            " monitor_id=%u",
            client->frame, client->id, desktop->id, stage->id,
            monitor_id);
    (void) snprintf(s_popup_lines[2], sizeof(s_popup_lines[2]),
            "geom=%ux%u%+d%+d",
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y);
    (void) snprintf(s_popup_lines[3], sizeof(s_popup_lines[3]),
            "flags=%#x state=%#x",
            client->properties.flags, client->properties.state);

    /* 'text_string_measure' needs the renderer already set up for this
     * popup's font; safe and cheap to call here even though
     * 'popup_repaint' calls it again later; it is a same-connection,
     * same-font no-op the second time (see its not-so-long comment). */
    (void) text_renderer_use_font(connection, cfg->theme.overlay.font);
    for (size_t i = 0; i < 4; ++i) {
        uint16_t line_width = text_string_measure(s_popup_lines[i]);

        if (line_width > widest_line) {
            widest_line = line_width;
        }
    }
    width = (int16_t) (widest_line + (uint16_t) (2 * text_margin));
    if (width < min_width) {
        width = min_width;
    }

    popup_close(connection);

    /* Position at the client window's coordinates, clamped to
     * screen */
    x = (int16_t) client->layout.geometry.cur.pos.x;
    y = (int16_t) client->layout.geometry.cur.pos.y;
    max_x = (int32_t) stage->properties.dim.w - (int32_t) width;
    max_y = (int32_t) stage->properties.dim.h - (int32_t) height;
    if ((int32_t) x > max_x) { x = (int16_t) max_x; }
    if ((int32_t) y > max_y) { y = (int16_t) max_y; }
    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }

    s_popup_modifier = (uint16_t) ((unsigned int) modifier &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    s_popup_keycode = keycode;
    s_popup_window = xcb_generate_id(connection);

    mask = XCB_CW_BACK_PIXEL        |
           XCB_CW_BORDER_PIXEL      |
           XCB_CW_OVERRIDE_REDIRECT |
           XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.overlay.color.background;
    values[1] = cfg->theme.overlay.border.color;
    values[2] = 1;  /* override_redirect: prevent WM from managing it */
    values[3] = XCB_EVENT_MASK_EXPOSURE    |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_KEY_PRESS;
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_popup_window,
            stage->screen->root,
            x, y,
            (uint16_t) width, (uint16_t) height,
            (uint16_t) cfg->theme.overlay.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* Advertise this as a tooltip window, so a compositor that
     * inspects '_NET_WM_WINDOW_TYPE' recognizes it for what it
     * is */
    if (xcb_ewmh_connection_get() != NULL) {
        xcb_atom_t window_type =
            xcb_ewmh_connection_get()->_NET_WM_WINDOW_TYPE_TOOLTIP;

        xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_get(),
                s_popup_window, 1, &window_type);
    }

    atom_set_window_opacity(connection, s_popup_window,
            config_theme_opacity_to_raw(cfg->theme.overlay.opacity));

    xcb_map_window(connection, s_popup_window);

    /* Record the time the popup was shown so the main loop can close
     * it automatically after 'WM_INFO_POPUP_TIMEOUT_MS'. */
    (void) clock_gettime(CLOCK_MONOTONIC, &s_popup_open_time);

    LOGGER_TRACE("Popup shown for client %#x", client->id);
}


/* Destroy the currently visible info popup */
void popup_close(xcb_connection_t *connection)
{
    if (connection == NULL || s_popup_window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_window_destroy(s_popup_window);
    s_popup_window = XCB_WINDOW_NONE;
    s_popup_modifier = 0;
    s_popup_keycode = 0;
    s_popup_open_time.tv_sec = 0;
    s_popup_open_time.tv_nsec = 0;
}


/* Return milliseconds until the popup should be auto-closed */
int popup_ms_remaining(void)
{
    long elapsed_ms;

    if (s_popup_window == XCB_WINDOW_NONE) {
        return -1;
    }

    if (s_popup_open_time.tv_sec == 0 &&
            s_popup_open_time.tv_nsec == 0) {
        return -1;
    }

    elapsed_ms = clock_ms_since(&s_popup_open_time);

    if (elapsed_ms >= (long) WM_INFO_POPUP_TIMEOUT_MS) {
        return 0;
    }

    return (int) ((long) WM_INFO_POPUP_TIMEOUT_MS - elapsed_ms);
}


/* Repaint the info popup from its cached text lines */
void popup_repaint(xcb_connection_t *connection,
        const config_td *cfg)
{
    if (connection == NULL || cfg == NULL ||
            s_popup_window == XCB_WINDOW_NONE) {
        return;
    }

    (void) text_renderer_use_font(connection,
            cfg->theme.overlay.font);
    text_renderer_set_color(cfg->theme.overlay.color.foreground,
            cfg->theme.overlay.color.background);
    menu_draw_label(connection, s_popup_window,
            (struct position_s) { 8, 16 }, s_popup_lines[0]);
    menu_draw_label(connection, s_popup_window,
            (struct position_s) { 8, 34 }, s_popup_lines[1]);
    menu_draw_label(connection, s_popup_window,
            (struct position_s) { 8, 52 }, s_popup_lines[2]);
    menu_draw_label(connection, s_popup_window,
            (struct position_s) { 8, 70 }, s_popup_lines[3]);
}


/* Query whether the info popup is currently visible */
bool popup_is_open(void)
{
    return s_popup_window != XCB_WINDOW_NONE;
}


/* Return the info popup window identifier */
xcb_window_t popup_window(void)
{
    return s_popup_window;
}
