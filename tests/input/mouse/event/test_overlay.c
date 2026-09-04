/**
 * @file tests/input/mouse/event/test_overlay.c
 *
 * @brief Test battery for dismissing open overlays on a button press
 *        (input/mouse/event/overlay.c)
 *
 * im_press_close_overlays checks, in priority order, whether a popup,
 * confirm dialog, info dialog, cycle menu, search widget, window menu,
 * root menu, or window list is currently open, and either forwards the
 * click into it or closes it.  Every one of those overlay modules is a
 * whole separate subsystem covered by its own tests, so every
 * X_is_open/X_window/X_owns_window/X_handle_click/X_close entry point
 * here is a recording stand-in, following the same pattern
 * tests/menu/context/test_winlist.c uses for its own dependents.
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
#include <string.h>

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
#include <menu/popup.h>
#include <menu/search.h>

/* Default initial values */
#include <defs/cycle.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/event.h>
#include <input/mouse/internal.h>


/** Controllable open/window state for every overlay type this file
 *  stubs, plus recorded call counts for their handle_click/close */
static bool s_popup_open;
static int s_popup_close_calls;

static bool s_confirm_open;
static xcb_window_t s_confirm_window;
static int s_confirm_handle_click_calls;
static int s_confirm_handle_click_x;
static int s_confirm_handle_click_y;

static bool s_info_open;
static xcb_window_t s_info_window;
static int s_info_handle_click_calls;
static int s_message_scroll_calls;
static int32_t s_message_scroll_delta;

static bool s_cycle_open;
static xcb_window_t s_cycle_window;
static int s_cycle_navigate_to_calls;
static unsigned int s_cycle_navigate_to_row;
static int s_cycle_confirm_calls;
static int s_cycle_destroy_calls;

static bool s_search_open;
static xcb_window_t s_search_window;
static int s_search_handle_click_calls;
static int s_search_destroy_calls;

static bool s_wincmenu_open;
static int s_wincmenu_owns_calls;
static xcb_window_t s_wincmenu_owned_window;
static int s_wincmenu_handle_click_calls;
static int s_wincmenu_close_calls;

static bool s_rootmenu_open;
static int s_rootmenu_owns_calls;
static xcb_window_t s_rootmenu_owned_window;
static int s_rootmenu_handle_click_calls;
static int s_rootmenu_close_calls;

static bool s_winlist_open;
static int s_winlist_owns_calls;
static xcb_window_t s_winlist_owned_window;
static int s_winlist_handle_click_calls;
static int s_winlist_close_calls;

/** Controllable return value for the next lookup_surface_for_root */
static surface_td *s_stub_lookup_surface;

/** Recorded calls to im_allow_and_flush */
static int s_allow_and_flush_calls;


/**
 * @brief Stand-in for @a lookup_surface_for_root
 * @note Complexity: @e O(1)
 */
surface_td *lookup_surface_for_root(list_td *surfaces, xcb_window_t root)
{
    (void) surfaces;
    (void) root;

    return s_stub_lookup_surface;
}


/**
 * @brief Recording stand-in for @a im_allow_and_flush
 * @note Complexity: @e O(1)
 */
void im_allow_and_flush(xcb_connection_t *connection, uint8_t mode,
        xcb_timestamp_t time)
{
    (void) connection;
    (void) mode;
    (void) time;

    s_allow_and_flush_calls++;
}


/**
 * @brief Controllable stand-in for @a popup_is_open
 * @note Complexity: @e O(1)
 */
bool popup_is_open(void)
{
    return s_popup_open;
}


/**
 * @brief Recording stand-in for @a popup_close
 * @note Complexity: @e O(1)
 */
void popup_close(xcb_connection_t *connection)
{
    (void) connection;

    s_popup_close_calls++;
}


/**
 * @brief Controllable stand-in for @a menu_confirm_dialog_is_open
 * @note Complexity: @e O(1)
 */
bool menu_confirm_dialog_is_open(void)
{
    return s_confirm_open;
}


/**
 * @brief Controllable stand-in for @a menu_confirm_dialog_window
 * @note Complexity: @e O(1)
 */
xcb_window_t menu_confirm_dialog_window(void)
{
    return s_confirm_window;
}


/**
 * @brief Recording stand-in for @a menu_confirm_dialog_handle_click
 * @note Complexity: @e O(1)
 */
