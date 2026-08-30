/**
 * @file input/mouse/event/overlay.c
 *
 * @brief Dismissing an open overlay on a button press
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
#include <stddef.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/info.h>
#include <menu/dialog/message.h>
#include <menu/dialog/run.h>
#include <menu/popup.h>
#include <menu/search.h>

/* Default initial values */
#include <defs/cycle.h>

/* Render includes */
#include <render/surface.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <input/mouse/event.h>
#include <input/mouse/internal.h>

/**
 * @brief Handle a button press on one already-open context menu type
 *        (window menu, root menu, or window list), forward the click if
 *        it landed on that menu, or close it otherwise
 *
 * Shared by @a im_press_close_overlays' three near-identical
 * context-menu cases below, which only differ in which module's
 * @a owns_window/handle_click/close functions to call; each of those
 * three menu types exposes the exact same signature for all three, so
 * passing them in directly loses no type safety over writing each case
 * out by hand.
 *
 * @param connection   XCB connection
 * @param surfaces     Surface list (for root lookup)
 * @param event        Incoming button-press event
 * @param config       Active configuration
 * @param owns_window  The menu type's @c X_owns_window
 * @param handle_click The menu type's @c X_handle_click
 * @param close        The menu type's @c X_close
 *
 * @note Complexity: @e O(1)
 */
static void s_mouse_handle_open_ctxmenu_click(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config,
        bool (*owns_window)(xcb_window_t),
        bool (*handle_click)(xcb_connection_t *, surface_td *,
                xcb_window_t, int, const config_td *),
        void (*close)(void))
{
    surface_td *const surface = lookup_surface_for_root(surfaces,
            event->root);
    bool owns_event = owns_window(event->event);

    if (owns_event || owns_window(event->child)) {
        xcb_window_t mw = (owns_event) ? event->event : event->child;
        (void) handle_click(connection, surface, mw,
                (int) event->root_y, config);
    } else {
        (void) close();
    }
    im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
            event->time);
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
 * @return Status of the operation
 * @retval  true when an overlay was open and the event was consumed;
 *               the caller must return without further processing;
 * @retval false when no overlay was open (or only the popup was closed)
 */
bool im_press_close_overlays(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    /* A popup closes on any click, and the click goes through */
    if (popup_is_open()) {
        surface_td *const surface = lookup_surface_for_root(surfaces,
                event->root);

        popup_close(connection);
        /* Marked rather than painted: the click below may well
         * change something else on this same turn, and one repaint
         * covers both */
        if (surface != NULL) {
            surface->is_outdated = true;
        }
        /* Do NOT consume: allow the click to proceed to the client */
        return false;
    }

    /* Generic confirm dialog (quit-confirmation or any other dialog
     * built on 'menu/dialog/confirm.h'; only one instance can ever be
     * open at a time, so which wrapper opened it does not matter
     * here) */
    if (menu_confirm_dialog_is_open()) {
        if (event->event == menu_confirm_dialog_window() ||
                event->child == menu_confirm_dialog_window()) {
            (void) menu_confirm_dialog_handle_click(connection, config,
                    (int) event->event_x, (int) event->event_y);
        }
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
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
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
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
                cycle_destroy(connection);
            }
        } else {
            cycle_destroy(connection);
        }
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
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
            search_destroy(connection);
        }
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
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
