/**
 * @file input/mouse/event.c
 *
 * @brief Mouse button-press, button-release, and enter-notify handlers
 *
 * Each non-trivial responsibility inside @c mouse_handle_press has been
 * extracted into its own static function so the public entry point
 * reads as a straightforward sequence of checks rather than a monolith.
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
#include <stdlib.h>     /* free */
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Utils includes */
#include <utils/cursor.h>

/* Render includes */
#include <render/surface.h>

/* Policy includes */
#include <policy/focus.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog/info.h>
#include <menu/dialog/quit.h>
#include <menu/popup.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/cursor.h>
#include <defs/cycle.h>
#include <defs/input.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* CMD includes */
#include <cmds/ccmd.h>

/* Local includes */
#include <input/mouse/drag.h>
#include <input/mouse/bounds.h>
#include <input/mouse.h>


/* Module state */

/**
 * @brief Flag set while a hover-triggered focus transfer is in flight
 *
 * Set in @a mouse_handle_enter before calling @a focus_apply and
 * cleared in @a handler_focus_in when the corresponding @c FocusIn
 * event arrives.
 */
static bool s_enter_focus_active = false;

/* State for double-click detection on titlebars.  A double-click on the
 * titlebar drag area (i.e., not on a button) toggles shade/unshade. */
static xcb_timestamp_t s_last_titlebar_press_time = 0;
static xcb_window_t s_last_titlebar_press_win = XCB_NONE;


/* Small utilities */

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
static void s_allow_and_flush(xcb_connection_t *connection,
        uint8_t mode, xcb_timestamp_t time)
{
    xcb_allow_events(connection, mode, time);
    xcb_flush(connection);
}


/**
 * @brief Check whether a pointer position is near the edge of a client
 *
 * Returns @c true when the pointer's root coordinates fall within the
 * adaptive resize-grab margin (see @c im_resize_bounds in
 * input/mouse/bounds.h) of any edge of the client's current
 * bounding box, indicating that a border-drag resize should be
 * initiated.
 *
 * @param client Client whose geometry is used for the test
 * @param root_x Pointer X position in root-window coordinates
 * @param root_y Pointer Y position in root-window coordinates
 *
 * @return @c true if the pointer is on the resize border, @c false
 *         otherwise
 *
 * @note Complexity: @e O(1)
 */
static bool s_mouse_near_edge(const client_td *client,
        int16_t root_x, int16_t root_y)
{
    im_resize_bounds_td b;

    if (client == NULL) {
        return false;
    }

    b = im_resize_bounds(client);

    return (int32_t) root_x < b.left + b.margin_left ||
        (int32_t) root_x >= b.right - b.margin_right ||
        (int32_t) root_y < b.top + b.margin_top ||
        (int32_t) root_y >= b.bottom - b.margin_bottom;
}


/**
 * @brief Which border/corner zone of a client's bounding box a point
 *        falls within, if any
 */
enum s_resize_zone_e {
    S_RESIZE_ZONE_NONE = 0,
    S_RESIZE_ZONE_N,
    S_RESIZE_ZONE_S,
    S_RESIZE_ZONE_E,
    S_RESIZE_ZONE_W,
    S_RESIZE_ZONE_NE,
    S_RESIZE_ZONE_NW,
    S_RESIZE_ZONE_SE,
    S_RESIZE_ZONE_SW,
    S_RESIZE_ZONE_COUNT,    /**< Not a real zone; array size marker */
};

/**
 * One allocated cursor per @c s_resize_zone_e value; index 0 (@c NONE)
 * holds the plain left-pointer cursor
 *
 * Zero (@c XCB_CURSOR_NONE) until @c mouse_create_resize_cursors runs */
static xcb_cursor_t s_resize_cursors[S_RESIZE_ZONE_COUNT];


/**
 * @brief Determine which border/corner zone, if any, a point falls in
 *
 * Uses the same adaptive resize-grab margins as @c s_mouse_near_edge
 * (both call @c im_resize_bounds), so the cursor changes exactly where
 * a resize can actually start, including the same titlebar-row
 * exclusion for the left/right margins (see @c im_resize_bounds_td's
 * own doc comment): a titlebar button such as close, typically placed
 * near the frame's own right edge, would otherwise still register as
 * near that edge, showing a resize cursor over it even though clicking
 * it still correctly closes the window rather than starting a resize
 * (titlebar buttons take priority over a border drag in the
 * button-press handler regardless of this function).
 *
 * @param client Client whose geometry is used for the test
 * @param root_x Pointer X position in root-window coordinates
 * @param root_y Pointer Y position in root-window coordinates
 *
 * @return The matching zone, or @c S_RESIZE_ZONE_NONE when @p root_x /
 *         @p root_y fall outside every border zone (including entirely
 *         outside the client, its titlebar row, or its non-resizable
 *         interior)
 *
 * @note Complexity: @e O(1)
 */
static enum s_resize_zone_e s_mouse_resize_zone(const client_td *client,
        int16_t root_x, int16_t root_y)
{
    im_resize_bounds_td b;
    bool near_left;
    bool near_right;
    bool near_top;
    bool near_bottom;
    enum s_resize_zone_e zone;

    if (client == NULL) {
        return S_RESIZE_ZONE_NONE;
    }

    b = im_resize_bounds(client);

    /* Do not add bounds checks here.  The caller only invokes this
     * function for motion events already known to belong to this
     * client's frame or window, and an extra geometric check may
     * disagree with X11's actual border hit-testing by one or more
     * pixels.
     */
    near_left = (int32_t) root_x < b.left + b.margin_left;
    near_right = (int32_t) root_x >= b.right - b.margin_right;
    near_top = (int32_t) root_y < b.top + b.margin_top;
    near_bottom = (int32_t) root_y >= b.bottom - b.margin_bottom;

    if (b.has_titlebar_row &&
            (int32_t) root_y >= b.titlebar_row_top &&
            (int32_t) root_y < b.titlebar_row_bottom) {
        near_left = false;
        near_right = false;
    }

    if (near_top && near_left) { zone = S_RESIZE_ZONE_NW; }
    else if (near_top && near_right) { zone = S_RESIZE_ZONE_NE; }
    else if (near_bottom && near_left) { zone = S_RESIZE_ZONE_SW; }
    else if (near_bottom && near_right) { zone = S_RESIZE_ZONE_SE; }
    else if (near_top) { zone = S_RESIZE_ZONE_N; }
    else if (near_bottom) { zone = S_RESIZE_ZONE_S; }
    else if (near_left) { zone = S_RESIZE_ZONE_W; }
    else if (near_right) { zone = S_RESIZE_ZONE_E; }
    else { zone = S_RESIZE_ZONE_NONE; }

    return zone;
}


