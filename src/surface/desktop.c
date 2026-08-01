/**
 * @file surface/desktop.c
 *
 * @brief Surface-level desktop and client operations implementation
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

/* ADT includes */
#include <adt/cdlist.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>


/* Unmap all non-sticky clients on the specified desktop */
void surface_clients_hide(surface_td *surface, uint32_t desktop_id)
{
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;

    if (surface == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, desktop_id);
    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return;
    }

    initial = node;
    do {
        client_td *client = (client_td *) cdlist_data(node);
        if (client != NULL &&
                !(client->properties.flags & CLIENT_FLAG_STICKY)) {
            xcb_window_t target =
                (client_is_decorated(client) && client->frame != 0)
                ? client->frame
                : client->window;

            /* Track WM-initiated unmaps so handler_unmap_notify skips
             * them */
            client->ignore_unmap += 1u;
            if (target != client->window) {
                client->ignore_unmap += 1u;
            }

            if (client->titlebar != 0) {
                xcb_unmap_window(surface->connection, client->titlebar);
            }

            /* For decorated clients, unmapping the frame also unmaps
             * its child client window; issuing an extra unmap on the
             * child would duplicate UnmapNotify handling and may
             * overwrite the remembered active client during desktop
             * switches. */
            xcb_unmap_window(surface->connection, target);

            if (client->icon_window != 0 && client->is_icon_mapped) {
                xcb_unmap_window(surface->connection, client->icon_window);
                client->is_icon_mapped = false;
            }
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);
}


/* Map all visible (non-hidden, non-iconified) clients on the specified
 * desktop */
