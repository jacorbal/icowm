/**
 * @file input/mouse/event/enter.c
 *
 * @brief Mouse enter-notify handling and hover-triggered focus state
 *
 * One of the files @c input/mouse/event/ is made of;
 * carries its own @c s_enter_focus_active state, used by nothing
 * outside this file and @c handler_focus_in (@c handler/focus.c), which
 * clears it.  See @c input/mouse/event/press.c's own comment for the
 * reasoning behind the three-way split.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Policy includes */
#include <policy/focus.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <input/mouse/cursor.h>
#include <input/mouse/event.h>
#include <input/mouse/hover.h>
#include <input/mouse/internal.h>


/**
 * @brief Flag set while a hover-triggered focus transfer is in flight
 *
 * Set in @a mouse_handle_enter before calling @a focus_apply and
 * cleared in @a handler_focus_in when the corresponding @c FocusIn
 * event arrives.
 */
static bool s_enter_focus_active = false;


/* Re-evaluate the resize cursor, then apply focus-follows-mouse, on
 * an enter-notify event */
void mouse_handle_enter(xcb_connection_t *connection,
        list_td *surfaces, xcb_enter_notify_event_t *event,
        const config_td *config)
{
    client_td *client;
    client_td *entered;
    desktop_td *desktop;
    surface_td *surface;

    if (connection == NULL || event == NULL || config == NULL) {
        return;
    }

    LOGGER_TRACE("Enter notify (event=0x%x, child=0x%x," \
            " root=%+d%+d, mode=%u, detail=%u)",
            event->event, event->child, event->root_x, event->root_y,
            event->mode, event->detail);

    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        return;
    }

    /* Independent of focus-follows-mouse below: a resizable client that
     * selects 'PointerMotion' for its own purposes (common in GTK/Qt
     * applications tracking hover for their own UI) intercepts motion
     * events at the X11 propagation level before they ever reach
     * 'mouse_handle_motion_hover', so the cursor set while hovering
     * this client's own border never gets re-evaluated once the pointer
     * moves on into that client's content area; this 'EnterNotify',
     * unlike motion, still fires reliably since it was selected
     * directly on this client's own window (see 'client.c'), giving the
     * resize-cursor logic a second, independent chance to catch what
     * motion alone might have missed. */
    entered = mouse_resize_cursor_update(connection, surfaces,
            event->event,
            (struct position_s) { event->root_x, event->root_y });

    /* An undecorated client has no separate frame window to fall back
     * on at all: moving from its border to its interior (or back)
     * happens entirely within this one same window, with no crossing
     * whatsoever for any further 'EnterNotify' to catch, and its own
     * 'PointerMotion' may be just as intercepted as any other client's;
     * only a periodic poll (see 'mouse_hover_poll_tick') can still
     * catch that transition, so track it for one here. */
    mouse_hover_track((entered != NULL && entered->frame == 0)
            ? event->event : XCB_WINDOW_NONE);

    if (event->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
        return;
    }

    if (!focus_is_sloppy(config)) {
        return;
    }

    client = lookup_find_client(surfaces, event->event, NULL,
            &desktop);
    if (client == NULL) {
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface == NULL || desktop == NULL) {
        return;
    }

    s_enter_focus_active = true;
    focus_apply(surfaces, surface, desktop, client, false, config);

}


/* Query whether a hover-triggered focus transfer is in progress */
bool mouse_enter_focus_is_active(void)
{
    return s_enter_focus_active;
}


/* Clear the hover-triggered focus flag */
void mouse_enter_focus_clear(void)
{
    s_enter_focus_active = false;
}
