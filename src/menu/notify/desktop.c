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
#include <i18n.h>
#include <logger.h>
#include <render/text.h>
#include <surface.h>
#include <utils/safe/safestr.h>


/* Default initial values */
#include <defs/desktop.h>
#include <defs/uistr.h>

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
        const char *desktop_name,
        enum notify_desktop_cause_e cause, const config_td *cfg)
{
    /* The name, plus every fixed part that can be appended to it,
     * each sized for a 'uint32_t' spelled out in full: the base
     * message, the ' {column, row}' page suffix and the
     * ' (on surface n)' one.  Sized rather than trimmed because a
     * truncation here would cut a coordinate in half and leave the
     * notice naming a page that does not exist. */
    char text[WM_DESKTOP_MAX_LENGTH_NAME + 96];
    const desktop_td *desktop;
    uint32_t vp_col;
    uint32_t vp_row;
    bool has_page;
    size_t used;

    if (connection == NULL || surface == NULL || cfg == NULL ||
            surface->screen == NULL) {
        return;
    }

    if ((cause == NOTIFY_DESKTOP_CAUSE_SWITCH &&
                !cfg->base.overlay.on_desktop_switch) ||
            (cause == NOTIFY_DESKTOP_CAUSE_VIEWPORT &&
                !cfg->base.overlay.on_viewport_move)) {
        return;
    }

    text[0] = '\0';
    desktop = surface_desktop_get(surface, desktop_idx);
    has_page = desktop != NULL &&
        scmd_surface_viewport_desktop_page(surface, desktop,
                &vp_col, &vp_row);

    /* Nothing worth naming: one desktop and a viewport that cannot
     * pan means the view never moves anywhere the user could not
     * already see, so no popup at all rather than an empty one */
    if (surface->desktop_count <= 1u && !has_page) {
        return;
    }

    /* Only the page, with the word spelled out, when the desktop has
     * nothing to add: with a single desktop its index and name name
     * the only thing there is */
    if (surface->desktop_count <= 1u) {
        (void) snprintf(text, sizeof(text),
                _(STR_NOTIFY_VIEWPORT_PAGE_FMT), vp_col, vp_row);
        notify_popup_show_centered(connection, surface,
                &s_desktop_notify, text, cfg);
        LOGGER_TRACE("Desktop notify shown: '%s'", text);
        return;
    }

    /* A named desktop leads with its own name, ahead of the
     * coordinates, since that is what the user chose and recognizes.
     * Everything after it is appended, never inserted, so an absent
     * name simply leaves the rest starting where it would anyway. */
    if (desktop_name != NULL && desktop_name[0] != '\0') {
        (void) snprintf(text, sizeof(text), "%s: ", desktop_name);
    }

    /* The one place that names a desktop is 'surface_desktop_label',
     * so that this overlay and every window list say it the same way.
     * The name is passed separately above, so it is not asked for
     * again here. */
    used = safe_strlen(text);
    if (used < sizeof(text)) {
        surface_desktop_label(surface, desktop_idx, desktop_name,
                false, false, text + used, sizeof(text) - used);
    }

    if (has_page) {
        char vp_buf[32];

        (void) snprintf(vp_buf, sizeof(vp_buf),
                _(STR_PAGE_SUFFIX_FMT), vp_col, vp_row);
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
