/**
 * @file input/mouse/event/press.c
 *
 * @brief Mouse button-press handling
 *
 * One of the files @c input/mouse/event/ is made of;
 * everything here feeds @c mouse_handle_press specifically.  Each
 * non-trivial responsibility inside it has been extracted into its
 * static function so the public entry point reads as a straightforward
 * sequence of checks rather than a monolith.  Button release lives in
 * @c input/mouse/event/release.c and enter-notify (including its,
 * unrelated hover-focus state) lives in @c input/mouse/event/enter.c
 * instead, neither of which this file's static helpers are ever
 * called from.
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
#include <stdlib.h>     /* free, NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Render includes */
#include <render/outdate.h>
#include <render/surface.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/placement/manual.h>

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
#include <cmds/client/maximize.h>
#include <cmds/client/state.h>

/* Local includes */
#include <input/mouse/bind.h>
#include <input/mouse/bounds.h>
#include <input/mouse/drag.h>
#include <input/mouse/drag/background.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/event.h>
#include <input/mouse/internal.h>
#include <utils/xcb/connection.h>


/* Small utilities */


/**
 * @brief Check whether a pointer position is near the edge of a client
 *
 * Returns @c true when the pointer's root coordinates fall within the
 * adaptive resize-grab margin of any edge of the client's current
 * bounding box, indicating that a border-drag resize should be
 * initiated.
 *
 * @param client   Client whose geometry is used for the test
 * @param root_pos Pointer position in root-window coordinates
 *
 * @return @c true if the pointer is on the resize border
 *
 * @note Complexity: @e O(1)
 *
 * @see @a im_bounds_resize in @c input/mouse/bounds.h
 */
static bool s_mouse_near_edge(const client_td *client,
        struct position_s root_pos)
{
    im_resize_bounds_td b;

    if (client == NULL) {
        return false;
    }

    b = im_bounds_resize(client);

    return root_pos.x < b.left + b.margin_left ||
        root_pos.x >= b.right - b.margin_right ||
        root_pos.y < b.top + b.margin_top ||
        root_pos.y >= b.bottom - b.margin_bottom;
}


/* Overlay dismissal */


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
        xcb_generic_error_t *err;
        struct position_s icon_pos;
        struct position_s root_pos;
        struct dimensions_s screen_dim;
        const surface_td *surface;

        gc = xcb_get_geometry(connection, client->icon_window);
        gr = xcb_get_geometry_reply(connection, gc, &err);
        if (err != NULL) {
            LOGGER_TRACE("xcb_get_geometry failed for icon window," \
                    " error=%d", err->error_code);
            free(err);
        }
        icon_pos.x = (gr != NULL)
            ? (int32_t) gr->x : (int32_t) client->icon_pos.x;
        icon_pos.y = (gr != NULL)
            ? (int32_t) gr->y : (int32_t) client->icon_pos.y;
        if (gr != NULL) {
            free(gr);
        }

        surface = wm_get_surface_by_id(client->screen_id);
        root_pos.x = event->root_x;
        root_pos.y = event->root_y;
        screen_dim.w = (surface != NULL) ? surface->properties.dim.w : 0u;
        screen_dim.h = (surface != NULL) ? surface->properties.dim.h : 0u;
        drag_icon_start(connection, event->root, client, desktop,
                icon_pos, event->time, root_pos, screen_dim);
    } else {
        /* Non-left-click: restore and focus */
        surface_td *surface;

        enact_client_restore(client);
        surface = lookup_surface_for_root(surfaces, event->root);
        if (surface != NULL && desktop != NULL) {
            focus_apply(surfaces, surface, desktop, client, true, config);
            im_sync_pinned_active(surface, desktop, client);
        }
    }

    im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER, event->time);
}