void surface_clients_show(surface_td *surface, uint32_t desktop_id)
{
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;
    bool focus_restored;
    client_td *focus_target;

    if (surface == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, desktop_id);
    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return;
    }

    initial = node;
    do {
        client_td *client = (client_td *) cdlist_data(node);
        if (client != NULL &&
                !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                client->properties.state !=
                    (uint16_t) CLIENT_STATE_ICONIFIED) {
            xcb_window_t target =
                (client_is_decorated(client) && client->frame != 0)
                ? client->frame
                : client->window;
            if (client->titlebar != 0) {
                xcb_map_window(surface->connection, client->titlebar);
            }
            xcb_map_window(surface->connection, target);
            if (target != client->window) {
                xcb_map_window(surface->connection, client->window);
            }
        } else if (client != NULL &&
                client->properties.state ==
                    (uint16_t) CLIENT_STATE_ICONIFIED &&
                client->icon_window != 0) {
            xcb_map_window(surface->connection, client->icon_window);
            xcb_configure_window(surface->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) { XCB_STACK_MODE_BELOW });
            client->is_icon_mapped = true;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    /* Restore Z-order: iterate from head (bottom) to tail (top),
     * raising each window to the top so the tail (topmost client) ends
     * up at the top of the X11 stacking order when all windows are
     * shown */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        initial = node;
        do {
            client_td *c = (client_td *) cdlist_data(node);
            if (c != NULL &&
                    !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    c->properties.state !=
                    (uint16_t) CLIENT_STATE_ICONIFIED) {
                xcb_window_t tgt =
                    (client_is_decorated(c) && c->frame != 0)
                    ? c->frame : c->window;
                xcb_configure_window(surface->connection, tgt,
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    /* Restore input focus to the previously active client.
     * If no suitable client is found, relinquish focus to PointerRoot
     * so the previous desktop's windows do not retain keyboard input. */
    focus_restored = false;
    focus_target = NULL;
    if (desktop->client_active_id != 0) {
        node = cdlist_head(desktop->stacking);
        if (node != NULL) {
            initial = node;
            do {
                client_td *c = (client_td *) cdlist_data(node);
                if (c != NULL && c->id == desktop->client_active_id &&
                        !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                        !client_is_shaded(c) &&
                        c->properties.state !=
                            (uint16_t) CLIENT_STATE_ICONIFIED &&
                        (c->properties.flags &
                             CLIENT_FLAG_FOCUSABLE)) {
                    focus_target = c;
                    break;
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }
    }


    if (focus_target == NULL && desktop->stacking != NULL) {
        node = cdlist_tail(desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                client_td *c = (client_td *) cdlist_data(node);
                if (c != NULL &&
                        !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                        !client_is_shaded(c) &&
                        c->properties.state !=
                            (uint16_t) CLIENT_STATE_ICONIFIED &&
                        (c->properties.flags &
                             CLIENT_FLAG_FOCUSABLE)) {
                    focus_target = c;
                    break;
                }
                node = cdlist_prev(node);
            } while (node != NULL && node != initial);
        }
    }

    if (focus_target != NULL) {
        desktop->client_active_id = focus_target->id;
        xcb_set_input_focus(surface->connection,
                XCB_INPUT_FOCUS_PARENT,
                focus_target->window, XCB_CURRENT_TIME);
        (void) desktop_action_client_send_front(desktop, focus_target);
        focus_restored = true;
    }

    if (!focus_restored) {
        desktop->client_active_id = 0;
        xcb_set_input_focus(surface->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);

    }

    desktop->is_outdated = true;

}


/* Move all sticky clients from every other desktop to the target
 * desktop */
void surface_clients_sticky_transfer_all(surface_td *surface,
        uint32_t to_id)
{
    cdlist_item_td *dnode;
    cdlist_item_td *dinitial;
    cdlist_item_td *cnode;
    cdlist_item_td *cinitial;
    desktop_td *to_desktop;
    desktop_td *from_desktop;
    client_td *sticky[32];

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return;
    }

    to_desktop = surface_desktop_get(surface, to_id);
    if (to_desktop == NULL) {
        return;
    }

    dnode = cdlist_head(surface->desktops);
    if (dnode == NULL) {
        return;
    }

    dinitial = dnode;
    do {
        from_desktop = (desktop_td *) cdlist_data(dnode);
        if (from_desktop != NULL && from_desktop != to_desktop &&
                from_desktop->stacking != NULL &&
                cdlist_size(from_desktop->stacking) > 0) {
            /* Collect sticky clients first to avoid modifying the
             * stacking list while iterating it. */
            int n = 0;

            cnode = cdlist_head(from_desktop->stacking);
            cinitial = cnode;
            do {
                client_td *c = (client_td *) cdlist_data(cnode);

                if (c != NULL && client_is_sticky(c) &&
                        n < (int) (sizeof(sticky) / sizeof(sticky[0]))) {
                    sticky[n++] = c;
                }
                cnode = cdlist_next(cnode);
            } while (cnode != NULL && cnode != cinitial);

            for (int i = 0; i < n; i++) {
                bool was_active =
                    (from_desktop->client_active_id == sticky[i]->id);
                if (was_active) {
                    from_desktop->client_active_id = 0;
                }

                desktop_action_client_rem(from_desktop, sticky[i]);
                desktop_action_client_add(to_desktop, sticky[i]);
                sticky[i]->desktop_id = to_id;

                /* Preserve focus: if this sticky client was the active
                 * window on the source desktop, make it active on the
                 * destination desktop so 'surface_clients_show'
                 * restores input focus to it */
                if (was_active) {
                    to_desktop->client_active_id = sticky[i]->id;
                }
            }
        }

        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


/* Add a new desktop to the surface */
int surface_action_desktop_add(surface_td *surface)
{
    desktop_td *desktop;

    LOGGER_DEBUG("Adding new desktop to surface %u", surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    desktop = desktop_init(surface->connection,
            surface->ewmh,
            surface->id,
            surface->desktop_count,
            &(surface->config->base),
            &(surface->config->theme));
    if (desktop == NULL) {
        LOGGER_ERROR("Failed to initialize new desktop on surface %u",
                surface->id);
        return 1;
    }

    if (surface_desktop_add(surface, desktop) != 0) {
        LOGGER_ERROR("Failed to add desktop to surface %u", surface->id);
        desktop_destroy(desktop);
        return 1;
    }

    surface->is_outdated = true;

    return 0;
}


/* Remove the last desktop from the surface */
int surface_action_desktop_remove(surface_td *surface)
{
    cdlist_item_td *tail_item;
    desktop_td *desktop;

    LOGGER_DEBUG("Removing desktop from surface %u", surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Need at least two desktops to remove one */
    if (surface->desktop_count <= 1) {
        LOGGER_NOTICE("Cannot remove the last desktop on surface %u",
                surface->id);
        return 1;
    }

    tail_item = cdlist_tail(surface->desktops);
    if (tail_item == NULL) {
        return 1;
    }

    desktop = (desktop_td *) cdlist_data(tail_item);
    if (desktop == NULL) {
        return 1;
    }

    /* If the desktop to be removed is the current one, switch first */
    if (desktop->id == surface->desktop_cur) {
        surface_clients_hide(surface, surface->desktop_cur);
        surface_desktop_select_prev(surface, false);
        surface_clients_show(surface, surface->desktop_cur);
    }

    if (surface_desktop_rem(surface, desktop->id) != 0) {
        LOGGER_ERROR("Failed to remove desktop from surface %u",
                surface->id);
        return 1;
    }

    surface->is_outdated = true;

    return 0;
}


/* Switch to a specific desktop by ID */
int surface_action_desktop_switch(surface_td *surface,
        uint32_t desktop_id)
{
    uint32_t old_id;

    LOGGER_DEBUG("Switching to desktop %u on surface %u",
            desktop_id, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    old_id = surface->desktop_cur;
    if (desktop_id == old_id) {
        return 0;
    }

    surface_clients_hide(surface, old_id);
    if (surface_desktop_select(surface, desktop_id) != 0) {
        /* Restore visibility on failure */
        surface_clients_show(surface, old_id);
        LOGGER_ERROR("Failed to switch to desktop %u on surface %u",
                desktop_id, surface->id);
        return 1;
    }

    surface_clients_show(surface, desktop_id);
    surface->is_outdated = true;
    xcb_flush(surface->connection);

    return 0;
}


/* Switch to the next desktop */
int surface_action_desktop_switch_next(surface_td *surface)
{
    uint32_t old_id;

    LOGGER_DEBUG("Switching to next desktop on surface %u", surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    old_id = surface->desktop_cur;
    surface_clients_hide(surface, old_id);
    surface_desktop_select_next(surface, true);

    if (surface->desktop_cur != old_id) {
        surface_clients_show(surface, surface->desktop_cur);
        surface->is_outdated = true;
        xcb_flush(surface->connection);
    } else {
        surface_clients_show(surface, old_id);
    }

    return 0;
}


/* Switch to the previous desktop */
int surface_action_desktop_switch_prev(surface_td *surface)
{
    uint32_t old_id;

    LOGGER_DEBUG("Switching to previous desktop on surface %u",
            surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    old_id = surface->desktop_cur;
    surface_clients_hide(surface, old_id);
    surface_desktop_select_prev(surface, true);

    if (surface->desktop_cur != old_id) {
        surface_clients_show(surface, surface->desktop_cur);
        surface->is_outdated = true;
        xcb_flush(surface->connection);
    } else {
        surface_clients_show(surface, old_id);
    }

    return 0;
}


/* Toggle full-surface mode */
int surface_action_toggle_fullsurface(surface_td *surface)
{
    LOGGER_DEBUG("Toggling full-surface mode on surface %u",
            surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    surface->fullsurface = !surface->fullsurface;
    surface->is_outdated = true;
    xcb_flush(surface->connection);

    return 0;
}


/* Set the surface resolution */
int surface_action_set_resolution(surface_td *surface,
        struct dimensions_s resolution)
{
    LOGGER_DEBUG("Setting resolution to %ux%u on surface %u",
            resolution.w, resolution.h, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    surface_resize(surface, resolution.w, resolution.h);
    surface->is_outdated = true;

    return 0;
}


/* Set the surface orientation */
int surface_action_set_orientation(surface_td *surface, int orientation)
{
    LOGGER_DEBUG("Setting orientation %d on surface %u",
            orientation, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Orientation control is currently unsupported in this backend */
    LOGGER_WARNING("Surface orientation change is unsupported",
            L_NARG);
    (void) orientation;

    return -1;
}


/* Set the surface brightness */
int surface_action_set_brightness(surface_td *surface,
        uint16_t brightness)
{
    LOGGER_DEBUG("Setting brightness %u on surface %u",
            (unsigned int) brightness, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Brightness control is currently unsupported in this backend */
    LOGGER_WARNING("Surface brightness control is unsupported",
            L_NARG);
    (void) brightness;

    return -1;
}


/* Set the surface contrast */
int surface_action_set_contrast(surface_td *surface, uint16_t contrast)
{
    LOGGER_DEBUG("Setting contrast %u on surface %u",
            (unsigned int) contrast, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Contrast control is currently unsupported in this backend */
    LOGGER_WARNING("Surface contrast control is unsupported",
            L_NARG);
    (void) contrast;

    return -1;
}


/* Apply current surface configuration settings */
int surface_action_configure_settings(surface_td *surface)
{
    LOGGER_DEBUG("Applying configuration settings on surface %u",
            surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    surface->is_outdated = true;
    xcb_flush(surface->connection);

    return 0;
}