bool menu_confirm_dialog_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y)
{
    (void) connection;
    (void) config;

    s_confirm_handle_click_calls++;
    s_confirm_handle_click_x = x;
    s_confirm_handle_click_y = y;

    return true;
}


/**
 * @brief Controllable stand-in for @a dialog_info_is_open
 * @note Complexity: @e O(1)
 */
bool dialog_info_is_open(void)
{
    return s_info_open;
}


/**
 * @brief Controllable stand-in for @a dialog_info_window
 * @note Complexity: @e O(1)
 */
xcb_window_t dialog_info_window(void)
{
    return s_info_window;
}


/**
 * @brief Recording stand-in for @a dialog_info_handle_click
 * @note Complexity: @e O(1)
 */
void dialog_info_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y)
{
    (void) connection;
    (void) config;
    (void) x;
    (void) y;

    s_info_handle_click_calls++;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_scroll
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_scroll(xcb_connection_t *connection,
        const config_td *config, int32_t delta)
{
    (void) connection;
    (void) config;

    s_message_scroll_calls++;
    s_message_scroll_delta = delta;
}


/**
 * @brief Controllable stand-in for @a cycle_is_open
 * @note Complexity: @e O(1)
 */
bool cycle_is_open(void)
{
    return s_cycle_open;
}


/**
 * @brief Controllable stand-in for @a cycle_window
 * @note Complexity: @e O(1)
 */
xcb_window_t cycle_window(void)
{
    return s_cycle_window;
}


/**
 * @brief Recording stand-in for @a cycle_navigate_to
 * @note Complexity: @e O(1)
 */
void cycle_navigate_to(unsigned int idx)
{
    s_cycle_navigate_to_calls++;
    s_cycle_navigate_to_row = idx;
}


/**
 * @brief Recording stand-in for @a cycle_confirm
 * @note Complexity: @e O(1)
 */
void cycle_confirm(xcb_connection_t *connection, list_td *surfaces,
        const config_td *cfg)
{
    (void) connection;
    (void) surfaces;
    (void) cfg;

    s_cycle_confirm_calls++;
}


/**
 * @brief Recording stand-in for @a cycle_destroy
 * @note Complexity: @e O(1)
 */
void cycle_destroy(xcb_connection_t *connection)
{
    (void) connection;

    s_cycle_destroy_calls++;
}


/**
 * @brief Controllable stand-in for @a search_is_open
 * @note Complexity: @e O(1)
 */
bool search_is_open(void)
{
    return s_search_open;
}


/**
 * @brief Controllable stand-in for @a search_window
 * @note Complexity: @e O(1)
 */
xcb_window_t search_window(void)
{
    return s_search_window;
}


/**
 * @brief Recording stand-in for @a search_handle_click
 * @note Complexity: @e O(1)
 */
void search_handle_click(xcb_connection_t *connection, list_td *surfaces,
        int16_t x, int16_t y, const config_td *cfg)
{
    (void) connection;
    (void) surfaces;
    (void) x;
    (void) y;
    (void) cfg;

    s_search_handle_click_calls++;
}


/**
 * @brief Recording stand-in for @a search_destroy
 * @note Complexity: @e O(1)
 */
void search_destroy(xcb_connection_t *connection)
{
    (void) connection;

    s_search_destroy_calls++;
}


/**
 * @brief Controllable stand-in for @a wincmenu_is_open
 * @note Complexity: @e O(1)
 */
bool wincmenu_is_open(void)
{
    return s_wincmenu_open;
}


/**
 * @brief Recording stand-in for @a wincmenu_owns_window
 * @note Complexity: @e O(1)
 */
bool wincmenu_owns_window(xcb_window_t win)
{
    s_wincmenu_owns_calls++;

    return win == s_wincmenu_owned_window && win != XCB_NONE;
}


/**
 * @brief Recording stand-in for @a wincmenu_handle_click
 * @note Complexity: @e O(1)
 */
bool wincmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int root_y,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) win;
    (void) root_y;
    (void) config;

    s_wincmenu_handle_click_calls++;

    return true;
}


/**
 * @brief Recording stand-in for @a wincmenu_close
 * @note Complexity: @e O(1)
 */
void wincmenu_close(void)
{
    s_wincmenu_close_calls++;
}


/**
 * @brief Controllable stand-in for @a rootmenu_is_open
 * @note Complexity: @e O(1)
 */
