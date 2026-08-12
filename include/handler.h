/**
 * @file handler.h
 *
 * @brief X event handler functions for the window manager core
 *
 * Declares the event handler functions for the X events that the window
 * manager processes after the input and menu events have been filtered
 * out.  Each handler receives the XCB connection, the managed surfaces
 * list, the active configuration, and the specific event pointer.
 *
 * @ingroup loop
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


/** ICCCM 'WM_CHANGE_STATE' 'IconicState' value */
#define ICCCM_ICONIC_STATE (3)


/* Public interface */
/**
 * @brief Handle a @c CONFIGURE_REQUEST event
 *
 * Applies geometry and stacking requests directly through XCB, keeping
 * the managed client's cached geometry synchronized when applicable.
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
 * @brief Handle a @c MAP_NOTIFY event
 *
 * Triggers an EWMH resync for the affected managed client when a
 * non-override-redirect window is mapped.  Override-redirect windows
 * (tooltips, pop-up menus) are silently ignored.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Map notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_map_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_map_notify_event_t *event);

/**
 * @brief Handle a @c GRAVITY_NOTIFY event
 *
 * Updates the cached frame position when the X server repositions
 * a frame window following a screen resize, according to the client's
 * @c win_gravity (stored as @c client->layout.gravity).  Re-syncs
 * decoration layout and schedules a repaint.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Gravity notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_gravity_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_gravity_notify_event_t *event);

/**
 * @brief Handle a @c CIRCULATE_NOTIFY event
 *
 * Marks the affected surface as outdated so that @c wm_ewmh_sync
 * updates @c _NET_CLIENT_LIST_STACKING to reflect the new stacking
 * order on the next main-loop iteration.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Circulate notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_circulate_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_notify_event_t *event);

/**
 * @brief Handle a @c CIRCULATE_REQUEST event (ICCCM §4.1.7)
 *
 * Raises or lowers the target window as directed by the @c place field:
 * @c XCB_PLACE_ON_TOP maps to @c XCB_STACK_MODE_ABOVE and
 * @c XCB_PLACE_ON_BOTTOM maps to @c XCB_STACK_MODE_BELOW.  The WM must
 * honor this request to remain ICCCM-compliant.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Circulate request event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_circulate_request(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_request_event_t *event);

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
 * Refreshes the client name when @c WM_NAME or @c _NET_WM_NAME changes.
 * When @c _NET_WM_STRUT_PARTIAL or @c _NET_WM_STRUT changes, re-reads
 * the strut values into @c client->layout.strut_partial and calls
 * @c desktop_update_workarea on all desktops of the owning surface so
 * that maximize and placement policies use the updated work area.
 *
 * @param wm         Pointer to the window manager itself
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Property notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_property_notify(wm_td *wm, xcb_connection_t *connection,
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
 * (fullscreen, maximize, close, desktop switch, iconify, &c.) to the
 * appropriate command functions so that they are honored by the window
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

/**
 * @brief Handle XRandR extension events
 *
 * Processes monitor/screen-change notifications and schedules geometry
 * and repaint refresh work for affected surfaces.
 *
 * @param wm    Window manager state
 * @param event Raw XCB event from the main loop
 */
void handler_randr_event(wm_td *wm, xcb_generic_event_t *event);

/**
 * @brief Handle XSync extension events
 *
 * Recognizes @c AlarmNotify events for @c _NET_WM_SYNC_REQUEST alarms
 * and applies the owning client's pending throttled resize, if any.
 * Events on alarms owned by no managed client, or arriving while XSync
 * is unavailable, are ignored.
 *
 * @param wm    Window manager state
 * @param event Raw XCB event from the main loop
 *
 * @see @c ccmd_client_resize_flush_pending
 */
void handler_sync_event(wm_td *wm, xcb_generic_event_t *event);


#endif  /* ! HANDLER_H */
