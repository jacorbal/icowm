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

/* Type includes */
#include <types/handles.h>


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
 * @note Implemented in @c handler/ewmh.c
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
 * @note Implemented in @c handler/ewmh.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_current_desktop(const wm_td *wm,
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
 * @note Implemented in @c handler/ewmh.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_desktop(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface,
        desktop_td *src_desktop);

/**
 * @brief Handle a @c _NET_MOVERESIZE_WINDOW client message
 *
 * Lets a @c Client (typically a pager or session-restore tool, rather
 * than the client owning the window itself) request a direct,
 * one-shot geometry change, the same request @c ccmd_client_move and
 * @c ccmd_client_resize apply for icowm's own internal callers, but
 * driven by an external @c ClientMessage instead.
 *
 * @c event's own @c data32[0] carries a bitmask of which of
 * @c data32[1..4] (@c x, @c y, @c width, @c height, in that order)
 * are actually present in this particular request; an axis whose own
 * flag bit is unset is left exactly as it already was.  @p width and
 * @p height, per the EWMH specification, describe the client's own
 * content size, so each is padded out by the matching pair of frame
 * extents before being applied to @p client's frame, when decorated.
 * @p y is floored at @c 0 and @p width/@p height at
 * @c WM_MIN_WINDOW_DIMENSION, the same floors @c ccmd_client_move and
 * @c ccmd_client_resize themselves already enforce.
 *
 * An iconified @p client is left iconified: unlike
 * @c ccmd_client_shade, @c ccmd_client_fullscreen, and
 * @c ccmd_client_maximize (whose own request is itself a visible
 * state change the person is asking for), silently un-iconifying a
 * window a person deliberately minimized just because an external
 * pager sent it a geometry hint would be a surprising side effect of
 * a request that, on any other client, has no visible effect at all.
 *
 * @param wm      Window manager state
 * @param event   Incoming client message event
 * @param client  Client the message targets
 * @param surface Surface (screen) @p client is on
 * @param desktop Desktop @p client is on
 *
 * @note Complexity: @e O(1)
 */
void hi_handle_net_moveresize_window(const wm_td *wm,
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
 * @note Implemented in @c handler/ewmh.c
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
 * @note Implemented in @c handler/ewmh.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_restack_window(const wm_td *wm,
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
 * @note Implemented in @c handler/ewmh.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_fullscreen_monitors(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop);


/**
 * @brief Handle a @c _NET_WM_MOVERESIZE client message
 *
 * Starts (or cancels) an icowm-managed interactive move or resize on
 * behalf of a client that draws its own titlebar or resize grips,
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
 * @note Implemented in @c handler/ewmh.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_moveresize(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop);


#endif  /* ! HANDLER_INTERNAL_H */
