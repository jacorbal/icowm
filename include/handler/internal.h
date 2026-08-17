/**
 * @file handler/internal.h
 *
 * @brief Private helpers shared across handler implementation modules
 *
 * Declares helper functions that are used by more than one of the
 * handler translation units (@c handler/configure.c, @c handler/map.c,
 * @c handler/focus.c, @c handler/expose.c, @c handler/message.c,
 * @c handler/randr.c) but must not be exposed as part of the public
 * handler API declared in @c handler.h.
 *
 * @note This header is private to the handler subsystem and must not be
 *       included outside of @c src/handler/
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef HANDLER_INTERNAL_H
#define HANDLER_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>


/**
 * @brief Handle a @c _NET_WM_STATE client message
 *
 * Dispatches up to two EWMH state atoms from a @c _NET_WM_STATE
 * @c ClientMessage and applies the requested state change to @p client.
 *
 * @param client  Target client
 * @param event   Received @c CLIENT_MESSAGE event
 * @param ewmh    EWMH connection handle
 * @param surface Surface owning the client
 * @param desktop Desktop where the client lives
 *
 * @note Implemented in @c handler/ewmhmsg.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_state(client_td *client,
        xcb_client_message_event_t *event,
        const xcb_ewmh_connection_t *ewmh,
        surface_td *surface, desktop_td *desktop);


/**
 * @brief Handle a @c _NET_CURRENT_DESKTOP client message
 *
 * Switches the active desktop on the surface identified by
 * @c event->window to the desktop index carried in the event data.
 *
 * @param wm    Window manager context
 * @param event Received @c CLIENT_MESSAGE event
 *
 * @note Implemented in @c handler/ewmhmsg.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_current_desktop(wm_td *wm,
        xcb_client_message_event_t *event);


/**
 * @brief Handle a @c _NET_WM_DESKTOP client message
 *
 * Moves @p client from @p src_desktop to the target desktop requested
 * by the event.
 *
 * @param wm          Window manager context
 * @param event       Received @c CLIENT_MESSAGE event
 * @param client      Client to move
 * @param surface     Surface owning the client
 * @param src_desktop Client's current desktop
 *
 * @note Implemented in @c handler/ewmhmsg.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_desktop(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface,
        desktop_td *src_desktop);

/**
 * @brief Handle a @c _NET_WM_MOVERESIZE client message
 *
 * Lets a @c Client that draws its own titlebar or resize grips (common
 * among GTK/Qt applications using client-side decoration) ask icowm to
 * take over an interactive move or resize, the same way dragging
 * icowm's own decoration would, rather than the Client having to track
 * pointer motion itself.
 *
 * A @c Client MAY choose to grab the pointer directly instead and never
 * send this message at all, so receiving it is optional to begin with,
 * but a @c Client that does relies on getting the exact same
 * move/resize behavior (edge snapping and so on) icowm's own decoration
 * already provides.
 *
 * @p direction selects the operation:
 * @c XCB_EWMH_WM_MOVERESIZE_MOVE starts a move; one of the eight
 * @c XCB_EWMH_WM_MOVERESIZE_SIZE_* values starts a resize anchored on;
 * @c XCB_EWMH_WM_MOVERESIZE_CANCEL cancels whichever of the two is
 * currently active for this same client, if any.
 *
 * The message itself carries no timestamp (@p only x_root, @p y_root,
 * direction, button, and source indication), so @c XCB_CURRENT_TIME is
 * used for both the pointer grab and, where a move is requested, the
 * drag state that would otherwise want the triggering event's own time.
 * The keyboard variants (@c XCB_EWMH_WM_MOVERESIZE_MOVE_KEYBOARD and
 * @c _SIZE_KEYBOARD) have no continuous, pointer-free equivalent in
 * @c icowm's own move/resize machinery to hand off to, so they are
 * acknowledged by being recognized at all but otherwise silently
 * ignored, the same way some other window managers (e.g., i3) treat the
 * full set of possible directions as more complexity than the few
 * @c Clients actually relying on this message call for.
 *
 * @param wm      Window manager state
 * @param event   Incoming client message event
 * @param client  Client the message targets
 * @param surface Surface (screen) @p client is on
 * @param desktop Desktop @p client is on
 *
 * @note Complexity: @e O(1)
 *
 * @see @a s_moveresize_direction_to_anchor
 */
void hi_handle_net_moveresize_window(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop);


/**
 * @brief Apply a @c _NET_SHOWING_DESKTOP request to one surface
 *
 * Hides or restores clients on the current desktop of @p surface.
 *
 * @param surface Surface to update
 * @param show    @c true to enter showing-desktop mode
 *
 * @note Implemented in @c handler/ewmhmsg.c
 * @note Complexity: @e O(n)
 */
void hi_handle_net_showing_desktop(surface_td *surface, bool show);


/**
 * @brief Handle a @c _NET_RESTACK_WINDOW client message
 *
 * Restacks @p client's window or frame per the requested stack mode.
 *
 * @param wm      Window manager state
 * @param event   Client-message event carrying the restack request
 * @param client  Target client
 * @param surface Surface containing the client
 * @param desktop Desktop containing the client
 *
 * @note Implemented in @c handler/ewmhmsg.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_restack_window(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop);


/**
 * @brief Handle a @c _NET_WM_FULLSCREEN_MONITORS client message
 *
 * Stores monitor indices on the client and reapplies fullscreen layout
 * if the client is currently fullscreen.
 *
 * @param wm      Window manager state
 * @param event   Client-message event carrying the monitor indices
 * @param client  Target client
 * @param surface Surface containing the client
 * @param desktop Desktop containing the client
 *
 * @note Implemented in @c handler/ewmhmsg.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_fullscreen_monitors(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop);


/**
 * @brief Handle a @c _NET_WM_MOVERESIZE client message
 *
 * Starts (or cancels) an icowm-managed interactive move or resize on
 * behalf of a Client that draws its own titlebar or resize grips,
 * matching whichever operation and anchor the message's direction
 * requests.
 *
 * @param wm      Window manager state
 * @param event   Client-message event carrying x_root, y_root,
 *                direction, button, and source indication
 * @param client  Target client
 * @param surface Surface containing the client
 * @param desktop Desktop containing the client
 *
 * @note Implemented in @c handler/ewmhmsg.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_moveresize(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop);


#endif  /* ! HANDLER_INTERNAL_H */