/**
 * @brief Test whether a button press should initiate a resize drag
 *
 * Returns @c true when the client is resizable, not fullscreen or fully
 * maximized (a fixed-size state with no border left to drag at all),
 * and the click landed on the frame border or near the edge of an
 * undecorated window.  A client maximized on just one axis
 * (horizontal-only or vertical-only) is allowed here.  Its still-free
 * axis can be resized normally, while its maximized one gets locked out
 * once the drag actually starts.
 *
 * @param client Client to test
 * @param window The X window that received the event
 * @param event  Incoming button-press event
 *
 * @return @c true if a resize drag should begin
 *
 * @see @a drag_start_resize_axis_locked, called from
 *      @a s_mouse_start_border_resize
 */
static bool s_mouse_can_resize_client(const client_td *client,
        xcb_window_t window, const xcb_button_press_event_t *event)
{
    if (!client_is_resizable(client)) {
        return false;
    }

    if (client_is_fullscreen(client) || client_is_maximized(client) ||
            client_is_locked(client)) {
        return false;
    }

    /* Decorated: click anywhere on the frame border */
    if (client->frame != 0 && window == client->frame) {
        return true;
    }

    /* Undecorated: click within the resize-grab margin of any edge
     * (see 's_mouse_near_edge' and 'im_bounds_resize') */
    if (client->frame == 0 && window == client->window &&
            s_mouse_near_edge(client,
                (struct position_s) { event->root_x, event->root_y })) {
        return true;
    }

    return false;
}


/**
 * @brief Show the window context menu at a right-click's root
 *        position, then finish handling the button-press event
 *
 * Shared by @c mouse_handle_press's two right-click-opens-the-menu
 * cases (a decorated frame's border, and an undecorated window's
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
                (struct position_s) { event->root_x, event->root_y },
                config);
    }
    im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER, event->time);
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
 */
static void s_mouse_start_border_resize(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop)
{
    surface_td *surface;
    struct position_s root_pos;
    struct dimensions_s screen_dim;

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    screen_dim.w = (surface != NULL) ? surface->properties.dim.w : 0u;
    screen_dim.h = (surface != NULL) ? surface->properties.dim.h : 0u;
    root_pos.x = event->root_x;
    root_pos.y = event->root_y;

    drag_start_resize_axis_locked(connection, event->root, client,
            desktop, event->time, root_pos,
            screen_dim,
            client_is_maximized_horz(client),
            client_is_maximized_vert(client));

    im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
            event->time);
}


/* Root window click */

/**
 * @brief Handle a button press on the root (desktop) window
 *
 * Right-click opens the root desktop menu; middle-click opens the
 * window list.  Left-press starts a background-pan drag right away
 * (see @c input/mouse/drag/background.h): dragging pans the viewport,
 * while a release with no real movement still unfocuses the active
 * client exactly like a plain background click always has.
 *
 * @param wm         Window manager instance, for the root menu
 * @param connection Active XCB connection
 * @param surfaces   Surface list
 * @param event      Incoming button-press event
 * @param config     Active configuration
 */
static void s_mouse_handle_root_press(wm_td *wm,
        xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    surface_td *const surface = lookup_surface_for_root(surfaces,
            event->root);

    if (surface == NULL) {
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_3) {
        rootmenu_show(wm, connection, surface,
                (struct position_s) { event->root_x, event->root_y },
                config);
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_1) {
        /* Left-press on the empty desktop: start a background-pan
         * drag right away.  'drag_background_end' (called from
         * 'mouse_handle_release') still unfocuses the active client,
         * exactly like this always did, whenever the drag turns out
         * to never have really moved. */
        drag_background_start(connection, surface, event->root,
                event->time,
                (struct position_s) { event->root_x, event->root_y });
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_2) {
        winlist_show(connection, surface,
                (struct position_s) { event->root_x, event->root_y },
                config);
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
    }
}


