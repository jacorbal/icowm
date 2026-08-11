/**
 * @file cmds/ccmd.c
 *
 * @brief Implementation on executions over clients using the XCB
 *        interface while updating EWMH and ICCCM hints
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
#include <stdlib.h>     /* NULL, free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/icon.h>

/* Windows & icons policy includes */
#include <policy/placement.h>

/* Utils includes */
#include <utils/geom.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <lookup.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <cmds/ccmd.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/meta.h>
#include <cmds/state.h>
#include <cmds/util.h>


/**
 * @brief Transfer focus away from a client that is leaving the current
 *        visible focus chain
 *
 * Picks the most recently used visible focusable client on the current
 * desktop and focuses it.  If no such client exists, focus is released
 * to the pointer root so global grabs continue working.
 *
 * @param client Client that is being hidden or iconified
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       current desktop
 */
static void s_client_focus_fallback(client_td *client)
{
    surface_td *surface;
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;
    client_td *next_focus;

    if (client == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL || desktop->client_active_id != client->id) {
        return;
    }

    desktop->client_active_id = 0;
    desktop->focus_dirty = true;
    next_focus = NULL;

    if (desktop->stacking != NULL && cdlist_size(desktop->stacking) > 0) {
        node = cdlist_tail(desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                client_td *candidate = (client_td *) cdlist_data(node);
                if (candidate != NULL && candidate != client &&
                        !(candidate->properties.flags &
                            CLIENT_FLAG_HIDDEN) &&
                        !client_is_shaded(candidate) &&
                        candidate->properties.state !=
                            (uint16_t) CLIENT_STATE_ICONIFIED &&
                        (candidate->properties.flags &
                         CLIENT_FLAG_FOCUSABLE)) {
                    next_focus = candidate;
                    break;
                }
                node = cdlist_prev(node);
            } while (node != NULL && node != initial);
        } /* ! if (node) */
    }

    if (next_focus != NULL) {
        desktop->client_active_id = next_focus->id;
        desktop->focus_dirty = true;
        (void) desktop_action_client_send_front(desktop, next_focus);
        wcmd_client_focus(next_focus);
    } else {
        xcb_set_input_focus(client->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
    }

    desktop->is_outdated = true;
    surface->is_outdated = true;
}


