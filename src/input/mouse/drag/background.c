/**
 * @file input/mouse/drag/background.c
 *
 * @brief Drag-to-pan the viewport by clicking and dragging the empty
 *        desktop background
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

/* ADT includes */
#include <adt/list.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/input.h>

/* Project includes */
#include <client.h>
#include <cmds/surface.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <input/mouse/cursor.h>
#include <input/mouse/drag/background.h>


/**
 * @brief Background-pan drag state
 *
 * Entirely separate from @c drag/internal.h's @c s_drag: there is no
 * client involved, only a surface and the viewport origin its current
 * desktop started this drag at.
 */
typedef struct {
    bool is_active;
    surface_td *surface;
    int16_t pointer_start_x;
    int16_t pointer_start_y;
    struct position_s origin_start;
} bg_drag_state_td;


/**
 * @brief Singleton background-pan drag state
 */
static bg_drag_state_td s_bg = {
    .is_active = false,
    .surface = NULL,
    .pointer_start_x = 0,
    .pointer_start_y = 0,
    .origin_start = { 0, 0 }
};


/* Begin a background-pan drag */
void drag_background_start(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t root,
        xcb_timestamp_t event_time, struct position_s root_pos)
{
    xcb_grab_pointer_cookie_t grab_cookie;
    xcb_grab_pointer_reply_t *grab_reply;
    desktop_td *desktop;
    xcb_cursor_t drag_cursor;

    if (connection == NULL || surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    /* A plain '{1,1}' desktop can never actually be panned (see
     * 'surface_viewport_has_room', 'surface/viewport.c'), so showing
     * the move cursor for the whole button hold would promise a pan
     * this drag can never deliver; the state below is still armed
     * exactly the same either way, so a release with no real movement
     * keeps unfocusing the active client like a plain background click
     * always has. */
    drag_cursor = (surface_viewport_has_room(surface))
        ? mouse_cursor_move()
        : mouse_plain_cursor();

    grab_cookie = xcb_grab_pointer(connection,
            0,
            root,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            drag_cursor,
            event_time);
    grab_reply = xcb_grab_pointer_reply(connection, grab_cookie, NULL);

    if (grab_reply == NULL ||
            grab_reply->status != XCB_GRAB_STATUS_SUCCESS) {
        LOGGER_WARNING("xcb_grab_pointer failed for background pan" \
                " start, status=%d",
                (grab_reply != NULL) ? (int) grab_reply->status : -1);
        free(grab_reply);
        return;
    }
    free(grab_reply);

    s_bg.is_active = true;
    s_bg.surface = surface;
    s_bg.pointer_start_x = (int16_t) root_pos.x;
    s_bg.pointer_start_y = (int16_t) root_pos.y;
    s_bg.origin_start = desktop->viewport_origin;
}


/* Update the in-progress background-pan drag on a motion-notify event */
void drag_background_update(xcb_connection_t *connection,
        struct position_s root_pos)
{
    int32_t dx;
    int32_t dy;

    (void) connection;

    if (!s_bg.is_active || s_bg.surface == NULL) {
        return;
    }

    dx = root_pos.x - (int32_t) s_bg.pointer_start_x;
    dy = root_pos.y - (int32_t) s_bg.pointer_start_y;

    scmd_surface_viewport_set(s_bg.surface,
            s_bg.origin_start.x - dx, s_bg.origin_start.y - dy);
}


/* Finish the background-pan drag on a button-release event */
void drag_background_end(xcb_connection_t *connection,
        list_td *surfaces, struct position_s root_pos)
{
    int32_t dx;
    int32_t dy;

    if (!s_bg.is_active) {
        return;
    }

    dx = root_pos.x - (int32_t) s_bg.pointer_start_x;
    dy = root_pos.y - (int32_t) s_bg.pointer_start_y;

    /* Never moved past the click threshold: treat exactly like the
     * plain background click this always was, unfocusing the active
     * client so every window loses its selection highlight */
    if (dx * dx + dy * dy < WM_ICON_DRAG_THRESHOLD &&
            s_bg.surface != NULL) {
        surface_td *const surface = s_bg.surface;
        desktop_td *const desktop = surface_desktop_get(surface,
                surface->desktop_cur);

        if (desktop != NULL && desktop->client_active_id != 0) {
            client_td *const active = lookup_find_client(surfaces,
                    desktop->client_active_id, NULL, NULL);

            if (active != NULL) {
                enact_client_unfocus(active);
            }
            desktop->client_active_id = 0;
            desktop->is_focus_dirty = true;
            desktop->is_outdated = true;
            surface->is_outdated = true;
        }
    }

    xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
    s_bg.is_active = false;
    s_bg.surface = NULL;
}


/* Query whether a background-pan drag is currently active */
bool drag_background_is_active(void)
{
    return s_bg.is_active;
}