/**
 * @brief Resolve the managed client under a button event, preferring
 *        its reparented child window over the frame it was grabbed on
 *
 * A button event's @p child field names the deepest window under
 * the pointer (usually the client's reparented content window)
 * while @p event names whichever window the grab was actually
 * established on, usually the frame.  Tried in that order so a click
 * landing on the client's content still resolves correctly even in
 * cases (an icon window, which has no frame of its own) where the
 * frame's window ID alone would not have matched anything.
 *
 * @param connection  Unused; kept only so this matches the signature
 *                     shape of the other handlers around it
 * @param surfaces    Singly-linked list of @c surface_td pointers
 * @param event_win   The @p event field from the triggering XCB event
 * @param child_win   The @p child field from the triggering XCB event
 * @param out_desktop If non-null, receives the owning desktop
 *
 * @return Pointer to the matching client, or @c NULL if neither window
 *         belongs to one
 *
 * @note Complexity: @e O(s * d * c)
 *
 * @see @a lookup_find_client
 */
static client_td *s_mouse_find_event_client(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t event_win,
        xcb_window_t child_win, desktop_td **out_desktop)
{
    client_td *client = NULL;

    (void) connection;

    if (child_win != XCB_NONE) {
        client = lookup_find_client(surfaces, child_win, NULL,
                out_desktop);
    }
    if (client == NULL) {
        client = lookup_find_client(surfaces, event_win, NULL,
                out_desktop);
    }

    return client;
}


/* Public event handlers */
/**
 * @brief Keep a pinned client's active-window state consistent across
 *        every desktop on its surface
 *
 * @a focus_apply only updates @a client_active_id on the one @p desktop
 * passed to it.  For an ordinary client that is enough, but a pinned
 * one (visible on every desktop; see @a client_is_pinned) is expected
 * to keep showing as the active window no matter which desktop the user
 * switches to next.  Without this,
 * @a surface_clients_pinned_transfer_all's "was this pinned client
 * active on the desktop being switched away from" check (see
 * surface/actions.c) would only see the single desktop @a focus_apply
 * touched, silently dropping the active-window highlight the next time
 * the user switches through any other desktop first.  A no-op for
 * a non-pinned @p client, or when either @p surface or @p client is
 * null.
 *
 * @param surface Surface whose desktops are kept in sync
 * @param desktop The one desktop @c focus_apply already updated,
 *                skipped here to avoid redundant work
 * @param client  The client that just received focus
 *
 * @note Complexity: @e O(d), where @e d is the number of desktops on
 *       @p surface
 */
