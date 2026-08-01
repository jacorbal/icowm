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
#include <stdarg.h>     /* va_arg, va_end, va_start */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* size_t, snprintf */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Default initial values */
#include <defs/wm.h>

/* Windows & icons policy includes */
#include <policy/placement.h>

/* Project includes */
#include <actdata.h>
#include <client.h>
#include <desktop.h>
#include <lookup.h>
#include <wm.h>

/* Local includes */
#include <cmds/ccmd.h>
#include <cmds/util.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/meta.h>


/**
 * @brief Create frame and titlebar windows for a previously undecorated
 *        client
 *
 * Reparents the client window into a newly created frame, creates the
 * titlebar child window, and updates the client layout fields so
 * rendering and geometry operations use the decorated extents.
 *
 * @param client Pointer to the client
 * @param bw     Border width to apply
 * @param th     Titlebar height to apply
 *
 * @note Complexity: @e O(1)
 */
static void s_client_enable_decoration(client_td *client,
        int32_t bw, int32_t th)
{
    uint32_t mask;
    uint32_t values[3];
    int32_t frame_x;
    int32_t frame_y;
    int32_t frame_w;
    int32_t frame_h;
    uint32_t border_color;
    uint32_t bg_color;

    if (client == NULL || client->parent_id == 0) {
        return;
    }

    border_color = (client->theme != NULL)
        ? client->theme->window.inactive.border_color
        : 0x999999U;
    bg_color = (client->theme != NULL)
        ? client->theme->window.inactive.background_color
        : 0x000000U;

    frame_x = client->layout.geometry.cur.pos.x - bw;
    frame_y = client->layout.geometry.cur.pos.y - (bw + th);
    frame_w = (int32_t) client->layout.geometry.cur.dim.w + 2 * bw;
    frame_h = (int32_t) client->layout.geometry.cur.dim.h + 2 * bw + th;

    if (frame_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        frame_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
    }
    if (frame_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        frame_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
    }

    client->frame = xcb_generate_id(client->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = border_color;
    values[1] = border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_create_window(client->connection,
            XCB_COPY_FROM_PARENT,
            client->frame,
            client->parent_id,
            (int16_t) frame_x, (int16_t) frame_y,
            (uint16_t) frame_w, (uint16_t) frame_h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    client->titlebar = xcb_generate_id(client->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = bg_color;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS;
    xcb_create_window(client->connection,
            XCB_COPY_FROM_PARENT,
            client->titlebar,
            client->frame,
            (int16_t) bw, (int16_t) bw,
            (uint16_t) client->layout.geometry.cur.dim.w,
            (uint16_t) th,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* Reparenting to an unmapped frame makes the content window
     * non-viewable, which emits two synthetic 'UnmapNotify' events:
     *
     *  1. From root's' SubstructureNotify' (event=root, window=content)
     *  2. From the content window's own 'StructureNotify' (event=window,
     *  window=content)
     *
     * Absorb both so focus is not stolen from the active window. */
    client->ignore_unmap += 2u;
    client->ignore_focus_unmap++;

    xcb_reparent_window(client->connection,
            client->window,
            client->frame,
            (int16_t) bw, (int16_t) (bw + th));

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, (const uint32_t[]) {0u});

    xcb_grab_button(client->connection,
            0,
            client->frame,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
            XCB_GRAB_MODE_SYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            XCB_NONE,
            XCB_BUTTON_INDEX_ANY,
            XCB_MOD_MASK_ANY);

    xcb_map_window(client->connection, client->frame);
    xcb_map_window(client->connection, client->titlebar);
    xcb_map_window(client->connection, client->window);

    client->layout.geometry.cur.pos.x = frame_x;
    client->layout.geometry.cur.pos.y = frame_y;
    client->layout.geometry.cur.dim.w = (uint16_t) frame_w;
    client->layout.geometry.cur.dim.h = (uint16_t) frame_h;
    client->layout.frame_extents.left = bw;
    client->layout.frame_extents.right = bw;
    client->layout.frame_extents.top = bw + th;
    client->layout.frame_extents.bottom = bw;
    client_set_decoration(client);

    if (client->ewmh != NULL) {
        uint32_t extents[4];
        extents[0] = (uint32_t) bw;
        extents[1] = (uint32_t) bw;
        extents[2] = (uint32_t) (bw + th);
        extents[3] = (uint32_t) bw;
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_FRAME_EXTENTS,
                XCB_ATOM_CARDINAL, 32, 4, extents);
    }
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

    if (client == NULL) {
        return;
    }

    /* Fullscreen clients must be un-fullscreened first so the
     * decoration and EWMH atom are cleaned up properly */
    if (client_is_fullscreen(client)) {
        wcmd_client_unfullscreen(client);
        return;
    }

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

    wcmd_rem_states(client, 3,
            "_NET_WM_STATE_HIDDEN",
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");

    wm_request_client_redraw(client);
}


/* Focus a client */
void wcmd_client_focus(client_td *client)
{
    uint32_t border_color;

    if (client == NULL) {
        return;
    }

    xcb_set_input_focus(client->connection, XCB_INPUT_FOCUS_PARENT,
                        client->window, XCB_CURRENT_TIME);
    xcb_map_window(client->connection, client->window);
    if ((!client_is_decorated(client) || client->frame == 0) &&
            client->theme != NULL) {
        border_color = client->theme->window.active.border_color;
        xcb_change_window_attributes(client->connection, client->window,
                XCB_CW_BORDER_PIXEL, &border_color);
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

    client_unfocus(client);
    if ((!client_is_decorated(client) || client->frame == 0) &&
            client->theme != NULL) {
        border_color = client->theme->window.inactive.border_color;
        xcb_change_window_attributes(client->connection, client->window,
                XCB_CW_BORDER_PIXEL, &border_color);
    }

    if (client->ewmh != NULL) {
        xcb_ewmh_set_active_window(client->ewmh,
                (int) client->screen_id,
                XCB_NONE);
    }
}

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
        }
    }

    if (next_focus != NULL) {
        desktop->client_active_id = next_focus->id;
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


/* Move client */
void wcmd_client_iconify(client_td *client)
{
    xcb_window_t target;
    uint32_t mask;
    uint32_t values[3];

    if (client == NULL) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    if (client->icon_window == 0) {
        uint16_t icon_h;
        uint16_t screen_w;
        uint16_t screen_h;
        int16_t ix;
        int16_t iy;
        enum config_icon_placement_e policy =
            CONFIG_ICON_PLACEMENT_BOTTOM;

        icon_h = (uint16_t) (WM_ICON_SQUARE_SIZE +
                ((client->theme->icon.general.is_captioned)
                 ? WM_ICON_CAPTION_HEIGHT
                 : 0u));

        screen_w = 1024u;
        screen_h = 768u;
        if (wcmd_screen_dim(client, &screen_w, &screen_h)) {
            /* dimensions updated */
        }

        if (client->config_base != NULL) {
            policy = client->config_base->icons.placement_policy;
        }

        /* Re-use saved position when the client was already iconified
         * once and manually repositioned by the user */
        if (client->icon_x >= 0 && client->icon_y >= 0) {
            ix = client->icon_x;
            iy = client->icon_y;
        } else {
            place_icon(client, wm_get_client_desktop(client), policy,
                    WM_ICON_SQUARE_SIZE, icon_h,
                    screen_w, screen_h,
                    &ix, &iy);
            client->icon_x = ix;
            client->icon_y = iy;
        }

        client->icon_window = xcb_generate_id(client->connection);
        mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
        values[0] = client->theme->icon.inactive.background_color;
        values[1] = client->theme->icon.inactive.border_color;
        values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                    XCB_EVENT_MASK_BUTTON_MOTION;
        xcb_create_window(client->connection,
                XCB_COPY_FROM_PARENT,
                client->icon_window,
                client->parent_id,
                ix, iy,
                (uint16_t) WM_ICON_SQUARE_SIZE, icon_h,
                (uint16_t) client->theme->icon.general.border_width,
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

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }
    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }
    xcb_map_window(client->connection, client->icon_window);
    xcb_configure_window(client->connection, client->icon_window,
            XCB_CONFIG_WINDOW_STACK_MODE,
            (const uint32_t[]) { XCB_STACK_MODE_BELOW });
    client->is_icon_mapped = true;

    client_set_hidden(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    /* Iconify per EWMH: window hidden with '_NET_WM_STATE_HIDDEN'.
     * Icon display handled by pager/desktop */

    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    s_client_focus_fallback(client);
    xcb_flush(client->connection);
}


/* Hide the client (minimize, but not iconify) */
void wcmd_client_hide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }
    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    client_set_hidden(client);

    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    s_client_focus_fallback(client);
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

    wcmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Shade client (roll-up), if decorated */
void wcmd_client_shade(client_td *client)
{
    xcb_window_t target;
    uint32_t shaded_h;

    if (client == NULL || !client_is_decorated(client) ||
            client_is_shaded(client) || client_is_fullscreen(client)) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    shaded_h = (uint32_t) (client->layout.frame_extents.top +
                           client->layout.frame_extents.bottom);
    if (shaded_h < WM_MIN_WINDOW_DIMENSION) {
        shaded_h = WM_MIN_WINDOW_DIMENSION;
    }

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) { shaded_h });
    xcb_map_window(client->connection, target);
    client->ignore_unmap++;
    xcb_unmap_window(client->connection, client->window);

    client->layout.geometry.cur.dim.h = (uint16_t) shaded_h;
    client_set_shade(client);
    client_sync_decoration_layout(client);    

    wcmd_add_states(client, 1, "_NET_WM_STATE_SHADED");
    wcmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Unshade client (roll-down), if decorated */
void wcmd_client_unshade(client_td *client)
{
    xcb_window_t target;
    uint32_t restored_h;

    if (client == NULL || !client_is_decorated(client) ||
            !client_is_shaded(client)) {
        return;
    }

    target = wcmd_target_win(client);

    /* Restore only the height from the saved geometry; keep the current
     * position so that moving the shaded window is honoured. */
    restored_h = client->layout.geometry.old.dim.h;
    client->layout.geometry.cur.dim.h = restored_h;

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) { restored_h });
    xcb_map_window(client->connection, client->window);

    client_unset_shade(client);
    client_unset_hidden(client);

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_SHADED",
            "_NET_WM_STATE_HIDDEN");

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Toggle shading */
void wcmd_client_toggle_shade(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    } else {
        wcmd_client_shade(client);
    }
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
    cdlist_item_td *snode;
    client_td *next_focus;

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

        /* Increment 'ignore_unmap' to prevent 'handler_unmap_notify'
         * from treating the WM-initiated unmaps as client self-closes.
         * The frame 'unmap' implicitly unmaps its child, so only one
         * extra increment is needed when target is the frame. */
        client->ignore_unmap += 1u;
        if (target != client->window) {
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
        if (current_desktop->client_active_id == client->id) {
            current_desktop->client_active_id = 0;
            next_focus = NULL;
            if (current_desktop->stacking != NULL &&
                    cdlist_size(current_desktop->stacking) > 0) {
                snode = cdlist_tail(current_desktop->stacking);
                while (snode != NULL) {
                    client_td *c = (client_td *) cdlist_data(snode);
                    if (c != NULL && c->id != client->id &&
                            !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                            !client_is_shaded(c) &&
                            c->properties.state !=
                                (uint16_t) CLIENT_STATE_ICONIFIED &&
                            (c->properties.flags &
                             CLIENT_FLAG_FOCUSABLE)) {
                        next_focus = c;
                        break;
                    }
                    snode = cdlist_prev(snode);
                    if (snode == cdlist_tail(current_desktop->stacking)) {
                        break;
                    }
                }
            }
            if (next_focus != NULL) {
                current_desktop->client_active_id = next_focus->id;
                wcmd_client_focus(next_focus);
            } else {
                xcb_set_input_focus(client->connection,
                        XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
            }
        }

        current_desktop->is_outdated = true;
        surface->is_outdated = true;
    }

    wm_request_client_redraw(client);
}