/**
 * @brief Mirror sticky-client focus on the currently shown desktop
 *
 * If @a client is sticky and its owning desktop is not the desktop
 * currently shown on @a surface, marks @a client as active on that
 * currently shown desktop.  The affected desktop and surface are then
 * marked outdated so their focus-dependent state can be refreshed.
 *
 * @param surface       Surface whose current desktop is inspected
 * @param owner_desktop Desktop that owns @a client
 * @param client        Sticky client whose active state is synchronized
 *
 * @note Does nothing if any argument is @c NULL, if @a client is not
 *       sticky, if the current desktop cannot be resolved, or if the
 *       owner desktop is already current
 * @note Complexity: @e O(1)
 */
static void s_mouse_sync_sticky_active(surface_td *surface,
        desktop_td *owner_desktop, client_td *client)
{
    desktop_td *current_desktop;

    if (surface == NULL || owner_desktop == NULL || client == NULL) {
        return;
    }

    if (!client_is_sticky(client)) {
        return;
    }

    current_desktop = lookup_current_desktop(surface);
    if (current_desktop == NULL ||
            current_desktop->id == owner_desktop->id) {
        return;
    }

    current_desktop->client_active_id = client->id;
    current_desktop->focus_dirty = true;
    current_desktop->is_outdated = true;
    surface->is_outdated = true;
}


/**
 * @brief Resolve an event window to its associated managed client
 *
 * Attempts to find the client corresponding to a button event by first
 * checking the given event or child window directly.  If the event
 * occurred on a child window that is not explicitly managed, the
 * function walks up the X11 window hierarchy using @a xcb_query_tree
 * until it finds a parent window associated with a known client or
 * reaches the root.
 *
 * @param connection   Active XCB connection, or @c NULL to disable
 *                     hierarchy traversal
 * @param surfaces     List of managed client surfaces
 * @param event_window The window where the event was reported
 * @param child_window The child window under the pointer, or @c XCB_NONE
 * @param out_desktop  Output pointer for the client's desktop, or @c NULL
 *
 * @return Pointer to the resolved client, or @c NULL if no matching
 *         client is found
 *
 * @note Optionally returns the desktop containing the resolved client
 * @note Complexity: @e O(h), where @e h is the height of the window
 *       hierarchy
 */
static client_td *s_mouse_find_event_client(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t event_window,
        xcb_window_t child_window, desktop_td **out_desktop)
{
    client_td *client;
    xcb_window_t window;

    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }

    if (surfaces == NULL) {
        return NULL;
    }

    window = (child_window != XCB_NONE) ? child_window : event_window;
    client = lookup_find_client(surfaces, window, NULL, out_desktop);
    if (client != NULL || connection == NULL ||
            child_window == XCB_NONE) {
        return client;
    }

    window = child_window;
    while (client == NULL) {
        xcb_query_tree_cookie_t qt_c = xcb_query_tree(connection,
                window);
        xcb_query_tree_reply_t *qt_r =
            xcb_query_tree_reply(connection, qt_c, NULL);
        xcb_window_t parent;
        xcb_window_t root;

        if (qt_r == NULL) {
            break;
        }

        parent = qt_r->parent;
        root = qt_r->root;
        free(qt_r);

        if (parent == XCB_NONE || parent == root) {
            break;
        }

        window = parent;
        client = lookup_find_client(surfaces, window, NULL,
                out_desktop);
    }

    return client;
}