/* Perform the action to close the client */
void wcmd_client_close(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* ICCCM §4.2.8: send a 'WM_DELETE_WINDOW' 'ClientMessage' when the
     * client advertises support in 'WM_PROTOCOLS'; fall back to
     * 'xcb_destroy_window' only when it does not */
    if (client->has_wm_delete_window && client->ewmh != NULL) {
        xcb_client_message_event_t ev;

        memset(&ev, 0, sizeof(ev));
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = client->window;
        ev.type = client->ewmh->WM_PROTOCOLS;
        ev.data.data32[0] = client->wm_delete_atom;
        ev.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(client->connection, 0, client->window,
                XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    } else {
        /* Client does not support 'WM_DELETE_WINDOW'; destroy directly */
        xcb_destroy_window(client->connection, client->window);
    }
}


/* Forcibly kill the client's X connection */
void wcmd_client_kill(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* Unlike 'wcmd_client_close' (a request to destroy a single window
     * resource), 'xcb_kill_client' terminates the owning client's
     * ENTIRE connection to the X server.  Meant as a last resort for
     * unresponsive clients that ignore a normal close request. */
    xcb_kill_client(client->connection, client->window);
}


/* Restore a client to its normal state */
void wcmd_client_restore(client_td *client)
{
    xcb_window_t target;
    xcb_atom_t icon_geom_atom;
    bool was_iconified;

    if (client == NULL) {
        return;
    }

    /* Fullscreen clients must be un-fullscreened first so the
     * decoration and EWMH atom are cleaned up properly */
    if (client_is_fullscreen(client)) {
        wcmd_client_unfullscreen(client);
        return;
    }

    /* Remember whether we are restoring from an iconified state so the
     * window can be raised and focused afterwards */
    was_iconified = client_is_iconified(client);

    target = wcmd_target_win(client);
    client_geometry_restore(client);

    if (client->icon_window != 0) {
        xcb_destroy_window(client->connection, client->icon_window);
        client->icon_window = 0;
        client->is_icon_mapped = false;
    }
    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }
    xcb_map_window(client->connection, target);
    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unset_hidden(client);
    client->properties.state = CLIENT_STATE_NORMAL;

    wcmd_set_wm_state(client, WCMD_WM_STATE_NORMAL, XCB_NONE);

    /* EWMH §5.9: remove icon geometry hint when restoring to normal */
    icon_geom_atom = wcmd_intern_atom(client->connection,
            "_NET_WM_ICON_GEOMETRY");
    xcb_delete_property(client->connection, client->window,
            icon_geom_atom);

    wcmd_rem_states(client, 3,
            "_NET_WM_STATE_HIDDEN",
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");

    /* Re-enter whichever state this client was in right before it was
     * iconified (see 'pre_iconify_state''s own comment in client.h and
     * where it is captured in 'wcmd_client_iconify'), rather than
     * always settling for plain normal.  Each of these re-computes
     * its own geometry fresh against the current workarea/monitor
     * rather than replaying a stale saved one, since the screen
     * layout may have changed while this client sat iconified. */
    if (was_iconified) {
        uint16_t pre_iconify_state = client->properties.pre_iconify_state;

        client->properties.pre_iconify_state =
            (uint16_t) CLIENT_STATE_NORMAL;

        switch (pre_iconify_state) {
            case CLIENT_STATE_MAXIMIZED:
                wcmd_client_maximize(client);
                break;
            case CLIENT_STATE_MAXIMIZED_HORZ:
                wcmd_client_maximize_horz(client);
                break;
            case CLIENT_STATE_MAXIMIZED_VERT:
                wcmd_client_maximize_vert(client);
                break;
            case CLIENT_STATE_FULLSCREEN:
                wcmd_client_fullscreen(client);
                break;
            default:
                break;
        }
    }

    /* When restoring from an icon, raise the client to the top of the
     * desktop stacking order and give it real input focus so that
     * keyboard shortcuts and other window manager operations target
     * this window immediately, rather than whichever window was
     * previously active */
    if (was_iconified && client_is_focusable(client)) {
        desktop_td *desktop = wm_get_client_desktop(client);
        if (desktop != NULL) {
            desktop->client_active_id = client->id;
            desktop->focus_dirty = true;
            (void) desktop_action_client_send_front(desktop, client);
            desktop->is_outdated = true;
        }
        wcmd_client_focus(client);
    }

    wm_request_client_redraw(client);
}


/* Focus a client */
void wcmd_client_focus(client_td *client)
{
    uint32_t border_color;

    if (client == NULL) {
        return;
    }

    /* ICCCM §4.2.7: only call 'SetInputFocus' when the client's input
     * model accepts it ('WM_HINTS' input field, default 'true').
     * Clients that set 'input=false' rely solely on the 'WM_TAKE_FOCUS'
     * message to direct keyboard focus to themselves. */
    if (client->wm_input_hint) {
        xcb_set_input_focus(client->connection, XCB_INPUT_FOCUS_PARENT,
                            client->window, XCB_CURRENT_TIME);
    }

    /* ICCCM §4.2.7: send 'WM_TAKE_FOCUS' 'ClientMessage' when the
     * client has registered that protocol.  This covers both the
     * Locally Active and Globally Active input models. */
    if (client->has_wm_take_focus && client->ewmh != NULL) {
        xcb_client_message_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = client->window;
        ev.type = client->ewmh->WM_PROTOCOLS;
        ev.data.data32[0] = client->wm_take_focus_atom;
        ev.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(client->connection, 0, client->window,
                XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    }

    /* EWMH: advertise keyboard focus via '_NET_WM_STATE_FOCUSED' */
    wcmd_add_states(client, 1, "_NET_WM_STATE_FOCUSED");

    xcb_map_window(client->connection, client->window);
    if ((!client_is_decorated(client) || client->frame == 0) &&
            client->theme != NULL) {
        border_color = client->theme->window.active.border.color;
        xcb_change_window_attributes(client->connection, client->window,
                XCB_CW_BORDER_PIXEL, &border_color);
        /* Undecorated clients have no separate frame to resize: an
         * X11 border is drawn entirely outside a window's own width
         * and height, so changing its thickness here never touches
         * the window's own geometry, unlike the decorated case just
         * below which does need to grow or shrink the frame. */
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const uint32_t[]) {
                    client->theme->window.active.border.width
                });
    } else {
        client_resync_theme_layout(client, true);
    }

    if (client->ewmh != NULL) {
        xcb_ewmh_set_active_window(client->ewmh,
                (int) client->screen_id,
                client->window);
    }
}


