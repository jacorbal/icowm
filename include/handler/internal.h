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
#include <actdata.h>
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>


/**
 * @brief Refresh the work areas for all desktops on a surface
 *
 * Recalculates and updates @c _NET_WORKAREA for every desktop on
 * @p surface after a strut or screen-geometry change.
 *
 * @param surface Surface whose work areas should be refreshed
 *
 * @note Implemented in @c handler/focus.c
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
void hi_refresh_workareas(surface_td *surface);


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
        xcb_ewmh_connection_t *ewmh,
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
 * @note Implemented in @c handler/ewmh.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_desktop(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface,
        desktop_td *src_desktop);


/**
 * @brief Handle a @c _NET_MOVERESIZE_WINDOW client message
 *
 * Applies the requested geometry change to @p client.
 *
 * @param wm      Window manager state
 * @param event   Client-message event carrying the requested geometry
 * @param client  Target client
 * @param surface Surface containing the client
 * @param desktop Desktop containing the client
 *
 * @note Implemented in @c handler/ewmh.c
 * @note Complexity: @e O(1)
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
 * @note Implemented in @c handler/ewmh.c
 * @note Complexity: @e O(1)
 */
void hi_handle_net_wm_fullscreen_monitors(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop);


#endif  /* ! HANDLER_INTERNAL_H */