/* Create the eight border-resize cursors used for hover feedback */
void mouse_create_resize_cursors(xcb_connection_t *connection)
{
    xcb_screen_t *screen;
    util_cursor_ctx_td *ctx;

    if (connection == NULL ||
            s_resize_cursors[S_RESIZE_ZONE_NONE] != 0) {
        return;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    ctx = util_cursor_ctx_new(connection, screen);

    s_resize_cursors[S_RESIZE_ZONE_NONE] = util_cursor_load(ctx,
            "left_ptr", WM_CURSOR_LEFT_PTR_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_N] = util_cursor_load(ctx,
            "top_side", WM_CURSOR_TOP_SIDE_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_S] = util_cursor_load(ctx,
            "bottom_side", WM_CURSOR_BOTTOM_SIDE_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_E] = util_cursor_load(ctx,
            "right_side", WM_CURSOR_RIGHT_SIDE_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_W] = util_cursor_load(ctx,
            "left_side", WM_CURSOR_LEFT_SIDE_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_NE] = util_cursor_load(ctx,
            "top_right_corner", WM_CURSOR_TOP_RIGHT_CORNER_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_NW] = util_cursor_load(ctx,
            "top_left_corner", WM_CURSOR_TOP_LEFT_CORNER_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_SE] = util_cursor_load(ctx,
            "bottom_right_corner", WM_CURSOR_BOTTOM_RIGHT_CORNER_GLYPH);
    s_resize_cursors[S_RESIZE_ZONE_SW] = util_cursor_load(ctx,
            "bottom_left_corner", WM_CURSOR_BOTTOM_LEFT_CORNER_GLYPH);

    util_cursor_ctx_free(ctx);
    xcb_flush(connection);
}


/* Free the cursors created by 'mouse_create_resize_cursors' */
void mouse_destroy_resize_cursors(xcb_connection_t *connection)
{
    if (connection == NULL) {
        return;
    }

    for (int i = 0; i < (int) S_RESIZE_ZONE_COUNT; ++i) {
        if (s_resize_cursors[i] != 0) {
            xcb_free_cursor(connection, s_resize_cursors[i]);
            s_resize_cursors[i] = 0;
        }
    }
}


/* The plain-pointer cursor, the same one shown for 'S_RESIZE_ZONE_NONE' */
xcb_cursor_t mouse_plain_cursor(void)
{
    return s_resize_cursors[S_RESIZE_ZONE_NONE];
}


/**
 * @brief Recompute and apply the resize-border cursor for a client
 *        window at a given pointer position
 *
 * Shared by @c mouse_handle_motion_hover (every pointer motion),
 * @c mouse_handle_enter (every time the pointer crosses into a new
 * window), and @c mouse_hover_poll_tick (a periodic fallback poll;
 * see its own doc comment for why one is needed at all), since any
 * one kind of event or poll can be the only signal a given transition
 * actually produces: a client that selects @c PointerMotion for its
 * own purposes (common in GTK/Qt applications tracking hover for
 * their own UI) intercepts motion events before they propagate to
 * whichever window this logic is watching, leaving @c EnterNotify as
 * the only remaining signal for a decorated client (where hovering
 * the frame's own border and then crossing into the client's own
 * child window is what needs catching); an undecorated client has no
 * separate frame to fall back on at all, so moving from its border to
 * its interior happens within one single window with no crossing
 * whatsoever, leaving periodic polling as the only remaining option.
 *
 * @param connection XCB connection
 * @param surfaces   Every managed surface, to look up the client
 *                   @p window belongs to
 * @param window     Window the crossing, motion, or poll was
 *                   evaluated for
 * @param root_x     Pointer X position in root-window coordinates
 * @param root_y     Pointer Y position in root-window coordinates
 *
 * @return The resolved client @p window belongs to, or @c NULL if it
 *         does not belong to a resizable client
 *
 * @note Complexity: @e O(1)
 */
static client_td *s_mouse_update_resize_cursor(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t window, int16_t root_x,
        int16_t root_y)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    enum s_resize_zone_e zone;

    if (connection == NULL || surfaces == NULL ||
            s_resize_cursors[S_RESIZE_ZONE_NONE] == 0) {
        return NULL;
    }

    client = lookup_find_client(surfaces, window, &surface, &desktop);
    if (client == NULL || !client_is_resizable(client)) {
        LOGGER_TRACE("Resize cursor: window=0x%x root=%d,%d ->" \
                " no resizable client found (client=%p)",
                window, root_x, root_y, (void *) client);
        return NULL;
    }

    zone = s_mouse_resize_zone(client, root_x, root_y);

    LOGGER_TRACE("Resize cursor: window=0x%x (client->window=0x%x" \
            " frame=0x%x) root=%d,%d zone=%d cursor=0x%x",
            window, client->window, client->frame, root_x, root_y,
            (int) zone, s_resize_cursors[zone]);

    xcb_change_window_attributes(connection, window,
            XCB_CW_CURSOR,
            (const uint32_t[]) { s_resize_cursors[zone] });
    xcb_flush(connection);

    return client;
}


/**
 * @brief Undecorated client whose resize cursor @c mouse_hover_poll_tick
 *        should keep re-evaluating, or @c XCB_WINDOW_NONE for none
 *
 * Set by @c mouse_handle_enter, cleared by @c mouse_hover_poll_clear
 * (called from the @c LeaveNotify handler in loop.c) or the next
 * @c mouse_handle_enter into a window that does not itself warrant
 * tracking.  Tracked by window id rather than a @c client_td pointer
 * kept live across calls, so a client destroyed while still hovered
 * simply stops resolving in @c lookup_find_client on the next poll
 * rather than leaving a dangling pointer to clean up.
 */
static xcb_window_t s_hover_window = XCB_WINDOW_NONE;

/** Absolute time of the next scheduled poll for 's_hover_window' */
static struct timespec s_hover_next_poll;

/** How often 's_hover_window', while set, gets re-evaluated */
#define MOUSE_HOVER_POLL_INTERVAL_MS (100)


/**
 * @brief Schedule the next poll for 's_hover_window' to run
 *        @c MOUSE_HOVER_POLL_INTERVAL_MS from now
 *
 * @note Complexity: @e O(1)
 */
static void s_hover_reschedule(void)
{
    if (clock_gettime(CLOCK_MONOTONIC, &s_hover_next_poll) != 0) {
        return;
    }

    s_hover_next_poll.tv_nsec +=
        (long) MOUSE_HOVER_POLL_INTERVAL_MS * 1000000L;
    if (s_hover_next_poll.tv_nsec >= 1000000000L) {
        s_hover_next_poll.tv_sec += 1;
        s_hover_next_poll.tv_nsec -= 1000000000L;
    }
}


/* Clear the tracked hover window if it currently matches 'window' */
void mouse_hover_poll_clear(xcb_window_t window)
{
    if (window != XCB_WINDOW_NONE && window == s_hover_window) {
        s_hover_window = XCB_WINDOW_NONE;
    }
}


/* Milliseconds until 's_hover_window' should next be polled */
int mouse_hover_poll_ms_remaining(void)
{
    struct timespec now;
    long remaining_ms;

    if (s_hover_window == XCB_WINDOW_NONE) {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }

    remaining_ms =
        (long) (s_hover_next_poll.tv_sec - now.tv_sec) * 1000L +
        (s_hover_next_poll.tv_nsec - now.tv_nsec) / 1000000L;

    return (remaining_ms < 0) ? 0 : (int) remaining_ms;
}


