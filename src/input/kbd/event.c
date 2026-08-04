/**
 * @file input/kbd/event.c
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
#include <signal.h>     /* SIGTERM */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Utils includes */
#include <utils/geom.h>

/* Windows & icons policy includes */
#include <policy/focus.h>

/* Menu includes */
#include <menu/dialog/quit.h>
#include <menu/cycle.h>
#include <menu/popup.h>

/* Command includes */
#include <cmds/ccmd.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <lifecycle.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <input/kbd/event.h>


/**
 * @brief Look up a surface associated to a root window, with fallback
 *
 * Attempts to find a @c surface_td that corresponds to the given X11
 * @c root window by searching the @c surfaces list.  If no matching
 * surface is found, but the list is non-empty, this function falls back
 * to returning the first surface in the list.
 *
 * @param surfaces List of available surfaces to search in, or @c NULL
 * @param root     X11 root window identifier used as lookup key
 *
 * @return Pointer to the matching @c surface_td, or the first surface
 *         in the list if no match is found; returns @c NULL if
 *         @p surfaces is null or empty.
 *
 * @note Intended for use when a specific root surface may not exist
 *       yet, providing a reasonable default for callers.
 */
static surface_td *s_lookup_surface_fallback(list_td *surfaces,
        xcb_window_t root)
{
    surface_td *surface;

    surface = lookup_surface_for_root(surfaces, root);
    if (surface == NULL && surfaces != NULL &&
            !list_is_empty(surfaces)) {
        surface = (surface_td *) list_data(list_head(surfaces));
    }

    return surface;
}


/**
 * @brief Compute a keyboard resize target for one axis
 *
 * Calculates the next frame size for either the horizontal or vertical
 * axis when resizing a @c client_td via keyboard, taking into account
 * frame extents and WM size hints such as base size, minimum size and
 * resize increment.  When valid size hints are present, the inner size
 * is snapped to the nearest increment starting from the base (or
 * minimum) size; otherwise a fixed keyboard resize step is applied.
 *
 * @param client     Pointer to the client whose geometry is being
 *                   resized; may be null, in which case @p cur_frame is
 *                   returned
 * @param horizontal @c true to operate on the horizontal axis (width),
 *                   @c false for the vertical axis (height)
 * @param cur_frame  Current outer frame size (including extents) for
 *                   the selected axis
 * @param grow       @c true to grow (increase) the size, @c false to
 *                   shrink (decrease) it
 *
 * @return The target outer frame size for the selected axis after
 *         applying keyboard resize semantics and clamping via
 *         @c geom_clamp_dim.
 *
 * @note With this, it's honored @c WM_NORMAL_HINTS increments when
 *       available, ensuring that keyboard resizing respects the
 *       client's preferred resize granularity.
 */