void im_sync_pinned_active(surface_td *surface,
        const desktop_td *desktop, const client_td *client)
{
    cdlist_item_td *dnode;
    const cdlist_item_td *dinitial;

    if (surface == NULL || surface->desktops == NULL ||
            client == NULL || !client_is_pinned(client)) {
        return;
    }

    dnode = cdlist_head(surface->desktops);
    if (dnode == NULL) {
        return;
    }

    dinitial = dnode;
    do {
        desktop_td *const d = (desktop_td *) cdlist_data(dnode);

        if (d != NULL && d != desktop &&
                d->client_active_id != client->id) {
            d->client_active_id = client->id;
            d->is_focus_dirty = true;
        }
        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


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
void im_allow_and_flush(xcb_connection_t *connection,
        uint8_t mode, xcb_timestamp_t time)
{
    xcb_allow_events(connection, mode, time);
    xcb_flush(connection);
}


/* Dispatch a button-press event */
void mouse_handle_press(wm_td *wm, xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    xcb_window_t window;
    client_td *client;
    desktop_td *desktop = NULL;
    surface_td *surface = NULL;
    enum wm_mousebind_type_e type = MOUSEBIND_NONE;
    struct dimensions_s screen_dim;
    struct position_s root_pos;

    if (connection == NULL || event == NULL || config == NULL) {
        return;
    }

    /* Deliberately ahead of the numbered steps below rather than one
     * of them: a window waiting to be placed by hand is holding the
     * pointer, so this press is the answer to the question it is
     * asking and never a click on anything those steps would go on to
     * resolve.  Nothing below it runs at all while one stands open. */
    if (place_manual_is_active()) {
        place_manual_handle_press(connection);
        return;
    }

    /* Step 1: dismiss any open overlay; return if the event was
     * consumed */
    if (im_press_close_overlays(connection, surfaces,
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
    type = im_resolve_binding((xcb_button_index_t) event->detail,
            event->state);

    /* Step 5: scroll bindings (desktop switch / titlebar shade or
     * maximize) */
    if (type == MOUSEBIND_DESKTOP_NORTH ||
            type == MOUSEBIND_DESKTOP_SOUTH ||
            type == MOUSEBIND_DESKTOP_EAST ||
            type == MOUSEBIND_DESKTOP_WEST) {
        im_press_scroll_binding(connection, surfaces, event,
                client, desktop, type, config);
        return;
    }

    /* Step 6: with no binding configured, handle the click on the
     * frame, the titlebar or the root window */
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
                    s_mouse_near_edge(client,
                        (struct position_s) { event->root_x,
                            event->root_y })) {
                s_mouse_show_wincmenu_at_click(connection, surface,
                        desktop, client, event, config);
                return;
            }

            /* Titlebar click */
            if (event->child == client->titlebar &&
                    client->titlebar != 0) {
                /* A move drag just claimed the pointer with its own
                 * active grab; returning here (rather than falling
                 * through to the resize check and the
                 * 'xcb_allow_events' call below, both dead code for
                 * a titlebar-owned 'window' anyway) keeps this click
                 * on the exact same path the alt-click move binding
                 * in Step 7 already takes, instead of racing that
                 * fresh grab against a stray release of the passive
                 * one that started it. */
                if (im_press_titlebar(connection, surfaces, event,
                        client, desktop, surface, config)) {
                    return;
                }
            }

            /* Border resize (decorated or undecorated) */
            if (s_mouse_can_resize_client(client, window, event)) {
                s_mouse_start_border_resize(connection, surfaces, event,
                        client, desktop);
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
                s_mouse_handle_root_press(wm, connection, surfaces, event,
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
    client = s_mouse_find_event_client(connection, surfaces,
            event->event, event->child, &desktop);

    if (client == NULL) {
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if (type == MOUSEBIND_RESIZE &&
            (!client_is_resizable(client) ||
             client_is_fullscreen(client) ||
             client_is_maximized(client) ||
             client_is_locked(client))) {
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if (type == MOUSEBIND_RESIZE && client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    if (type == MOUSEBIND_LOWER) {
        enact_client_lower(client);
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface != NULL && desktop != NULL) {
        focus_apply(surfaces, surface, desktop, client, true, config);
        im_sync_pinned_active(surface, desktop, client);
    }

    if (type == MOUSEBIND_MOVE &&
            (client_is_maximized(client) ||
             client_is_fullscreen(client) ||
             client_is_locked(client))) {
        im_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    screen_dim.w = (surface != NULL) ? surface->properties.dim.w : 0u;
    screen_dim.h = (surface != NULL) ? surface->properties.dim.h : 0u;
    root_pos.x = event->root_x;
    root_pos.y = event->root_y;

    /* A window maximized on just one axis still allows this
     * binding to resize its free axis, the same as a plain
     * border drag does (see 's_mouse_start_border_resize'); a
     * fully maximized or fullscreen client never reaches here at
     * all, per the check above. */
    if (type == MOUSEBIND_RESIZE) {
        drag_start_resize_axis_locked(connection, event->root,
                client, desktop, event->time,
                root_pos, screen_dim,
                client_is_maximized_horz(client),
                client_is_maximized_vert(client));
    } else {
        drag_start(connection, event->root, client, desktop,
                CLIENT_OPERATION_MOVING,
                event->time,
                root_pos, screen_dim);
    }
}
