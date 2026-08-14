/**
 * @file enact.c
 *
 * @brief Every action this window manager can carry out, one typed
 *        function per action
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
#include <stdio.h>      /* snprintf */

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <scratchpad.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Command includes */
#include <cmds/client/basic.h>
#include <cmds/client/geom.h>
#include <cmds/client/layer.h>
#include <cmds/client/meta.h>
#include <cmds/surface.h>

/* Menu includes */
#include <menu/cycle.h>
#include <menu/dialog/info.h>

/* Policy includes */
#include <policy/placement.h>

/* Handler includes */
#include <handler/internal.h>

/* Local includes */
#include <enact.h>


/**
 * @brief Broadcast an event whose payload is just the standard
 *        client/desktop/surface identifier triple
 *
 * Shared by every 'enact_client_*' action below whose own IPC event
 * needs nothing beyond identifying which client it happened to and
 * where; avoids repeating the same three-field 'cJSON' object at
 * each of those call sites individually. Not used by an action whose
 * own event payload needs anything more than these three fields
 * (e.g., a renamed client's own new name).
 *
 * @param client Client the event happened to; a NULL client is a
 *               silent no-op, matching every caller's own existing
 *               'if (client != NULL)' guard around its own
 *               'xcb_flush'
 * @param type   Which event this is
 *
 * @note Complexity: @e O(1)
 */
static void s_broadcast_client_event(client_td *client,
        uint32_t type)
{
    cJSON *fields;

    if (client == NULL) {
        return;
    }

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "client_id",
                (double) client->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) client->desktop_id);
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) client->screen_id);
    }
    ipc_broadcast_event(type, fields);
}


/**
 * @brief Broadcast an event whose payload is just the standard
 *        desktop/surface identifier pair, with no specific client
 *        involved
 *
 * @param desktop Desktop the event happened to; a NULL desktop is a
 *                silent no-op
 * @param type    Which event this is
 *
 * @note Complexity: @e O(1)
 */
static void s_broadcast_desktop_event(desktop_td *desktop,
        uint32_t type)
{
    cJSON *fields;

    if (desktop == NULL) {
        return;
    }

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) desktop->id);
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) desktop->screen_id);
    }
    ipc_broadcast_event(type, fields);
}


/* == action_client_e == */

