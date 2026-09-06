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
#include <cmds/surface.h>
#include <config.h>
#include <logger.h>
#include <render/text.h>
#include <surface.h>
#include <utils/safe/safestr.h>


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
    char text[WM_DESKTOP_MAX_LENGTH_NAME + 64];
    const desktop_td *desktop;
    uint32_t vp_col;
    uint32_t vp_row;

    if (connection == NULL || surface == NULL || cfg == NULL ||
            surface->screen == NULL) {
        return;
    }

    if (!cfg->desktops.show_overlay) {
        return;
    }

    /* The one place that names a desktop is 'surface_desktop_label',
     * so that this overlay and every window list say it the same way.
     * The name is wanted here: an overlay announcing a switch has
     * room for it and nothing else to identify the desktop by. */
    surface_desktop_label(surface, desktop_idx, desktop_name, false,
            true, text, sizeof(text));

    /* Append the viewport page the desktop being switched to is
     * panned to, the same 'nothing to add on a single-page setup'
     * gating 'scmd_surface_viewport_desktop_page' already does on its
     * own, so this stays silent unless a viewport is actually
     * configured. */
    desktop = surface_desktop_get(surface, desktop_idx);
    if (desktop != NULL &&
            scmd_surface_viewport_desktop_page(surface, desktop,
                &vp_col, &vp_row)) {
        char vp_buf[24];

        (void) snprintf(vp_buf, sizeof(vp_buf), " {%u,%u}",
                vp_col, vp_row);
        (void) safe_strncat(text, vp_buf, sizeof(text));
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