/* Unfocus the client */
void wcmd_client_unfocus(client_td *client)
{
    uint32_t border_color;

    if (client == NULL) {
        return;
    }

    /* EWMH: clear '_NET_WM_STATE_FOCUSED' when the window loses focus */
    wcmd_rem_states(client, 1, "_NET_WM_STATE_FOCUSED");

    client_unfocus(client);

    /* Actually redirect the X server's real input focus away from this
     * client, not just the window manager's own bookkeeping of which
     * client looks focused.  Without this, a client that keeps
     * 'WM_HINTS.input=true' (the default) still receives every
     * 'KeyPress'/'KeyRelease' after being visually unfocused (e.g., by
     * clicking the empty desktop), since nothing ever told the X
     * server to stop delivering keyboard events to its window.
     * A caller that is unfocusing this client only to immediately
     * focus another one (see 'focus_apply') harmlessly overrides this
     * a moment later via that client's own 'SetInputFocus' call, same
     * as the existing pattern in 'wcmd_client_close' below. */
    if (client->connection != NULL) {
        xcb_set_input_focus(client->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
    }

    if ((!client_is_decorated(client) || client->frame == 0) &&
            client->theme != NULL) {
        border_color = client->theme->window.inactive.border.color;
        xcb_change_window_attributes(client->connection, client->window,
                XCB_CW_BORDER_PIXEL, &border_color);
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const uint32_t[]) {
                    client->theme->window.inactive.border.width
                });
    } else {
        client_resync_theme_layout(client, false);
    }

    if (client->ewmh != NULL) {
        xcb_ewmh_set_active_window(client->ewmh,
                (int) client->screen_id,
                XCB_NONE);
    }
}


/**
 * @brief Whether a remembered icon position is already occupied
 *
 * Checks @p client's saved @p icon_x/@p icon_y against every other
 * client on the same desktop that currently has a mapped icon, so
 * @c wcmd_client_iconify can tell a genuinely free remembered spot
 * from one that another window's icon has since claimed (e.g., because
 * that other window was iconified while @p client was still restored,
 * and happened to land where @p client's own icon last was).
 *
 * @param client Client about to be iconified; its own @p icon_window
 *               may still be non-zero from a previous iconify, in
 *               which case it is skipped so it never collides with
 *               itself
 * @param icon_w Icon width, in pixels
 * @param icon_h Icon height, in pixels
 *
 * @return @c true if another icon already overlaps that position
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
static bool s_icon_slot_is_taken(const client_td *client,
        uint16_t icon_w, uint16_t icon_h)
{
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;

    if (client == NULL || client->icon_x < 0 || client->icon_y < 0) {
        return false;
    }

    desktop = wm_get_client_desktop(client);
    if (desktop == NULL || desktop->stacking == NULL) {
        return false;
    }

    node = cdlist_head(desktop->stacking);
    initial = node;
    if (node == NULL) {
        return false;
    }

    do {
        const client_td *other = (const client_td *) cdlist_data(node);

        if (other != NULL && other != client &&
                other->icon_window != 0u && other->is_icon_mapped &&
                geom_intersection_area(
                        client->icon_x, client->icon_y, icon_w, icon_h,
                        other->icon_x, other->icon_y, icon_w, icon_h)
                    > 0u) {
            return true;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return false;
}


/* Move client */
/**
 * @brief Create the client's icon window if it does not exist yet, or
 *        reposition the existing one at its saved coordinates
 *
 * A new window is placed either at the client's own remembered
 * @c icon_x/icon_y (if any, and not since claimed by another icon;
 * see @c s_icon_slot_is_taken) or via @c place_icon otherwise, then
 * created with the theme's inactive icon colors.  An already-existing
 * icon window is simply re-configured to its saved position, which
 * may have changed since if the user dragged it.
 *
 * @param client     Client whose icon window to create or reposition
 * @param icon_h_out Icon window height, including the caption band if
 *                   the theme captions icons
 *
 * @note Complexity: @e O(n), where @e n is the number of already-
 *       iconified clients on the same desktop (see
 *       @c s_icon_slot_is_taken)
 */
