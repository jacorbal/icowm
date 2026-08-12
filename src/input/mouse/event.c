/**
 * @file input/mouse/event.c
 *
 * @brief Mouse button-press, button-release, and enter-notify handlers
 *
 * Each non-trivial responsibility inside @c mouse_handle_press has been
 * extracted into its own static function so the public entry point
 * reads as a straightforward sequence of checks rather than a monolith.
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
#include <adt/cdlist.h>
#include <adt/list.h>

/* Utils includes */
#include <utils/cursor.h>

/* Render includes */
#include <render/surface.h>

/* Policy includes */
#include <policy/focus.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/info.h>
#include <menu/dialog/message.h>
#include <menu/popup.h>
#include <menu/search.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/cursor.h>
#include <defs/cycle.h>
#include <defs/input.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* CMD includes */
#include <cmds/client/basic.h>

/* Local includes */
#include <input/mouse/drag.h>
#include <input/mouse/bounds.h>
#include <input/mouse/internal.h>
#include <input/mouse.h>


/* Module state */

/**
 * @brief Flag set while a hover-triggered focus transfer is in flight
 *
 * Set in @a mouse_handle_enter before calling @a focus_apply and
 * cleared in @a handler_focus_in when the corresponding @c FocusIn
 * event arrives.
 */
static bool s_enter_focus_active = false;

/* State for double-click detection on titlebars.  A double-click on the
 * titlebar drag area (i.e., not on a button) toggles shade/unshade. */
static xcb_timestamp_t s_last_titlebar_press_time = 0;
static xcb_window_t s_last_titlebar_press_win = XCB_NONE;


/* Small utilities */

/**
 * @brief Allow pointer events and flush the XCB connection
 *
 * This idiom appears at almost every return point inside
 * @c mouse_handle_press.  Centralizing it removes the repetition and
 * makes each call site self-documenting.
 *
 * @param connection Active XCB connection
 * @param mode       @c XCB_ALLOW_ASYNC_POINTER or
 *                   @c XCB_ALLOW_REPLAY_POINTER
 * @param time       Event timestamp
 */
static void s_allow_and_flush(xcb_connection_t *connection,
        uint8_t mode, xcb_timestamp_t time)
{
    xcb_allow_events(connection, mode, time);
    xcb_flush(connection);
}


/**
 * @brief Check whether a pointer position is near the edge of a client
 *
 * Returns @c true when the pointer's root coordinates fall within the
 * adaptive resize-grab margin (see @c im_resize_bounds in
 * input/mouse/bounds.h) of any edge of the client's current
 * bounding box, indicating that a border-drag resize should be
 * initiated.
 *
 * @param client Client whose geometry is used for the test
 * @param root_x Pointer X position in root-window coordinates
 * @param root_y Pointer Y position in root-window coordinates
 *
 * @return @c true if the pointer is on the resize border, @c false
 *         otherwise
 *
 * @note Complexity: @e O(1)
 */
static bool s_mouse_near_edge(const client_td *client,
        int16_t root_x, int16_t root_y)
{
    im_resize_bounds_td b;

    if (client == NULL) {
        return false;
    }

    b = im_resize_bounds(client);

    return (int32_t) root_x < b.left + b.margin_left ||
        (int32_t) root_x >= b.right - b.margin_right ||
        (int32_t) root_y < b.top + b.margin_top ||
        (int32_t) root_y >= b.bottom - b.margin_bottom;
}


/* Overlay dismissal                                                    */

/**
 * @brief Handle a button press on one already-open context menu type
 *        (window menu, root menu, or window list): forward the click
 *        if it landed on that menu, or close it otherwise
 *
 * Shared by @c s_mouse_close_open_overlays' three near-identical
 * context-menu cases below, which only differ in which module's own
 * @c owns_window/handle_click/close functions to call; each of those
 * three menu types exposes the exact same signature for all three, so
 * passing them in directly loses no type safety over writing each
 * case out by hand.
 *
 * @param connection  XCB connection
 * @param surfaces    Surface list (for root lookup)
 * @param event       Incoming button-press event
 * @param config      Active configuration
 * @param owns_window The menu type's own @c X_owns_window
 * @param handle_click The menu type's own @c X_handle_click
 * @param close       The menu type's own @c X_close
 *
 * @note Complexity: @e O(1)
 */
static void s_mouse_handle_open_ctxmenu_click(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config,
        bool (*owns_window)(xcb_window_t),
        bool (*handle_click)(xcb_connection_t *, surface_td *,
                xcb_window_t, int, int, const config_td *),
        void (*close)(void))
{
    surface_td *surface = lookup_surface_for_root(surfaces, event->root);
    bool owns_event = owns_window(event->event);

    if (owns_event || owns_window(event->child)) {
        xcb_window_t mw = (owns_event) ? event->event : event->child;
        (void) handle_click(connection, surface, mw,
                (int) event->root_x, (int) event->root_y, config);
    } else {
        close();
    }
    s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER, event->time);
}