static uint32_t s_kb_resize_axis_target(const client_td *client,
        bool horizontal, uint32_t cur_frame, bool grow)
{
    uint32_t ext_a;
    uint32_t ext_b;
    uint32_t cur_inner;
    int32_t base_i;
    int32_t min_i;
    int32_t inc_i;
    int32_t target;

    if (client == NULL) {
        return cur_frame;
    }

    if (horizontal) {
        ext_a = (uint32_t) client->layout.frame_extents.left;
        ext_b = (uint32_t) client->layout.frame_extents.right;
    } else {
        ext_a = (uint32_t) client->layout.frame_extents.top;
        ext_b = (uint32_t) client->layout.frame_extents.bottom;
    }

    cur_inner = (cur_frame > ext_a + ext_b)
        ? cur_frame - ext_a - ext_b : 0u;
    if (!client->size_hints.valid) {
        target = grow
            ? (int32_t) cur_frame + WM_KEYBOARD_RESIZE_STEP
            : (int32_t) cur_frame - WM_KEYBOARD_RESIZE_STEP;
        return geom_clamp_dim(target);
    }

    if (horizontal) {
        base_i = client->size_hints.base_w;
        min_i = client->size_hints.min_w;
        inc_i = client->size_hints.inc_w;
    } else {
        base_i = client->size_hints.base_h;
        min_i = client->size_hints.min_h;
        inc_i = client->size_hints.inc_h;
    }

    if (inc_i > 1) {
        uint32_t base = (base_i > 0)
            ? (uint32_t) base_i
            : ((min_i > 0) ? (uint32_t) min_i : 0u);
        uint32_t inc = (uint32_t) inc_i;
        uint32_t over;
        uint32_t snapped;
        uint32_t target_inner;

        if (cur_inner < base) {
            cur_inner = base;
        }

        over = (cur_inner > base) ? (cur_inner - base) : 0u;
        snapped = base + (over / inc) * inc;

        if (grow) {
            target_inner = snapped + inc;
        } else {
            target_inner = (snapped > base) ? (snapped - inc) : base;
        }

        return geom_clamp_dim((int32_t) (target_inner + ext_a + ext_b));
    }

    target = grow
        ? (int32_t) cur_frame + WM_KEYBOARD_RESIZE_STEP
        : (int32_t) cur_frame - WM_KEYBOARD_RESIZE_STEP;
    return geom_clamp_dim(target);
}


/**
 * @brief Directly apply a keyboard resize to a client
 *
 * Applies the resize synchronously without going through the event
 * queue.  This mirrors the interactive (mouse-drag) resize path so that
 * both input methods share identical behaviour: the geometry is
 * constrained per-axis, applied to the correct X window (frame for
 * decorated clients, content window for undecorated clients), and
 * followed by a synthetic 'ConfigureNotify' so the application learns
 * its new geometry immediately.
 *
 * @param client Pointer to the client to resize
 * @param new_x  New frame X position (screen-relative)
 * @param new_y  New frame Y position (screen-relative)
 * @param new_w  New frame width
 * @param new_h  New frame height
 *
 * @note This function flushes the XCB connection before returning.
 */
static void s_kbd_resize_apply(client_td *client,
        int32_t new_x, int32_t new_y, uint32_t new_w, uint32_t new_h)
{
    bool pos_changed;
    uint16_t mask;
    uint32_t values[4];
    xcb_window_t target_win;

    if (client == NULL) {
        return;
    }

    /* A shaded client shows only the titlebar; restore the full window
     * before applying the new dimensions */
    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    /* The caller ('keyboard_handle_press' via
     * 's_kb_resize_axis_target') already produced fully snapped,
     * increment-aligned frame dimensions.  Re-applying
     * 'client_constrain_size' here would snap the values a second time
     * and could produce a size different from what the position
     * correction ('new_y += old_h - new_h') was computed for, causing
     * the top edge of the window to shift by the wrong amount on
     * 'RESIZE_UP'. */
    pos_changed = (new_x != client->layout.geometry.cur.pos.x ||
                   new_y != client->layout.geometry.cur.pos.y);

    /* Apply the new geometry to the correct X window.  Decorated
     * clients are reparented into a frame; undecorated clients are
     * direct children of the root. */
    target_win = (client->frame != 0 && client_is_decorated(client))
        ? client->frame : client->window;

    if (pos_changed) {
        mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
               XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[0] = (uint32_t) new_x;
        values[1] = (uint32_t) new_y;
    } else {
        mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    }
    xcb_configure_window(client->connection, target_win, mask, values);

    /* Update the stored geometry after configuring X so that
     * 'client_sync_decoration_layout' and the synthetic
     * 'ConfigureNotify' both see the final values */
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    client->layout.geometry.cur.dim.w = new_w;
    client->layout.geometry.cur.dim.h = new_h;

    /* Reposition and resize the inner window and titlebar to match the
     * new frame dimensions (no-op for undecorated clients) */
    client_sync_decoration_layout(client);

    /* ICCCM §4.2.3: send a synthetic 'ConfigureNotify' with
     * screen-relative coordinates so the application always knows its
     * true on-screen position and content-area size, regardless of
     * reparenting. */
    client_send_synthetic_configure_notify(client->connection, client);


    /* Force a repaint AFTER the synthetic 'ConfigureNotify' so the
     * application (e.g., gVim) draws at the correct screen-relative
     * geometry.  Placing the 'Expose' here ensures it arrives in the
     * client's event queue after both the xcb_configure_window (from
     * 'client_sync_decoration_layout') and the synthetic
     * 'ConfigureNotify', giving (strange) programs like 'gVim' the
     * correct size and position before its 'Expose' handler runs. */
    xcb_clear_area(client->connection, 1, client->window, 0, 0, 0, 0);

    xcb_flush(client->connection);

    /* Mark the desktop as needing a repaint so frame decorations are
     * refreshed at the correct new dimensions */
    wm_request_client_redraw(client);
}


