/**
 * @file handler/map.c
 *
 * @brief X @c MAP_REQUEST, @c UNMAP_NOTIFY, and @c DESTROY_NOTIFY event
 *        handlers
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

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Default initial values */
#include <defs/desktop.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Rules includes */
#include <rules.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/placement.h>

/* Input includes */
#include <input/mouse.h>
#include <input/mouse/drag.h>

/* Render includes */
#include <render/outdate.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <memguard.h>
#include <surface.h>
#include <wm.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Command includes */
#include <cmds/client/basic.h>
#include <cmds/client/layer.h>
#include <cmds/client/internal.h>

/* IPC includes */
#include <ipc.h>

/* Project includes */
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <handler.h>


/**
 * @brief Map a window without adopting it under window manager control
 *
 * Shared by every early-return path in @c handler_map_request below
 * that declines to manage the window (an unresolvable surface or
 * current desktop, @c client_init itself failing, or the client
 * failing to be added to its desktop): the requesting application
 * gets its window on screen either way, just without a frame or any
 * window-manager tracking.
 *
 * @param connection XCB connection
 * @param window     Window to map as-is
 *
 * @note Complexity: @e O(1)
 */
static void s_map_unmanaged(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_map_window(connection, window);
    xcb_flush(connection);
}


/**
 * @brief Restore focus after the active client disappears from a desktop
 *
 * Selects the most recent visible focusable client in reverse stacking
 * order and focuses it.  If none is found, focus is released to pointer
 * root so keyboard grabs continue to work.
 */
static void s_restore_focus_after_client_loss(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, client_td *lost_client)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    bool focus_set = false;

    if (desktop == NULL || desktop->stacking == NULL) {
        return;
    }

    node = cdlist_tail(desktop->stacking);
    initial = node;
    if (node != NULL) {
        do {
            client_td *c = (client_td *) cdlist_data(node);
            if (c != NULL && c != lost_client &&
                    !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    !client_is_shaded(c) &&
                    c->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED &&
                    (c->properties.flags & CLIENT_FLAG_FOCUSABLE)) {
                desktop->client_active_id = c->id;
                desktop->focus_dirty = true;

                if (connection != NULL) {
                    xcb_set_input_focus(connection,
                            XCB_INPUT_FOCUS_PARENT,
                            c->window, XCB_CURRENT_TIME);
                }
                wm_outdate_desktop(desktop);
                wm_outdate_surface(surface);
                focus_set = true;
                break;
            }
            node = cdlist_prev(node);
        } while (node != NULL && node != initial);
    }

    if (!focus_set && connection != NULL) {
        xcb_set_input_focus(connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
        wm_outdate_desktop(desktop);
        wm_outdate_surface(surface);
    }
}