static void s_client_ensure_icon_window(client_td *client,
        uint16_t icon_h_out)
{
    if (client->icon_window == 0) {
        uint16_t screen_w;
        uint16_t screen_h;
        int16_t ix;
        int16_t iy;
        int32_t mx = 0;
        int32_t my = 0;
        monitor_td monitor;
        surface_td *surface = NULL;
        enum config_icon_placement_e policy =
            CONFIG_ICON_PLACEMENT_BOTTOM;
        uint32_t mask;
        uint32_t values[3];

        screen_w = 1024u;
        screen_h = 768u;

        if (wcmd_client_monitor(client, &surface, &monitor)) {
            mx = monitor.x;
            my = monitor.y;
            screen_w = geom_clamp_dim((int32_t) monitor.w);
            screen_h = geom_clamp_dim((int32_t) monitor.h);
        } else if (wcmd_screen_dim(client, &screen_w, &screen_h)) {
            /* dimensions updated */
        }

        /* 'monitor' above is deliberately raw (see wcmd_client_
         * monitor's own doc comment), the same as 'desktop_update_
         * workarea' (desktop.c) starts from before folding in
         * 'desktops.margins' and the systray's own reservation for
         * windows; applied here the same way, per monitor rather than
         * once across the whole surface: top/left shift this
         * monitor's own placement origin inward, and right/bottom
         * shrink the available area, so the icon grid never lands
         * within a margin a window's own maximize and placement
         * already stay clear of, nor under the systray's own dock
         * window (which would otherwise sit right on top of a
         * restored icon left behind there, blocking that dock
         * window's own repaint). */
        if (surface != NULL) {
            const struct strut_partial_s *tray_strut =
                systray_get_reserved_strut(surface);
            uint32_t margin_left = 0u;
            uint32_t margin_right = 0u;
            uint32_t margin_top = 0u;
            uint32_t margin_bottom = 0u;
            uint32_t horiz;
            uint32_t vert;

            if (surface->config != NULL) {
                const struct config_desktop_s *cd =
                    &surface->config->desktops;

                margin_left += cd->margins.left;
                margin_right += cd->margins.right;
                margin_top += cd->margins.top;
                margin_bottom += cd->margins.bottom;
            }
            if (tray_strut != NULL) {
                margin_left += (uint32_t) tray_strut->sides.left;
                margin_right += (uint32_t) tray_strut->sides.right;
                margin_top += (uint32_t) tray_strut->sides.top;
                margin_bottom += (uint32_t) tray_strut->sides.bottom;
            }

            horiz = margin_left + margin_right;
            vert = margin_top + margin_bottom;

            mx += (int32_t) margin_left;
            my += (int32_t) margin_top;
            screen_w = ((uint32_t) screen_w > horiz)
                ? (uint16_t) ((uint32_t) screen_w - horiz) : 0u;
            screen_h = ((uint32_t) screen_h > vert)
                ? (uint16_t) ((uint32_t) screen_h - vert) : 0u;
        }

        if (client->config_base != NULL) {
            policy = client->config_base->icons.placement_policy;
        }

        /* Re-use the saved position when the client was already
         * iconified once (and possibly manually repositioned by the
         * user), UNLESS another client's icon has since claimed
         * that exact spot (e.g., it was free when this client was
         * last iconified, but has since been taken by a window that
         * got iconified while this one was restored).  In that case
         * fall through to 'place_icon' just like a client with no
         * remembered position at all, so the two icons never
         * overlap. */
        if (client->icon_x >= 0 && client->icon_y >= 0 &&
                !s_icon_slot_is_taken(client, WM_ICON_SQUARE_SIZE,
                    icon_h_out)) {
            ix = client->icon_x;
            iy = client->icon_y;
        } else {
            place_icon(client, wm_get_client_desktop(client), policy,
                    WM_ICON_SQUARE_SIZE, icon_h_out,
                    screen_w, screen_h,
                    &ix, &iy);
            /* 'place_icon' works in a (0,0)-relative coordinate
             * space bounded by 'screen_w'/'screen_h' alone; offset by
             * 'mx'/'my', the target monitor's own origin plus its
             * top/left margin, so the icon lands on that monitor
             * within the combined surface, past whatever margin is
             * configured, rather than always in its raw top-left
             * corner. */
            ix = (int16_t) (ix + mx);
            iy = (int16_t) (iy + my);
            client->icon_x = ix;
            client->icon_y = iy;
        }

        client->icon_window = xcb_generate_id(client->connection);
        mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
            XCB_CW_EVENT_MASK;

        values[0] = client->theme->icon.inactive.color.background;
        values[1] = client->theme->icon.inactive.border.color;
        values[2] = XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_MOTION;

        xcb_create_window(client->connection,
                XCB_COPY_FROM_PARENT,
                client->icon_window,
                client->parent_id,
                ix, iy,
                (uint16_t) WM_ICON_SQUARE_SIZE, icon_h_out,
                (uint16_t) client->theme->icon.active.border.width,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                XCB_COPY_FROM_PARENT,
                mask, values);
    } else {
        /* Re-map at the saved position (may have been dragged) */
        xcb_configure_window(client->connection, client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                (const uint32_t[]) {
                (uint32_t) client->icon_x,
                (uint32_t) client->icon_y
                });
    }
}