bool rootmenu_is_open(void)
{
    return s_rootmenu_open;
}


/**
 * @brief Recording stand-in for @a rootmenu_owns_window
 * @note Complexity: @e O(1)
 */
bool rootmenu_owns_window(xcb_window_t win)
{
    s_rootmenu_owns_calls++;

    return win == s_rootmenu_owned_window && win != XCB_NONE;
}


/**
 * @brief Recording stand-in for @a rootmenu_handle_click
 * @note Complexity: @e O(1)
 */
bool rootmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int root_y,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) win;
    (void) root_y;
    (void) config;

    s_rootmenu_handle_click_calls++;

    return true;
}


/**
 * @brief Recording stand-in for @a rootmenu_close
 * @note Complexity: @e O(1)
 */
void rootmenu_close(void)
{
    s_rootmenu_close_calls++;
}


/**
 * @brief Controllable stand-in for @a winlist_is_open
 * @note Complexity: @e O(1)
 */
bool winlist_is_open(void)
{
    return s_winlist_open;
}


/**
 * @brief Recording stand-in for @a winlist_owns_window
 * @note Complexity: @e O(1)
 */
bool winlist_owns_window(xcb_window_t win)
{
    s_winlist_owns_calls++;

    return win == s_winlist_owned_window && win != XCB_NONE;
}


/**
 * @brief Recording stand-in for @a winlist_handle_click
 * @note Complexity: @e O(1)
 */
bool winlist_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int root_y,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) win;
    (void) root_y;
    (void) config;

    s_winlist_handle_click_calls++;

    return true;
}


/**
 * @brief Recording stand-in for @a winlist_close
 * @note Complexity: @e O(1)
 */
void winlist_close(void)
{
    s_winlist_close_calls++;
}


static void s_reset(void)
{
    s_popup_open = false;
    s_popup_close_calls = 0;

    s_confirm_open = false;
    s_confirm_window = XCB_NONE;
    s_confirm_handle_click_calls = 0;
    s_confirm_handle_click_x = 0;
    s_confirm_handle_click_y = 0;

    s_info_open = false;
    s_info_window = XCB_NONE;
    s_info_handle_click_calls = 0;
    s_message_scroll_calls = 0;
    s_message_scroll_delta = 0;

    s_cycle_open = false;
    s_cycle_window = XCB_NONE;
    s_cycle_navigate_to_calls = 0;
    s_cycle_navigate_to_row = 0u;
    s_cycle_confirm_calls = 0;
    s_cycle_destroy_calls = 0;

    s_search_open = false;
    s_search_window = XCB_NONE;
    s_search_handle_click_calls = 0;
    s_search_destroy_calls = 0;

    s_wincmenu_open = false;
    s_wincmenu_owns_calls = 0;
    s_wincmenu_owned_window = XCB_NONE;
    s_wincmenu_handle_click_calls = 0;
    s_wincmenu_close_calls = 0;

    s_rootmenu_open = false;
    s_rootmenu_owns_calls = 0;
    s_rootmenu_owned_window = XCB_NONE;
    s_rootmenu_handle_click_calls = 0;
    s_rootmenu_close_calls = 0;

    s_winlist_open = false;
    s_winlist_owns_calls = 0;
    s_winlist_owned_window = XCB_NONE;
    s_winlist_handle_click_calls = 0;
    s_winlist_close_calls = 0;

    s_stub_lookup_surface = NULL;
    s_allow_and_flush_calls = 0;
}


static xcb_button_press_event_t s_make_event(xcb_window_t event_win,
        xcb_window_t child_win, xcb_window_t root, int16_t ex,
        int16_t ey, int16_t root_y, uint8_t detail)
{
    xcb_button_press_event_t event;

    memset(&event, 0, sizeof(event));
    event.event = event_win;
    event.child = child_win;
    event.root = root;
    event.event_x = ex;
    event.event_y = ey;
    event.root_y = root_y;
    event.detail = detail;
    event.time = 1000;

    return event;
}


/* No overlay open at all: nothing is consumed, nothing is dismissed */
static void s_test_no_overlay_open_returns_false(void)
{
    xcb_button_press_event_t event = s_make_event(1, 2, 3, 5, 5, 5, 1);
    bool consumed;

    s_reset();

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(!consumed, "no overlay open: event not consumed");
}


/* Popup open: always closes, but never consumes the click, and marks
 * the resolved surface outdated */
