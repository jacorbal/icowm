/**
 * @file input/mouse/event.c
 *
 * @brief Mouse button-press, button-release, and enter-notify handlers
 *
 * Implements the three public event-dispatch functions: button-press
 * (handles popup/cycle dismissal, icon drag, titlebar buttons, and
 * configured move/resize/lower bindings), button-release (finalizes
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

/* Policy includes */
#include <policy/focus.h>

/* Menu includes */
#include <menu/cycle.h>
#include <menu/dialog/quit.h>
#include <menu/popup.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* CMD includes */
#include <cmds/ccmd.h>

/* Local includes */
#include <input/mouse/drag.h>
#include <input/mouse.h>


/**
 * @brief Flag set while a hover-triggered focus transfer is in flight
 *
 * Set in @a mouse_handle_enter before calling @a focus_apply and
 * cleared in @a handler_focus_in when the corresponding @c FocusIn
 * event arrives.
 */
static bool s_enter_focus_active = false;

/* State for double-click detection on titlebars.  A double-click on the
 * titlebar drag area (i.e. not on a button) toggles shade/unshade. */
static xcb_timestamp_t s_last_titlebar_press_time = 0;
static xcb_window_t s_last_titlebar_press_win = XCB_NONE;


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
    current_desktop->focus_dirty = true;
    current_desktop->is_outdated = true;
    surface->is_outdated = true;
}


/**
 * @brief Resolve an event window to its associated managed client
 *
 * Attempts to find the client corresponding to a button event by first
 * checking the given event or child window directly.  If the event
 * occurred on a child window that is not explicitly managed, the
 * function walks up the X11 window hierarchy using @a xcb_query_tree
 * until it finds a parent window associated with a known client or
 * reaches the root.
 *
 * @param connection   Active XCB connection, or @c NULL to disable
 *                     hierarchy traversal
 * @param surfaces     List of managed client surfaces
 * @param event_window The window where the event was reported
 * @param child_window The child window under the pointer, or @c XCB_NONE
 * @param out_desktop  Output pointer for the client's desktop, or @c NULL
 *
 * @return Pointer to the resolved client, or @c NULL if no matching
 *         client is found
 *
 * @note Optionally returns the desktop containing the resolved client
 * @note Complexity: @e O(h), where @e h is the height of the window
 *       hierarchy
 */
static client_td *s_mouse_find_event_client(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t event_window,
        xcb_window_t child_window, desktop_td **out_desktop)
{
    client_td *client;
    xcb_window_t window;

    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }

    if (surfaces == NULL) {
        return NULL;
    }

    window = (child_window != XCB_NONE) ? child_window : event_window;
    client = lookup_find_client(surfaces, window, NULL, out_desktop);
    if (client != NULL || connection == NULL ||
            child_window == XCB_NONE) {
        return client;
    }

    window = child_window;
    while (client == NULL) {
        xcb_query_tree_cookie_t qt_c = xcb_query_tree(connection,
                window);
        xcb_query_tree_reply_t *qt_r =
            xcb_query_tree_reply(connection, qt_c, NULL);
        xcb_window_t parent;
        xcb_window_t root;

        if (qt_r == NULL) {
            break;
        }

        parent = qt_r->parent;
        root = qt_r->root;
        free(qt_r);

        if (parent == XCB_NONE || parent == root) {
            break;
        }

        window = parent;
        client = lookup_find_client(surfaces, window, NULL,
                out_desktop);
    }

    return client;
}


