/**
 * @file menu/notify/desktop.c
 *
 * @brief Desktop-switch notification popup implementation
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
#include <stdio.h>      /* snprintf */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <render/text.h>
#include <surface.h>


/* Default initial values */
#include <defs/desktop.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/notify.h>
#include <menu/notify/desktop.h>


/* State shared by all notification popup instances */
static struct notify_popup_state_s s_desktop_notify = {
    .window = XCB_WINDOW_NONE,
    .open_time = { 0, 0 },
    .text = { 0 }
};


/* Show the desktop-switch notification popup, centered on screen */
void notify_desktop_show(xcb_connection_t *connection,
        surface_td *surface, uint32_t desktop_idx,
        const char *desktop_name, const config_td *cfg)
{
    char text[WM_DESKTOP_MAX_LENGTH_NAME + 32];
    uint32_t row = 0u;
    uint32_t col = 0u;
    bool has_row_col;
    bool show_row_col;

    if (connection == NULL || surface == NULL || cfg == NULL ||
            surface->screen == NULL) {
        return;
    }

    if (!cfg->desktops.show_overlay) {
        return;
    }

    has_row_col = surface_desktop_row_col(surface, desktop_idx,
            &row, &col);
    /* Only worth showing once the grid is genuinely more than the
     * one row a desktop's own ID already fully describes on its
     * own; see 'surface_desktop_row_col' itself (surface.h) for
     * what "row 0" always means on a linear (or unconfigured)
     * layout, the exact case this excludes here. */
    show_row_col = has_row_col &&
        surface->config != NULL &&
        surface->id < (uint32_t) CONFIG_MAX_SCREENS &&
        surface->config->base.screens[surface->id]
            .desktop_layout.rows > 1u;

    if (desktop_name != NULL && desktop_name[0] != '\0') {
        if (show_row_col) {
            (void) snprintf(text, sizeof(text), "[%u (%u, %u)] -- %s",
                    desktop_idx, row, col, desktop_name);
        } else {
            (void) snprintf(text, sizeof(text),
                    "[%u] -- %s", desktop_idx, desktop_name);
        }
    } else {
        if (show_row_col) {
            (void) snprintf(text, sizeof(text), "[%u (%u, %u)]",
                    desktop_idx, row, col);
        } else {
            (void) snprintf(text, sizeof(text),
                    "[%u]", desktop_idx);
        }
    }

    notify_popup_show_centered(connection, surface, &s_desktop_notify,
            text, cfg);

    LOGGER_TRACE("Desktop notify shown: '%s'", text);
}


/* Destroy the currently visible desktop-switch notification */
void notify_desktop_close(xcb_connection_t *connection)
{
    notify_popup_close(connection, &s_desktop_notify);
}


/* Repaint the desktop-switch notification from its cached text */
void notify_desktop_repaint(xcb_connection_t *connection,
        const config_td *cfg)
{
    notify_popup_repaint_centered(connection, &s_desktop_notify, cfg);
}


/* Query whether the desktop notification is currently visible */
bool notify_desktop_is_open(void)
{
    return notify_popup_is_open(&s_desktop_notify);
}


/* Return the desktop notification window identifier */
xcb_window_t notify_desktop_window(void)
{
    return notify_popup_window(&s_desktop_notify);
}


/* Return milliseconds remaining before the notification auto-closes */
int notify_desktop_ms_remaining(void)
{
    return notify_popup_ms_remaining(&s_desktop_notify,
            WM_DESKTOP_NOTIFY_TIMEOUT_MS);
}