void wcmd_client_iconify(client_td *client)
{
    xcb_window_t target;
    xcb_get_property_reply_t *handled_reply;
    xcb_atom_t handled_atom;
    xcb_atom_t wm_state_atom;
    xcb_atom_t net_wm_state_atom;
    xcb_atom_t icon_geom_atom;
    xcb_atom_t skip_atoms[2];
    uint32_t wm_state_vals[2];
    uint32_t icon_geom[4];
    uint16_t icon_h_out;
    bool skip_icon_win;

    if (client == NULL) {
        return;
    }

    /* Remember the state this client is in right now (normal,
     * maximized in any of its three variants, or fullscreen) so
     * 'wcmd_client_restore' can later re-enter that exact state
     * instead of always landing back on plain normal: "A window
     * manager may implement [additional states] as proper substates
     * of NormalState and IconicState, or it may treat them as
     * independent flags, allowing e.g. a maximized window to be
     * iconified and to re-appear as maximized upon de-iconification"
     * (X Desktop Group, 2013, "Extended Window Manager Hints", v1.5,
     * §2.1.1).  Guarded against an already-iconified client calling
     * this again, which would otherwise overwrite the real remembered
     * state with 'CLIENT_STATE_ICONIFIED' itself. */
    if (client->properties.state != (uint16_t) CLIENT_STATE_ICONIFIED) {
        client->properties.pre_iconify_state = client->properties.state;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    /* Un-fullscreen first, the same reasoning as unshading above: an
     * iconified client's geometry is meant to be restored to its
     * pre-iconify size later (see the 'client_geometry_save' call
     * just below), and while still fullscreen that size is the whole
     * screen, not the window's real one. */
    if (client_is_fullscreen(client)) {
        wcmd_client_unfullscreen(client);
    }

    target = wcmd_target_win(client);
    /* Only remember the geometry to restore to if it is not already
     * a maximized state's geometry: iconifying a maximized window must
     * not overwrite the true pre-maximize geometry already held in
     * 'layout.geometry.old' (see 'client_is_maximized_any' and the
     * matching guard in the 'wcmd_client_maximize*' functions), or
     * un-iconifying it later would restore it at the maximized size
     * instead of its original one */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    /* EWMH: if a pager sets '_NET_WM_HANDLED_ICONS' on the root window,
     * it manages icon display itself; the window manager must not
     * create icon windows */
    handled_atom = wcmd_intern_atom(client->connection,
            "_NET_WM_HANDLED_ICONS");
    handled_reply = xcb_get_property_reply(client->connection,
            xcb_get_property(client->connection, 0, client->parent_id,
                handled_atom, XCB_ATOM_CARDINAL, 0, 1), NULL);
    skip_icon_win = (handled_reply != NULL &&
            xcb_get_property_value_length(handled_reply) > 0);

    free(handled_reply);

    /* Compute icon height once, shared by icon window creation
     * below and the '_NET_WM_ICON_GEOMETRY' property published
     * further down */
    icon_h_out = (uint16_t) (WM_ICON_SQUARE_SIZE +
            ((client->theme->icon.is_captioned)
             ? WM_ICON_CAPTION_HEIGHT
             : 0u));

    if (!skip_icon_win) {
        s_client_ensure_icon_window(client, icon_h_out);
    }

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }

    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    if (!skip_icon_win) {
        xcb_window_t tray_below;

        xcb_map_window(client->connection, client->icon_window);

        /* Icons are meant to sit even lower than the tray whenever it
         * is in the 'below' layer, "stuck to the desktop": stack just
         * below it explicitly, rather than via an unqualified 'below'
         * with no sibling, which would only put the icon under the
         * tray by coincidence of restack order rather than guarantee
         * it.  The other direction (the tray restacking after icons
         * already exist) is handled on the tray's own side; see
         * 'systray_layout_restack'. */
        tray_below = systray_below_window();
        if (tray_below != XCB_WINDOW_NONE) {
            xcb_configure_window(client->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_SIBLING |
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) {
                    tray_below, XCB_STACK_MODE_BELOW
                    });
        } else {
            xcb_configure_window(client->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) { XCB_STACK_MODE_BELOW });
        }
        client->is_icon_mapped = true;

        /* ICCCM §4.1.3: mark the WM icon window as Withdrawn so pagers
         * that scan window trees treat it as unmanaged */
        wm_state_atom = wcmd_intern_atom(client->connection, "WM_STATE");
        wm_state_vals[0] = WCMD_WM_STATE_WITHDRAWN;
        wm_state_vals[1] = XCB_NONE;

        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->icon_window, wm_state_atom, wm_state_atom,
                32, 2, wm_state_vals);

        /* EWMH: tell pagers and taskbars to skip the WM icon window */
        net_wm_state_atom = wcmd_intern_atom(client->connection,
                "_NET_WM_STATE");
        skip_atoms[0] = wcmd_intern_atom(client->connection,
                "_NET_WM_STATE_SKIP_PAGER");
        skip_atoms[1] = wcmd_intern_atom(client->connection,
                "_NET_WM_STATE_SKIP_TASKBAR");
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->icon_window, net_wm_state_atom,
                XCB_ATOM_ATOM, 32, 2, skip_atoms);

        /* EWMH §5.9: publish icon geometry on the client window so
         * taskbars can animate the iconify transition */
        icon_geom[0] = (uint32_t) client->icon_x;
        icon_geom[1] = (uint32_t) client->icon_y;
        icon_geom[2] = WM_ICON_SQUARE_SIZE;
        icon_geom[3] = icon_h_out;
        icon_geom_atom = wcmd_intern_atom(client->connection,
                "_NET_WM_ICON_GEOMETRY");
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, icon_geom_atom,
                XCB_ATOM_CARDINAL, 32, 4, icon_geom);
    }

    client_set_hidden(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    wcmd_set_wm_state(client, WCMD_WM_STATE_ICONIC,
            (skip_icon_win) ? XCB_NONE : client->icon_window);

    /* Iconify per EWMH: window hidden with '_NET_WM_STATE_HIDDEN'.
     * Icon display handled by pager/desktop */
    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    s_client_focus_fallback(client);

    /* Ensure taskbars reflect the iconified state even when the client
     * was not the active one.  Function 's_client_focus_fallback' only
     * marks is_outdated when it changes focus, so a non-active
     * iconification would otherwise not trigger wm_ewmh_sync. */
    wm_request_client_redraw(client);

    xcb_flush(client->connection);
}


