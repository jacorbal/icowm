/**
 * @file input/mouse/event.c
 *
 * @brief Mouse button-press, button-release, and enter-notify handlers
 *
 * Implements the three public event-dispatch functions: button-press
 * (handles popup/cycle dismissal, icon drag, titlebar buttons, and
 * configured move/resize/lower bindings), button-release (finalises
 * drags), and enter-notify (focus-follows-mouse).
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
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <lookup.h>
#include <surface.h>

/* Defs includes */
#include <defs/wm.h>

/* Policy includes */
#include <policy/focus.h>

/* Menu includes */
#include <menu/cycle.h>
#include <menu/popup.h>

/* Local includes */
#include <input/drag.h>
#include <input/mouse.h>


/* Flag set while a hover-triggered focus transfer is in flight.  Set in
 * 'mouse_handle_enter' before calling focus_apply, cleared in
 * 'handler_focus_in' so that the 'FocusIn' event from the hover does
 * not move 'client_active_id' away from the explicitly-focused
 * window. */
static bool s_enter_focus_active = false;


/* Mirror sticky focus on the currently shown desktop of a surface */
static void s_mouse_sync_sticky_active(surface_td *surface,
        desktop_td *owner_desktop, client_td *client)
{
    desktop_td *current_desktop;

    if (surface == NULL || owner_desktop == NULL || client == NULL) {
        return;
    }

    if (!client_is_sticky(client)) {
        return;
    }

    current_desktop = lookup_current_desktop(surface);
    if (current_desktop == NULL ||
            current_desktop->id == owner_desktop->id) {
        return;
    }

    current_desktop->client_active_id = client->id;
    current_desktop->is_outdated = true;
    surface->is_outdated = true;
}


