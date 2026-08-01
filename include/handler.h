/**
 * @file handler.h
 *
 * @brief X event handler functions for the window manager core
 *
 * Declares the nine event handler functions for the X events that the
 * window manager processes after the input and menu events have been
 * filtered out.  Each handler receives the XCB connection, the managed
 * surfaces list, the active configuration, and the specific event
 * pointer.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef HANDLER_H
#define HANDLER_H


/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <wm.h>


/* Public interface */
/**
 * @brief Handle a @c CONFIGURE_REQUEST event
 *
 * Applies geometry and stacking requests directly through XCB, keeping
 * the managed client's cached geometry synchronised when applicable.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Configure request event
 *
 * @note Complexity: @e O(1)
 */
void handler_configure_request(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_configure_request_event_t *event);

/**
 * @brief Handle a @c CONFIGURE_NOTIFY event
 *
 * Updates the client's cached geometry from the event.  Only frame
 * (outermost) window events are trusted to avoid corrupting cached
 * screen-relative positions with frame-relative values from inner
 * windows.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Configure notify event
 *
 * @note Complexity: @e O(1)
 */
void handler_configure_notify(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_configure_notify_event_t *event);

/**
 * @brief Handle a @c MAP_REQUEST event
 *
 * Adopts newly visible windows into the window manager's desktop
 * hierarchy, applies placement policy, and optionally focuses the new
 * client.
 *
 * @param wm     Window manager state
 * @param event  Map request event
 *
 * @note Complexity: @e O(1)
 */
void handler_map_request(wm_td *wm, xcb_map_request_event_t *event);

/**
 * @brief Handle an @c UNMAP_NOTIFY event
 *
 * Updates desktop active-client tracking when a managed client is
 * unmapped.  Does not set the hidden flag, to avoid interfering with
 * desktop switching.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Unmap notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_unmap_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_unmap_notify_event_t *event);

/**
 * @brief Handle a @c DESTROY_NOTIFY event
 *
 * Cancels any in-progress drag for the destroyed client, removes it
 * from its desktop, and releases its resources.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Destroy notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_destroy_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_destroy_notify_event_t *event);

/**
 * @brief Handle a @c PROPERTY_NOTIFY event
 *
 * Refreshes the client name when @c WM_NAME changes.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Property notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_property_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_property_notify_event_t *event);

/**
 * @brief Handle a @c FOCUS_IN event
 *
 * Synchronizes the desktop active-client identifier with the real X11
 * input focus when a managed client receives focus.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Focus-in event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_focus_in(xcb_connection_t *connection,
        list_td *surfaces, xcb_focus_in_event_t *event);

/**
 * @brief Handle a @c MAPPING_NOTIFY event
 *
 * Refreshes the cached keyboard-mapping table and re-establishes all
 * passive key and button grabs.
 *
 * @param keysyms  XCB key-symbols table to refresh
 * @param surfaces All managed surfaces
 * @param event    Mapping notify event
 * @param cfg      Active configuration
 *
 * @note Complexity: @e O(k * s), where @e k is the number of bindings
 *       and @e s is the number of surfaces
 */
void handler_mapping_notify(xcb_key_symbols_t *keysyms,
        list_td *surfaces, xcb_mapping_notify_event_t *event,
        const config_td *cfg);

/**
 * @brief Handle an @c EXPOSE event for decoration repaints
 *
 * Repaints the info popup, cycle menu, icon window captions, and
 * client titlebars as needed.  Only the final event in a sequence
 * @c (count == 0) triggers a repaint.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Expose event
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_expose(xcb_connection_t *connection,
        list_td *surfaces, xcb_expose_event_t *event,
        const config_td *cfg);

/**
 * @brief Handle a @c CLIENT_MESSAGE event
 *
 * Dispatches EWMH and ICCCM client-message requests from applications
 * (fullscreen, maximize, close, desktop switch, iconify, etc.) to the
 * appropriate command functions so that they are honoured by the window
 * manager.
 *
 * @param wm    Window manager state
 * @param event Client message event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
void handler_client_message(wm_td *wm,
        xcb_client_message_event_t *event);


#endif  /* ! HANDLER_H */