/* Hide the client (minimize, but not iconify) */
void wcmd_client_hide(client_td *client)
{
    xcb_window_t target;
    surface_td *surface;

    if (client == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    if (surface != NULL && surface->showing_desktop) {
        surface->showing_desktop = false;
    }

    target = wcmd_target_win(client);

    /* Account for the 'UnmapNotify' events that 'handler_unmap_notify'
     * must skip.  Two events arrive for the unmapped target
     * ('SubstructureNotify' on parent + 'StructureNotify' on target)
     * and one additional event for the titlebar via the frame's
     * 'SubstructureNotify'.  If 'target' is the frame, the content
     * window is also unmapped explicitly below, producing two more
     * events for 'client->window'. */
    client->ignore_unmap += 2u;
    if (client->titlebar != 0) {
        client->ignore_unmap += 1u;
    }
    if (target != client->window) {
        client->ignore_unmap += 2u;
    }

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }

    xcb_unmap_window(client->connection, target);

    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    client_set_hidden(client);

    wcmd_set_wm_state(client, WCMD_WM_STATE_ICONIC, XCB_NONE);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    s_client_focus_fallback(client);
    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Show (unhide) the client */
void wcmd_client_unhide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);

    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }

    xcb_map_window(client->connection, target);

    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unset_hidden(client);

    wcmd_set_wm_state(client, WCMD_WM_STATE_NORMAL, XCB_NONE);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");

    /* Raise the unhidden client to the top of the desktop stacking
     * order and give it real input focus, matching the deiconify
     * behavior, so that clicking a hidden window in the window menu
     * immediately activates it for keyboard input */
    if (client_is_focusable(client)) {
        desktop_td *desktop = wm_get_client_desktop(client);
        if (desktop != NULL) {
            desktop->client_active_id = client->id;
            desktop->focus_dirty = true;
            (void) desktop_action_client_send_front(desktop, client);
            desktop->is_outdated = true;
        }
        wcmd_client_focus(client);
    }
    wm_request_client_redraw(client);
}


