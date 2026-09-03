/**
 * @file handler/crossing.c
 *
 * @brief X @c LEAVE_NOTIFY event handler
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* Policy includes */
#include <policy/focus.h>

/* Input includes */
#include <input/mouse/event.h>
#include <input/mouse/hover.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <handler.h>


/* Handle a 'LEAVE_NOTIFY' event */
void handler_leave_notify(const wm_td *wm,
        xcb_leave_notify_event_t *event)
{
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    const config_td *config = wm_config(wm);

    if (wm == NULL || event == NULL) {
        return;
    }

    /* Independent of focus-follows-mouse below: a resize-cursor poll
     * target (see 'mouse_hover_poll_tick' in input/mouse/hover.h)
     * tracked for this window must stop being polled once the pointer
     * has actually left it, regardless of whether hover also affects
     * focus. */
    mouse_hover_poll_clear(event->event);

    /* Same reasoning, for a delayed sloppy-focus armed by this same
     * window's 'EnterNotify' (see 'windows.focus.delay-ms',
     * 'mouse_handle_enter' in input/mouse/event/enter.c): a pointer
     * leaving before that delay elapses must never end up focusing a
     * client it already moved past. */
    mouse_enter_focus_cancel(event->event);

    if (focus_is_sloppy(config) &&
            event->mode == XCB_NOTIFY_MODE_NORMAL &&
            event->detail != XCB_NOTIFY_DETAIL_INFERIOR &&
            lookup_find_client(wm_surfaces(wm), event->event,
                    &surface, &desktop) != NULL &&
            desktop != NULL && desktop->client_active_id != 0) {
        client_td *const active = lookup_find_client(wm_surfaces(wm),
                desktop->client_active_id, NULL, NULL);

        /* Pointer left a managed window; release focus the same way
         * clicking the empty desktop background does (see
         * 'mouse_handle_press', input/mouse/event/press.c), through
         * 'enact_client_unfocus' rather than by hand, so the EWMH
         * '_NET_WM_STATE_FOCUSED' mark, '_NET_ACTIVE_WINDOW', and the
         * client's own decoration all stay in sync with the real
         * input focus this already redirects to the pointer root */
        if (active != NULL) {
            enact_client_unfocus(active);
        }
        desktop->client_active_id = 0;
        desktop->is_focus_dirty = true;
        desktop->is_outdated = true;
        if (surface != NULL) {
            surface->is_outdated = true;
        }
    }
}