/* Handle a key-release event to auto-confirm the cycle menu or close
 * the popup */
void keyboard_handle_release(xcb_key_symbols_t *keysyms,
        xcb_key_release_event_t *event, list_td *surfaces,
        const config_td *config)
{
    xcb_keysym_t keysym;

    if (keysyms == NULL || event == NULL) {
        return;
    }

    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);

    /* Auto-confirm cycle menu when its modifier is released */
    if (cycle_is_open() && cycle_modifier() != 0 &&
            keyboard_is_modifier_for_mask(keysym, cycle_modifier())) {
        surface_td *surface = s_lookup_surface_fallback(surfaces,
                event->root);
        if (surface != NULL) {
            cycle_confirm(surface->connection, surfaces, config);
        }
        return;
    }
}


/* Translate a key-press event into an action and dispatch it */
void keyboard_handle_press(xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event, list_td *surfaces,
        const config_td *config)
{
    xcb_keysym_t keysym;
    uint16_t state;
    surface_td *surface;

    if (keysyms == NULL || event == NULL || config == NULL) {
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

    surface = s_lookup_surface_fallback(surfaces, event->root);

    if (cycle_is_open()) {
        /* Up arrow */
        if (keysym == 0xff52u) {
            cycle_navigate_prev();
            if (surface != NULL && surface->connection != NULL) {
                cycle_draw(surface->connection, config);
            }
            return;
        }

        /* Down arrow */
        if (keysym == 0xff54u) {
            cycle_navigate_next();
            if (surface != NULL && surface->connection != NULL) {
                cycle_draw(surface->connection, config);
            }
            return;
        }

        /* Enter/Return */
        if (keysym == 0xff0du || keysym == 0xff8du) {
            if (surface != NULL && surface->connection != NULL) {
                cycle_confirm(surface->connection, surfaces, config);
            }
            return;
        }

        /* Escape */
        if (keysym == 0xff1bu) {
            if (surface != NULL && surface->connection != NULL) {
                cycle_close(surface->connection);
            }
            return;
        }

        /* Configured cycle-next binding */
        if (cycle_next_keysym() != XCB_NO_SYMBOL &&
                keysym == cycle_next_keysym() &&
                state == cycle_next_modmask()) {
            cycle_navigate_next();
            if (surface != NULL && surface->connection != NULL) {
                cycle_draw(surface->connection, config);
            }
            return;
        }

        /* Configured cycle-prev binding */
        if (cycle_prev_keysym() != XCB_NO_SYMBOL &&
                keysym == cycle_prev_keysym() &&
                state == cycle_prev_modmask()) {
            cycle_navigate_prev();
            if (surface != NULL && surface->connection != NULL) {
                cycle_draw(surface->connection, config);
            }
            return;
        }

        /* Any other key while menu is open: close without action */
        if (surface != NULL && surface->connection != NULL) {
            cycle_close(surface->connection);
        }
        return;
    }

    /* Confirmation dialog key handling */
    if (dialog_quit_is_open()) {
        /* Tab, Left, Right arrows: toggle selected button */
        if (keysym == 0xff09u || keysym == 0xff51u ||
                keysym == 0xff53u) {
            dialog_quit_toggle_selection();
            if (surface != NULL && surface->connection != NULL) {
                dialog_quit_repaint(surface->connection, config);
            }
            return;
        }

        /* Enter/Return: activate selected button */
        if (keysym == 0xff0du || keysym == 0xff8du) {
            if (surface != NULL && surface->connection != NULL) {
                dialog_quit_accept(surface->connection);
            }
            return;
        }

        /* Escape: close without action */
        if (keysym == 0xff1bu) {
            if (surface != NULL && surface->connection != NULL) {
                dialog_quit_close(surface->connection);
            }
            return;
        }
        return;
    }

    /* Emergency exit 'Ctrl+Mod1+BackSpace' */
    if (keysym == 0xff08u &&
            (event->state & XCB_MOD_MASK_CONTROL) &&
            (event->state & XCB_MOD_MASK_1)) {
        LOGGER_NOTICE("Emergency exit key combination detected", L_NARG);
        raise(SIGTERM);
        return;
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
                    desktop_td *desktop =
                        lookup_current_desktop(surface);
                    xcb_connection_t *conn = surface->connection;
                    if (desktop != NULL && conn != NULL) {
                        int dir = (btype == KEYBIND_CLIENT_CYCLE_NEXT)
                            ? 1 : -1;
                        cycle_open(surfaces, conn, surface, desktop,
                                false, dir, bmm, config);
                        cycle_draw(conn, config);
                    }
                }
                return;

            case KEYBIND_DESKTOP_ICON_NEXT:
            case KEYBIND_DESKTOP_ICON_PREV:
                if (surface != NULL) {
                    desktop_td *desktop =
                        lookup_current_desktop(surface);
                    xcb_connection_t *conn = surface->connection;
                    if (desktop != NULL && conn != NULL) {
                        int dir = (btype == KEYBIND_DESKTOP_ICON_NEXT)
                            ? 1 : -1;
                        cycle_open(surfaces, conn, surface, desktop,
                                true, dir, bmm, config);
                        cycle_draw(conn, config);
                    }
                }
                return;

            case KEYBIND_WM_REDRAW:
                wm_request_full_redraw();
                return;

            case KEYBIND_WM_QUIT:
                if (surface != NULL && surface->connection != NULL) {
                    dialog_quit_show(surface->connection, surface, config);
                }
                return;
            case KEYBIND_WM_RELOAD:
                (void) wm_action_config_reload();
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
            case KEYBIND_CLIENT_CYCLE_LAYER:
                if (surface != NULL) {
                    desktop_td *desktop = lookup_current_desktop(surface);
                    if (desktop != NULL && desktop->client_active_id != 0) {
                        surface_td *cs = NULL;
                        desktop_td *cd = NULL;
                        client_td *client = lookup_find_client(surfaces,
                                desktop->client_active_id, &cs, &cd);

                        if (client != NULL) {
                            enum action_client_e act = ACTION_CLIENT_ICONIFY;
                            bool needs_resize_capability = false;

                            if (btype == KEYBIND_CLIENT_INFO) {
                                popup_show(surface->connection,
                                        surface, desktop, client,
                                        bmm, event->detail, config);
                                return;
                            }

                            if (btype == KEYBIND_CLIENT_HIDE)
                                act = ACTION_CLIENT_HIDE;
                            else if (btype == KEYBIND_CLIENT_CLOSE)
                                act = ACTION_CLIENT_CLOSE;
                            else if (btype == KEYBIND_CLIENT_KILL)
                                act = ACTION_CLIENT_KILL;
                            else if (btype == KEYBIND_CLIENT_MAXIMIZE) {
                                needs_resize_capability = true;
                                act = ACTION_CLIENT_MAXIMIZE;
                            }
                            else if (btype == KEYBIND_CLIENT_CENTER)
                                act = ACTION_CLIENT_CENTER;
                            else if (btype == KEYBIND_CLIENT_SHADE)
                                act = ACTION_CLIENT_TOGGLE_SHADE;
                            else if (btype == KEYBIND_CLIENT_FULLSCREEN) {
                                needs_resize_capability = true;
                                act = ACTION_CLIENT_TOGGLE_FULLSCREEN;
                                }
                            else if (btype == KEYBIND_CLIENT_PIN)
                                act = ACTION_CLIENT_TOGGLE_STICKY;
                            else if (btype == KEYBIND_CLIENT_TOGGLE_DECORATION)
                                act = ACTION_CLIENT_TOGGLE_DECORATION;
                            else if (btype == KEYBIND_CLIENT_CYCLE_LAYER)
                                act = ACTION_CLIENT_CYCLE_LAYER;

                            if (needs_resize_capability &&
                                    !client_is_resizable(client)) {
                                return;
                            }
                            client_send_event(client, act, PRIORITY_NORMAL);
                        }
                    }
                }
                return;

            case KEYBIND_LAUNCH_TERMINAL:
                lifecycle_dispatch_launch(surface,
                        config->base.programs.terminal);
                return;

            case KEYBIND_LAUNCH_LAUNCHER:
                lifecycle_dispatch_launch(surface,
                        config->base.programs.launcher);
                return;

            case KEYBIND_LAUNCH_FILE_MANAGER:
                lifecycle_dispatch_launch(surface,
                        config->base.programs.file_manager);
                return;

            case KEYBIND_LAUNCH_WEB_BROWSER:
                lifecycle_dispatch_launch(surface,
                        config->base.programs.web_browser);
                return;

            case KEYBIND_LAUNCH_EDITOR:
                lifecycle_dispatch_launch(surface,
                        config->base.programs.editor);
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

                    if (desktop != NULL &&
                            desktop->client_active_id != 0) {
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
                            int32_t new_x =
                                client->layout.geometry.cur.pos.x;
                            int32_t new_y =
                                client->layout.geometry.cur.pos.y;
                            /* Operate in frame space (outer dimensions
                             * including decoration extents).
                             * 's_kbd_resize_apply' converts to inner
                             * space internally when applying size
                             * hints */
                            uint32_t old_w =
                                client->layout.geometry.cur.dim.w;
                            uint32_t old_h = (client_is_shaded(client))
                                ? client->layout.geometry.old.dim.h
                                : client->layout.geometry.cur.dim.h;
                            int32_t new_w = (int32_t)
                                old_w;
                            int32_t new_h = (int32_t) old_h;

                            if (client->properties.state ==
                                        (uint16_t) CLIENT_STATE_FULLSCREEN ||
                                    client->properties.state ==
                                        (uint16_t) CLIENT_STATE_MAXIMIZED ||
                                    client->properties.state ==
                                        (uint16_t)
                                            CLIENT_STATE_MAXIMIZED_HORZ ||
                                    client->properties.state ==
                                        (uint16_t)
                                            CLIENT_STATE_MAXIMIZED_VERT) {
                                return;
                            }

                            if (btype == KEYBIND_CLIENT_RESIZE_LEFT) {
                                new_w = (int32_t) s_kb_resize_axis_target(
                                        client, true, old_w, false);
                                new_x += (int32_t) old_w - new_w;
                            } else if (btype == KEYBIND_CLIENT_RESIZE_RIGHT) {
                                new_w = (int32_t) s_kb_resize_axis_target(
                                        client, true, old_w, true);
                            } else if (btype == KEYBIND_CLIENT_RESIZE_UP) {
                                new_h = (int32_t) s_kb_resize_axis_target(
                                        client, false, old_h, false);
                                new_y += (int32_t) old_h - new_h;
                            } else if (btype == KEYBIND_CLIENT_RESIZE_DOWN) {
                                new_h = (int32_t) s_kb_resize_axis_target(
                                        client, false, old_h, true);
                            }

                            s_kbd_resize_apply(client,
                                    new_x, new_y,
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