/* Set client sticky mode */
void wcmd_client_sticky(client_td *client)
{
    uint32_t all_desktops;

    if (client == NULL) {
        return;
    }

    client_set_sticky(client);
    wcmd_add_states(client, 1, "_NET_WM_STATE_STICKY");
    if (client->ewmh != NULL) {
        all_desktops = WM_DESKTOP_ID_ALL;
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &all_desktops);
    }

    wm_request_client_redraw(client);
}


/* Remove client sticky mode */
void wcmd_client_unsticky(client_td *client)
{
    desktop_td *owner_desktop;
    desktop_td *current_desktop;
    surface_td *surface;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    client_unset_sticky(client);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_STICKY");
    if (client->ewmh != NULL) {
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &client->desktop_id);
    }

    owner_desktop = wm_get_client_desktop(client);
    surface = wm_get_surface_by_id(client->screen_id);
    current_desktop = (surface != NULL)
        ? lookup_current_desktop(surface)
        : NULL;
    if (owner_desktop != NULL && current_desktop != NULL &&
            owner_desktop->id != current_desktop->id) {
        target = wcmd_target_win(client);

        /* Account for the 'UnmapNotify' events that
         * 'handler_unmap_notify' must skip.  Two events arrive for the
         * unmapped target ('SubstructureNotify' on parent
         * + 'StructureNotify' on target) and one additional event for
         * the titlebar via the frame's 'SubstructureNotify'. */
        client->ignore_unmap += 2u;
        if (client->titlebar != 0) {
            client->ignore_unmap += 1u;
        }

        if (client->titlebar != 0) {
            xcb_unmap_window(client->connection, client->titlebar);
        }

        xcb_unmap_window(client->connection, target);

        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_unmap_window(client->connection, client->icon_window);
            client->is_icon_mapped = false;
        }

        /* If the unstickied client held focus on the current desktop,
         * transfer focus to the MRU client still on that desktop */
        s_client_focus_fallback(client);
    }

    wm_request_client_redraw(client);
}