/**
 * @brief Close any open overlay (popup, dialogs, cycle menu, menus)
 *        when a mouse button is pressed elsewhere
 *
 * Checks each overlay in priority order.  For the popup, processing
 * continues so the click can reach its target client.  For all other
 * overlays the event is fully consumed and the caller must return.
 *
 * @param connection Active XCB connection
 * @param surfaces   Surface list (for root lookup)
 * @param event      Incoming button-press event
 * @param config     Active configuration (passed to cycle confirm)
 *
 * @return @c true when an overlay was open and the event was consumed;
 *         the caller must return without further processing;
 *         @c false when no overlay was open (or only the popup was
 *         closed)
 */
static bool s_mouse_close_open_overlays(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    surface_td *surface;

    /* Popup: close unconditionally on any click, then allow processing */
    if (popup_is_open()) {
        surface = lookup_surface_for_root(surfaces, event->root);
        popup_close(connection);
        if (surface != NULL) {
            surface_render_current_desktop_repaint(surface);
        }
        xcb_flush(connection);
        /* Do NOT consume: allow the click to proceed to the client */
        return false;
    }

    /* Generic confirm dialog (quit-confirmation or any other dialog
     * built on 'menu/dialog/confirm.h'; only one instance can ever be
     * open at a time, so which wrapper opened it does not matter here) */
    if (menu_confirm_dialog_is_open()) {
        if (event->event == menu_confirm_dialog_window() ||
                event->child == menu_confirm_dialog_window()) {
            (void) menu_confirm_dialog_handle_click(connection, config,
                    (int) event->event_x, (int) event->event_y);
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    /* Info dialog */
    if (dialog_info_is_open()) {
        if (event->event == dialog_info_window() ||
                event->child == dialog_info_window()) {
            if ((xcb_button_index_t) event->detail ==
                    XCB_BUTTON_INDEX_4) {
                menu_message_dialog_scroll(connection, config, -3);
            } else if ((xcb_button_index_t) event->detail ==
                    XCB_BUTTON_INDEX_5) {
                menu_message_dialog_scroll(connection, config, 3);
            } else {
                dialog_info_handle_click(connection, config,
                        (int) event->event_x, (int) event->event_y);
            }
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    /* Cycle menu */
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
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    /* Fuzzy window-search widget: a click on a result row selects
     * and confirms it (search_handle_click resolves the row from its
     * own Y internally); a click anywhere else closes it */
    if (search_is_open()) {
        if (event->event == search_window() ||
                event->child == search_window()) {
            search_handle_click(connection, surfaces,
                    (int16_t) event->event_x, (int16_t) event->event_y,
                    config);
        } else {
            search_close(connection);
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    /* Context menus: window menu, root menu, window list */
    if (wincmenu_is_open()) {
        s_mouse_handle_open_ctxmenu_click(connection, surfaces, event,
                config, wincmenu_owns_window, wincmenu_handle_click,
                wincmenu_close);
        return true;
    }

    if (rootmenu_is_open()) {
        s_mouse_handle_open_ctxmenu_click(connection, surfaces, event,
                config, rootmenu_owns_window, rootmenu_handle_click,
                rootmenu_close);
        return true;
    }

    if (winlist_is_open()) {
        s_mouse_handle_open_ctxmenu_click(connection, surfaces, event,
                config, winlist_owns_window, winlist_handle_click,
                winlist_close);
        return true;
    }

    return false;
}


/**
 * @brief Keep a sticky client's active-window state consistent across
 *        every desktop on its surface
 *
 * @c focus_apply only updates @c client_active_id on the one @p desktop
 * passed to it.  For an ordinary client that is enough, but a sticky
 * one (visible on every desktop; see @c client_is_sticky) is expected
 * to keep showing as the active window no matter which desktop the
 * user switches to next.  Without this, @c surface_clients_sticky_
 * transfer_all's own "was this sticky client active on the desktop
 * being switched away from" check (see surface/actions.c) would only
 * see the single desktop @c focus_apply touched, silently dropping the
 * active-window highlight the next time the user switches through any
 * other desktop first.  A no-op for a non-sticky @p client, or when
 * either @p surface or @p client is @c NULL.
 *
 * @param surface Surface whose desktops are kept in sync
 * @param desktop The one desktop @c focus_apply already updated,
 *                skipped here to avoid redundant work
 * @param client  The client that just received focus
 *
 * @note Complexity: @e O(d), where @e d is the number of desktops on
 *       @p surface
 */
static void s_mouse_sync_sticky_active(surface_td *surface,
        desktop_td *desktop, client_td *client)
{
    cdlist_item_td *dnode;
    cdlist_item_td *dinitial;

    if (surface == NULL || surface->desktops == NULL ||
            client == NULL || !client_is_sticky(client)) {
        return;
    }

    dnode = cdlist_head(surface->desktops);
    if (dnode == NULL) {
        return;
    }

    dinitial = dnode;
    do {
        desktop_td *d = (desktop_td *) cdlist_data(dnode);

        if (d != NULL && d != desktop && d->client_active_id != client->id) {
            d->client_active_id = client->id;
            d->focus_dirty = true;
        }
        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


/* Icon click handling */

/**
 * @brief Handle a button press on an iconified client's icon window
 *
 * Left-click starts a drag for the icon; any other button restores the
 * client and focuses it.
 *
 * @param connection Active XCB connection
 * @param surfaces   Surface list (for focus)
 * @param event      Incoming button-press event
 * @param client     The client whose icon was clicked
 * @param desktop    The desktop that owns @p client
 * @param config     Active configuration
 */
static void s_mouse_handle_icon(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop,
        const config_td *config)
{
    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_1) {
        xcb_get_geometry_cookie_t gc;
        xcb_get_geometry_reply_t *gr;
        int32_t icon_x;
        int32_t icon_y;
        surface_td *surface;

        gc = xcb_get_geometry(connection, client->icon_window);
        gr = xcb_get_geometry_reply(connection, gc, NULL);
        icon_x = (gr != NULL)
            ? (int32_t) gr->x : (int32_t) client->icon_x;
        icon_y = (gr != NULL)
            ? (int32_t) gr->y : (int32_t) client->icon_y;
        if (gr != NULL) {
            free(gr);
        }

        surface = wm_get_surface_by_id(client->screen_id);
        drag_start_icon(connection, event->root, client, desktop,
                icon_x, icon_y,
                event->time, event->root_x, event->root_y,
                (surface != NULL) ? surface->properties.dim.w : 0u,
                (surface != NULL) ? surface->properties.dim.h : 0u);
    } else {
        /* Non-left-click: restore and focus */
        surface_td *surface;

        enact_client_restore(client);
        surface = lookup_surface_for_root(surfaces, event->root);
        if (surface != NULL && desktop != NULL) {
            focus_apply(surfaces, surface, desktop, client, true, config);
            s_mouse_sync_sticky_active(surface, desktop, client);
        }
    }

    s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER, event->time);
}


/* Scroll-binding handling (shade/unshade on titlebar, desktop switch) */

/**
 * @brief Handle a scroll-wheel event matched to a @c DESKTOP_PREV /
 *        @c DESKTOP_NEXT binding
 *
 * When the scroll is over a client's titlebar, @c DESKTOP_PREV shades
 * the window (and moves focus to the next client), while
 * @c DESKTOP_NEXT unshades it.  When the scroll is over the root or
 * over a client's content area, the desktop switch happens right
 * away, synchronously.
 *
 * @param connection Active XCB connection
 * @param surfaces   Full surface list
 * @param event      Incoming button-press event
 * @param client     Client under the pointer, or @c NULL
 * @param desktop    Desktop owning @p client, or @c NULL
 * @param type       @c MOUSEBIND_DESKTOP_PREV or @c MOUSEBIND_DESKTOP_NEXT
 * @param config     Active configuration
 */
static void s_mouse_handle_scroll_binding(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop,
        enum wm_mousebind_type_e type, const config_td *config)
{
    surface_td *surface = lookup_surface_for_root(surfaces, event->root);

    if (client != NULL) {
        bool on_titlebar = false;

        /* Detect whether the scroll landed on the titlebar via both
         * the child-window identity and a Y-range check (to handle
         * frame sync-grab events where 'event->child' may be the
         * content window). */
        if (client->titlebar != 0) {
            int32_t bw = client->layout.frame_extents.left;
            int32_t fy = client->layout.geometry.cur.pos.y;
            int32_t ty0 = fy + bw;
            int32_t ty1 = fy + client->layout.frame_extents.top;
            int32_t ry = (int32_t) event->root_y;

            if (event->child == client->titlebar ||
                    (ry >= ty0 && ry < ty1)) {
                on_titlebar = true;
            }
        }

        if (on_titlebar) {
            if (type == MOUSEBIND_DESKTOP_PREV) {
                /* Scroll-up on titlebar: shade and transfer focus */
                if (!client_is_shaded(client)) {
                    cdlist_item_td *node = NULL;
                    client_td *prev_c = NULL;

                    ccmd_client_shade(client);

                    if (desktop != NULL && surface != NULL) {
                        if (desktop->stacking != NULL) {
                            cdlist_item_td *tail =
                                cdlist_tail(desktop->stacking);
                            if (tail != NULL) {
                                node = cdlist_prev(tail);
                            }
                        }

                        while (node != NULL &&
                                node != cdlist_tail(desktop->stacking)) {
                            client_td *c =
                                (client_td *) cdlist_data(node);
                            if (c != NULL && c != client &&
                                    client_is_focusable(c) &&
                                    !client_is_iconified(c)) {
                                prev_c = c;
                                break;
                            }
                            node = cdlist_prev(node);
                        }

                        if (prev_c != NULL) {
                            focus_apply(surfaces, surface, desktop,
                                    prev_c, false, config);
                            s_mouse_sync_sticky_active(surface,
                                    desktop, prev_c);
                        } else {
                            enact_client_unfocus(client);
                            desktop->client_active_id = 0;
                            desktop->focus_dirty = true;
                        }

                        desktop->is_outdated = true;
                        surface->is_outdated = true;
                    }
                }
            } else { /* MOUSEBIND_DESKTOP_NEXT */
                /* Scroll-down on titlebar: unshade and focus */
                if (client_is_shaded(client)) {
                    ccmd_client_unshade(client);

                    if (surface != NULL && desktop != NULL) {
                        focus_apply(surfaces, surface, desktop,
                                client, false, config);
                        s_mouse_sync_sticky_active(surface,
                                desktop, client);
                        desktop->is_outdated = true;
                        surface->is_outdated = true;
                    }
                }
            }

            s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                    event->time);
            return;
        }

        /* Scroll over client content area: replay so the application
         * receives the scroll event */
        s_allow_and_flush(connection, XCB_ALLOW_REPLAY_POINTER,
                event->time);
        return;
    }

    /* No client under pointer: switch desktop right away */
    if (surface != NULL) {
        if (type == MOUSEBIND_DESKTOP_NEXT) {
            enact_surface_desktop_switch_next(surface);
        } else {
            enact_surface_desktop_switch_prev(surface);
        }
    }

    s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER, event->time);
}


/* Titlebar button hit-test */

/**
 * @brief Mark a desktop and its surface outdated, either of which may
 *        be @c NULL
 *
 * Shared by every titlebar-click and scroll case in @c s_mouse_hit_
 * titlebar_buttons that changes the client's state and needs the next
 * render pass to pick it up.
 *
 * @param desktop Desktop to mark outdated, or @c NULL to skip
 * @param surface Surface to mark outdated, or @c NULL to skip
 *
 * @note Complexity: @e O(1)
 */
static void s_mark_outdated(desktop_td *desktop, surface_td *surface)
{
    if (desktop != NULL) { desktop->is_outdated = true; }
    if (surface != NULL) { surface->is_outdated = true; }
}


/**
 * @brief Look up which titlebar button, if any, a client's own button
 *        list has at a given frame-relative X position
 */
static bool s_titlebar_button_at(
        const struct titlebar_button_layout_s *entries, uint8_t count,
        int16_t x, enum config_titlebar_button_e *out)
{
    for (uint8_t i = 0u; i < count; ++i) {
        if (x >= entries[i].x &&
                x < entries[i].x + (int16_t) WM_DECOR_BTN_SIZE) {
            *out = entries[i].button;
            return true;
        }
    }

    return false;
}


/**
 * @brief Dispatch the action a titlebar button click should trigger
 *
 * @param button       Which button was clicked
 * @param client       Client whose titlebar was clicked
 * @param can_maximize Whether maximize/fullscreen are currently enabled
 * @param event        Incoming button-press event (button 1/2/3 select
 *                      full/vertical/horizontal maximize respectively)
 */
static void s_titlebar_button_action(enum config_titlebar_button_e button,
        client_td *client, bool can_maximize,
        const xcb_button_press_event_t *event)
{
    switch (button) {
        case CONFIG_TITLEBAR_BUTTON_PIN:
            enact_client_toggle_sticky(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_LAYER:
            enact_client_cycle_layer(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_ICONIZE:
            enact_client_iconify(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_HIDE:
            enact_client_hide(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_SHADE:
            enact_client_toggle_shade(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_MAXIMIZE:
            if (!can_maximize) {
                break;
            }
            if ((xcb_button_index_t) event->detail ==
                    XCB_BUTTON_INDEX_2) {
                enact_client_maximize_vert(client);
            } else if ((xcb_button_index_t) event->detail ==
                    XCB_BUTTON_INDEX_3) {
                enact_client_maximize_horz(client);
            } else {
                enact_client_maximize(client);
            }
            break;

        case CONFIG_TITLEBAR_BUTTON_FULLSCREEN:
            if (!can_maximize) {
                break;
            }
            enact_client_toggle_fullscreen(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_CLOSE:
            enact_client_close(client);
            break;
    }
}


/**
 * @brief Test whether a click on the titlebar landed on a configured
 *        button and dispatch its action
 *
 * Uses @c client_titlebar_layout to find each button's position, the
 * exact same computation @c desktop_draw_titlebar_buttons uses to
 * paint them, so a click can never land "between" where a button
 * looks like it is and where this function thinks it is.  If the
 * click lands on a button its action is dispatched and the function
 * returns @c true.  Scroll-wheel events (buttons 4 and 5) on the
 * titlebar area also count as a hit and are handled here.
 *
 * @param connection Active XCB connection (unused directly but kept for
 *                   symmetry)
 * @param client     The client whose titlebar was clicked
 * @param desktop    The desktop that owns @p client
 * @param surface    Current surface
 * @param event      Incoming button-press event
 *
 * @return @c true when the click was consumed by a button
 */
static bool s_mouse_hit_titlebar_buttons(xcb_connection_t *connection,
        client_td *client, desktop_td *desktop, surface_td *surface,
        xcb_button_press_event_t *event)
{
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;
    int ex = (int) event->event_x;
    int ey = (int) event->event_y;
    int left_extent = (int) client->layout.frame_extents.left;
    int right_extent = (int) client->layout.frame_extents.right;
    int frame_w = (int) client->layout.geometry.cur.dim.w;
    int fw = (frame_w > left_extent + right_extent)
        ? frame_w - left_extent - right_extent : 1;
    int title_h = (int) client->title_height;
    bool can_maximize;
    bool hide_pin;
    enum config_titlebar_button_e button;

    (void) connection;
    (void) title_x;
    (void) title_w;

    if (client->theme == NULL) {
        return false;
    }

    can_maximize = !client_is_fullscreen(client) &&
        (bool) client_is_resizable(client);
    hide_pin = surface != NULL && surface->desktop_count <= 1u;

    /* Same layout the render pass just painted from, computed first
     * (not just when the click Y already looks close) since it is what
     * determines 'btn_y' now that button rows can be vertically inset
     * by 'padding.vertical', not just centered in the full titlebar
     * height. */
    client_titlebar_layout(client->theme, (uint16_t) fw, (uint16_t) title_h,
            hide_pin, left, &left_n, right, &right_n, &title_x, &title_w,
            &btn_y);

    /* Only test buttons when the click Y is within the button row */
    if (ey >= btn_y && ey < btn_y + (int) WM_DECOR_BTN_SIZE) {
        if (s_titlebar_button_at(left, left_n, (int16_t) ex, &button) ||
                s_titlebar_button_at(right, right_n, (int16_t) ex,
                    &button)) {
            s_titlebar_button_action(button, client, can_maximize, event);
            s_mark_outdated(desktop, surface);
            return true;
        }
    }

    /* Scroll wheel on the titlebar body: shade / unshade */
    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_4) {
        if (!client_is_shaded(client)) {
            enact_client_shade(client);
            s_mark_outdated(desktop, surface);
        }
        return true;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_5) {
        if (client_is_shaded(client)) {
            enact_client_unshade(client);
            s_mark_outdated(desktop, surface);
        }
        return true;
    }

    return false;
}


/* Titlebar interaction (buttons + drag + double-click) */

/**
 * @brief Handle a click on the client titlebar
 *
 * Delegates to @c s_mouse_hit_titlebar_buttons first.
 * If no button was hit:
 * - Left-click starts a move drag, or toggles shade on double-click.
 * - Right-click opens the window context menu.
 *
 * @param connection Active XCB connection
 * @param surfaces   Full surface list (for context menu)
 * @param event      Incoming button-press event
 * @param client     Client whose titlebar was clicked
 * @param desktop    Desktop owning @p client
 * @param surface    Current surface
 * @param config     Active configuration
 */
static void s_mouse_handle_titlebar(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop, surface_td *surface,
        const config_td *config)
{
    bool hit_btn;

    hit_btn = s_mouse_hit_titlebar_buttons(connection, client, desktop,
            surface, event);

    if (!hit_btn &&
            (xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_1) {
        xcb_timestamp_t dt = event->time - s_last_titlebar_press_time;
        xcb_window_t prev_win = s_last_titlebar_press_win;

        s_last_titlebar_press_time = event->time;
        s_last_titlebar_press_win = client->titlebar;

        if (prev_win == client->titlebar &&
                dt <= (xcb_timestamp_t) WM_DOUBLE_CLICK_MS) {
            /* Double-click: toggle shade */
            s_last_titlebar_press_time = 0;
            s_last_titlebar_press_win = XCB_NONE;
            enact_client_toggle_shade(client);
            s_mark_outdated(desktop, surface);
        } else {
            /* Single left-click: start move drag */
            if (!client_is_maximized(client) &&
                    !client_is_fullscreen(client)) {
                drag_start(connection, event->root, client, desktop,
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
        }
    }

    /* Right-click on titlebar drag area (no button hit): window menu */
    if (!hit_btn &&
            (xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_3) {
        if (surface != NULL && desktop != NULL) {
            wincmenu_show(connection, surface, desktop, client,
                    (int16_t) event->root_x,
                    (int16_t) event->root_y, config);
        }
    }

    (void) surfaces;
}


/* Border resize */

/**
 * @brief Test whether a button press should initiate a resize drag
 *
 * Returns @c true when the client is resizable, not fullscreen or
 * fully maximized (a fixed-size state with no border left to drag at
 * all: "Maximized windows can't be moved or resized", Karp, O'Reilly,
 * & Mott, 2005, 'Windows XP in a Nutshell', 2nd ed., ch. 2), and the
 * click landed on the frame border or near the edge of an undecorated
 * window.  A client maximized on just one axis (horizontal-only or
 * vertical-only) is allowed here: its still-free axis can be resized
 * normally, while its maximized one gets locked out once the drag
 * actually starts (see @c drag_start_resize_axis_locked, called from
 * @c s_mouse_start_border_resize).
 *
 * @param client Client to test
 * @param window The X window that received the event
 * @param event  Incoming button-press event
 *
 * @return @c true if a resize drag should begin
 */
static bool s_mouse_can_resize_client(const client_td *client,
        xcb_window_t window, const xcb_button_press_event_t *event)
{
    if (!client_is_resizable(client)) {
        return false;
    }

    if (client_is_fullscreen(client) || client_is_maximized(client)) {
        return false;
    }

    /* Decorated: click anywhere on the frame border */
    if (client->frame != 0 && window == client->frame) {
        return true;
    }

    /* Undecorated: click within the resize-grab margin of any edge
     * (see 's_mouse_near_edge' and 'im_resize_bounds') */
    if (client->frame == 0 && window == client->window &&
            s_mouse_near_edge(client, event->root_x, event->root_y)) {
        return true;
    }

    return false;
}


/**
 * @brief Show the window context menu at a right-click's own root
 *        position, then finish handling the button-press event
 *
 * Shared by @c mouse_handle_press's two right-click-opens-the-menu
 * cases (a decorated frame's border, and an undecorated window's own
 * near-edge margin): both resolve the same way once the click itself
 * is confirmed to be the right one, differing only in how that
 * confirmation is reached.
 *
 * @param connection XCB connection
 * @param surface    Surface the client is on, or @c NULL to skip
 *                   showing the menu (the click is still acknowledged
 *                   either way)
 * @param desktop    Desktop the client is on, or @c NULL to skip
 *                   showing the menu
 * @param client     Client the menu belongs to
 * @param event      The right-click event that triggered this
 * @param config     Active configuration
 *
 * @note Complexity: @e O(1)
 */
static void s_mouse_show_wincmenu_at_click(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, client_td *client,
        const xcb_button_press_event_t *event, const config_td *config)
{
    if (surface != NULL && desktop != NULL) {
        wincmenu_show(connection, surface, desktop, client,
                (int16_t) event->root_x,
                (int16_t) event->root_y, config);
    }
    s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER, event->time);
}


/**
 * @brief Start a resize drag when the conditions are satisfied
 *
 * Unshades the client if shaded, resolves screen dimensions, and
 * calls @c drag_start_resize_axis_locked with
 * @c CLIENT_OPERATION_RESIZING, locking out the width axis for a
 * horizontally-maximized client, the height axis for a
 * vertically-maximized one, or neither for any other client (see
 * @c s_mouse_can_resize_client for why a fully maximized or
 * fullscreen client never reaches this function at all).
 *
 * @param connection Active XCB connection
 * @param surfaces   Surface list (to resolve screen size)
 * @param event      Incoming button-press event
 * @param client     Client to resize
 * @param desktop    Desktop owning @p client
 * @param config     Active configuration (for snap distance)
 */
static void s_mouse_start_border_resize(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop,
        const config_td *config)
{
    surface_td *surface;
    uint32_t screen_w;
    uint32_t screen_h;

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    screen_w = (surface != NULL) ? surface->properties.dim.w : 0u;
    screen_h = (surface != NULL) ? surface->properties.dim.h : 0u;

    drag_start_resize_axis_locked(connection, event->root, client,
            desktop, event->time, event->root_x, event->root_y,
            screen_w, screen_h, config->base.windows.snap,
            client_is_maximized_horz(client),
            client_is_maximized_vert(client));

    s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
            event->time);
}


/* Root window click */

/**
 * @brief Handle a button press on the root (desktop) window
 *
 * Right-click opens the root desktop menu; middle-click opens the
 * window list.
 *
 * @param connection Active XCB connection
 * @param surfaces   Surface list
 * @param event      Incoming button-press event
 * @param config     Active configuration
 */
static void s_mouse_handle_root_press(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    surface_td *surface = lookup_surface_for_root(surfaces, event->root);

    if (surface == NULL) {
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_3) {
        rootmenu_show(connection, surface,
                (int16_t) event->root_x, (int16_t) event->root_y,
                config);
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_1) {
        /* Left-click on the empty desktop: unfocus the active client so
         * all windows lose their selection highlight */
        desktop_td *desktop =
            surface_desktop_get(surface, surface->desktop_cur);

        if (desktop != NULL && desktop->client_active_id != 0) {
            client_td *active = lookup_find_client(surfaces,
                    desktop->client_active_id, NULL, NULL);
            if (active != NULL) {
                enact_client_unfocus(active);
            }
            desktop->client_active_id = 0;
            desktop->focus_dirty = true;
            desktop->is_outdated = true;
            surface->is_outdated = true;
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_2) {
        winlist_show(connection, surface,
                (int16_t) event->root_x, (int16_t) event->root_y,
                config);
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
    }
}


/**
 * @brief Resolve the managed client under a button event, preferring
 *        its reparented child window over the frame it was grabbed on
 *
 * A button event's own @c child field names the deepest window under
 * the pointer -- usually the client's own reparented content window --
 * while @c event names whichever window the grab was actually
 * established on, usually the frame.  Tried in that order so a click
 * landing on the client's own content still resolves correctly even in
 * cases (an icon window, which has no frame of its own) where the
 * frame's window ID alone would not have matched anything.
 *
 * @param connection  Unused; kept only so this matches the signature
 *                     shape of the other handlers around it
 * @param surfaces    Singly-linked list of @c surface_td pointers
 * @param event_win   The @c event field from the triggering XCB event
 * @param child_win   The @c child field from the triggering XCB event
 * @param out_desktop If non-null, receives the owning desktop
 *
 * @return Pointer to the matching client, or @c NULL if neither window
 *         belongs to one
 *
 * @note Complexity: @e O(s * d * c); see @c lookup_find_client
 */
static client_td *s_mouse_find_event_client(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t event_win, xcb_window_t child_win,
        desktop_td **out_desktop)
{
    client_td *client = NULL;

    (void) connection;

    if (child_win != XCB_NONE) {
        client = lookup_find_client(surfaces, child_win, NULL, out_desktop);
    }
    if (client == NULL) {
        client = lookup_find_client(surfaces, event_win, NULL, out_desktop);
    }

    return client;
}


/* Public event handlers */

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

    /* Step 1: dismiss any open overlay; return if the event was
     * consumed */
    if (s_mouse_close_open_overlays(connection, surfaces,
                event, config)) {
        return;
    }

    /* Step 2: resolve the window and client under the pointer */
    window = (event->child != XCB_NONE) ? event->child : event->event;
    client = s_mouse_find_event_client(connection, surfaces,
            event->event, event->child, &desktop);

    /* Step 3: icon click */
    if (client != NULL && window == client->icon_window) {
        s_mouse_handle_icon(connection, surfaces, event, client,
                desktop, config);
        return;
    }

    /* Step 4: determine matching mouse binding */
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

        if ((xcb_button_index_t) event->detail == btn &&
                (req == 0 || (state & req) == req)) {
            type = t;
            break;
        }
    }

    /* Step 5: scroll bindings (desktop prev/next / titlebar shade) */
    if (type == MOUSEBIND_DESKTOP_NEXT ||
            type == MOUSEBIND_DESKTOP_PREV) {
        s_mouse_handle_scroll_binding(connection, surfaces, event,
                client, desktop, type, config);
        return;
    }

    /* Step 6: no configured binding: handle frame/titlebar/root clicks */
    if (type == MOUSEBIND_NONE) {
        if (client != NULL) {
            surface = lookup_surface_for_root(surfaces, event->root);

            /* Apply click-to-focus */
            if (surface != NULL && desktop != NULL &&
                    client_is_focusable(client)) {
                focus_apply(surfaces, surface, desktop, client,
                        true, config);
            }

            /* Right-click on frame border (not titlebar, not content):
             * open the window context menu.  Titlebar clicks are
             * deferred so that buttons on the titlebar consume the
             * right-click before the menu is shown.  When the click
             * lands on the content window the frame passive grab fired
             * but the user clicked inside the application, so the menu
             * must NOT appear. */
            if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_3 &&
                    event->event == client->frame &&
                    client->frame != 0 &&
                    event->child != client->window &&
                    event->child != client->titlebar) {
                s_mouse_show_wincmenu_at_click(connection, surface,
                        desktop, client, event, config);
                return;
            }

            /* Right-click near border of an undecorated window: open
             * the window context menu.  The click must be within the
             * resize-grab margin of any edge (see 's_mouse_near_edge')
             * so that the menu does not steal clicks from the
             * application content. */
            if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_3 &&
                    client->frame == 0 &&
                    window == client->window &&
                    s_mouse_near_edge(client, event->root_x,
                        event->root_y)) {
                s_mouse_show_wincmenu_at_click(connection, surface,
                        desktop, client, event, config);
                return;
            }

            /* Titlebar click */
            if (event->child == client->titlebar &&
                    client->titlebar != 0) {
                s_mouse_handle_titlebar(connection, surfaces, event,
                        client, desktop, surface, config);
            }

            /* Border resize (decorated or undecorated) */
            if (s_mouse_can_resize_client(client, window, event)) {
                s_mouse_start_border_resize(connection, surfaces, event,
                        client, desktop, config);
                return;
            }

            /* Replay or async-allow depending on where the click
             * landed.  Systray icon sub-windows embedded in dock
             * clients must receive their button events, so we only
             * consume clicks that hit the WM-owned frame border or
             * titlebar directly. */
            if (window != client->frame && window != client->titlebar) {
                xcb_allow_events(connection, XCB_ALLOW_REPLAY_POINTER,
                        event->time);
            } else {
                xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                        event->time);
            }
        } else {
            /* No managed client: check for a root-window click */
            if (event->event == event->root ||
                    event->child == XCB_NONE) {
                s_mouse_handle_root_press(connection, surfaces, event,
                        config);
                return;
            }

            if (event->event != event->root) {
                xcb_allow_events(connection, XCB_ALLOW_ASYNC_POINTER,
                        event->time);
            }
        }

        xcb_flush(connection);
        return;
    }

    /* Step 7: configured MOVE or RESIZE binding; re-resolve client */
    window = (event->child != XCB_NONE) ? event->child : event->event;
    client = s_mouse_find_event_client(connection, surfaces,
            event->event, event->child, &desktop);

    if (client == NULL) {
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if (type == MOUSEBIND_RESIZE &&
            (!client_is_resizable(client) || client_is_fullscreen(client) ||
             client_is_maximized(client))) {
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if (type == MOUSEBIND_RESIZE && client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    if (type == MOUSEBIND_LOWER) {
        enact_client_lower(client);
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface != NULL && desktop != NULL) {
        focus_apply(surfaces, surface, desktop, client, true, config);
        s_mouse_sync_sticky_active(surface, desktop, client);
    }

    if (type == MOUSEBIND_MOVE &&
            (client_is_maximized(client) ||
             client_is_fullscreen(client))) {
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    screen_w = (surface != NULL)
        ? surface->properties.dim.w : 0u;
    screen_h = (surface != NULL)
        ? surface->properties.dim.h : 0u;

    /* A window maximized on just one axis still allows this
     * binding to resize its free axis, the same as a plain
     * border drag does (see 's_mouse_start_border_resize'); a
     * fully maximized or fullscreen client never reaches here at
     * all, per the check above. */
    if (type == MOUSEBIND_RESIZE) {
        drag_start_resize_axis_locked(connection, event->root,
                client, desktop, event->time,
                event->root_x, event->root_y,
                screen_w, screen_h, config->base.windows.snap,
                client_is_maximized_horz(client),
                client_is_maximized_vert(client));
    } else {
        drag_start(connection, event->root, client, desktop,
                CLIENT_OPERATION_MOVING,
                event->time,
                event->root_x, event->root_y,
                screen_w, screen_h,
                config->base.windows.snap);
    }
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
            " root=%d+%d, mode=%u, detail=%u)",
            event->event, event->child, event->root_x, event->root_y,
            event->mode, event->detail);

    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        return;
    }

    /* Independent of focus-follows-mouse below: a resizable client
     * that selects 'PointerMotion' for its own purposes (common in
     * GTK/Qt applications tracking hover for their own UI) intercepts
     * motion events at the X11 propagation level before they ever
     * reach 'mouse_handle_motion_hover', so the cursor set while
     * hovering this client's own border never gets re-evaluated once
     * the pointer moves on into that client's content area; this
     * 'EnterNotify', unlike motion, still fires reliably since it was
     * selected directly on this client's own window (see client.c),
     * giving the resize-cursor logic a second, independent chance to
     * catch what motion alone might have missed. */
    entered = im_update_resize_cursor(connection, surfaces,
            event->event, event->root_x, event->root_y);

    /* An undecorated client has no separate frame window to fall
     * back on at all: moving from its border to its interior (or
     * back) happens entirely within this one same window, with
     * no crossing whatsoever for any further 'EnterNotify' to
     * catch, and its own 'PointerMotion' may be just as
     * intercepted as any other client's; only a periodic poll
     * (see 'mouse_hover_poll_tick') can still catch that
     * transition, so track it for one here. */
    im_hover_track((entered != NULL && entered->frame == 0)
            ? event->event : XCB_WINDOW_NONE);

    if (event->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
        return;
    }

    if (!focus_is_follow_mouse(config)) {
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