/* Re-evaluate the resize cursor for 's_hover_window', if due */
void mouse_hover_poll_tick(xcb_connection_t *connection, list_td *surfaces)
{
    xcb_query_pointer_reply_t *reply;

    if (connection == NULL || surfaces == NULL ||
            s_hover_window == XCB_WINDOW_NONE ||
            mouse_hover_poll_ms_remaining() > 0) {
        return;
    }

    reply = xcb_query_pointer_reply(connection,
            xcb_query_pointer(connection, s_hover_window), NULL);
    if (reply != NULL) {
        if (reply->same_screen) {
            (void) s_mouse_update_resize_cursor(connection, surfaces,
                    s_hover_window, reply->root_x, reply->root_y);
        }
        free(reply);
    }

    s_hover_reschedule();
}


/* Update the pointer cursor to match a window's resize border */
void mouse_handle_motion_hover(xcb_connection_t *connection,
        list_td *surfaces, xcb_motion_notify_event_t *event)
{
    if (event == NULL) {
        return;
    }

    (void) s_mouse_update_resize_cursor(connection, surfaces,
            event->event, event->root_x, event->root_y);
}


/* Overlay dismissal                                                    */

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
 * @return @c true when an overlay was open and the event was consumed;
 *         the caller must return without further processing;
 *         @c false when no overlay was open (or only the popup was
 *         closed)
 */
