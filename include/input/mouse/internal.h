/**
 * @file input/mouse/internal.h
 *
 * @brief Private cross-file declarations shared between the resize-
 *        cursor and hover-poll topic files
 *
 * Splitting the mouse input subsystem into per-topic files (resize
 * cursors in @c cursor.c, hover polling in @c hover.c, and the press/
 * release/enter dispatch, itself further split into
 * @c input/mouse/event/press.c, @c event/release.c, and
 * @c event/enter.c) still leaves the two functions declared below,
 * each one topic's file exposes for the other (and for
 * @c event/enter.c) to call directly, since the underlying resize-
 * cursor and hover state is genuinely shared, not duplicated per file
 * the way, say, @c menu/dialog/confirm.c and @c menu/dialog/
 * fortune.c each own their separate state.
 *
 * The drag subsystem keeps headers of its own, one per drag/ file
 * (@c drag/overlay.h,
 * @c drag/snap.h, @c drag/outline.h, @c drag/warp.h) plus
 * @c drag/internal.h for its singleton state alone.  Nothing under
 * @c input/mouse/event/, @c cursor.c, or @c hover.c ever actually
 * called into any of it, so keeping it here served no purpose beyond
 * a second, silently drifting copy of declarations @c drag/
 * internal.h already had right.
 *
 * @note This header is private to the mouse input subsystem and must
 *       not be included outside of @c src/input/mouse/, for it is
 *       NOT part of the public API in @c input/mouse/bind.h,
 *       @c input/mouse/event.h, @c input/mouse/hover.h, or
 *       @c input/mouse/cursor.h
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_INTERNAL_H
#define INPUT_MOUSE_INTERNAL_H

/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Input includes */
#include <input/mouse/bind.h>


/**
 * @brief Acknowledge a button press and flush
 *
 * Every path that decides a press needs no further handling ends
 * here, so that the pointer grab is released the same way regardless
 * of which one it was.
 *
 * @param connection XCB connection
 * @param mode       @c XCB_ALLOW_ASYNC_POINTER to consume the press,
 *                   @c XCB_ALLOW_REPLAY_POINTER to hand it back to
 *                   the window underneath
 * @param time       X server timestamp of the press
 *
 * @note Complexity: @e O(1)
 */
void im_allow_and_flush(xcb_connection_t *connection, uint8_t mode,
        xcb_timestamp_t time);

/**
 * @brief Keep a sticky client's active state in step across
 *        desktops
 *
 * @param surface Surface the client belongs to
 * @param desktop Desktop the press happened on
 * @param client  Client that just became active
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       @p surface
 */
void im_sync_sticky_active(surface_td *surface,
        const desktop_td *desktop, const client_td *client);

/**
 * @brief Dismiss whatever overlay a press lands outside of
 *
 * Asked first, before the press is resolved against anything else, so
 * that a click meant to close a menu never also reaches the window
 * underneath it.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Button press event
 * @param config     Active configuration
 *
 * @return @c true when the press was consumed dismissing an overlay
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
bool im_press_close_overlays(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config);

/**
 * @brief Carry out a scroll-wheel binding
 *
 * Switches desktop when the wheel turns over the root window, and
 * shades, unshades or maximizes when it turns over a titlebar.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Button press event carrying the wheel direction
 * @param client     Client under the pointer, may be @c NULL
 * @param desktop    Desktop under the pointer, may be @c NULL
 * @param type       Resolved binding, one of the
 *                   @c MOUSEBIND_DESKTOP_* directions
 * @param config     Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the affected desktop
 */
void im_press_scroll_binding(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop,
        enum wm_mousebind_type_e type, const config_td *config);

/**
 * @brief Handle a press that landed on a client's titlebar
 *
 * Decides between a titlebar button, a double click, and the start of
 * a move drag.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Button press event
 * @param client     Client whose titlebar was pressed
 * @param desktop    Desktop the client belongs to
 * @param surface    Surface the client belongs to
 * @param config     Active configuration
 *
 * @note Complexity: @e O(b), where @e b is the number of configured
 *       titlebar buttons
 */
void im_press_titlebar(xcb_connection_t *connection, list_td *surfaces,
        xcb_button_press_event_t *event, client_td *client,
        desktop_td *desktop, surface_td *surface,
        const config_td *config);

/**
 * @brief Find which action a button and modifier combination is bound
 *        to
 *
 * Pure over the binding table: it reads no X state and answers the
 * same for the same arguments.
 *
 * @param button Button index of the press
 * @param state  Modifier mask of the press, as reported by the X
 *               server; its lock bits are ignored
 *
 * @return The action bound to the combination, or @c MOUSEBIND_NONE
 *         when no binding matches
 *
 * @note Complexity: @e O(b), where @e b is the number of configured
 *       button bindings
 */
enum wm_mousebind_type_e im_resolve_binding(xcb_button_index_t button,
        uint16_t state);


#endif  /* ! INPUT_MOUSE_INTERNAL_H */
