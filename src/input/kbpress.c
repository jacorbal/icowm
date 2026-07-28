/**
 * @file input/kbpress.c
 *
 * @brief Key-press and key-release event dispatch
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

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Utils includes */
#include <utils/geom.h>

/* Windows & icons policy includes */
#include <policy/focus.h>

/* Defs includes */
#include <defs/wm.h>

/* Menu includes */
#include <menu/cycle.h>
#include <menu/popup.h>

/* Project includes */
#include <lifecycle.h>
#include <lookup.h>

/* Local includes */
#include <input/keyboard.h>
#include <input/kbpress.h>


/* Handle a key-release event to auto-confirm the cycle menu or close
 * the popup */
void keyboard_handle_release(xcb_key_symbols_t *keysyms,
        xcb_key_release_event_t *event, list_td *surfaces,
        const config_td *cfg)
{
    xcb_keysym_t keysym;
    surface_td *s = NULL;

    if (keysyms == NULL || event == NULL) {
        return;
    }

    if (surfaces != NULL) {
        list_item_td *head = list_head(surfaces);
        if (head != NULL) {
            s = (surface_td *) list_data(head);
        }
    }
    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);
    /* Auto-confirm cycle menu when its modifier is released */
    if (cycle_is_open() && cycle_modifier() != 0 &&
            keyboard_is_modifier_for_mask(keysym, cycle_modifier())) {
        if (s != NULL) {
            cycle_confirm(s->connection, surfaces, cfg);
        }
        return;
    }

    /* Auto-close info popup when its modifier is released */
    if (popup_is_open() && popup_modifier() != 0 &&
            keyboard_is_modifier_for_mask(keysym, popup_modifier())) {
        if (s != NULL) {
            popup_close(s->connection);
        }
    }
}