/* Close the client's window */
void enact_client_close(client_td *client)
{
    ccmd_client_close(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Forcibly kill the client's owning connection */
void enact_client_kill(client_td *client)
{
    ccmd_client_kill(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Restore the client to its normal state */
void enact_client_restore(client_td *client)
{
    ccmd_client_restore(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_CLIENT_DEICONIFIED);
    }
}


/* Give input focus to the client */
void enact_client_focus(client_td *client)
{
    ccmd_client_focus(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Take input focus away from the client */
void enact_client_unfocus(client_td *client)
{
    ccmd_client_unfocus(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        if (scratchpad_is_client(client) && !client_is_hidden(client)) {
            enact_client_hide(client);
        }
    }
}


/* Resize the client to a specific frame geometry */
void enact_client_resize(client_td *client, int32_t x, int32_t y,
        uint32_t w, uint32_t h)
{
    ccmd_client_resize(client, x, y, w, h);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Move the client to a specific position */
void enact_client_move(client_td *client, int32_t x, int32_t y)
{
    ccmd_client_move(client, x, y);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Center the client on its current screen */
void enact_client_center(client_td *client)
{
    ccmd_client_center(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Move the client to the next monitor on its surface */
void enact_client_move_next_monitor(client_td *client)
{
    ccmd_client_move_to_next_monitor(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Move the client to a specific monitor index */
void enact_client_move_to_monitor(client_td *client,
        uint32_t monitor_index)
{
    ccmd_client_move_to_monitor(client, monitor_index);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_WINDOW_MOVED);
    }
}


/* Change the client's 'WM_CLASS' class and instance names */
void enact_client_reclass(client_td *client, const char *class_name,
        const char *instance_name)
{
    ccmd_client_reclass(client, class_name, instance_name);
    if (client != NULL) {
        xcb_flush(client->connection);

        {
            cJSON *fields = cJSON_CreateObject();

            if (fields != NULL) {
                cJSON_AddNumberToObject(fields, "client_id",
                        (double) client->id);
                cJSON_AddNumberToObject(fields, "desktop_id",
                        (double) client->desktop_id);
                cJSON_AddNumberToObject(fields, "surface_id",
                        (double) client->screen_id);
                cJSON_AddStringToObject(fields, "class_name",
                        (class_name != NULL) ? class_name : "");
                cJSON_AddStringToObject(fields, "instance_name",
                        (instance_name != NULL) ? instance_name : "");
            }
            ipc_broadcast_event(IPC_EVENT_CLIENT_RECLASSED, fields);
        }
    }
}


/* Change the client's 'WM_WINDOW_ROLE' */
void enact_client_rerole(client_td *client, const char *role)
{
    ccmd_client_rerole(client, role);
    if (client != NULL) {
        xcb_flush(client->connection);

        {
            cJSON *fields = cJSON_CreateObject();

            if (fields != NULL) {
                cJSON_AddNumberToObject(fields, "client_id",
                        (double) client->id);
                cJSON_AddNumberToObject(fields, "desktop_id",
                        (double) client->desktop_id);
                cJSON_AddNumberToObject(fields, "surface_id",
                        (double) client->screen_id);
                cJSON_AddStringToObject(fields, "role",
                        (role != NULL) ? role : "");
            }
            ipc_broadcast_event(IPC_EVENT_CLIENT_REROLED, fields);
        }
    }
}


/* Rename the client's window title */
void enact_client_rename(client_td *client, const char *name)
{
    ccmd_client_rename(client, name);
    if (client != NULL) {
        xcb_flush(client->connection);

        {
            cJSON *fields = cJSON_CreateObject();

            if (fields != NULL) {
                cJSON_AddNumberToObject(fields, "client_id",
                        (double) client->id);
                cJSON_AddNumberToObject(fields, "desktop_id",
                        (double) client->desktop_id);
                cJSON_AddNumberToObject(fields, "surface_id",
                        (double) client->screen_id);
                cJSON_AddStringToObject(fields, "name",
                        (name != NULL) ? name : "");
            }
            ipc_broadcast_event(IPC_EVENT_CLIENT_RENAMED, fields);
        }
    }
}


/* Maximize the client both horizontally and vertically */
void enact_client_maximize(client_td *client)
{
    ccmd_client_maximize(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Maximize the client horizontally only */
void enact_client_maximize_horz(client_td *client)
{
    ccmd_client_maximize_horz(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Maximize the client vertically only */
void enact_client_maximize_vert(client_td *client)
{
    ccmd_client_maximize_vert(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_WINDOW_RESIZED);
    }
}


/* Iconify the client */
void enact_client_iconify(client_td *client)
{
    ccmd_client_iconify(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_CLIENT_ICONIFIED);
    }
}


/* Hide the client's window */
void enact_client_hide(client_td *client)
{
    ccmd_client_hide(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_HIDE_SET);
    }
}


/* Show a previously hidden client */
void enact_client_unhide(client_td *client)
{
    ccmd_client_unhide(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_HIDE_CLEARED);
    }
}


/* Shade (roll up) the client */
void enact_client_shade(client_td *client)
{
    ccmd_client_shade(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_SHADE_SET);
    }
}


/* Unshade (roll down) the client */
void enact_client_unshade(client_td *client)
{
    ccmd_client_unshade(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_SHADE_CLEARED);
    }
}


/* Toggle the client's shade status */
void enact_client_toggle_shade(client_td *client)
{
    ccmd_client_toggle_shade(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, client_is_shaded(client)
                ? IPC_EVENT_SHADE_SET : IPC_EVENT_SHADE_CLEARED);
    }
}


/* Set the client's pin mode */
void enact_client_pin(client_td *client)
{
    ccmd_client_pin(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_PIN_SET);
    }
}


/* Remove the client's pin mode */
void enact_client_unpin(client_td *client)
{
    ccmd_client_unpin(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_PIN_CLEARED);
    }
}


/* Toggle the client's pin mode */
void enact_client_toggle_pin(client_td *client)
{
    ccmd_client_toggle_pin(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, client_is_pinned(client)
                ? IPC_EVENT_PIN_SET : IPC_EVENT_PIN_CLEARED);
    }
}


/* Set the client to full screen mode */
void enact_client_fullscreen(client_td *client)
{
    ccmd_client_fullscreen(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_FULLSCREEN_SET);
    }
}


/* Remove the client's full screen mode */
void enact_client_unfullscreen(client_td *client)
{
    ccmd_client_unfullscreen(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_FULLSCREEN_CLEARED);
    }
}


/* Toggle the client's full screen mode */
void enact_client_toggle_fullscreen(client_td *client)
{
    ccmd_client_toggle_fullscreen(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, client_is_fullscreen(client)
                ? IPC_EVENT_FULLSCREEN_SET : IPC_EVENT_FULLSCREEN_CLEARED);
    }
}