static void s_test_popup_always_closes_and_lets_click_through(void)
{
    xcb_button_press_event_t event = s_make_event(1, 2, 3, 5, 5, 5, 1);
    surface_td surface;
    bool consumed;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_popup_open = true;
    s_stub_lookup_surface = &surface;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(!consumed, "popup open: click is not consumed (passes"
            " through to the client)");
    TAP_EQ_INT(s_popup_close_calls, 1, "the popup is closed");
    TAP_OK(surface.is_outdated,
            "the resolved surface is marked outdated");
}


/* Popup open, no surface resolved for the root: still closes, still
 * lets the click through, simply skips the outdated mark */
static void s_test_popup_with_no_surface_still_closes(void)
{
    xcb_button_press_event_t event = s_make_event(1, 2, 3, 5, 5, 5, 1);
    bool consumed;

    s_reset();
    s_popup_open = true;
    s_stub_lookup_surface = NULL;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(!consumed, "popup open, no surface: click still not"
            " consumed");
    TAP_EQ_INT(s_popup_close_calls, 1, "the popup still closes");
}


/* Confirm dialog open, click lands on its own window: forwards the
 * click and consumes the event */
static void s_test_confirm_dialog_click_on_window_forwards(void)
{
    xcb_button_press_event_t event = s_make_event(42, 0, 3, 7, 9, 5, 1);
    bool consumed;

    s_reset();
    s_confirm_open = true;
    s_confirm_window = 42;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "confirm dialog open, click on it: event"
            " consumed");
    TAP_EQ_INT(s_confirm_handle_click_calls, 1,
            "the click is forwarded to the confirm dialog");
    TAP_EQ_INT(s_confirm_handle_click_x, 7, "with the event's own X");
    TAP_EQ_INT(s_confirm_handle_click_y, 9, "and the event's own Y");
    TAP_EQ_INT(s_allow_and_flush_calls, 1,
            "and the pointer grab is released asynchronously");
}


/* Confirm dialog open, click lands elsewhere entirely: the event is
 * still fully consumed, but nothing is forwarded to the dialog */
static void s_test_confirm_dialog_click_elsewhere_still_consumes(void)
{
    xcb_button_press_event_t event = s_make_event(99, 0, 3, 7, 9, 5, 1);
    bool consumed;

    s_reset();
    s_confirm_open = true;
    s_confirm_window = 42;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "confirm dialog open, click elsewhere: event"
            " still consumed");
    TAP_EQ_INT(s_confirm_handle_click_calls, 0,
            "but nothing is forwarded to the dialog itself");
}


/* Info dialog open, click on it, scroll-up (button 4): scrolls the
 * message up by 3 rather than treating it as a click */
static void s_test_info_dialog_scroll_up(void)
{
    xcb_button_press_event_t event = s_make_event(7, 0, 3, 1, 1, 5, 4);
    bool consumed;

    s_reset();
    s_info_open = true;
    s_info_window = 7;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "info dialog, scroll up: event consumed");
    TAP_EQ_INT(s_message_scroll_calls, 1,
            "the message is scrolled rather than clicked");
    TAP_OK(s_message_scroll_delta == -3, "scroll up moves by -3");
    TAP_EQ_INT(s_info_handle_click_calls, 0,
            "and no plain click is forwarded");
}


/* Info dialog open, click on it, scroll-down (button 5): scrolls the
 * message down by 3 */
static void s_test_info_dialog_scroll_down(void)
{
    xcb_button_press_event_t event = s_make_event(7, 0, 3, 1, 1, 5, 5);
    bool consumed;

    s_reset();
    s_info_open = true;
    s_info_window = 7;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "info dialog, scroll down: event consumed");
    TAP_EQ_INT(s_message_scroll_calls, 1, "message scrolled once");
    TAP_OK(s_message_scroll_delta == 3, "scroll down moves by +3");
}


/* Info dialog open, ordinary left-click on it: forwards a plain click,
 * not a scroll */
static void s_test_info_dialog_plain_click(void)
{
    xcb_button_press_event_t event = s_make_event(7, 0, 3, 1, 1, 5, 1);
    bool consumed;

    s_reset();
    s_info_open = true;
    s_info_window = 7;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "info dialog, plain click: event consumed");
    TAP_EQ_INT(s_info_handle_click_calls, 1,
            "the click forwards to the info dialog's own handler");
    TAP_EQ_INT(s_message_scroll_calls, 0, "and no scroll happens");
}


