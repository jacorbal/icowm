/**
 * @file loop/event/motion.c
 *
 * @brief Pointer motion event handler for the main loop
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
#include <stdlib.h>     /* free, NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Input includes */
#include <input/mouse/drag.h>
#include <input/mouse/drag/background.h>
#include <input/mouse/event.h>
#include <input/mouse/hover.h>
#include <input/mouse/viewport/edge.h>

/* Policy includes */
#include <policy/placement/manual.h>

/* Menu includes */
#include <menu/context/iconmenu.h>
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/search.h>

/* Local includes */
#include <loop/event.h>
#include <utils/xcb/connection.h>


/** Whoever owns the pointer at the moment a motion event arrives */
enum s_loop_event_motion_target_e {
    S_MOTION_TARGET_MANUAL,     /**< A window is being placed */
    S_MOTION_TARGET_WINCMENU,   /**< Window context menu is open */
    S_MOTION_TARGET_ROOTMENU,   /**< Root menu is open */
    S_MOTION_TARGET_WINLIST,    /**< Window list is open */
    S_MOTION_TARGET_ICONMENU,   /**< Icon context menu is open */
    S_MOTION_TARGET_SEARCH,     /**< Search widget has the pointer */
    S_MOTION_TARGET_HOVER,      /**< Nobody: plain hover tracking */
    S_MOTION_TARGET_NONE        /**< A drag wants it all to itself */
};


/**
 * @brief Collapse a run of consecutive pending motion events
 *
 * The X server can queue many of these faster than one round of
 * window-move (or resize) plus @c xcb_flush can be processed,
 * especially for a large or decorated window whose move is more
 * expensive per event (the frame itself repaints, and reparented-child
 * bookkeeping adds further server-side cost on top of a plain top-level
 * window's move).  Reacting to every stale intermediate position
 * instead of jumping straight to the newest one is what makes a drag
 * visibly lag behind the pointer, worse the more expensive that
 * per-event work is.
 *
 * @param ctx   Main loop context, whose lookahead slot receives the
 *              first non-motion event found, if any
 * @param event Newest motion event so far, replaced in place by any
 *              newer one found; superseded events are freed here
 *
 * @note The non-motion event that ends the run is kept rather than
 *       dropped, so it is still handled on the very next pass
 * @note Complexity: @e O(q), where @e q is the number of queued motion
 *       events collapsed
 */
static void s_loop_event_motion_collapse(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    while ((ctx->pending_event =
                xcb_poll_for_event(xcb_connection_get())) != NULL) {
        if ((uint8_t) (ctx->pending_event->response_type & ~0x80u) !=
                XCB_MOTION_NOTIFY) {
            break;
        }
        free(*event);
        *event = ctx->pending_event;
    }
}


/**
 * @brief Decide who should receive a motion event
 *
 * @param me Motion event, for the windows it names
 *
 * @return The owner of the pointer for this event
 *
 * @note Complexity: @e O(1)
 */
static enum s_loop_event_motion_target_e s_loop_event_motion_target(
        const xcb_motion_notify_event_t *me)
{
    /* Asked before every menu below, not after: a window that opens
     * while one of them happens to be up takes the pointer away from it
     * outright, so the menu is no longer the one being pointed at
     * whatever it still believes about itself. */
    if (place_manual_is_active()) {
        return S_MOTION_TARGET_MANUAL;
    }

    if (wincmenu_is_open()) {
        return S_MOTION_TARGET_WINCMENU;
    }
    if (rootmenu_is_open()) {
        return S_MOTION_TARGET_ROOTMENU;
    }
    if (winlist_is_open()) {
        return S_MOTION_TARGET_WINLIST;
    }
    if (iconmenu_is_open()) {
        return S_MOTION_TARGET_ICONMENU;
    }
    if (search_is_open() && (me->event == search_window() ||
                me->child == search_window())) {
        return S_MOTION_TARGET_SEARCH;
    }
    if (drag_is_active() || drag_background_is_active()) {
        return S_MOTION_TARGET_NONE;
    }

    return S_MOTION_TARGET_HOVER;
}


/* Handle a 'MOTION_NOTIFY' event */
void loop_event_motion_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    xcb_motion_notify_event_t *me;

    if (ctx == NULL || event == NULL || *event == NULL) {
        return;
    }

    s_loop_event_motion_collapse(ctx, event);
    me = (xcb_motion_notify_event_t *) *event;

    /* Fed the newest position unconditionally, before deciding who else
     * gets it: an inactive drag ignores it anyway */
    drag_update(xcb_connection_get(),
            (struct position_s) { me->root_x, me->root_y });
    drag_background_update(xcb_connection_get(),
            (struct position_s) { me->root_x, me->root_y });

    switch (s_loop_event_motion_target(me)) {
        case S_MOTION_TARGET_MANUAL:
            place_manual_handle_motion(xcb_connection_get(),
                    (struct position_s) { me->root_x, me->root_y });
            break;

        case S_MOTION_TARGET_WINCMENU:
            wincmenu_handle_motion(me->event, me->event_x, me->event_y);
            break;

        case S_MOTION_TARGET_ROOTMENU:
            rootmenu_handle_motion(me->event, me->event_x, me->event_y);
            break;

        case S_MOTION_TARGET_WINLIST:
            winlist_handle_motion(me->event, me->event_x, me->event_y);
            break;

        case S_MOTION_TARGET_ICONMENU:
            iconmenu_handle_motion(me->event, me->event_x,
                    me->event_y);
            break;

        case S_MOTION_TARGET_SEARCH:
            search_handle_motion(me->event_x, me->event_y);
            break;

        case S_MOTION_TARGET_HOVER:
            mouse_handle_motion_hover(xcb_connection_get(), ctx->surfaces,
                    me);
            mouse_viewport_edge_check(ctx->surfaces, me->root,
                    me->root_x, me->root_y);
            break;

        case S_MOTION_TARGET_NONE:
            /* A running drag already got the position above, and wants
             * nothing else looking at the pointer while it lasts */
            break;
    }
}