/* Translate a key-press event into an action and dispatch it */
void keyboard_handle_press(xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event, list_td *surfaces,
        const config_td *cfg)
{
    xcb_keysym_t keysym;
    uint16_t state;
    surface_td *surface;

    if (keysyms == NULL || event == NULL || cfg == NULL) {
        LOGGER_ERROR("Received null pointer in key press handler",
                L_NARG);
        return;
    }

    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);
    state = (uint16_t) ((unsigned int) event->state &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    LOGGER_TRACE("Key press event: keysym=0x%x, state=0x%x",
            keysym, state);

    if (cycle_is_open()) {
        /* Up arrow */
        if (keysym == 0xff52u) {
            cycle_navigate_prev();
            /* find connection for redraw */
            if (surfaces != NULL) {
                list_item_td *head = list_head(surfaces);
                if (head != NULL) {
                    surface_td *s = (surface_td *) list_data(head);
                    if (s != NULL) {
                        cycle_draw(s->connection, cfg);
                    }
                }
            }
            return;
        }

        /* Down arrow */
        if (keysym == 0xff54u) {
            cycle_navigate_next();
            if (surfaces != NULL) {
                list_item_td *head = list_head(surfaces);
                if (head != NULL) {
                    surface_td *s = (surface_td *) list_data(head);
                    if (s != NULL) {
                        cycle_draw(s->connection, cfg);
                    }
                }
            }
            return;
        }

        /* Enter/Return */
        if (keysym == 0xff0du || keysym == 0xff8du) {
            if (surfaces != NULL) {
                list_item_td *head = list_head(surfaces);
                if (head != NULL) {
                    surface_td *s = (surface_td *) list_data(head);
                    if (s != NULL) {
                        cycle_confirm(s->connection, surfaces, cfg);
                    }
                }
            }
            return;
        }

        /* Escape */
        if (keysym == 0xff1bu) {
            if (surfaces != NULL) {
                list_item_td *head = list_head(surfaces);
                if (head != NULL) {
                    surface_td *s = (surface_td *) list_data(head);
                    if (s != NULL) {
                        cycle_close(s->connection);
                    }
                }
            }
            return;
        }

        /* Configured cycle-next binding */
        if (cycle_next_keysym() != XCB_NO_SYMBOL &&
                keysym == cycle_next_keysym() &&
                state == cycle_next_modmask()) {
            cycle_navigate_next();
            if (surfaces != NULL) {
                list_item_td *head = list_head(surfaces);
                if (head != NULL) {
                    surface_td *s = (surface_td *) list_data(head);
                    if (s != NULL) {
                        cycle_draw(s->connection, cfg);
                    }
                }
            }
            return;
        }

        /* Configured cycle-prev binding */
        if (cycle_prev_keysym() != XCB_NO_SYMBOL &&
                keysym == cycle_prev_keysym() &&
                state == cycle_prev_modmask()) {
            cycle_navigate_prev();
            if (surfaces != NULL) {
                list_item_td *head = list_head(surfaces);
                if (head != NULL) {
                    surface_td *s = (surface_td *) list_data(head);
                    if (s != NULL) {
                        cycle_draw(s->connection, cfg);
                    }
                }
            }
            return;
        }

        /* Any other key while menu is open: close without action */
        if (surfaces != NULL) {
            list_item_td *head = list_head(surfaces);
            if (head != NULL) {
                surface_td *s = (surface_td *) list_data(head);
                if (s != NULL) {
                    cycle_close(s->connection);
                }
            }
        }
        return;
    }

    /* Emergency exit 'Ctrl+Mod1+BackSpace' */
    if (keysym == 0xff08u &&
            (event->state & XCB_MOD_MASK_CONTROL) &&
            (event->state & XCB_MOD_MASK_1)) {
        LOGGER_NOTICE("Emergency exit key combination detected", L_NARG);
        (void) wm_request_stop();
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface == NULL && surfaces != NULL && !list_is_empty(surfaces)) {
        surface = (surface_td *) list_data(list_head(surfaces));
    }

    for (int i = 0; i < keyboard_binding_count(); ++i) {
        xcb_keysym_t bks;
        uint16_t bmm;
        enum wm_keybind_type_e btype;
        uint16_t bind_state;

        btype = keyboard_binding_at(i, &bks, &bmm);
        bind_state = (uint16_t) ((unsigned int) bmm &
                ~((unsigned int) XCB_MOD_MASK_LOCK |
                    (unsigned int) XCB_MOD_MASK_2));

        if (keysym != bks || state != bind_state) {
            continue;
        }

        switch (btype) {
            case KEYBIND_DESKTOP_NEXT:
                if (surface != NULL) {
                    event_td *ev;
                    action_td action;
                    action.type = ACTION_TYPE_SURFACE;
                    action.object.surface =
                        ACTION_SURFACE_DESKTOP_SWITCH_NEXT;
                    ev = event_init((void *) surface, NULL, action,
                            PRIORITY_NORMAL);
                    if (ev != NULL) { eventq_add(ev); }
                }
                return;

            case KEYBIND_DESKTOP_PREV:
                if (surface != NULL) {
                    event_td *ev;
                    action_td action;
                    action.type = ACTION_TYPE_SURFACE;
                    action.object.surface =
                        ACTION_SURFACE_DESKTOP_SWITCH_PREV;
                    ev = event_init((void *) surface, NULL, action,
                            PRIORITY_NORMAL);
                    if (ev != NULL) { eventq_add(ev); }
                }
                return;

            case KEYBIND_CLIENT_CYCLE_NEXT:
            case KEYBIND_CLIENT_CYCLE_PREV:
                if (surface != NULL) {
                    desktop_td *desktop = lookup_current_desktop(surface);
                    xcb_connection_t *conn = surface->connection;
                    if (desktop != NULL && conn != NULL) {
                        int dir = (btype == KEYBIND_CLIENT_CYCLE_NEXT)
                            ? 1 : -1;
                        cycle_open(surfaces, conn, surface, desktop,
                                false, dir, bmm, cfg);
                        cycle_draw(conn, cfg);
                    }
                }
                return;

            case KEYBIND_DESKTOP_ICON_NEXT:
            case KEYBIND_DESKTOP_ICON_PREV:
                if (surface != NULL) {
                    desktop_td *desktop = lookup_current_desktop(surface);
                    xcb_connection_t *conn = surface->connection;
                    if (desktop != NULL && conn != NULL) {
                        int dir = (btype == KEYBIND_DESKTOP_ICON_NEXT)
                            ? 1 : -1;
                        cycle_open(surfaces, conn, surface, desktop,
                                true, dir, bmm, cfg);
                        cycle_draw(conn, cfg);
                    }
                }
                return;

            case KEYBIND_WM_REDRAW:
                wm_request_full_redraw();
                return;

            case KEYBIND_CLIENT_ICONIFY:
            case KEYBIND_CLIENT_HIDE:
            case KEYBIND_CLIENT_CLOSE:
            case KEYBIND_CLIENT_KILL:
            case KEYBIND_CLIENT_MAXIMIZE:
            case KEYBIND_CLIENT_CENTER:
            case KEYBIND_CLIENT_SHADE:
            case KEYBIND_CLIENT_FULLSCREEN:
            case KEYBIND_CLIENT_PIN:
            case KEYBIND_CLIENT_INFO:
            case KEYBIND_CLIENT_TOGGLE_DECORATION:
                if (surface != NULL) {
                    desktop_td *desktop = lookup_current_desktop(surface);
                    if (desktop != NULL && desktop->client_active_id != 0) {
                        surface_td *cs = NULL;
                        desktop_td *cd = NULL;
                        client_td *client = lookup_find_client(surfaces,
                                desktop->client_active_id, &cs, &cd);
                        if (client != NULL) {
                            enum action_client_e act = ACTION_CLIENT_ICONIFY;
                            if (btype == KEYBIND_CLIENT_INFO) {
                                popup_show(surface->connection,
                                        surface, desktop, client,
                                        bmm, cfg);
                                return;
                            }
                            if (btype == KEYBIND_CLIENT_HIDE)
                                act = ACTION_CLIENT_HIDE;
                            else if (btype == KEYBIND_CLIENT_CLOSE)
                                act = ACTION_CLIENT_CLOSE;
                            else if (btype == KEYBIND_CLIENT_KILL)
                                act = ACTION_CLIENT_KILL;
                            else if (btype == KEYBIND_CLIENT_MAXIMIZE)
                                act = ACTION_CLIENT_MAXIMIZE;
                            else if (btype == KEYBIND_CLIENT_CENTER)
                                act = ACTION_CLIENT_CENTER;
                            else if (btype == KEYBIND_CLIENT_SHADE)
                                act = ACTION_CLIENT_TOGGLE_SHADE;
                            else if (btype == KEYBIND_CLIENT_FULLSCREEN)
                                act = ACTION_CLIENT_TOGGLE_FULLSCREEN;
                            else if (btype == KEYBIND_CLIENT_PIN)
                                act = ACTION_CLIENT_TOGGLE_STICKY;
                            else if (btype == KEYBIND_CLIENT_TOGGLE_DECORATION)
                                act = ACTION_CLIENT_TOGGLE_DECORATION;
                            client_send_event(client, act, PRIORITY_NORMAL);
                        }
                    }
                }
                return;

            case KEYBIND_LAUNCH_TERMINAL:
                lifecycle_dispatch_launch(surface,
                        cfg->base.programs.terminal);
                return;

            case KEYBIND_LAUNCH_LAUNCHER:
                lifecycle_dispatch_launch(surface,
                        cfg->base.programs.launcher);
                return;

            case KEYBIND_LAUNCH_FILE_MANAGER:
                lifecycle_dispatch_launch(surface,
                        cfg->base.programs.file_manager);
                return;

            case KEYBIND_LAUNCH_WEB_BROWSER:
                lifecycle_dispatch_launch(surface,
                        cfg->base.programs.web_browser);
                return;

            case KEYBIND_LAUNCH_EDITOR:
                lifecycle_dispatch_launch(surface,
                        cfg->base.programs.editor);
                return;

            case KEYBIND_CLIENT_MOVE_LEFT:
            case KEYBIND_CLIENT_MOVE_RIGHT:
            case KEYBIND_CLIENT_MOVE_UP:
            case KEYBIND_CLIENT_MOVE_DOWN:
            case KEYBIND_CLIENT_MOVE_TOP_LEFT:
            case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
            case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
            case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
                if (surface != NULL) {
                    desktop_td *desktop = lookup_current_desktop(surface);
                    if (desktop != NULL && desktop->client_active_id != 0) {
                        surface_td *cs = NULL;
                        desktop_td *cd = NULL;
                        client_td *client = lookup_find_client(surfaces,
                                desktop->client_active_id, &cs, &cd);
                        if (client != NULL) {
                            int32_t new_x = client->layout.geometry.cur.pos.x;
                            int32_t new_y = client->layout.geometry.cur.pos.y;
                            int32_t max_x = (cs != NULL)
                                ? (int32_t) cs->properties.dim.w -
                                    (int32_t) client->layout.geometry.cur.dim.w
                                : new_x;
                            int32_t max_y = (cs != NULL)
                                ? (int32_t) cs->properties.dim.h -
                                    (int32_t) client->layout.geometry.cur.dim.h
                                : new_y;

                            if (btype == KEYBIND_CLIENT_MOVE_LEFT)
                                new_x -= WM_KEYBOARD_MOVE_STEP;
                            else if (btype == KEYBIND_CLIENT_MOVE_RIGHT)
                                new_x += WM_KEYBOARD_MOVE_STEP;
                            else if (btype == KEYBIND_CLIENT_MOVE_UP)
                                new_y -= WM_KEYBOARD_MOVE_STEP;
                            else if (btype == KEYBIND_CLIENT_MOVE_DOWN)
                                new_y += WM_KEYBOARD_MOVE_STEP;
                            else if (btype == KEYBIND_CLIENT_MOVE_TOP_LEFT)
                                { new_x = 0; new_y = 0; }
                            else if (btype == KEYBIND_CLIENT_MOVE_TOP_RIGHT)
                                { new_x = max_x; new_y = 0; }
                            else if (btype == KEYBIND_CLIENT_MOVE_BOTTOM_LEFT)
                                { new_x = 0; new_y = max_y; }
                            else if (btype == KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT)
                                { new_x = max_x; new_y = max_y; }

                            (void) client_send_event_move(client,
                                    new_x, new_y);
                        }
                    }
                }
                return;

            case KEYBIND_CLIENT_RESIZE_LEFT:
            case KEYBIND_CLIENT_RESIZE_RIGHT:
            case KEYBIND_CLIENT_RESIZE_UP:
            case KEYBIND_CLIENT_RESIZE_DOWN:
                if (surface != NULL) {
                    desktop_td *desktop = lookup_current_desktop(surface);
                    if (desktop != NULL && desktop->client_active_id != 0) {
                        surface_td *cs = NULL;
                        desktop_td *cd = NULL;
                        client_td *client = lookup_find_client(surfaces,
                                desktop->client_active_id, &cs, &cd);
                        if (client != NULL && client_is_resizable(client)) {
                            int32_t new_w = (int32_t)
                                client->layout.geometry.cur.dim.w;
                            int32_t new_h = (int32_t)
                                client->layout.geometry.cur.dim.h;

                            if (btype == KEYBIND_CLIENT_RESIZE_LEFT)
                                new_w -= WM_KEYBOARD_RESIZE_STEP;
                            else if (btype == KEYBIND_CLIENT_RESIZE_RIGHT)
                                new_w += WM_KEYBOARD_RESIZE_STEP;
                            else if (btype == KEYBIND_CLIENT_RESIZE_UP)
                                new_h -= WM_KEYBOARD_RESIZE_STEP;
                            else if (btype == KEYBIND_CLIENT_RESIZE_DOWN)
                                new_h += WM_KEYBOARD_RESIZE_STEP;

                            (void) client_send_event_resize(client,
                                    geom_clamp_dim(new_w),
                                    geom_clamp_dim(new_h));
                        }
                    }
                }
                return;

            case KEYBIND_NONE:
                LOGGER_TRACE("Ignoring 'KEYBIND_NONE' entry", L_NARG);
                return;
        }
    }
}