/* Cycle menu open, click inside the row area: navigates to the
 * computed row and confirms the selection */
static void s_test_cycle_menu_click_in_row_area_confirms(void)
{
    xcb_button_press_event_t event = s_make_event(9, 0, 3, 5,
            (int16_t) (WM_CYCLE_MENU_PAD_Y + WM_CYCLE_MENU_ROW_HEIGHT
                * 2 + 3), 5, 1);
    bool consumed;

    s_reset();
    s_cycle_open = true;
    s_cycle_window = 9;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "cycle menu, click in row area: event consumed");
    TAP_EQ_INT(s_cycle_navigate_to_calls, 1,
            "navigates to the row under the click");
    TAP_OK(s_cycle_navigate_to_row == 2u,
            "the row math resolves the intended row index");
    TAP_EQ_INT(s_cycle_confirm_calls, 1, "and the selection confirms");
    TAP_EQ_INT(s_cycle_destroy_calls, 0, "without also destroying it");
}


/* Cycle menu open, click above the row area (in the padding): just
 * destroys the menu, no navigation */
static void s_test_cycle_menu_click_in_pad_area_destroys(void)
{
    xcb_button_press_event_t event = s_make_event(9, 0, 3, 5,
            (int16_t) (WM_CYCLE_MENU_PAD_Y - 2), 5, 1);
    bool consumed;

    s_reset();
    s_cycle_open = true;
    s_cycle_window = 9;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "cycle menu, click in the pad area: event"
            " consumed");
    TAP_EQ_INT(s_cycle_navigate_to_calls, 0, "no row navigation");
    TAP_EQ_INT(s_cycle_destroy_calls, 1, "menu destroyed instead");
}


/* Cycle menu open, click entirely outside the menu window: destroys
 * the menu */
static void s_test_cycle_menu_click_outside_destroys(void)
{
    xcb_button_press_event_t event = s_make_event(123, 0, 3, 5, 30, 5,
            1);
    bool consumed;

    s_reset();
    s_cycle_open = true;
    s_cycle_window = 9;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "cycle menu, click outside it: event consumed");
    TAP_EQ_INT(s_cycle_destroy_calls, 1, "menu destroyed");
    TAP_EQ_INT(s_cycle_navigate_to_calls, 0, "no navigation attempted");
}


/* Search widget open, click on it: forwards the click */
static void s_test_search_click_on_window_forwards(void)
{
    xcb_button_press_event_t event = s_make_event(11, 0, 3, 4, 4, 5, 1);
    bool consumed;

    s_reset();
    s_search_open = true;
    s_search_window = 11;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "search widget, click on it: event consumed");
    TAP_EQ_INT(s_search_handle_click_calls, 1,
            "the click is forwarded to the search widget");
    TAP_EQ_INT(s_search_destroy_calls, 0, "widget is not destroyed");
}


/* Search widget open, click elsewhere: destroys the widget */
static void s_test_search_click_elsewhere_destroys(void)
{
    xcb_button_press_event_t event = s_make_event(999, 0, 3, 4, 4, 5,
            1);
    bool consumed;

    s_reset();
    s_search_open = true;
    s_search_window = 11;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "search widget, click elsewhere: event consumed");
    TAP_EQ_INT(s_search_destroy_calls, 1, "the widget is destroyed");
    TAP_EQ_INT(s_search_handle_click_calls, 0,
            "and no click is forwarded to it");
}


/* Window context menu open, click on the menu's own window: forwards
 * the click, using event->event as the resolved menu window since it
 * is the one owned */
static void s_test_wincmenu_click_on_event_window_forwards(void)
{
    xcb_button_press_event_t event = s_make_event(21, 0, 3, 4, 4, 88,
            1);
    bool consumed;

    s_reset();
    s_wincmenu_open = true;
    s_wincmenu_owned_window = 21;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "wincmenu open, click on event window: event"
            " consumed");
    TAP_EQ_INT(s_wincmenu_handle_click_calls, 1,
            "the click is forwarded to the window menu");
    TAP_EQ_INT(s_wincmenu_close_calls, 0, "menu is not closed");
}


/* Window context menu open, click owned only via event->child (not
 * event->event): still forwards the click */