static bool s_mouse_close_open_overlays(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    surface_td *surface;
    bool owns_event;

    /* Popup: close unconditionally on any click, then allow processing */
    if (popup_is_open()) {
        surface = lookup_surface_for_root(surfaces, event->root);
        popup_close(connection);
        if (surface != NULL) {
            surface_render_current_desktop_repaint(surface);
        }
        xcb_flush(connection);
        /* Do NOT consume: allow the click to proceed to the client */
        return false;
    }

    /* Quit-confirmation dialog */
    if (dialog_quit_is_open()) {
        if (event->event == dialog_quit_window() ||
                event->child == dialog_quit_window()) {
            (void) dialog_quit_handle_click(connection,
                    (int) event->event_x, (int) event->event_y);
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    /* Info dialog */
    if (dialog_info_is_open()) {
        if (event->event == dialog_info_window() ||
                event->child == dialog_info_window()) {
            dialog_info_handle_click(connection,
                    (int) event->event_x, (int) event->event_y);
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
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
                cycle_close(connection);
            }
        } else {
            cycle_close(connection);
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    /* Context menus: window menu, root menu, window list */
    if (wincmenu_is_open()) {
        surface = lookup_surface_for_root(surfaces, event->root);
        owns_event = wincmenu_owns_window(event->event);
        if (owns_event || wincmenu_owns_window(event->child)) {
            xcb_window_t mw = (owns_event) ? event->event : event->child;
            (void) wincmenu_handle_click(connection, surface, mw,
                    (int) event->root_x, (int) event->root_y, config);
        } else {
            wincmenu_close();
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    if (rootmenu_is_open()) {
        surface = lookup_surface_for_root(surfaces, event->root);
        owns_event = rootmenu_owns_window(event->event);
        if (owns_event || rootmenu_owns_window(event->child)) {
            xcb_window_t mw = (owns_event) ? event->event : event->child;
            (void) rootmenu_handle_click(connection, surface, mw,
                    (int) event->root_x, (int) event->root_y, config);
        } else {
            rootmenu_close();
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    if (winlist_is_open()) {
        surface = lookup_surface_for_root(surfaces, event->root);
        owns_event = winlist_owns_window(event->event);
        if (owns_event || winlist_owns_window(event->child)) {
            xcb_window_t mw = (owns_event) ? event->event : event->child;
            (void) winlist_handle_click(connection, surface, mw,
                    (int) event->root_x, (int) event->root_y, config);
        } else {
            winlist_close();
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return true;
    }

    return false;
}


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
        int32_t icon_x;
        int32_t icon_y;

        gc = xcb_get_geometry(connection, client->icon_window);
        gr = xcb_get_geometry_reply(connection, gc, NULL);
        icon_x = (gr != NULL)
            ? (int32_t) gr->x : (int32_t) client->icon_x;
        icon_y = (gr != NULL)
            ? (int32_t) gr->y : (int32_t) client->icon_y;
        if (gr != NULL) {
            free(gr);
        }

        drag_start_icon(connection, event->root, client,
                icon_x, icon_y,
                event->time, event->root_x, event->root_y);
    } else {
        /* Non-left-click: restore and focus */
        surface_td *surface;

        (void) client_send_event_restore(client);
        surface = lookup_surface_for_root(surfaces, event->root);
        if (surface != NULL && desktop != NULL) {
            focus_apply(surfaces, surface, desktop, client, true, config);
            s_mouse_sync_sticky_active(surface, desktop, client);
        }
    }

    s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER, event->time);
}


/* Scroll-binding handling (shade/unshade on titlebar, desktop switch) */

/**
 * @brief Handle a scroll-wheel event matched to a @c DESKTOP_PREV /
 *        @c DESKTOP_NEXT binding
 *
 * When the scroll is over a client's titlebar, @c DESKTOP_PREV shades
 * the window (and moves focus to the next client), while
 * @c DESKTOP_NEXT unshades it.  When the scroll is over the root or
 * over a client's content area, a desktop-switch event is queued.
 *
 * @param connection Active XCB connection
 * @param surfaces   Full surface list
 * @param event      Incoming button-press event
 * @param client     Client under the pointer, or @c NULL
 * @param desktop    Desktop owning @p client, or @c NULL
 * @param type       @c MOUSEBIND_DESKTOP_PREV or @c MOUSEBIND_DESKTOP_NEXT
 * @param config     Active configuration
 */
static void s_mouse_handle_scroll_binding(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop,
        enum wm_mousebind_type_e type, const config_td *config)
{
    surface_td *surface = lookup_surface_for_root(surfaces, event->root);

    if (client != NULL) {
        bool on_titlebar = false;

        /* Detect whether the scroll landed on the titlebar via both
         * the child-window identity and a Y-range check (to handle
         * frame sync-grab events where 'event->child' may be the
         * content window). */
        if (client->titlebar != 0) {
            int32_t bw = client->layout.frame_extents.left;
            int32_t fy = client->layout.geometry.cur.pos.y;
            int32_t ty0 = fy + bw;
            int32_t ty1 = fy + client->layout.frame_extents.top;
            int32_t ry = (int32_t) event->root_y;

            if (event->child == client->titlebar ||
                    (ry >= ty0 && ry < ty1)) {
                on_titlebar = true;
            }
        }

        if (on_titlebar) {
            if (type == MOUSEBIND_DESKTOP_PREV) {
                /* Scroll-up on titlebar: shade and transfer focus */
                if (!client_is_shaded(client)) {
                    cdlist_item_td *node = NULL;
                    client_td *prev_c = NULL;

                    wcmd_client_shade(client);

                    if (desktop != NULL && surface != NULL) {
                        if (desktop->stacking != NULL) {
                            cdlist_item_td *tail =
                                cdlist_tail(desktop->stacking);
                            if (tail != NULL) {
                                node = cdlist_prev(tail);
                            }
                        }

                        while (node != NULL &&
                                node != cdlist_tail(desktop->stacking)) {
                            client_td *c =
                                (client_td *) cdlist_data(node);
                            if (c != NULL && c != client &&
                                    client_is_focusable(c) &&
                                    !client_is_iconified(c)) {
                                prev_c = c;
                                break;
                            }
                            node = cdlist_prev(node);
                        }

                        if (prev_c != NULL) {
                            focus_apply(surfaces, surface, desktop,
                                    prev_c, false, config);
                            s_mouse_sync_sticky_active(surface,
                                    desktop, prev_c);
                        } else {
                            (void) client_send_event_unfocus(client);
                            desktop->client_active_id = 0;
                            desktop->focus_dirty = true;
                        }

                        desktop->is_outdated = true;
                        surface->is_outdated = true;
                    }
                }
            } else { /* MOUSEBIND_DESKTOP_NEXT */
                /* Scroll-down on titlebar: unshade and focus */
                if (client_is_shaded(client)) {
                    wcmd_client_unshade(client);

                    if (surface != NULL && desktop != NULL) {
                        focus_apply(surfaces, surface, desktop,
                                client, false, config);
                        s_mouse_sync_sticky_active(surface,
                                desktop, client);
                        desktop->is_outdated = true;
                        surface->is_outdated = true;
                    }
                }
            }

            s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                    event->time);
            return;
        }

        /* Scroll over client content area: replay so the application
         * receives the scroll event */
        s_allow_and_flush(connection, XCB_ALLOW_REPLAY_POINTER,
                event->time);
        return;
    }

    /* No client under pointer: queue a desktop-switch action */
    if (surface != NULL) {
        event_td *ev;
        action_td action;
        action.type = ACTION_TYPE_SURFACE;
        action.object.surface = (type == MOUSEBIND_DESKTOP_NEXT)
            ? ACTION_SURFACE_DESKTOP_SWITCH_NEXT
            : ACTION_SURFACE_DESKTOP_SWITCH_PREV;
        ev = event_init((void *) surface, NULL, action, PRIORITY_NORMAL);
        if (ev != NULL) { eventq_add(ev); }
    }

    s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER, event->time);
}


/* Titlebar button hit-test */

/**
 * @brief Look up which titlebar button, if any, a client's own button
 *        list has at a given frame-relative X position
 */
static bool s_titlebar_button_at(
        const struct titlebar_button_layout_s *entries, uint8_t count,
        int16_t x, enum config_titlebar_button_e *out)
{
    for (uint8_t i = 0u; i < count; ++i) {
        if (x >= entries[i].x &&
                x < entries[i].x + (int16_t) WM_DECOR_BTN_SIZE) {
            *out = entries[i].button;
            return true;
        }
    }

    return false;
}


/**
 * @brief Dispatch the action a titlebar button click should trigger
 *
 * @param button       Which button was clicked
 * @param client       Client whose titlebar was clicked
 * @param can_maximize Whether maximize/fullscreen are currently enabled
 * @param event        Incoming button-press event (button 1/2/3 select
 *                      full/vertical/horizontal maximize respectively)
 */
static void s_titlebar_button_action(enum config_titlebar_button_e button,
        client_td *client, bool can_maximize,
        const xcb_button_press_event_t *event)
{
    switch (button) {
        case CONFIG_TITLEBAR_BUTTON_PIN:
            client_send_event(client, ACTION_CLIENT_TOGGLE_STICKY,
                    PRIORITY_NORMAL);
            break;

        case CONFIG_TITLEBAR_BUTTON_LAYER:
            client_send_event(client, ACTION_CLIENT_CYCLE_LAYER,
                    PRIORITY_NORMAL);
            break;

        case CONFIG_TITLEBAR_BUTTON_ICONIZE:
            client_send_event(client, ACTION_CLIENT_ICONIFY,
                    PRIORITY_NORMAL);
            break;

        case CONFIG_TITLEBAR_BUTTON_HIDE:
            client_send_event(client, ACTION_CLIENT_HIDE,
                    PRIORITY_NORMAL);
            break;

        case CONFIG_TITLEBAR_BUTTON_SHADE:
            client_send_event(client, ACTION_CLIENT_TOGGLE_SHADE,
                    PRIORITY_NORMAL);
            break;

        case CONFIG_TITLEBAR_BUTTON_MAXIMIZE:
            if (!can_maximize) {
                break;
            }
            if ((xcb_button_index_t) event->detail ==
                    XCB_BUTTON_INDEX_2) {
                client_send_event(client, ACTION_CLIENT_MAXIMIZE_VERT,
                        PRIORITY_NORMAL);
            } else if ((xcb_button_index_t) event->detail ==
                    XCB_BUTTON_INDEX_3) {
                client_send_event(client, ACTION_CLIENT_MAXIMIZE_HORZ,
                        PRIORITY_NORMAL);
            } else {
                client_send_event(client, ACTION_CLIENT_MAXIMIZE,
                        PRIORITY_NORMAL);
            }
            break;

        case CONFIG_TITLEBAR_BUTTON_FULLSCREEN:
            if (!can_maximize) {
                break;
            }
            client_send_event(client, ACTION_CLIENT_TOGGLE_FULLSCREEN,
                    PRIORITY_NORMAL);
            break;

        case CONFIG_TITLEBAR_BUTTON_CLOSE:
            client_send_event(client, ACTION_CLIENT_CLOSE,
                    PRIORITY_NORMAL);
            break;
    }
}


/**
 * @brief Test whether a click on the titlebar landed on a configured
 *        button and dispatch its action
 *
 * Uses @c client_titlebar_layout to find each button's position, the
 * exact same computation @c desktop_draw_titlebar_buttons uses to
 * paint them, so a click can never land "between" where a button
 * looks like it is and where this function thinks it is.  If the
 * click lands on a button its action is dispatched and the function
 * returns @c true.  Scroll-wheel events (buttons 4 and 5) on the
 * titlebar area also count as a hit and are handled here.
 *
 * @param connection Active XCB connection (unused directly but kept for
 *                   symmetry)
 * @param client     The client whose titlebar was clicked
 * @param desktop    The desktop that owns @p client
 * @param surface    Current surface
 * @param event      Incoming button-press event
 *
 * @return @c true when the click was consumed by a button
 */
static bool s_mouse_hit_titlebar_buttons(xcb_connection_t *connection,
        client_td *client, desktop_td *desktop, surface_td *surface,
        xcb_button_press_event_t *event)
{
    struct titlebar_button_layout_s left[CONFIG_MAX_TITLEBAR_BUTTONS];
    struct titlebar_button_layout_s right[CONFIG_MAX_TITLEBAR_BUTTONS];
    uint8_t left_n;
    uint8_t right_n;
    int16_t title_x;
    uint16_t title_w;
    int16_t btn_y;
    int ex = (int) event->event_x;
    int ey = (int) event->event_y;
    int left_extent = (int) client->layout.frame_extents.left;
    int right_extent = (int) client->layout.frame_extents.right;
    int frame_w = (int) client->layout.geometry.cur.dim.w;
    int fw = (frame_w > left_extent + right_extent)
        ? frame_w - left_extent - right_extent : 1;
    int title_h = (int) client->title_height;
    bool can_maximize;
    enum config_titlebar_button_e button;

    (void) connection;
    (void) title_x;
    (void) title_w;

    if (client->theme == NULL) {
        return false;
    }

    can_maximize = !client_is_fullscreen(client) &&
        (bool) client_is_resizable(client);

    /* Same layout the render pass just painted from, computed first
     * (not just when the click Y already looks close) since it is what
     * determines 'btn_y' now that button rows can be vertically inset
     * by 'padding.vertical', not just centered in the full titlebar
     * height. */
    client_titlebar_layout(client->theme, (uint16_t) fw, (uint16_t) title_h,
            left, &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    /* Only test buttons when the click Y is within the button row */
    if (ey >= btn_y && ey < btn_y + (int) WM_DECOR_BTN_SIZE) {
        if (s_titlebar_button_at(left, left_n, (int16_t) ex, &button) ||
                s_titlebar_button_at(right, right_n, (int16_t) ex,
                    &button)) {
            s_titlebar_button_action(button, client, can_maximize, event);
            if (desktop != NULL) { desktop->is_outdated = true; }
            if (surface != NULL) { surface->is_outdated = true; }
            return true;
        }
    }

    /* Scroll wheel on the titlebar body: shade / unshade */
    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_4) {
        if (!client_is_shaded(client)) {
            client_send_event(client, ACTION_CLIENT_SHADE,
                    PRIORITY_NORMAL);
            if (desktop != NULL) { desktop->is_outdated = true; }
            if (surface != NULL) { surface->is_outdated = true; }
        }
        return true;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_5) {
        if (client_is_shaded(client)) {
            client_send_event(client, ACTION_CLIENT_UNSHADE,
                    PRIORITY_NORMAL);
            if (desktop != NULL) { desktop->is_outdated = true; }
            if (surface != NULL) { surface->is_outdated = true; }
        }
        return true;
    }

    return false;
}


/* Titlebar interaction (buttons + drag + double-click) */

/**
 * @brief Handle a click on the client titlebar
 *
 * Delegates to @c s_mouse_hit_titlebar_buttons first.
 * If no button was hit:
 * - Left-click starts a move drag, or toggles shade on double-click.
 * - Right-click opens the window context menu.
 *
 * @param connection Active XCB connection
 * @param surfaces   Full surface list (for context menu)
 * @param event      Incoming button-press event
 * @param client     Client whose titlebar was clicked
 * @param desktop    Desktop owning @p client
 * @param surface    Current surface
 * @param config     Active configuration
 */
static void s_mouse_handle_titlebar(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop, surface_td *surface,
        const config_td *config)
{
    bool hit_btn;

    hit_btn = s_mouse_hit_titlebar_buttons(connection, client, desktop,
            surface, event);

    if (!hit_btn &&
            (xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_1) {
        xcb_timestamp_t dt = event->time - s_last_titlebar_press_time;
        xcb_window_t prev_win = s_last_titlebar_press_win;

        s_last_titlebar_press_time = event->time;
        s_last_titlebar_press_win = client->titlebar;

        if (prev_win == client->titlebar &&
                dt <= (xcb_timestamp_t) WM_DOUBLE_CLICK_MS) {
            /* Double-click: toggle shade */
            s_last_titlebar_press_time = 0;
            s_last_titlebar_press_win = XCB_NONE;
            client_send_event(client, ACTION_CLIENT_TOGGLE_SHADE,
                    PRIORITY_NORMAL);
            if (desktop != NULL) { desktop->is_outdated = true; }
            if (surface != NULL) { surface->is_outdated = true; }
        } else {
            /* Single left-click: start move drag */
            if (!client_is_maximized(client) &&
                    !client_is_fullscreen(client)) {
                drag_start(connection, event->root, client, desktop,
                        CLIENT_OPERATION_MOVING,
                        event->time,
                        event->root_x, event->root_y,
                        (surface != NULL)
                            ? surface->properties.dim.w : 0u,
                        (surface != NULL)
                            ? surface->properties.dim.h : 0u,
                        (config != NULL)
                            ? config->base.windows.snap : 0u);
            }
        }
    }

    /* Right-click on titlebar drag area (no button hit): window menu */
    if (!hit_btn &&
            (xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_3) {
        if (surface != NULL && desktop != NULL) {
            wincmenu_show(connection, surface, desktop, client,
                    (int16_t) event->root_x,
                    (int16_t) event->root_y, config);
        }
    }

    (void) surfaces;
}


/* Border resize */

/**
 * @brief Test whether a button press should initiate a resize drag
 *
 * Returns @c true when the client is resizable, not in a fixed-size
 * state (fullscreen or maximized), and the click landed on the frame
 * border or near the edge of an undecorated window.
 *
 * @param client Client to test
 * @param window The X window that received the event
 * @param event  Incoming button-press event
 *
 * @return @c true if a resize drag should begin
 */
static bool s_mouse_can_resize_client(const client_td *client,
        xcb_window_t window, const xcb_button_press_event_t *event)
{
    if (!client_is_resizable(client)) {
        return false;
    }

    if (client->properties.state ==
                (uint16_t) CLIENT_STATE_FULLSCREEN ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_VERT ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ) {
        return false;
    }

    /* Decorated: click anywhere on the frame border */
    if (client->frame != 0 && window == client->frame) {
        return true;
    }

    /* Undecorated: click within the resize-grab margin of any edge
     * (see 's_mouse_near_edge' and 'im_resize_bounds') */
    if (client->frame == 0 && window == client->window &&
            s_mouse_near_edge(client, event->root_x, event->root_y)) {
        return true;
    }

    return false;
}


/**
 * @brief Start a resize drag when the conditions are satisfied
 *
 * Unshades the client if shaded, resolves screen dimensions, and calls
 * @c drag_start with @c CLIENT_OPERATION_RESIZING.
 *
 * @param connection Active XCB connection
 * @param surfaces   Surface list (to resolve screen size)
 * @param event      Incoming button-press event
 * @param client     Client to resize
 * @param desktop    Desktop owning @p client
 * @param config     Active configuration (for snap distance)
 */
static void s_mouse_start_border_resize(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop,
        const config_td *config)
{
    surface_td *surface;
    uint32_t screen_w;
    uint32_t screen_h;

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    screen_w = (surface != NULL) ? surface->properties.dim.w : 0u;
    screen_h = (surface != NULL) ? surface->properties.dim.h : 0u;

    drag_start(connection, event->root, client, desktop,
            CLIENT_OPERATION_RESIZING,
            event->time, event->root_x, event->root_y,
            screen_w, screen_h, config->base.windows.snap);

    s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
            event->time);
}


/* Root window click */

/**
 * @brief Handle a button press on the root (desktop) window
 *
 * Right-click opens the root desktop menu; middle-click opens the
 * window list.
 *
 * @param connection Active XCB connection
 * @param surfaces   Surface list
 * @param event      Incoming button-press event
 * @param config     Active configuration
 */
static void s_mouse_handle_root_press(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    surface_td *surface = lookup_surface_for_root(surfaces, event->root);

    if (surface == NULL) {
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_3) {
        rootmenu_show(connection, surface,
                (int16_t) event->root_x, (int16_t) event->root_y,
                config, wm_get_config_dir());
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_1) {
        /* Left-click on the empty desktop: unfocus the active client so
         * all windows lose their selection highlight */
        desktop_td *desktop =
            surface_desktop_get(surface, surface->desktop_cur);

        if (desktop != NULL && desktop->client_active_id != 0) {
            client_td *active = lookup_find_client(surfaces,
                    desktop->client_active_id, NULL, NULL);
            if (active != NULL) {
                (void) client_send_event_unfocus(active);
            }
            desktop->client_active_id = 0;
            desktop->focus_dirty = true;
            desktop->is_outdated = true;
            surface->is_outdated = true;
        }
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_2) {
        winlist_show(connection, surface,
                (int16_t) event->root_x, (int16_t) event->root_y,
                config);
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
    }
}


/* Public event handlers */

/* Dispatch a button-press event */
void mouse_handle_press(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    xcb_window_t window;
    client_td *client;
    desktop_td *desktop = NULL;
    surface_td *surface = NULL;
    uint16_t state;
    enum wm_mousebind_type_e type = MOUSEBIND_NONE;

    if (connection == NULL || event == NULL || config == NULL) {
        return;
    }

    /* Step 1: dismiss any open overlay; return if the event was
     * consumed */
    if (s_mouse_close_open_overlays(connection, surfaces,
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
    state = (uint16_t) ((unsigned int) event->state &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    for (int i = 0; i < mousebind_count(); ++i) {
        xcb_button_index_t btn;
        uint16_t req;
        enum wm_mousebind_type_e t = mousebind_at(i, &btn, &req);

        if (t == MOUSEBIND_NONE) {
            continue;
        }

        if ((xcb_button_index_t) event->detail == btn &&
                (req == 0 || (state & req) == req)) {
            type = t;
            break;
        }
    }

    /* Step 5: scroll bindings (desktop prev/next / titlebar shade) */
    if (type == MOUSEBIND_DESKTOP_NEXT ||
            type == MOUSEBIND_DESKTOP_PREV) {
        s_mouse_handle_scroll_binding(connection, surfaces, event,
                client, desktop, type, config);
        return;
    }

    /* Step 6: no configured binding: handle frame/titlebar/root clicks */
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
                if (surface != NULL && desktop != NULL) {
                    wincmenu_show(connection, surface, desktop, client,
                            (int16_t) event->root_x,
                            (int16_t) event->root_y, config);
                }
                s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                        event->time);
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
                    s_mouse_near_edge(client, event->root_x,
                        event->root_y)) {
                if (surface != NULL && desktop != NULL) {
                    wincmenu_show(connection, surface, desktop, client,
                            (int16_t) event->root_x,
                            (int16_t) event->root_y, config);
                }
                s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                        event->time);
                return;
            }

            /* Titlebar click */
            if (event->child == client->titlebar &&
                    client->titlebar != 0) {
                s_mouse_handle_titlebar(connection, surfaces, event,
                        client, desktop, surface, config);
            }

            /* Border resize (decorated or undecorated) */
            if (s_mouse_can_resize_client(client, window, event)) {
                s_mouse_start_border_resize(connection, surfaces, event,
                        client, desktop, config);
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
                s_mouse_handle_root_press(connection, surfaces, event,
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
    window = (event->child != XCB_NONE) ? event->child : event->event;
    client = s_mouse_find_event_client(connection, surfaces,
            event->event, event->child, &desktop);

    if (client == NULL) {
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if (type == MOUSEBIND_RESIZE &&
            (!client_is_resizable(client) ||
             client->properties.state ==
                 (uint16_t) CLIENT_STATE_FULLSCREEN ||
             client->properties.state ==
                 (uint16_t) CLIENT_STATE_MAXIMIZED)) {
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    if (type == MOUSEBIND_RESIZE && client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    if (type == MOUSEBIND_LOWER) {
        (void) client_send_event_lower(client);
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface != NULL && desktop != NULL) {
        focus_apply(surfaces, surface, desktop, client, true, config);
        s_mouse_sync_sticky_active(surface, desktop, client);
    }

    if (type == MOUSEBIND_MOVE &&
            (client_is_maximized(client) ||
             client_is_fullscreen(client))) {
        s_allow_and_flush(connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        return;
    }

    {
        uint32_t screen_w = (surface != NULL)
            ? surface->properties.dim.w : 0u;
        uint32_t screen_h = (surface != NULL)
            ? surface->properties.dim.h : 0u;

        drag_start(connection, event->root, client, desktop,
                (type == MOUSEBIND_MOVE)
                    ? CLIENT_OPERATION_MOVING
                    : CLIENT_OPERATION_RESIZING,
                event->time,
                event->root_x, event->root_y,
                screen_w, screen_h,
                config->base.windows.snap);
    }
}


/* Handle a button-release event to end a drag */
void mouse_handle_release(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_release_event_t *event,
        const config_td *config)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    int16_t root_x = 0;
    int16_t root_y = 0;

    (void) config;

    if (!drag_is_active()) {
        return;
    }

    if (event != NULL) {
        root_x = event->root_x;
        root_y = event->root_y;
    }

    client = drag_client();
    if (client != NULL && surfaces != NULL) {
        (void) lookup_find_client(surfaces, client->id,
                &surface, &desktop);
    }

    drag_end(connection, surface, desktop, root_x, root_y);
}


/* Re-evaluate the resize cursor, then apply focus-follows-mouse, on
 * an enter-notify event */
void mouse_handle_enter(xcb_connection_t *connection,
        list_td *surfaces, xcb_enter_notify_event_t *event,
        const config_td *config)
{
    client_td *client;
    client_td *entered;
    desktop_td *desktop;
    surface_td *surface;

    if (connection == NULL || event == NULL || config == NULL) {
        return;
    }

    LOGGER_TRACE("Enter notify: event->event=0x%x child=0x%x" \
            " root=%d,%d mode=%u detail=%u",
            event->event, event->child, event->root_x, event->root_y,
            event->mode, event->detail);

    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        return;
    }

    /* Independent of focus-follows-mouse below: a resizable client
     * that selects 'PointerMotion' for its own purposes (common in
     * GTK/Qt applications tracking hover for their own UI) intercepts
     * motion events at the X11 propagation level before they ever
     * reach 'mouse_handle_motion_hover', so the cursor set while
     * hovering this client's own border never gets re-evaluated once
     * the pointer moves on into that client's content area; this
     * 'EnterNotify', unlike motion, still fires reliably since it was
     * selected directly on this client's own window (see client.c),
     * giving the resize-cursor logic a second, independent chance to
     * catch what motion alone might have missed. */
    entered = s_mouse_update_resize_cursor(connection, surfaces,
            event->event, event->root_x, event->root_y);

    /* An undecorated client has no separate frame window to fall
     * back on at all: moving from its border to its interior (or
     * back) happens entirely within this one same window, with
     * no crossing whatsoever for any further 'EnterNotify' to
     * catch, and its own 'PointerMotion' may be just as
     * intercepted as any other client's; only a periodic poll
     * (see 'mouse_hover_poll_tick') can still catch that
     * transition, so track it for one here. */
    s_hover_window = (entered != NULL && entered->frame == 0)
        ? event->event : XCB_WINDOW_NONE;
    if (s_hover_window != XCB_WINDOW_NONE) {
        s_hover_reschedule();
    }

    if (event->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
        return;
    }

    if (!focus_is_follow_mouse(config)) {
        return;
    }

    client = lookup_find_client(surfaces, event->event, NULL,
            &desktop);
    if (client == NULL) {
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->root);
    if (surface == NULL || desktop == NULL) {
        return;
    }

    s_enter_focus_active = true;
    focus_apply(surfaces, surface, desktop, client, false, config);

    xcb_flush(connection);
}


/* Query whether a hover-triggered focus transfer is in progress */
bool mouse_enter_focus_is_active(void)
{
    return s_enter_focus_active;
}


/* Clear the hover-triggered focus flag */
void mouse_enter_focus_clear(void)
{
    s_enter_focus_active = false;
}