/* Toggle stickiness */
void wcmd_client_toggle_sticky(client_td *client)
{
    surface_td *surface;

    if (client == NULL) {
        return;
    }

    /* Meaningless with only one desktop: "visible on every desktop"
     * and "visible on this one desktop" are the exact same thing when
     * there is only the one, so there is nothing to actually toggle.
     * The menu entry for this is already hidden in that case (see
     * 'wincmenu.c', which omits the whole "Send to desktop" submenu
     * it lives in), but the keyboard shortcut has no such menu to
     * hide behind, so it is guarded here instead, at the one place
     * both of them ultimately call through. */
    surface = wm_get_surface_by_id(client->screen_id);
    if (surface != NULL && surface->desktop_count <= 1u) {
        return;
    }

    if (client_is_sticky(client)) {
        wcmd_client_unsticky(client);
    } else {
        wcmd_client_sticky(client);
    }
}


/* Raise the client to the top */
void wcmd_client_set_urgent(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_set_urgent(client);
    wcmd_add_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Clear client urgency */
void wcmd_client_clear_urgent(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_unset_urgent(client);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Publish '_NET_WM_ALLOWED_ACTIONS' based on the client's current
 * properties */
void wcmd_client_update_allowed_actions(client_td *client)
{
    xcb_atom_t actions[12];
    uint32_t n = 0u;

    if (client == NULL || client->ewmh == NULL) {
        return;
    }

    /* Actions available to all managed, visible clients */
    actions[n++] = client->ewmh->_NET_WM_ACTION_CLOSE;
    actions[n++] = client->ewmh->_NET_WM_ACTION_CHANGE_DESKTOP;
    if (client_is_resizable(client)) {
        actions[n++] = client->ewmh->_NET_WM_ACTION_MOVE;
        actions[n++] = client->ewmh->_NET_WM_ACTION_RESIZE;
        actions[n++] = client->ewmh->_NET_WM_ACTION_MAXIMIZE_HORZ;
        actions[n++] = client->ewmh->_NET_WM_ACTION_MAXIMIZE_VERT;
    }

    /* Not folded into the 'client_is_resizable' block above, unlike
     * maximize: fullscreen is a WM-forced override of the client's
     * own preferred geometry, not a user-convenience resize the
     * client's own fixed size hints have any say over; see
     * 'wcmd_client_fullscreen''s own comment for the full reasoning.
     * A DOS-emulation or retro-game window that fixes its own size is
     * exactly the case this matters for: some such clients check this
     * very property before ever attempting '_NET_WM_STATE_FULLSCREEN'
     * at all, so advertising it as disallowed here would have kept
     * the fix in wcmd_client_fullscreen itself from ever being
     * reached. */
    actions[n++] = client->ewmh->_NET_WM_ACTION_FULLSCREEN;

    if (client_is_focusable(client)) {
        actions[n++] = client->ewmh->_NET_WM_ACTION_MINIMIZE;
    }

    /* All clients may be shaded, sticked, and re-stacked */
    actions[n++] = client->ewmh->_NET_WM_ACTION_SHADE;
    actions[n++] = client->ewmh->_NET_WM_ACTION_STICK;
    actions[n++] = client->ewmh->_NET_WM_ACTION_ABOVE;
    actions[n++] = client->ewmh->_NET_WM_ACTION_BELOW;
    xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
            client->window, client->ewmh->_NET_WM_ALLOWED_ACTIONS,
            XCB_ATOM_ATOM, 32, n, actions);
}