static void s_test_wincmenu_click_on_child_window_forwards(void)
{
    xcb_button_press_event_t event = s_make_event(1, 21, 3, 4, 4, 88,
            1);
    bool consumed;

    s_reset();
    s_wincmenu_open = true;
    s_wincmenu_owned_window = 21;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "wincmenu open, click on child window: event"
            " consumed");
    TAP_EQ_INT(s_wincmenu_handle_click_calls, 1,
            "the click still forwards via the child window");
}


/* Window context menu open, click matches neither window: closes it */
static void s_test_wincmenu_click_neither_window_closes(void)
{
    xcb_button_press_event_t event = s_make_event(1, 2, 3, 4, 4, 88, 1);
    bool consumed;

    s_reset();
    s_wincmenu_open = true;
    s_wincmenu_owned_window = 21;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "wincmenu open, click matches neither window:"
            " event consumed");
    TAP_EQ_INT(s_wincmenu_close_calls, 1, "the window menu is closed");
    TAP_EQ_INT(s_wincmenu_handle_click_calls, 0,
            "and no click is forwarded");
}


/* Root menu open, click matches neither window: closes it, and never
 * touches wincmenu or winlist since rootmenu is checked independently */
static void s_test_rootmenu_click_neither_window_closes(void)
{
    xcb_button_press_event_t event = s_make_event(1, 2, 3, 4, 4, 88, 1);
    bool consumed;

    s_reset();
    s_rootmenu_open = true;
    s_rootmenu_owned_window = 55;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "rootmenu open, click matches neither window:"
            " event consumed");
    TAP_EQ_INT(s_rootmenu_close_calls, 1, "the root menu is closed");
    TAP_EQ_INT(s_wincmenu_owns_calls, 0,
            "wincmenu is never even queried, since it was checked"
            " first and was not open");
}


/* Window list open, click on its own window: forwards the click */
static void s_test_winlist_click_on_window_forwards(void)
{
    xcb_button_press_event_t event = s_make_event(77, 0, 3, 4, 4, 88,
            1);
    bool consumed;

    s_reset();
    s_winlist_open = true;
    s_winlist_owned_window = 77;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "winlist open, click on its window: event"
            " consumed");
    TAP_EQ_INT(s_winlist_handle_click_calls, 1,
            "the click is forwarded to the window list");
}


/* Priority order: when both the confirm dialog and the cycle menu
 * would otherwise report open, the confirm dialog wins, since it is
 * checked first */
static void s_test_confirm_dialog_takes_priority_over_cycle(void)
{
    xcb_button_press_event_t event = s_make_event(42, 0, 3, 7, 9, 5, 1);
    bool consumed;

    s_reset();
    s_confirm_open = true;
    s_confirm_window = 42;
    s_cycle_open = true;
    s_cycle_window = 9;

    consumed = im_press_close_overlays((xcb_connection_t *) 1, NULL,
            &event, NULL);

    TAP_OK(consumed, "both confirm dialog and cycle menu open: event"
            " consumed");
    TAP_EQ_INT(s_confirm_handle_click_calls, 1,
            "the confirm dialog handles the click");
    TAP_EQ_INT(s_cycle_navigate_to_calls, 0,
            "the cycle menu is never even reached");
    TAP_EQ_INT(s_cycle_destroy_calls, 0,
            "nor is it destroyed, since its own is_open check is"
            " never evaluated");
}


int main(void)
{
    TAP_PLAN(57);

    s_test_no_overlay_open_returns_false();
    s_test_popup_always_closes_and_lets_click_through();
    s_test_popup_with_no_surface_still_closes();
    s_test_confirm_dialog_click_on_window_forwards();
    s_test_confirm_dialog_click_elsewhere_still_consumes();
    s_test_info_dialog_scroll_up();
    s_test_info_dialog_scroll_down();
    s_test_info_dialog_plain_click();
    s_test_cycle_menu_click_in_row_area_confirms();
    s_test_cycle_menu_click_in_pad_area_destroys();
    s_test_cycle_menu_click_outside_destroys();
    s_test_search_click_on_window_forwards();
    s_test_search_click_elsewhere_destroys();
    s_test_wincmenu_click_on_event_window_forwards();
    s_test_wincmenu_click_on_child_window_forwards();
    s_test_wincmenu_click_neither_window_closes();
    s_test_rootmenu_click_neither_window_closes();
    s_test_winlist_click_on_window_forwards();
    s_test_confirm_dialog_takes_priority_over_cycle();

    return TAP_DONE();
}