/* Handle a 'MAP_REQUEST' event */
void handler_map_request(wm_td *wm, xcb_map_request_event_t *event)
{
    surface_td *surface;
    desktop_td *desktop;
    client_td *client;
    uint32_t max_clients;

    if (wm == NULL || event == NULL) {
        LOGGER_ERROR("Received null pointer in map request handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Map request event (window=0x%x, parent=0x%x)",
            event->window, event->parent);

    if (lookup_find_client(wm->surfaces,
                event->window, NULL, NULL) != NULL) {
        LOGGER_TRACE("Window %#x already managed; mapping directly",
                event->window);

        s_map_unmanaged(wm->connection, event->window);
        return;
    }

    surface = lookup_surface_for_root(wm->surfaces, event->parent);
    if (surface == NULL && !list_is_empty(wm->surfaces)) {
        surface = (surface_td *) list_data(list_head(wm->surfaces));
    }
    if (surface == NULL) {
        LOGGER_ERROR("No surface found for 'MAP_REQUEST' on root %#x",
                event->parent);
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        LOGGER_ERROR("No current desktop on surface %u; mapping without"
                " management", surface->id);

        s_map_unmanaged(wm->connection, event->window);
        return;
    }

    /* Checked before 'client_init' does any of its own (comparatively
     * expensive) setup work, so a client refused here never pays for
     * work that would just be thrown away.  Deliberately left
     * unmapped, unlike every other early-return path in this function
     * that declines to manage a window: an unmanaged-but-mapped
     * window is genuinely broken, not merely undecorated, since it
     * has no frame, is not tracked in any client list, and cannot be
     * moved or closed through IcoWM at all; if it somehow ends up
     * with keyboard focus regardless (a real, mapped top-level window
     * can still receive it, even one IcoWM never decided to manage),
     * later code that assumes "whatever currently has focus is a
     * tracked client" has nothing valid to find, which is exactly
     * what produced the abrupt, broken behavior this comment used to
     * defend against creating in the first place.  Simply never
     * mapping the window instead means the requesting application is
     * left waiting for a MapNotify that will not come, rather than
     * being handed a window it cannot use through the one channel
     * (the window manager) applications normally rely on for that.
     * 'memguard_max_clients' already reads 0 as "restricted-memory
     * mode is off, no cap", so nothing else needs to check that
     * separately here. */
    max_clients = memguard_max_clients();

    if (max_clients > 0u &&
            ohtbl_size(desktop->clients) >= max_clients) {
        memguard_warn_client_cap(wm->connection, surface,
                wm->config);
        return;
    }

    client = client_init(wm->connection, wm->ewmh,
            event->window, &wm->config->theme, &wm->config->base);
    if (client == NULL) {
        s_map_unmanaged(wm->connection, event->window);
        return;
    }

    client->screen_id = surface->id;
    client->desktop_id = desktop->id;

    if (desktop_action_client_add(desktop, client) != 0) {
        LOGGER_ERROR("Failed to add client %#x to desktop %u",
                event->window, desktop->id);
        client->window = 0;
        client_destroy(client);
        s_map_unmanaged(wm->connection, event->window);
        return;
    }

    /* Advertise the desktop this client belongs to per EWMH */
    if (wm->ewmh != NULL) {
        uint32_t did = (client->properties.flags & CLIENT_FLAG_PIN)
            ? WM_DESKTOP_ID_ALL : desktop->id;

        xcb_change_property(wm->connection, XCB_PROP_MODE_REPLACE,
                client->window, wm->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &did);
    }

    /* Refresh work area in case the new client declares struts */
    surface_refresh_workareas(surface);

    /* Apply map-time rules before placement so explicit rule geometry
     * can lock the client position and exempt it from policy placement */
    if (rules_apply(wm, client, &surface, &desktop, RULES_TRIGGER_MAP)) {
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
    }

    /* Dock/panel windows self-position; do not override their geometry */
    if (client->properties.type != (uint16_t) CLIENT_TYPE_DOCK &&
            !client->rule_position_locked) {
        place_apply(wm, surface, client);
    }

    /* ICCCM §4.1.2.4: honor 'WM_HINTS' 'initial_state' when
     * 'IconicState' */
    if (client->initial_iconic) {
        ccmd_client_iconify(client);
    } else {
        if (client->titlebar != 0) {
            xcb_map_window(wm->connection, client->titlebar);
        }

        if (client->frame != 0) {
            xcb_map_window(wm->connection, client->frame);
            xcb_map_window(wm->connection, client->window);
        } else {
            xcb_map_window(wm->connection, event->window);
        }

        client_unhide(client);

        if (wm->config->base.windows.focus.is_new_focused &&
                client_is_focusable(client)) {
            focus_apply(wm->surfaces, surface, desktop, client, true,
                    wm->config);
        }

        /* ICCCM §4.2.3: after both place_apply and rules_apply have
         * settled the final frame position, send a synthetic
         * 'ConfigureNotify' with screen-relative coordinates to the
         * client so it knows its true screen position from the outset.
         *
         * For decorated clients the only 'ConfigureNotify' received so
         * far was generated by 'client_sync_decoration_layout' (called
         * inside 'ci_create_decorations'), which carries frame-relative
         * coordinates (x=border, y=titlebar+border) rather than the
         * actual on-screen position, causing misaligned popups and
         * imperfect initial viewport layout.
         *
         * For undecorated clients the X server supplies the correct
         * screen-relative coordinates, but some applications require an
         * explicit 'ConfigureNotify' to commit their initial layout;
         * without it a fragment of the window content can appear
         * outside the configured frame bounds until a focus change or
         * move forces a redraw. */
        client_send_synthetic_configure_notify(wm->connection, client);
        xcb_clear_area(wm->connection, 1, client->window, 0, 0, 0, 0);
    }

    /* Re-apply layer stacking so newly mapped windows do not obscure
     * clients already assigned to the above layer */
    ccmd_desktop_enforce_layers(desktop);

    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);
    xcb_flush(wm->connection);

    LOGGER_DEBUG("Mapped and adopted window %#x ('%s') on desktop %u",
            event->window, client->info.name, desktop->id);

    {
        cJSON *fields = cJSON_CreateObject();

        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) desktop->id);
            cJSON_AddNumberToObject(fields, "surface_id",
                    (double) surface->id);
        }
        ipc_broadcast_event(IPC_EVENT_WINDOW_MAPPED, fields);
    }
}