/* Toggle stickiness */
void wcmd_client_toggle_sticky(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client_is_sticky(client)) {
        wcmd_client_unsticky(client);
    } else {
        wcmd_client_sticky(client);
    }
}


/* Set full screen mode */
void wcmd_client_fullscreen(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;
    uint32_t border_width;
    bool was_decorated;
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    if (!client_is_resizable(client)) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    if (!wcmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    client_geometry_save(client);
    was_decorated = client_is_decorated(client);
    client->was_decorated_fullscreen = was_decorated;
    target = wcmd_target_win(client);

    if (was_decorated && client->frame != 0) {
        border_width = 0u;
        if (client->titlebar != 0) {
            xcb_unmap_window(client->connection, client->titlebar);
        }
        xcb_configure_window(client->connection, client->frame,
                XCB_CONFIG_WINDOW_BORDER_WIDTH, &border_width);
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_X |
                XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT |
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const uint32_t[]) {
                0u, 0u,
                (uint32_t) sw, (uint32_t) sh,
                0u
                });
        client->layout.frame_extents.left = 0;
        client->layout.frame_extents.right = 0;
        client->layout.frame_extents.top = 0;
        client->layout.frame_extents.bottom = 0;
    }

    /* Resize the visible target to fill screen */
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                0u, 0u,
                (uint32_t) sw,
                (uint32_t) sh
            });

    client->layout.geometry.cur.pos.x = 0;
    client->layout.geometry.cur.pos.y = 0;
    client->layout.geometry.cur.dim.w = (uint32_t) sw;
    client->layout.geometry.cur.dim.h = (uint32_t) sh;

    client->properties.state = CLIENT_STATE_FULLSCREEN;

    if (client->ewmh != NULL) {
        uint32_t extents[4] = {0u, 0u, 0u, 0u};
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_FRAME_EXTENTS,
                XCB_ATOM_CARDINAL, 32, 4, extents);
    }

    /* Retain focus: keep this client active on its desktop and give it
     * input focus so the window is not lost from the active window
     * tracking when going fullscreen. */
    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        desktop->client_active_id = client->id;
        (void) desktop_action_client_send_front(desktop, client);
        desktop->is_outdated = true;
    }
    wcmd_client_focus(client);

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
    wcmd_add_states(client, 1, "_NET_WM_STATE_FULLSCREEN");

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Remove full screen mode */
void wcmd_client_unfullscreen(client_td *client)
{
    xcb_window_t target;
    uint16_t border_width;
    uint16_t title_height;
    uint16_t inner_w;
    uint16_t inner_h;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_restore(client);

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,
                (uint32_t) client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h
            });

    if (client->was_decorated_fullscreen && client->frame != 0) {
        border_width = (client->theme != NULL)
            ? (uint16_t) client->theme->window.general.border_width
            : 0u;
        title_height = client->title_height;
        inner_w = (client->layout.geometry.cur.dim.w >
                (uint16_t) (border_width * 2u))
            ? (uint16_t) (client->layout.geometry.cur.dim.w -
                    (uint16_t) (border_width * 2u))
            : (uint16_t) WM_MIN_WINDOW_DIMENSION;
        inner_h = (client->layout.geometry.cur.dim.h >
                (uint16_t) (border_width * 2u + title_height))
            ? (uint16_t) (client->layout.geometry.cur.dim.h -
                    (uint16_t) (border_width * 2u + title_height))
            : (uint16_t) WM_MIN_WINDOW_DIMENSION;

        client->layout.frame_extents.left = border_width;
        client->layout.frame_extents.right = border_width;
        client->layout.frame_extents.top =
            (uint16_t) (border_width + title_height);
        client->layout.frame_extents.bottom = border_width;

        xcb_configure_window(client->connection, client->frame,
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const uint32_t[]) { (uint32_t) border_width });
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_X |
                XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT |
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const uint32_t[]) {
                    (uint32_t) border_width,
                    (uint32_t) (border_width + title_height),
                    (uint32_t) inner_w,
                    (uint32_t) inner_h,
                0u
                });
        if (client->titlebar != 0) {
            xcb_configure_window(client->connection, client->titlebar,
                    XCB_CONFIG_WINDOW_X |
                    XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT,
                        (const uint32_t[]) {
                        (uint32_t) border_width,
                        (uint32_t) border_width,
                        (uint32_t) inner_w,
                        (uint32_t) title_height
                    });
            xcb_map_window(client->connection, client->titlebar);
        }

    }
    client->was_decorated_fullscreen = false;

    client->properties.state = CLIENT_STATE_NORMAL;

    if (client->ewmh != NULL) {
        uint32_t extents[4];
        extents[0] = (uint32_t) client->layout.frame_extents.left;
        extents[1] = (uint32_t) client->layout.frame_extents.right;
        extents[2] = (uint32_t) client->layout.frame_extents.top;
        extents[3] = (uint32_t) client->layout.frame_extents.bottom;
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_FRAME_EXTENTS,
                XCB_ATOM_CARDINAL, 32, 4, extents);
    }


    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Toggle full screen mode */
