/**
 * @file input/mouse/event/titlebar.c
 *
 * @brief Button presses landing on a client's titlebar
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Policy includes */
#include <policy/focus.h>

/* Command includes */
#include <cmds/client/state.h>
#include <cmds/client/visibility.h>

/* Menu includes */
#include <menu/context/wincmenu.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <render/outdate.h>
#include <surface.h>
#include <wm.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/input.h>

/* Local includes */
#include <input/mouse/drag.h>
#include <input/mouse/event.h>
#include <input/mouse/internal.h>

/* State for double-click detection on titlebars.  A double-click on the
 * titlebar drag area (i.e., not on a button) toggles shade/unshade. */
static xcb_timestamp_t s_last_titlebar_press_time = 0;
static xcb_window_t s_last_titlebar_press_win = XCB_NONE;


/**
 * @brief Mark a client, its desktop, and its surface as
 *        outdated together
 *
 * Shared by every titlebar-click and scroll case in
 * @c s_mouse_hit_titlebar_buttons that changes the client's state and
 * needs the next render pass to pick it up.
 *
 * @param client  Client whose visual state just changed, or
 *                @c NULL to skip
 * @param desktop Desktop to mark outdated, or @c NULL to skip
 * @param surface Surface to mark outdated, or @c NULL to skip
 *
 * @note Complexity: @e O(1)
 */
static void s_mark_outdated(client_td *client, desktop_td *desktop,
        surface_td *surface)
{
    wm_outdate_client(client);
    if (desktop != NULL) { desktop->is_outdated = true; }
    if (surface != NULL) { surface->is_outdated = true; }
}


/**
 * @brief Look up which titlebar button, if any, a client's button
 *        list has at a given frame-relative X position
 */