/* Handle an 'UNMAP_NOTIFY' event */
void handler_unmap_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_unmap_notify_event_t *event)
{
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in unmap handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Unmap notify event (window=0x%x)", event->window);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client != NULL) {
        if (event->window != client->window) {
            if (client->ignore_unmap > 0) {
                client->ignore_unmap--;
            }
            return;
        }

        if (client->ignore_unmap > 0) {
            client->ignore_unmap--;
            return;
        }

        if (desktop != NULL &&
                desktop->client_active_id == client->id) {
            desktop->client_active_id = 0;
            desktop->focus_dirty = true;
            s_restore_focus_after_client_loss(connection, surface,
                    desktop, client);
        }

        /* When a managed client withdraws itself (for example, to
         * a system tray), unmap every WM-created decoration and mark
         * the client hidden so later render passes never remap the
         * ghost frame */
        client_hide(client);
        if (client->frame != 0) {
            client->ignore_unmap++;
            xcb_unmap_window(client->connection, client->frame);
        }
        if (client->titlebar != 0) {
            client->ignore_unmap++;
            xcb_unmap_window(client->connection, client->titlebar);
        }
        ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC, XCB_NONE);
        ccmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
        ccmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");
        wm_request_client_redraw(client);

        if (connection != NULL) {
            xcb_flush(connection);
        }
    }
}


/* Handle a 'DESTROY_NOTIFY' event */
void handler_destroy_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_destroy_notify_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in destroy handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Destroy notify event (window=0x%x)", event->window);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client == NULL) {
        return;
    }

    if (event->window != client->window) {
        return;
    }

    /* Cancel any drag that was using this client */
    if (drag_is_active() && drag_client() == client) {
        drag_cancel(connection, client);
    }

    if (desktop != NULL && desktop->client_active_id == client->id) {
        desktop->client_active_id = 0;
        desktop->focus_dirty = true;
        s_restore_focus_after_client_loss(connection, surface,
                desktop, client);
        if (connection != NULL) {
            xcb_flush(connection);
        }
    }

    if (desktop != NULL) {
        desktop_action_client_rem(desktop, client);
        /* Refresh work area in case the removed client had struts */
        surface_refresh_workareas(surface);
    }

    /* ICCCM withdrawn state: remove WM_STATE on unmanage */
    ccmd_clear_wm_state(client);

    /* When the frame is destroyed the X server also destroys all its
     * children ('client->window', 'client->titlebar').  Zero them all
     * out so client_destroy does not issue redundant
     * 'xcb_destroy_window' calls. */
    if (event->window == client->frame) {
        client->frame = 0;
        client->titlebar = 0;
        client->window = 0;
    } else {
        /* The content window was destroyed (e.g., the client process
         * exited or the app closed without a prior 'UnmapNotify').
         * Immediately destroy the WM-created frame (which takes its
         * titlebar child with it) so no ghost frame is left on screen.
         * Increment 'ignore_unmap' so the 'UnmapNotify' the X server
         * generates for the mapped frame is swallowed and does not
         * re-enter the unmap handler.  Zero both pointers to prevent
         * client_destroy from issuing redundant destroy calls. */
        if (connection != NULL && client->frame != 0) {
            client->ignore_unmap++;
            xcb_destroy_window(connection, client->frame);
            xcb_flush(connection);
        }
        client->frame = 0;
        client->titlebar = 0;
        client->window = 0;
    }

    {
        cJSON *fields = cJSON_CreateObject();

        if (fields != NULL) {
            cJSON_AddNumberToObject(fields, "client_id",
                    (double) client->id);
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) desktop->id);
            cJSON_AddNumberToObject(fields, "surface_id",
                    (double) surface->id);
        }
        ipc_broadcast_event(IPC_EVENT_WINDOW_CLOSED, fields);
    }

    client_destroy(client);

    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);

    LOGGER_DEBUG("Removed destroyed window %#x", event->window);
}


