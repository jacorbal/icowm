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
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdlib.h>     /* free */

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

        /* A 'NONLINEAR' crossing raised by one of the active client's
         * own sub-windows (its content and titlebar are siblings
         * under the frame, so moving between them is never
         * 'INFERIOR') is not the pointer actually leaving that
         * client, only passing through their shared parent; unfocusing
         * here would just be undone by 'mouse_handle_enter' focusing
         * the same client back a moment later, with nothing in
         * between but a redundant repaint.  'event->child' names
         * nothing useful for a 'NONLINEAR' leave (ICCCM has no window
         * to put there), so only a fresh 'QueryPointer' on the root
         * reveals whether the destination frame still belongs to
         * 'active'. */
        if (event->detail == XCB_NOTIFY_DETAIL_NONLINEAR &&
                active != NULL && surface != NULL &&
                surface->screen != NULL) {
            xcb_query_pointer_reply_t *const pointer_reply =
                xcb_query_pointer_reply(wm_connection(wm),
                        xcb_query_pointer(wm_connection(wm),
                                surface->screen->root), NULL);

            if (pointer_reply != NULL) {
                client_td *const entered =
                    (pointer_reply->child != XCB_WINDOW_NONE)
                        ? lookup_find_client(wm_surfaces(wm),
                                pointer_reply->child, NULL, NULL)
                        : NULL;
                const bool is_sibling_crossing = (entered == active);

                free(pointer_reply);
                if (is_sibling_crossing) {
                    return;
                }
            }
        }

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