static bool s_titlebar_button_at(
        const struct titlebar_button_layout_s *entries, uint8_t count,
        int16_t x, enum config_titlebar_button_e *out)
{
    for (uint8_t i = 0u; i < count; ++i) {
        /* Measured as a distance from the button's left edge:
         * comparing against a sum lets the optimizer assume that sum
         * never overflows */
        if (x >= entries[i].x &&
                (unsigned int) (x - entries[i].x) <
                    (unsigned int) WM_DECOR_BTN_SIZE) {
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
 *                     full/vertical/horizontal maximize respectively)
 */
static void s_titlebar_button_action(enum config_titlebar_button_e button,
        client_td *client, bool can_maximize,
        const xcb_button_press_event_t *event)
{
    switch (button) {
        case CONFIG_TITLEBAR_BUTTON_PIN:
            enact_client_toggle_pin(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_LAYER:
            enact_client_cycle_layer(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_ICONIZE:
            enact_client_iconify(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_HIDE:
            enact_client_hide(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_SHADE:
            enact_client_toggle_shade(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_MAXIMIZE:
            if (!can_maximize) {
                break;
            }
            if ((xcb_button_index_t) event->detail ==
                    XCB_BUTTON_INDEX_2) {
                enact_client_maximize_vert(client);
            } else if ((xcb_button_index_t) event->detail ==
                    XCB_BUTTON_INDEX_3) {
                enact_client_maximize_horz(client);
            } else {
                enact_client_maximize(client);
            }
            break;

        case CONFIG_TITLEBAR_BUTTON_FULLSCREEN:
            if (!can_maximize) {
                break;
            }
            enact_client_toggle_fullscreen(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_CLOSE:
            enact_client_close(client);
            break;

        case CONFIG_TITLEBAR_BUTTON_STICKY:
            enact_client_toggle_stick(client);
            break;
    }
}


/**
 * @brief Test whether a click on the titlebar landed on a configured
 *        button and dispatch its action
 *
 * Uses @c client_titlebar_layout to find each button's position, the
 * exact same computation @c desktop_titlebar_buttons_draw uses to paint
 * them, so a click can never land "between" where a button looks like
 * it is and where this function thinks it is.  If the click lands on
 * a button its action is dispatched and the function returns @c true.
 * Scroll-wheel events (buttons 4 and 5) on the titlebar area also count
 * as a hit and are handled here.
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
    int top_extent = (int) client->layout.frame_extents.top;
    int frame_w = (int) client->layout.geometry.cur.dim.w;
    int fw = (frame_w > left_extent + right_extent)
        ? frame_w - left_extent - right_extent : 1;
    int title_h = (int) client->title_height;
    int title_y = (top_extent > title_h)
        ? top_extent - title_h : 0;
    bool can_maximize;
    bool hide_pin;
    bool hide_sticky;
    enum config_titlebar_button_e button;

    (void) connection;
    (void) title_x;
    (void) title_w;

    if (client->config == NULL) {
        return false;
    }

    /* Per the X11 protocol, 'event_x'/'event_y' are always relative
     * to the origin of 'event->event' (here, 'client->frame', the
     * window this whole button-press grab was established on) never to
     * 'event->child' ('client->titlebar', the window the click actually
     * landed in), regardless of which one the click hit.
     *
     * Every button position 'client_titlebar_layout' computes below is
     * relative to the titlebar's origin instead, the same origin
     * the titlebar's physical window is created and kept synced at,
     * '(left, title_y)', within the frame, both times
     * ('ci_create_decorations' and 'client_decoration_layout_sync',
     * both client/geom.c). Left unconverted, comparing a frame-relative
     * click straight against titlebar-relative button positions is off
     * by exactly that offset on both axes, '(left, title_y)',
     * imperceptible at the traditional 1px border this bug shipped with
     * for years, severe with a large one, since the offset grows with
     * it. */
    ex -= left_extent;
    ey -= title_y;

    can_maximize = !client_is_fullscreen(client) &&
        (bool) client_is_maximizable(client);
    hide_pin = surface != NULL && surface->desktop_count <= 1u;

    /* Same idea as 'hide_pin' above, gated on the pannable viewport
     * size instead of the desktop count: a sticky client stays put
     * across a viewport pan, so the button is pointless on a surface
     * whose viewport is not even wide enough or tall enough to pan
     * across, and a missing 'surface'/'config' answers the same as
     * a genuinely 1x1 one, hiding the button rather than guessing. */
    hide_sticky = !surface_viewport_has_room(surface);

    /* Same layout the render pass just painted from, computed first
     * (not just when the click Y already looks close) since it is what
     * determines 'btn_y' now that button rows can be vertically inset
     * by 'padding.vertical', not just centered in the full titlebar
     * height. */
    client_titlebar_layout(&client->config->theme, (uint16_t) fw,
            (uint16_t) title_h, hide_pin, hide_sticky,
            left, &left_n, right, &right_n, &title_x, &title_w, &btn_y);

    /* Only test buttons when the click Y is within the button row */
    if (ey >= btn_y && (unsigned int) (ey - btn_y) <
            (unsigned int) WM_DECOR_BTN_SIZE) {
        if (s_titlebar_button_at(left, left_n, (int16_t) ex, &button) ||
                s_titlebar_button_at(right, right_n,
                    (int16_t) ex, &button)) {
            s_titlebar_button_action(button, client,
                    can_maximize, event);
            s_mark_outdated(client, desktop, surface);
            return true;
        }
    }

    /* Scroll wheel on the titlebar body: shade / unshade */
    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_4) {
        if (!client_is_shaded(client)) {
            enact_client_shade(client);
            s_mark_outdated(client, desktop, surface);
        }
        return true;
    }

    if ((xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_5) {
        if (client_is_shaded(client)) {
            enact_client_unshade(client);
            s_mark_outdated(client, desktop, surface);
        }
        return true;
    }

    return false;
}


/* Titlebar interaction (buttons + drag + double-click) */

/**
 * @brief Handle a click on the client titlebar
 *
 * Delegates to @a s_mouse_hit_titlebar_buttons first.
 * If no button was hit:
 *
 * - Left-click starts a move drag, or toggles shade on double-click.
 * - Right-click opens the window context menu.
 *
 * A single left-click that starts a move drag hands the pointer off
 * to an active grab of its own (see @c drag_start); the caller must
 * return immediately once that happens instead of falling through to
 * its own border-resize check and @c xcb_allow_events call, the same
 * way the alt-click move binding in @c mouse_handle_press does.  Every
 * other outcome here (a button hit, the double-click shade toggle,
 * lower, the context menu, or a no-op click on a maximized/fullscreen
 * client) never grabs the pointer, so the caller's usual fallthrough
 * is exactly what those still need.
 *
 * @param connection Active XCB connection
 * @param surfaces   Full surface list (for context menu)
 * @param event      Incoming button-press event
 * @param client     Client whose titlebar was clicked
 * @param desktop    Desktop owning @p client
 * @param surface    Current surface
 * @param config     Active configuration
 *
 * @return @c true when the click just started a move drag, @c false
 *         otherwise
 */
bool im_press_titlebar(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        client_td *client, desktop_td *desktop, surface_td *surface,
        const config_td *config)
{
    bool hit_btn;
    bool drag_started = false;

    hit_btn = s_mouse_hit_titlebar_buttons(connection, client, desktop,
            surface, event);

    if (!hit_btn &&
            (xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_1) {
        xcb_timestamp_t dt = event->time - s_last_titlebar_press_time;
        xcb_window_t prev_win = s_last_titlebar_press_win;

        s_last_titlebar_press_time = event->time;
        s_last_titlebar_press_win = client->titlebar;

        if (prev_win == client->titlebar &&
                dt <= (xcb_timestamp_t) ((config != NULL)
                    ? config->a11y.interaction.double_click_ms
                    : WM_DOUBLE_CLICK_MS)) {
            /* Double-click: toggle shade */
            s_last_titlebar_press_time = 0;
            s_last_titlebar_press_win = XCB_NONE;
            enact_client_toggle_shade(client);
            s_mark_outdated(client, desktop, surface);
        } else {
            /* Single left-click: start move drag */
            if (!client_is_maximized(client) &&
                    !client_is_fullscreen(client)) {
                struct position_s root_pos;
                struct dimensions_s screen_dim;

                root_pos.x = event->root_x;
                root_pos.y = event->root_y;
                screen_dim.w = (surface != NULL)
                    ? surface->properties.dim.w : 0u;
                screen_dim.h = (surface != NULL)
                    ? surface->properties.dim.h : 0u;
                drag_start(connection, event->root, client, desktop,
                        CLIENT_OPERATION_MOVING,
                        event->time,
                        root_pos, screen_dim);
                drag_started = true;
            }
        }
    }

    /* Middle-click on the titlebar drag area sends the window to the
     * back.  It only ever sends down, since an ordinary click on
     * whatever it went behind is how it comes back, and one button
     * that hid or revealed a window depending on where it happened to
     * sit would be the harder one to aim */
    if (!hit_btn &&
            (xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_2) {
        enact_client_lower(client);
        s_mark_outdated(client, desktop, surface);
    }

    /* Right-click on titlebar drag area (no button hit): window menu */
    if (!hit_btn &&
            (xcb_button_index_t) event->detail == XCB_BUTTON_INDEX_3) {
        if (surface != NULL && desktop != NULL) {
            wincmenu_show(connection, surface, desktop, client,
                    (struct position_s) { event->root_x, event->root_y },
                    config);
        }
    }

    (void) surfaces;

    return drag_started;
}
