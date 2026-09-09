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

/* Project includes */
#include <types/handles.h>


/** ICCCM @c WM_CHANGE_STATE @c IconicState value */
#define ICCCM_ICONIC_STATE (3)


/* Public interface */
/**
 * @brief Handle a @c CONFIGURE_REQUEST event
 *
 * Applies geometry and stacking requests directly through XCB, keeping
 * the managed client's cached geometry synchronized when applicable.
 * A request is ignored outright while the window manager itself is
 * actively moving, resizing, or has this same client in fullscreen,
 * and also, for a short grace period afterward, right after the
 * window manager itself shaded, unshaded, or entered or left
 * fullscreen on this same client, as a stale echo of that transition
 * rather than a genuine independent request (see
 * @c WM_SHADE_CONFIGURE_COOLDOWN_MS and
 * @c WM_FULLSCREEN_CONFIGURE_COOLDOWN_MS).
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
 * hierarchy, applies the placement policy, and optionally focuses the
 * new client.
 *
 * Under @c windows.placement.policy of @c manual the window is instead
 * held unmapped and unfocused while the person is asked where it goes;
 * everything that follows the placement, the mapping, the focus, the
 * ICCCM @c ConfigureNotify and the IPC notification, is handed to
 * @a place_manual_enqueue to carry out once that question is settled.
 *
 * @param wm     Window manager state
 * @param event  Map request event
 *
 * @note Complexity: @e O(g * n), where @e g is the number of grid
 *       positions the placement policy tests and @e n is the number
 *       of clients on the desktop
 */
void handler_map_request(const wm_td *wm,
        xcb_map_request_event_t *event);

/**
 * @brief Handle a @c MAP_NOTIFY event
 *
 * Triggers an EWMH resync for the affected managed client when
 * a non-override-redirect window is mapped.  Override-redirect windows
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
 * Marks the affected surface as outdated so that @a wm_ewmh_sync
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
 * Raises or lowers the target window as directed by the @p place field:
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
 * Drops the window from the systray when it was a docked icon, cancels
 * any drag in progress for the destroyed client, drops it from the
 * queue of windows waiting to be placed by hand, removes it from its
 * desktop, and releases its resources.
 *
 * @param wm         Window-manager singleton
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Destroy notify event
 *
 * @note Complexity: @e O(n + t), where @e n is the number of managed
 *       surfaces and @e t the number of docked systray icons
 */
void handler_destroy_notify(wm_td *wm, xcb_connection_t *connection,
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
void handler_property_notify(const wm_td *wm,
        xcb_connection_t *connection,
        list_td *surfaces, xcb_property_notify_event_t *event);

/**
 * @brief Handle a @c LEAVE_NOTIFY event
 *
 * Stops polling the resize-cursor target the pointer just left and,
 * under focus-follows-mouse, releases the X11 input focus so that no
 * client stays visually focused while the pointer rests on the root
 * background.
 *
 * @param wm    Window-manager singleton
 * @param event Leave-notify event to process
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected by @a lookup_find_client
 */
void handler_leave_notify(const wm_td *wm,
        xcb_leave_notify_event_t *event);

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
        list_td *surfaces, const xcb_focus_in_event_t *event);

/**
 * @brief Handle a @c FOCUS_OUT event
 *
 * Marks the surface owning the client that lost the real X11 input
 * focus as outdated, so its decoration colors are repainted on the
 * next render pass.
 *
 * @param wm    Window-manager singleton
 * @param event Focus-out event to process
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected by @a lookup_find_client
 */
void handler_focus_out(const wm_td *wm, xcb_focus_out_event_t *event);

/**
 * @brief Handle a @c COLORMAP_NOTIFY event
 *
 * ICCCM §4.1.8: a client's colormap attribute changed on one of
 * the windows named in its @c WM_COLORMAP_WINDOWS list (or ceased to
 * be installed at all).  Updates the matching cached entry in
 * @c colormap_windows.colormap_ids, and, when the owning client
 * currently holds real input focus, installs the updated colormap
 * immediately rather than waiting for the next focus change.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Colormap notify event
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e c
 *       the number of clients per desktop
 */
void handler_colormap_notify(xcb_connection_t *connection,
        list_td *surfaces, const xcb_colormap_notify_event_t *event);

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
 * Repaints the info popup, cycle menu, icon window captions, and client
 * titlebars as needed.  Only the final event in a sequence
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
 * @brief Handle an X protocol error delivered as an event
 *
 * XCB reports a failed request whose reply was never waited for as
 * a regular event with a response type of @c 0, which is what this
 * receives, reinterpreted as the @c xcb_generic_error_t it actually
 * is.  A @c BadWindow, @c BadDrawable, or @c BadMatch on one of the
 * routine per-window requests the window manager issues constantly
 * (mapping, configuring, reading properties) is logged at DEBUG
 * level, since it is the expected outcome of a client destroying its
 * own window in the gap between the request and the server processing
 * it, and races of that kind cannot be prevented from this side.
 *
 * Any other error (@c BadValue, @c BadAlloc, @c BadAccess, or even
 * @c BadWindow/@c BadMatch on a request outside that routine set)
 * is logged at WARNING level instead, so it is not lost among routine
 * traffic, since it is far more likely to be a genuine bug worth
 * noticing.
 *
 * @param event The raw event received with response type @c 0
 *
 * @note Complexity: @e O(1)
 */
void handler_protocol_error(const xcb_generic_event_t *event);

/**
 * @brief Human-readable description for an @a xcb_connection_has_error
 *        return value
 *
 * Distinguishes an ordinary per-window protocol error, which XCB
 * delivers as a regular event and never causes this, from an actual
 * connection failure: the socket to the X server itself is gone,
 * something no window manager can recover from, since the window
 * manager is just another client of that same server.  Logging which
 * one occurred is the most the caller can do about it; an
 * @c XCB_CONN_ERROR in particular, especially right after a client
 * (e.g., a game attempting hardware-accelerated rendering) was seen
 * doing something unusual, is worth checking the system's logs
 * (Xorg's log file, @c dmesg for a GPU driver crash) for, outside
 * of icowm entirely.
 *
 * @param error_code Value returned by @a xcb_connection_has_error
 *
 * @return A short, constant description; never @c NULL
 *
 * @note Complexity: @e O(1)
 */
const char *handler_connection_error_string(int error_code);

/**
 * @brief Handle XRandR extension events
 *
 * Processes monitor/screen-change notifications and schedules geometry
 * and repaint refresh work for affected surfaces.
 *
 * @param wm    Window manager state
 * @param event Raw XCB event from the main loop
 *
 * @note Complexity: @e O(b * s * k), where @e b is the number of key
 *       bindings, @e s is the number of surfaces, and @e k is the
 *       number of keycodes per keysym
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
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across every desktop and surface
 *
 * @see @c ccmd_client_resize_flush_pending
 */
void handler_sync_event(const wm_td *wm, xcb_generic_event_t *event);


#endif  /* ! HANDLER_H */