/* Handle a 'MAP_NOTIFY' event */
void handler_map_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_map_notify_event_t *event)
{
    client_td *client;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in map notify handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Map notify event (window=0x%x," \
            " override-redirect=%u)",
            event->window, event->override_redirect);

    /* Unmanaged override-redirect windows (tooltips, menus) are
     * intentionally ignored; only managed clients need a EWMH resync */
    if (event->override_redirect != 0) {
        return;
    }

    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client != NULL) {
        wm_request_client_redraw(client);

        /* ICCCM §4.2.3: re-send the synthetic 'ConfigureNotify' at
         * 'MapNotify' time so that some programs have the correct
         * screen-relative position in their event queue before their
         * first 'Expose'-driven draw.  The primary fix for the
         * initial-open misalignment is in 'desktop_render_clients',
         * which sends a synthetic 'ConfigureNotify' after every
         * inner-window configure to ensure the screen-relative one is
         * always the last event the client receives. */
        if (event->window == client->window &&
                client->frame != 0 &&
                client_is_decorated(client)) {
            client_send_synthetic_configure_notify(connection, client);
        }

        /* Force the content window to repaint immediately after it
         * becomes visible.  Applications like gVim do not repaint on
         * 'ConfigureNotify' or 'MapNotify' alone; without an 'Expose'
         * event the lower portion of the window may remain blank until
         * the user triggers a focus change or another redraw action.
         * Using 'exposures=1' causes the X server to generate an
         * 'Expose' event so the application redraws the full client
         * area from the outset. */
        if (event->window == client->window) {
            xcb_clear_area(connection, 1, client->window, 0, 0, 0, 0);

            /* Re-assert the plain-pointer cursor 'client_init'
             * already set once on this same window (see client.c).
             * Many GTK/GDK applications explicitly set their own
             * top-level window's cursor as part of their own
             * realization, which can run after (and so silently
             * overwrite) that first assignment; MapNotify, confirming
             * the window has actually become visible, is reliably
             * later than that realization, so setting it again here
             * wins whatever race existed. */
            xcb_change_window_attributes(connection, client->window,
                    XCB_CW_CURSOR,
                    (const uint32_t[]) { mouse_plain_cursor() });
            LOGGER_TRACE("Set cursor (window=0x%x, cursor=0x%x)",
                    client->window, mouse_plain_cursor());
        }
        xcb_flush(connection);
    }
}


/* Handle a 'GRAVITY_NOTIFY' event */
void handler_gravity_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_gravity_notify_event_t *event)
{
    client_td *client;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in gravity notify handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Gravity notify event (window=0x%x, pos=%d+%d)",
            event->window, event->x, event->y);

    /* The X server repositioned a frame window ('event->window') within
     * root because the screen was resized and the client's win_gravity
     * ('client->layout.gravity') placed it at a non-NW anchor.
     * Update the cached frame position and re-sync decorations. */
    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client != NULL) {
        client->layout.geometry.cur.pos.x = event->x;
        client->layout.geometry.cur.pos.y = event->y;
        client_sync_decoration_layout(client);
        wm_request_client_redraw(client);

        /* ICCCM §4.2.3: the frame moved, so the client's
         * screen-relative position changed; notify it with correct
         * coordinates */
        if (client->frame != 0 && client_is_decorated(client)) {
            client_send_synthetic_configure_notify(connection, client);
        }
    }
}


/* Handle a 'CIRCULATE_NOTIFY' event */
void handler_circulate_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_notify_event_t *event)
{
    surface_td *surface;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in circulate notify",
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Circulate notify event (window=0x%x, place=%u)",
            event->window, event->place);

    /* The stacking order changed; mark the surface so 'wm_ewmh_sync'
     * updates '_NET_CLIENT_LIST_STACKING' on the next iteration */
    surface = lookup_surface_for_root(surfaces, event->event);
    wm_outdate_surface(surface);
}


/* Handle a 'CIRCULATE_REQUEST' event (ICCCM §4.1.7) */
void handler_circulate_request(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_request_event_t *event)
{
    client_td *client;
    xcb_window_t target;
    uint32_t stack_mode;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in circulate request" \
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Circulate request event (window=0x%x, place=%u)",
            event->window, event->place);

    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client == NULL) {
        return;
    }

    target = ccmd_target_win(client);
    stack_mode = (event->place == XCB_PLACE_ON_TOP)
        ? (uint32_t) XCB_STACK_MODE_ABOVE
        : (uint32_t) XCB_STACK_MODE_BELOW;

    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    xcb_flush(connection);
    wm_request_client_redraw(client);
}