/* Dispatch a button-press event */
void mouse_handle_press(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *cfg)
{
    xcb_window_t window;
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;
    uint16_t state;
    enum wm_mousebind_type_e type = MOUSEBIND_NONE;
    xcb_window_t w;
    xcb_query_tree_cookie_t qt_c;
    xcb_query_tree_reply_t *qt_r;
    xcb_window_t qt_parent;
    xcb_window_t qt_root;

    if (connection == NULL || event == NULL || cfg == NULL) {
        return;
    }

    if (popup_is_open()) {
        surface = lookup_surface_for_root(surfaces, event->root);
        if (event->event == popup_window() ||
                event->child == popup_window()) {
            popup_close(connection);
            if (surface != NULL) {
                desktop_td *cur = surface_desktop_get(surface,
                        surface->desktop_cur);
                if (cur != NULL) {
                    cur->is_outdated = true;
                }
                (void) surface_render_all_desktops(surface);
            }
            xcb_flush(connection);
            return;
        }
        popup_close(connection);
        if (surface != NULL) {
            desktop_td *cur = surface_desktop_get(surface,
                    surface->desktop_cur);
            if (cur != NULL) {
                cur->is_outdated = true;
            }
            (void) surface_render_all_desktops(surface);
        }
    }

    if (cycle_is_open()) {
        if (event->event == cycle_window() ||
                event->child == cycle_window()) {
            if ((int) event->event_y >= WM_CYCLE_MENU_PAD_Y) {
                unsigned int row = (unsigned int)(
                        ((int) event->event_y - WM_CYCLE_MENU_PAD_Y) /
                        WM_CYCLE_MENU_ROW_HEIGHT);
                cycle_navigate_to(row);
                cycle_confirm(connection, surfaces, cfg);
            } else {
                cycle_close(connection);
            }
        } else {
            cycle_close(connection);
        }
        xcb_allow_events(connection,
                XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(connection);
        return;
    }

    window = (event->child != XCB_NONE) ? event->child : event->event;
    client = lookup_find_client(surfaces, window, NULL, &desktop);

    if (client != NULL && window == client->icon_window) {
        if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_1) {
            xcb_get_geometry_cookie_t gc;
            xcb_get_geometry_reply_t *gr;
            int32_t icon_x;
            int32_t icon_y;

            gc = xcb_get_geometry(connection, client->icon_window);
            gr = xcb_get_geometry_reply(connection, gc, NULL);
            icon_x = (gr != NULL)
                ? (int32_t) gr->x : (int32_t) client->icon_x;
            icon_y = (gr != NULL)
                ? (int32_t) gr->y : (int32_t) client->icon_y;
            if (gr != NULL) {
                free(gr);
            }

            drag_start_icon(connection, event->root, client,
                    icon_x, icon_y,
                    event->time, event->root_x, event->root_y);

            xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                    event->time);
            xcb_flush(connection);
            return;
        }

        /* Non-left-click on icon: restore immediately */
        (void) client_send_event_restore(client);
        surface = lookup_surface_for_root(surfaces, event->root);
        if (surface != NULL && desktop != NULL) {
            focus_apply(surfaces, surface, desktop, client,
                    true, cfg);
            s_mouse_sync_sticky_active(surface, desktop, client);
        }
        xcb_allow_events(connection,
                XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(connection);
        return;
    }

    state = (uint16_t) ((unsigned int) event->state &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    for (int i = 0; i < mousebind_count(); ++i) {
        xcb_button_index_t btn;
        uint16_t req;
        enum wm_mousebind_type_e t = mousebind_at(i, &btn, &req);

        if (t == MOUSEBIND_NONE) {
            continue;
        }
        if ((xcb_button_index_t) event->detail == btn) {
            if (req == 0 || (state & req) == req) {
                type = t;
                break;
            }
        }
    }

    if (type == MOUSEBIND_DESKTOP_NEXT ||
            type == MOUSEBIND_DESKTOP_PREV) {
        if (client != NULL) {
            /* Cursor is over a managed window; ignore desktop scroll */
            xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                    event->time);
            xcb_flush(connection);
            return;
        }

        surface = lookup_surface_for_root(surfaces, event->root);
        if (surface != NULL) {
            event_td *ev;
            action_td action;
            action.type = ACTION_TYPE_SURFACE;
            action.object.surface = (type == MOUSEBIND_DESKTOP_NEXT)
                ? ACTION_SURFACE_DESKTOP_SWITCH_NEXT
                : ACTION_SURFACE_DESKTOP_SWITCH_PREV;
            ev = event_init((void *) surface, NULL, action,
                    PRIORITY_NORMAL);
            if (ev != NULL) {
                eventq_add(ev);
            }
        }
        xcb_allow_events(connection,
                XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(connection);
        return;
    }

    if (type == MOUSEBIND_NONE) {
        if (client != NULL) {
            surface = lookup_surface_for_root(surfaces, event->root);
            if (surface != NULL && desktop != NULL) {
                focus_apply(surfaces, surface, desktop, client,
                        true, cfg);
            }

            if (event->child == client->titlebar &&
                    client->titlebar != 0) {
                int ex = (int) event->event_x;
                int ey = (int) event->event_y;
                int left = (int) client->layout.frame_extents.left;
                int right = (int) client->layout.frame_extents.right;
                int frame_w = (int) client->layout.geometry.cur.dim.w;
                int fw = (frame_w > left + right)
                    ? frame_w - left - right
                    : 1;
                int btn = (int) WM_DECOR_BTN_SIZE;
                int gap = (int) WM_DECOR_BTN_GAP;
                int pad = (int) WM_DECOR_BTN_PAD;
                int step = btn + gap;
                int title_h = (int) client->title_height;
                int btn_y = (title_h > btn) ? (title_h - btn) / 2 : 0;

                if (ey >= btn_y && ey < btn_y + btn) {
                    if (ex >= pad && ex < pad + btn) {
                        client_send_event(client,
                                ACTION_CLIENT_TOGGLE_STICKY,
                                PRIORITY_NORMAL);
                    } else {
                        static const enum action_client_e btn_actions[6] = {
                            ACTION_CLIENT_CLOSE,
                            ACTION_CLIENT_TOGGLE_FULLSCREEN,
                            ACTION_CLIENT_MAXIMIZE,
                            ACTION_CLIENT_TOGGLE_SHADE,
                            ACTION_CLIENT_HIDE,
                            ACTION_CLIENT_ICONIFY
                        };
                        for (int bi = 0; bi < 6; ++bi) {
                            int bx = fw - pad - btn - bi * step;
                            if (ex >= bx && ex < bx + btn) {
                                client_send_event(client,
                                        btn_actions[bi],
                                        PRIORITY_NORMAL);
                                break;
                            }
                        } /* ! for (bi) */
                    }
                } /* ! if (ey) */
            }

            if (event->child == client->window) {
                xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER,
                        event->time);
            } else {
                xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                        event->time);
            }
        } else {
            if (event->event != event->root) {
                xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                        event->time);
            }
        }
        xcb_flush(connection);
        return;
    }

    /* Re-lookup after determining the binding type: walk up tree if
     * needed to find the managed ancestor */
    window = (event->child != XCB_NONE) ? event->child : event->event;
    client = lookup_find_client(surfaces, window, NULL, &desktop);
    if (client == NULL && event->child != XCB_NONE) {
        w = event->child;
        while (client == NULL) {
            qt_c = xcb_query_tree(connection, w);
            qt_r = xcb_query_tree_reply(connection, qt_c, NULL);
            if (qt_r == NULL) {
                break;
            }
            qt_parent = qt_r->parent;
            qt_root = qt_r->root;
            free(qt_r);
            if (qt_parent == XCB_NONE || qt_parent == qt_root) {
                break;
            }
            w = qt_parent;
            client = lookup_find_client(surfaces, w, NULL, &desktop);
        }
    }

    if (client == NULL) {
        xcb_allow_events(connection,
                XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(connection);
        return;
    }

    if (type == MOUSEBIND_RESIZE && !client_is_resizable(client)) {
        return;
    }

    if (type == MOUSEBIND_LOWER) {
        if (desktop != NULL) {
            surface = lookup_surface_for_root(surfaces, event->root);
            if (surface != NULL) {
                focus_apply(surfaces, surface, desktop, client,
                        true, cfg);
                s_mouse_sync_sticky_active(surface, desktop, client);
            } else {
                desktop->client_active_id = client->id;
            }
        }
        (void) client_send_event_lower(client);
        xcb_allow_events(connection,
                XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(connection);
        return;
    }

    if (desktop != NULL) {
        desktop->client_active_id = client->id;
        (void) desktop_action_client_send_front(desktop, client);
        surface = lookup_surface_for_root(surfaces, event->root);
        s_mouse_sync_sticky_active(surface, desktop, client);
    }

    drag_start(connection, event->root, client,
            (type == MOUSEBIND_MOVE)
                ? CLIENT_OPERATION_MOVING
                : CLIENT_OPERATION_RESIZING,
            event->time,
            event->root_x, event->root_y);
}


/* Handle a button-release event to end a drag */
void mouse_handle_release(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_release_event_t *event,
        const config_td *cfg)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    int16_t root_x = 0;
    int16_t root_y = 0;

    (void) cfg;

    if (!drag_is_active()) {
        return;
    }

    if (event != NULL) {
        root_x = event->root_x;
        root_y = event->root_y;
    }

    client = drag_client();
    if (client != NULL && surfaces != NULL) {
        (void) lookup_find_client(surfaces, client->id,
                &surface, &desktop);
    }

    drag_end(connection, surface, desktop, root_x, root_y);
}


/* Apply focus-follows-mouse on an enter-notify event */
void mouse_handle_enter(xcb_connection_t *connection,
        list_td *surfaces, xcb_enter_notify_event_t *event,
        const config_td *cfg)
{
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;
    xcb_window_t prev_active;

    if (connection == NULL || event == NULL || cfg == NULL) {
        return;
    }

    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        return;
    }

    if (event->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
        return;
    }

    if (!focus_is_follow_mouse(cfg)) {
        return;
    }

    client = lookup_find_client(surfaces, event->event, NULL, &desktop);
    if (client == NULL) {
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface == NULL || desktop == NULL) {
        return;
    }

    prev_active = desktop->client_active_id;
    s_enter_focus_active = true;
    focus_apply(surfaces, surface, desktop, client, false, cfg);
    desktop->client_active_id = prev_active;

    xcb_flush(connection);
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