/* Raise the client to the top of its layer */
void enact_client_raise(client_td *client)
{
    ccmd_client_raise(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
    }
}


/* Lower the client to the bottom of its layer */
void enact_client_lower(client_td *client)
{
    ccmd_client_lower(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
    }
}


/* Move the client to the always-on-top layer */
void enact_client_layer_above(client_td *client)
{
    ccmd_client_layer_above(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Move the client to the normal layer */
void enact_client_layer_normal(client_td *client)
{
    ccmd_client_layer_normal(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Move the client to the always-on-bottom layer */
void enact_client_layer_below(client_td *client)
{
    ccmd_client_layer_below(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Cycle the client through the normal/above/below layers */
void enact_client_cycle_layer(client_td *client)
{
    ccmd_client_cycle_layer(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, IPC_EVENT_LAYER_CHANGED);
    }
}


/* Mark the client as urgent */
void enact_client_urge(client_td *client)
{
    ccmd_client_urge(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Clear the client's urgency level */
void enact_client_unurge(client_td *client)
{
    ccmd_client_unurge(client);
    if (client != NULL) {
        xcb_flush(client->connection);
    }
}


/* Set the client's icon name */
void enact_client_set_icon(client_td *client, const char *icon_name)
{
    ccmd_client_set_icon(client, icon_name);
    if (client != NULL) {
        xcb_flush(client->connection);

        {
            cJSON *fields = cJSON_CreateObject();

            if (fields != NULL) {
                cJSON_AddNumberToObject(fields, "client_id",
                        (double) client->id);
                cJSON_AddNumberToObject(fields, "desktop_id",
                        (double) client->desktop_id);
                cJSON_AddNumberToObject(fields, "surface_id",
                        (double) client->screen_id);
                cJSON_AddStringToObject(fields, "icon_name",
                        (icon_name != NULL) ? icon_name : "");
            }
            ipc_broadcast_event(IPC_EVENT_CLIENT_ICON_CHANGED, fields);
        }
    }
}


/* Toggle the client's decoration */
void enact_client_toggle_decorate(client_td *client)
{
    ccmd_client_toggle_decorate(client);
    if (client != NULL) {
        xcb_flush(client->connection);
        s_broadcast_client_event(client, client_is_decorated(client)
                ? IPC_EVENT_DECORATION_SET : IPC_EVENT_DECORATION_CLEARED);
    }
}


/* == action_desktop_e == */

/* Set the desktop's background color */
void enact_desktop_set_background(desktop_td *desktop, uint32_t color)
{
    surface_td *surface;

    if (desktop == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(desktop->screen_id);
    if (surface == NULL) {
        return;
    }

    desktop->background.is_image = false;
    desktop->background.use_root_pixmap = false;
    desktop->background.bg.color = color;
    desktop->is_outdated = true;
    /* Marking only 'desktop->is_outdated' is not enough on its own:
     * 'loop_update' only calls 'surface_render_all_desktops' at all
     * when this desktop's own surface is itself outdated (see
     * 'enact_desktop_show', right below, for the same pattern);
     * without this, the new color never actually repaints until
     * something else marks the surface outdated for an unrelated
     * reason, e.g., switching desktops away and back. */
    surface->is_outdated = true;
    xcb_flush(desktop->connection);
    s_broadcast_desktop_event(desktop,
            IPC_EVENT_DESKTOP_BACKGROUND_CHANGED);
}


/* Toggle whether the desktop's own surface shows the desktop */
void enact_desktop_show(desktop_td *desktop, bool show)
{
    surface_td *surface;

    if (desktop == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(desktop->screen_id);
    if (surface == NULL) {
        return;
    }

    hi_handle_net_showing_desktop(surface, show);
    xcb_flush(surface->connection);
    s_broadcast_desktop_event(desktop,
            (show) ? IPC_EVENT_DESKTOP_SHOWN : IPC_EVENT_DESKTOP_HIDDEN);
}


/* Add a client to the desktop */
void enact_desktop_client_add(desktop_td *desktop, client_td *client)
{
    if (desktop == NULL || client == NULL) {
        return;
    }

    desktop_action_client_add(desktop, client);
    xcb_flush(desktop->connection);
}


/* Remove a client from the desktop */
void enact_desktop_client_remove(desktop_td *desktop, client_td *client)
{
    if (desktop == NULL || client == NULL) {
        return;
    }

    desktop_action_client_rem(desktop, client);
    xcb_flush(desktop->connection);
}


/* Send a client from one desktop to another */
void enact_desktop_client_send(desktop_td *desktop, client_td *client,
        desktop_td *target)
{
    surface_td *surface;
    xcb_window_t win_target;

    if (desktop == NULL || client == NULL || target == NULL) {
        return;
    }

    LOGGER_TRACE("Sending client window=0x%x from desktop %u to" \
            " desktop %u", client->window, desktop->id, target->id);

    /* If the client is currently visible on the active desktop, unmap
     * it immediately so it disappears from the source desktop without
     * waiting for the user to switch away */
    surface = wm_get_surface_by_id(client->screen_id);
    if (surface != NULL &&
            desktop->id == surface->desktop_cur &&
            !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
            client->properties.state != (uint16_t) CLIENT_STATE_ICONIFIED) {
        win_target = (client_is_decorated(client) && client->frame != 0)
            ? client->frame : client->window;
        client->ignore_unmap += 2u;
        if (client->titlebar != 0) {
            client->ignore_unmap += 1u;
            xcb_unmap_window(surface->connection, client->titlebar);
        }
        xcb_unmap_window(surface->connection, win_target);
        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_unmap_window(surface->connection, client->icon_window);
            client->is_icon_mapped = false;
        }
        xcb_flush(surface->connection);
    }

    desktop_action_client_rem(desktop, client);
    desktop_action_client_add(target, client);
    client->desktop_id = target->id;
    xcb_flush(desktop->connection);
    s_broadcast_client_event(client, IPC_EVENT_CLIENT_DESKTOP_CHANGED);
}


/* Send a client to the front of the desktop's window stack */
void enact_desktop_client_send_front(desktop_td *desktop,
        client_td *client)
{
    if (desktop == NULL || client == NULL) {
        return;
    }

    (void) desktop_action_client_send_front(desktop, client);
    xcb_flush(desktop->connection);
    s_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
}


/* Send a client to the back of the desktop's window stack */
void enact_desktop_client_send_back(desktop_td *desktop,
        client_td *client)
{
    if (desktop == NULL || client == NULL) {
        return;
    }

    (void) desktop_action_client_send_back(desktop, client);
    xcb_flush(desktop->connection);
    s_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
}


/* Re-apply the configured placement policy to every client on the
 * desktop */
void enact_desktop_clients_rearrange(wm_td *wm, surface_td *surface,
        desktop_td *desktop)
{
    cdlist_item_td *node;
    enum config_placement_policy_e policy;
    bool single_spot_policy;
    bool is_first;

    if (wm == NULL || wm->config == NULL || surface == NULL ||
            desktop == NULL) {
        return;
    }

    policy = wm->config->base.windows.placement_policy;
    single_spot_policy =
        (policy == CONFIG_PLACEMENT_POLICY_CENTERED) ||
        (policy == CONFIG_PLACEMENT_POLICY_UNDER_MOUSE);
    is_first = true;

    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        /* 'desktop->stacking' is circular (see 'cdlist_next''s own
         * doc comment in adt/cdlist.h): its own tail wraps back to
         * its own head rather than ever handing back NULL, so a
         * caller has to remember where it started and stop once it
         * gets back there, the same 'initial' pattern already used
         * to walk this same list elsewhere (e.g.,
         * 'desktop_action_client_rem' in desktop/dclient.c).  A
         * plain 'for (...; node != NULL; ...)' loop over it, as this
         * one used to be, never terminates for a non-empty desktop:
         * it silently spins inside this one call forever, which
         * blocks the whole event loop (this function's own caller
         * runs synchronously from it) from ever processing another
         * key press, mouse click, or menu, until the process is
         * killed from outside. */
        cdlist_item_td *initial = node;

        do {
            client_td *client = (client_td *) cdlist_data(node);

            if (client != NULL && !client_is_locked(client)) {
                /* Every client on the desktop goes through
                 * 'place_apply' / 'place_apply_cascade', the same
                 * general-purpose placement engine a newly mapped
                 * window is run through, not a simplified
                 * rearrange-only positioning routine.  That means a
                 * transient dialog among them (a client with its own
                 * 'transient_for' set) is not repositioned by the
                 * configured placement policy below at all:
                 * 'place_apply' re-centers it over its own parent
                 * per ICCCM §4.1.2.6 instead, the same as it would
                 * have been placed there in the first place.
                 * Finding that parent is why this function needs the
                 * full 'wm_td' rather than just 'desktop' or
                 * 'config': the parent can live on a different
                 * surface entirely, so locating it means searching
                 * 'wm->surfaces' as a whole (see
                 * 's_place_transient_centered' in policy/
                 * placement.c). */

                /* 'centered'/'under-mouse' always resolve to the
                 * exact same single spot, so every client after the
                 * first would land stacked on top of one another;
                 * only the first client uses the real configured
                 * policy, the rest fall back to cascade so the
                 * desktop ends up spread out instead of piled up */
                if (single_spot_policy && !is_first) {
                    place_apply_cascade(wm, surface, client);
                } else {
                    place_apply(wm, surface, client);
                }
                is_first = false;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    xcb_flush(surface->connection);
}


/* Iconify every client on the desktop */
void enact_desktop_clients_iconify_all(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_clients_iconify_all(desktop);
    xcb_flush(desktop->connection);
}


/* Restore every iconified client on the desktop */
void enact_desktop_clients_deiconify_all(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_clients_deiconify_all(desktop);
    xcb_flush(desktop->connection);
}


/* Cycle input focus to the next non-iconified client */
void enact_desktop_cycle_clients_active(list_td *surfaces,
        xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, uint16_t modifier, const config_td *cfg)
{
    if (surface == NULL || desktop == NULL) {
        return;
    }

    cycle_init(surfaces, connection, surface, desktop, false, 1,
            modifier, cfg);
    cycle_draw(connection, cfg);
    xcb_flush(connection);
}


/* Cycle input focus to the previous non-iconified client */
void enact_desktop_cycle_clients_prev(list_td *surfaces,
        xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, uint16_t modifier, const config_td *cfg)
{
    if (surface == NULL || desktop == NULL) {
        return;
    }

    cycle_init(surfaces, connection, surface, desktop, false, -1,
            modifier, cfg);
    cycle_draw(connection, cfg);
    xcb_flush(connection);
}


/* Cycle input focus to the next iconified client */
void enact_desktop_cycle_clients_icons_next(list_td *surfaces,
        xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, uint16_t modifier, const config_td *cfg)
{
    if (surface == NULL || desktop == NULL) {
        return;
    }

    cycle_init(surfaces, connection, surface, desktop, true, 1,
            modifier, cfg);
    cycle_draw(connection, cfg);
    xcb_flush(connection);
}


/* Cycle input focus to the previous iconified client */
void enact_desktop_cycle_clients_icons_prev(list_td *surfaces,
        xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, uint16_t modifier, const config_td *cfg)
{
    if (surface == NULL || desktop == NULL) {
        return;
    }

    cycle_init(surfaces, connection, surface, desktop, true, -1,
            modifier, cfg);
    cycle_draw(connection, cfg);
    xcb_flush(connection);
}


/* Launch a program associated with the desktop */
pid_t enact_desktop_command_launch(desktop_td *desktop,
        const char *command)
{
    int result;
    char msg[256];
    surface_td *surface;
    const config_td *config;

    if (desktop == NULL || command == NULL) {
        return -1;
    }

    result = desktop_action_process_launch(desktop, command);

    if (result == -2) {
        /* 'execvp' failed: already logged by
         * 'desktop_action_process_launch'.  Also show an informational
         * dialog so the user gets feedback. */
        (void) snprintf(msg, sizeof(msg), "Cannot launch: '%s'", command);

        surface = wm_get_surface_by_id(desktop->screen_id);
        config = (surface != NULL) ? surface->config : NULL;
        if (surface != NULL && config != NULL &&
                surface->connection != NULL) {
            dialog_info_show(surface->connection, surface, config,
                    msg, MENU_MSG_LEVEL_WARNING);
        }
        return -1;
    }

    return (pid_t) result;
}


/* == action_surface_e == */

/* Switch the surface to a specific desktop */
/**
 * @brief Broadcast a 'desktop_switched' event for the surface's own
 *        current desktop, as it stands right now
 *
 * Shared by @c enact_surface_desktop_switch and both of its own
 * next/prev siblings just below, all three of which change the same
 * one thing (which desktop is current) and so broadcast the exact
 * same event afterward, differing only in how they got there.
 *
 * @param surface The surface whose own current desktop just changed
 *
 * @note Complexity: @e O(1)
 */
static void s_broadcast_desktop_switched(const surface_td *surface)
{
    cJSON *fields = cJSON_CreateObject();

    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) surface->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) surface->desktop_cur);
    }
    ipc_broadcast_event(IPC_EVENT_DESKTOP_SWITCHED, fields);
}


void enact_surface_desktop_switch(surface_td *surface, uint32_t desktop_id)
{
    scmd_surface_desktop_switch(surface, desktop_id);
    s_broadcast_desktop_switched(surface);
}


/* Switch the surface to the next desktop, in cyclic order */
void enact_surface_desktop_switch_next(surface_td *surface)
{
    scmd_surface_desktop_switch_next(surface);
    s_broadcast_desktop_switched(surface);
}


/* Switch the surface to the previous desktop, in cyclic order */
void enact_surface_desktop_switch_prev(surface_td *surface)
{
    scmd_surface_desktop_switch_prev(surface);
    s_broadcast_desktop_switched(surface);
}


/* == action_wm_e == */

/* Request that the window manager stop and exit */
int enact_wm_exit(void)
{
    return wm_request_stop();
}


/* Reload the window manager's configuration */
int enact_wm_configuration_reload(void)
{
    int status = wm_action_config_reload();

    if (status == 0) {
        ipc_broadcast_event(IPC_EVENT_CONFIG_RELOADED, NULL);
    }
    return status;
}