void wcmd_client_toggle_fullscreen(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (!client_is_resizable(client) &&
            client->properties.state != CLIENT_STATE_FULLSCREEN) {
        return;
    }

    if (client->properties.state == CLIENT_STATE_FULLSCREEN) {
        wcmd_client_unfullscreen(client);
    } else {
        wcmd_client_fullscreen(client);
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


/* Set icon for the client */
void wcmd_client_toggle_decoration(client_td *client)
{
    int32_t bw;
    int32_t th;
    desktop_td *desktop;
    bool keep_focus;

    if (client == NULL) {
        return;
    }

    bw = (client->theme != NULL)
        ? (int32_t) client->theme->window.general.border_width
        : 0;
    th = (int32_t) client->title_height;
    desktop = wm_get_client_desktop(client);
    keep_focus = true;

    if (client_is_decorated(client)) {  /* Remove decoration */
        if (client->frame != 0) {
            int32_t inner_x = client->layout.geometry.cur.pos.x +
                client->layout.frame_extents.left;
            int32_t inner_y = client->layout.geometry.cur.pos.y +
                client->layout.frame_extents.top;
            int32_t inner_w = (int32_t) client->layout.geometry.cur.dim.w -
                client->layout.frame_extents.left -
                client->layout.frame_extents.right;
            int32_t inner_h = (int32_t) client->layout.geometry.cur.dim.h -
                client->layout.frame_extents.top -
                client->layout.frame_extents.bottom;

            if (inner_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
                inner_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
            }
            if (inner_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
                inner_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
            }

            if (client->titlebar != 0) {
                xcb_destroy_window(client->connection, client->titlebar);
                client->titlebar = 0;
            }

            /* Reparenting generates a synthetic 'UnmapNotify' for the
             * content window.  Absorb it so 'handler_unmap_notify' does
             * not mistake the event for a voluntary hide and does not
             * steal focus from the window. */
            client->ignore_unmap+=2;
            client->ignore_focus_unmap++;
            xcb_reparent_window(client->connection,
                    client->window,
                    client->parent_id,
                    (int16_t) inner_x, (int16_t) inner_y);

            xcb_configure_window(client->connection, client->window,
                    XCB_CONFIG_WINDOW_X     |
                    XCB_CONFIG_WINDOW_Y     |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT |
                    XCB_CONFIG_WINDOW_BORDER_WIDTH,
                    (const uint32_t[]) {
                        (uint32_t) inner_x, (uint32_t) inner_y,
                        (uint32_t) inner_w, (uint32_t) inner_h,
                        (uint32_t) bw
                    });

            xcb_destroy_window(client->connection, client->frame);
            client->frame = 0;

            client->layout.geometry.cur.pos.x = inner_x;
            client->layout.geometry.cur.pos.y = inner_y;
            client->layout.geometry.cur.dim.w = (uint16_t) inner_w;
            client->layout.geometry.cur.dim.h = (uint16_t) inner_h;
        } else {
            xcb_configure_window(client->connection, client->window,
                    XCB_CONFIG_WINDOW_BORDER_WIDTH,
                    (const uint32_t[]) { (uint32_t) bw });
        }
        client->layout.frame_extents.left = 0;
        client->layout.frame_extents.right = 0;
        client->layout.frame_extents.top = 0;
        client->layout.frame_extents.bottom = 0;
        client_unset_decoration(client);

        if (client->ewmh != NULL) {
            uint32_t extents[4] = {0u, 0u, 0u, 0u};
            xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                    client->window, client->ewmh->_NET_FRAME_EXTENTS,
                    XCB_ATOM_CARDINAL, 32, 4, extents);
        }
    } else {                            /* Restore decoration */
        if (client->frame == 0) {
            s_client_enable_decoration(client, bw, th);
        } else {
            int32_t frame_x = client->layout.geometry.cur.pos.x - bw;
            int32_t frame_y = client->layout.geometry.cur.pos.y - (bw + th);
            int32_t frame_w =
                (int32_t) client->layout.geometry.cur.dim.w + 2 * bw;
            int32_t frame_h =
                (int32_t) client->layout.geometry.cur.dim.h + 2 * bw + th;

            if (frame_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
                frame_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
            }
            if (frame_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
                frame_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
            }

            xcb_configure_window(client->connection, client->frame,
                    XCB_CONFIG_WINDOW_X     |
                    XCB_CONFIG_WINDOW_Y     |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT,
                    (const uint32_t[]) {
                        (uint32_t) frame_x, (uint32_t) frame_y,
                        (uint32_t) frame_w, (uint32_t) frame_h
                    });

            xcb_configure_window(client->connection, client->window,
                    XCB_CONFIG_WINDOW_X |
                    XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT |
                    XCB_CONFIG_WINDOW_BORDER_WIDTH,
                    (const uint32_t[]) {
                        (uint32_t) bw, (uint32_t) (bw + th),
                        (uint32_t) client->layout.geometry.cur.dim.w,
                        (uint32_t) client->layout.geometry.cur.dim.h,
                        0u
                    });

            if (client->titlebar != 0) {
                xcb_configure_window(client->connection, client->titlebar,
                        XCB_CONFIG_WINDOW_X     |
                        XCB_CONFIG_WINDOW_Y     |
                        XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT,
                        (const uint32_t[]) {
                            (uint32_t) bw, (uint32_t) bw,
                            (uint32_t) client->layout.geometry.cur.dim.w,
                            (uint32_t) th
                        });
                xcb_map_window(client->connection, client->titlebar);
            }
            xcb_map_window(client->connection, client->frame);
            xcb_map_window(client->connection, client->window);

            client->layout.geometry.cur.pos.x = frame_x;
            client->layout.geometry.cur.pos.y = frame_y;
            client->layout.geometry.cur.dim.w = (uint16_t) frame_w;
            client->layout.geometry.cur.dim.h = (uint16_t) frame_h;
            client->layout.frame_extents.left = bw;
            client->layout.frame_extents.right = bw;
            client->layout.frame_extents.top = bw + th;
            client->layout.frame_extents.bottom = bw;
            client_set_decoration(client);

            if (client->ewmh != NULL) {
                uint32_t extents[4];
                extents[0] = (uint32_t) bw;
                extents[1] = (uint32_t) bw;
                extents[2] = (uint32_t) (bw + th);
                extents[3] = (uint32_t) bw;
                xcb_change_property(client->connection,
                        XCB_PROP_MODE_REPLACE,
                        client->window,
                        client->ewmh->_NET_FRAME_EXTENTS,
                        XCB_ATOM_CARDINAL, 32, 4, extents);
            }
        }
    }

    if (keep_focus) {
        if (desktop != NULL) {
            desktop->client_active_id = client->id;
            (void) desktop_action_client_send_front(desktop, client);
            desktop->is_outdated = true;
        }

        wcmd_client_raise(client);
        wcmd_client_focus(client);
    }

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}