/* Dispatch a button-press event */
void mouse_handle_press(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    xcb_window_t window;
    client_td *client;
    desktop_td *desktop = NULL;
    surface_td *surface = NULL;
    uint16_t state;
    enum wm_mousebind_type_e type = MOUSEBIND_NONE;
    uint32_t screen_w;
    uint32_t screen_h;

    if (connection == NULL || event == NULL || config == NULL) {
        return;
    }

    if (popup_is_open()) {
        surface = lookup_surface_for_root(surfaces, event->root);
        if (event->event == popup_window() ||
                event->child == popup_window()) {
            popup_close(connection);

            if (surface != NULL) {
                surface_render_current_desktop_repaint(surface);
            }
            xcb_flush(connection);
            return;
        }

        popup_close(connection);

        if (surface != NULL) {
            surface_render_current_desktop_repaint(surface);
        }
    }

    if (dialog_quit_is_open()) {
        if (event->event == dialog_quit_window() ||
                event->child == dialog_quit_window()) {
            if (dialog_quit_handle_click(connection,
                        (int) event->event_x,
                        (int) event->event_y)) {
                xcb_allow_events(connection,
                        XCB_ALLOW_ASYNC_POINTER, event->time);
                xcb_flush(connection);
                return;
            }
        }

        /* Click outside dialog: close without action */
        dialog_quit_close(connection);
        xcb_allow_events(connection,
                XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(connection);
        return;
    }

    if (cycle_is_open()) {
        if (event->event == cycle_window() ||
                event->child == cycle_window()) {
            if ((int) event->event_y >= WM_CYCLE_MENU_PAD_Y) {
                unsigned int row = (unsigned int)(
                        ((int) event->event_y - WM_CYCLE_MENU_PAD_Y) /
                        WM_CYCLE_MENU_ROW_HEIGHT);
                cycle_navigate_to(row);
                cycle_confirm(connection, surfaces, config);
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
    client = s_mouse_find_event_client(connection, surfaces,
            event->event, event->child, &desktop);

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
                    true, config);
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
            /* Scroll on titlebar: shade (up) or unshade (down).
             * For root-grabbed buttons event->child is the frame, not
             * the titlebar, so verify the Y coordinate instead. */
            if (client->titlebar != 0 &&
                    (event->child == client->frame ||
                     event->child == client->titlebar)) {
                int32_t bw = client->layout.frame_extents.left;
                int32_t fy = client->layout.geometry.cur.pos.y;
                int32_t ty0 = fy + bw;
                int32_t ty1 = fy + client->layout.frame_extents.top;
                int32_t ry = (int32_t) event->root_y;
                if (ry >= ty0 && ry < ty1) {
                    if (type == MOUSEBIND_DESKTOP_PREV) {
                        if (!client_is_shaded(client)) {
                            client_send_event(client, ACTION_CLIENT_SHADE,
                                    PRIORITY_NORMAL);
                            if (desktop != NULL) {
                                desktop->is_outdated = true;
                            }
                            surface = lookup_surface_for_root(surfaces,
                                    event->root);
                            if (surface != NULL) {
                                surface->is_outdated = true;
                            }
                        }
                    } else if (client_is_shaded(client)) {
                        client_send_event(client, ACTION_CLIENT_UNSHADE,
                                PRIORITY_NORMAL);
                        if (desktop != NULL) {
                            desktop->is_outdated = true;
                        }
                        surface = lookup_surface_for_root(surfaces,
                                event->root);
                        if (surface != NULL) {
                            surface->is_outdated = true;
                        }
                    }
                }
            }
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
            if (client != NULL && surface != NULL && desktop != NULL &&
                    client_is_focusable(client)) {
                focus_apply(surfaces, surface, desktop, client,
                        true, config);
            }

            if (event->child == client->titlebar &&
                    client->titlebar != 0) {
                static const enum action_client_e btn_actions[6] = {
                    ACTION_CLIENT_CLOSE,
                    ACTION_CLIENT_TOGGLE_FULLSCREEN,
                    ACTION_CLIENT_MAXIMIZE,
                    ACTION_CLIENT_TOGGLE_SHADE,
                    ACTION_CLIENT_HIDE,
                    ACTION_CLIENT_ICONIFY
                };
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
                bool can_maximize;
                int btn_y = (title_h > btn) ? (title_h - btn) / 2 : 0;
                bool hit_btn = false;

                can_maximize = !client_is_fullscreen(client) &&
                    (bool) client_is_resizable(client);

                if (ey >= btn_y && ey < btn_y + btn) {
                    if (ex >= pad && ex < pad + btn) {
                        /* Left-aligned button 0: pin/sticky */
                        hit_btn = true;
                        client_send_event(client,
                                ACTION_CLIENT_TOGGLE_STICKY,
                                PRIORITY_NORMAL);
                    } else if (ex >= pad + step && ex < pad + step + btn) {
                        /* Left-aligned button 1: layer cycle */
                        hit_btn = true;
                        client_send_event(client,
                                ACTION_CLIENT_CYCLE_LAYER,
                                PRIORITY_NORMAL);
                        if (desktop != NULL) {
                            desktop->is_outdated = true;
                        }
                        if (surface != NULL) {
                            surface->is_outdated = true;
                        }
                    } else {
                        for (int bi = 0; bi < 6; ++bi) {
                            int bx = fw - pad - btn - bi * step;
                            if (ex >= bx && ex < bx + btn) {
                                if (!can_maximize &&
                                        (bi == 1 || bi == 2)) {
                                    hit_btn = true;
                                    break;
                                }
                                hit_btn = true;

                                if (bi == 2) {
                                    /* Maximize button:
                                     * - button 1: full maximization;
                                     * - button 2: vert. maximiz.;
                                     * - button 3: horz. maximiz. */
                                    if ((xcb_button_index_t)
                                            event->detail ==
                                            XCB_BUTTON_INDEX_2) {
                                        client_send_event(client,
                                                ACTION_CLIENT_MAXIMIZE_VERT,
                                                PRIORITY_NORMAL);
                                    } else if ((xcb_button_index_t)
                                            event->detail ==
                                            XCB_BUTTON_INDEX_3) {
                                        client_send_event(client,
                                                ACTION_CLIENT_MAXIMIZE_HORZ,
                                                PRIORITY_NORMAL);
                                    } else {
                                        client_send_event(client,
                                                ACTION_CLIENT_MAXIMIZE,
                                                PRIORITY_NORMAL);
                                    }
                                } else {
                                    client_send_event(client,
                                            btn_actions[bi],
                                            PRIORITY_NORMAL);
                                }

                                if (desktop != NULL) {
                                    desktop->is_outdated = true;
                                }
                                if (surface != NULL) {
                                    surface->is_outdated = true;
                                }
                                break;
                            }
                        } /* ! for (bi) */
                    }
                } /* ! if (ey) */

                /* Scroll wheel on titlebar shades or unshades the
                 * client; don't start a drag for these events. */
                if (!hit_btn) {
                    if ((xcb_button_index_t) event->detail ==
                            XCB_BUTTON_INDEX_4) {
                        hit_btn = true;

                        if (!client_is_shaded(client)) {
                            client_send_event(client,
                                    ACTION_CLIENT_SHADE,
                                    PRIORITY_NORMAL);
                            if (desktop != NULL) {
                                desktop->is_outdated = true;
                            }
                            if (surface != NULL) {
                                surface->is_outdated = true;
                            }
                        }
                    } else if ((xcb_button_index_t) event->detail ==
                            XCB_BUTTON_INDEX_5) {
                        hit_btn = true;

                        if (client_is_shaded(client)) {
                            client_send_event(client,
                                    ACTION_CLIENT_UNSHADE,
                                    PRIORITY_NORMAL);
                            if (desktop != NULL) {
                                desktop->is_outdated = true;
                            }
                            if (surface != NULL) {
                                surface->is_outdated = true;
                            }
                        }
                    }
                }

                /* Clicks that land on the titlebar but miss all buttons
                 * start a window-move drag, making the titlebar serve as
                 * a drag handle.  A double-click on the same titlebar
                 * within the threshold, toggles shade instead. */
                if (!hit_btn) {
                    xcb_timestamp_t dt = event->time -
                        s_last_titlebar_press_time;
                    xcb_window_t prev_win = s_last_titlebar_press_win;
                    s_last_titlebar_press_time = event->time;
                    s_last_titlebar_press_win = client->titlebar;

                    if (prev_win == client->titlebar &&
                            dt <= (xcb_timestamp_t) WM_DOUBLE_CLICK_MS) {
                        /* Double-click: reset state and toggle shade */
                        s_last_titlebar_press_time = 0;
                        s_last_titlebar_press_win = XCB_NONE;
                        client_send_event(client,
                                ACTION_CLIENT_TOGGLE_SHADE,
                                PRIORITY_NORMAL);

                        if (desktop != NULL) {
                            desktop->is_outdated = true;
                        }

                        if (surface != NULL) {
                            surface->is_outdated = true;
                        }
                    } else {
                        drag_start(connection, event->root, client,
                                desktop,
                                CLIENT_OPERATION_MOVING,
                                event->time,
                                event->root_x, event->root_y,
                                (surface != NULL)
                                    ? surface->properties.dim.w : 0u,
                                (surface != NULL)
                                    ? surface->properties.dim.h : 0u,
                                (config != NULL)
                                    ? config->base.windows.snap : 0u);
                    }
                } /* ! if (!hit_btn) */
            }

            /* Clicks that land directly on the frame window (not on the
             * titlebar or the embedded client window) initiate a resize
             * drag.  The resize direction is determined automatically
             * in drag_start from the pointer position relative to the
             * window centre, so any part of the frame border acts as
             * a resize handle.  Only applies to resizable,
             * non-maximized/fullscreen windows. */
            if (window == client->frame && client->frame != 0 &&
                    client_is_resizable(client) &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_FULLSCREEN &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_MAXIMIZED &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_MAXIMIZED_VERT &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ) {
                if (client_is_shaded(client)) {
                    wcmd_client_unshade(client);
                }
                surface = lookup_surface_for_root(surfaces, event->root);
                screen_w = (surface != NULL)
                    ? surface->properties.dim.w : 0u;
                screen_h = (surface != NULL)
                    ? surface->properties.dim.h : 0u;
                drag_start(connection, event->root, client, desktop,
                        CLIENT_OPERATION_RESIZING,
                        event->time,
                        event->root_x, event->root_y,
                        screen_w, screen_h,
                        config->base.windows.snap);
                xcb_allow_events(connection,
                        XCB_ALLOW_ASYNC_POINTER, event->time);
                xcb_flush(connection);
                return;
            }

            /* Replay to the application when the click landed on the
             * client content window or any of its descendants.  Only
             * consume clicks that hit the WM-owned frame border or
             * titlebar directly.  This ensures systray icon sub-windows
             * embedded in dock clients receive their button events. */
            if (window != client->frame && window != client->titlebar) {
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
    client = s_mouse_find_event_client(connection, surfaces,
            event->event, event->child, &desktop);

    if (client == NULL) {
        xcb_allow_events(connection,
                XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(connection);
        return;
    }

    if (type == MOUSEBIND_RESIZE && (!client_is_resizable(client) ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_FULLSCREEN ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_VERT ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ)) {
        xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        xcb_flush(connection);
        return;
    }

    if (type == MOUSEBIND_RESIZE && client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    if (type == MOUSEBIND_LOWER) {
        (void) client_send_event_lower(client);
        xcb_allow_events(connection,
                XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(connection);
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface != NULL && desktop != NULL) {
        focus_apply(surfaces, surface, desktop, client, true, config);
        s_mouse_sync_sticky_active(surface, desktop, client);
    }

    screen_w = 0;
    screen_h = 0;
    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface != NULL) {
        screen_w = surface->properties.dim.w;
        screen_h = surface->properties.dim.h;
    }

    drag_start(connection, event->root, client, desktop,
            (type == MOUSEBIND_MOVE)
                ? CLIENT_OPERATION_MOVING
                : CLIENT_OPERATION_RESIZING,
            event->time,
            event->root_x, event->root_y,
            screen_w, screen_h,
            config->base.windows.snap);
}


/* Handle a button-release event to end a drag */
void mouse_handle_release(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_release_event_t *event,
        const config_td *config)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    int16_t root_x = 0;
    int16_t root_y = 0;

    (void) config;

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
        const config_td *config)
{
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;

    if (connection == NULL || event == NULL || config == NULL) {
        return;
    }

    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        return;
    }

    if (event->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
        return;
    }

    if (!focus_is_follow_mouse(config)) {
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

    s_enter_focus_active = true;
    focus_apply(surfaces, surface, desktop, client, false, config);

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
