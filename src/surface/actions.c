/**
 * @file surface/actions.c
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
#include <string.h>     /* memcpy, memset */
#include <strings.h>    /* strcasecmp */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/randr.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Default initial values */

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <systray.h>


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
            /* Only unmap and track events for clients whose windows are
             * currently mapped.  Hidden and iconified clients have
             * already had their windows unmapped by other code paths;
             * issuing another unmap would generate no 'UnmapNotify'
             * events, yet incrementing 'ignore_unmap' would leave the
             * counter positive.  That residual count would then
             * silently absorb the next genuine 'UnmapNotify' (e.g., the
             * app self-unmapping to go to the system tray), preventing
             * 'handler_unmap_notify' from setting 'CLIENT_FLAG_HIDDEN'
             * and breaking the systray restore path in
             * 'handler_message'. */
            if (!(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                xcb_window_t target =
                    (client_is_decorated(client) && client->frame != 0)
                    ? client->frame
                    : client->window;
                /* Two 'UnmapNotify' events arrive for the unmapped
                 * target: one via the parent's 'SubstructureNotify'
                 * (event=parent, window=target) and one via the
                 * target's own 'StructureNotify' (event=target,
                 * window=target).  An additional event arrives for the
                 * titlebar via the frame's 'SubstructureNotify'.
                 * Desktop switches must not toggle
                 * 'CLIENT_FLAG_HIDDEN': that flag represents an
                 * explicit user/application hidden state, not temporary
                 * invisibility on another desktop. */
                client->ignore_unmap += 2u;
                if (client->titlebar != 0) {
                    client->ignore_unmap += 1u;
                }

                if (client->titlebar != 0) {
                    xcb_unmap_window(surface->connection, client->titlebar);
                }
                xcb_unmap_window(surface->connection, target);
            }

            if (client->icon_window != 0 && client->is_icon_mapped) {
                xcb_unmap_window(surface->connection,
                        client->icon_window);
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
            xcb_window_t tray_below;

            xcb_map_window(surface->connection, client->icon_window);
            /* Icons stay lower than the tray even within the shared
             * 'below' layer, "stuck to the desktop"; see
             * 'wcmd_client_iconify' for the fuller explanation of why
             * an unqualified 'below' with no sibling is not enough to
             * guarantee that on its own. */
            tray_below = systray_below_window();
            if (tray_below != XCB_WINDOW_NONE) {
                xcb_configure_window(surface->connection,
                        client->icon_window,
                        XCB_CONFIG_WINDOW_SIBLING |
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) {
                        tray_below, XCB_STACK_MODE_BELOW
                        });
            } else {
                xcb_configure_window(surface->connection,
                        client->icon_window,
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) { XCB_STACK_MODE_BELOW });
            }
            client->is_icon_mapped = true;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    /* Restore Z-order: iterate from head (bottom) to tail (top),
     * raising each window so the tail (topmost client) ends up at the
     * top of the X11 stacking order when all windows are shown.
     * Every window after the first is raised relative to the one
     * just placed (sibling + above), not to the absolute top of the
     * whole stack: an unqualified 'above' claims the very top every
     * time, so with more than one window this would momentarily place
     * each one over literally everything else -- including the icons
     * and tray already pushed to 'below' just above -- until the next
     * iteration covered it again, visible as a rapid, distracting
     * flash on every desktop switch with more than a couple of
     * windows on it. */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        xcb_window_t prev_tgt = XCB_WINDOW_NONE;

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

                if (prev_tgt != XCB_WINDOW_NONE) {
                    xcb_configure_window(surface->connection, tgt,
                            XCB_CONFIG_WINDOW_SIBLING |
                            XCB_CONFIG_WINDOW_STACK_MODE,
                            (const uint32_t[]) {
                            prev_tgt, XCB_STACK_MODE_ABOVE
                            });
                } else {
                    xcb_configure_window(surface->connection, tgt,
                            XCB_CONFIG_WINDOW_STACK_MODE,
                            (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
                }
                prev_tgt = tgt;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    /* Restore input focus to the previously active client.
     * If no suitable client is found, relinquish focus to 'PointerRoot'
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
        desktop->focus_dirty = true;
        xcb_set_input_focus(surface->connection,
                XCB_INPUT_FOCUS_PARENT,
                focus_target->window, XCB_CURRENT_TIME);
        (void) desktop_action_client_send_front(desktop, focus_target);
        focus_restored = true;
    }

    if (!focus_restored) {
        desktop->client_active_id = 0;
        desktop->focus_dirty = true;
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

            for (int i = 0; i < n; ++i) {
                bool was_active =
                    (from_desktop->client_active_id == sticky[i]->id);
                if (was_active) {
                    from_desktop->client_active_id = 0;
                    from_desktop->focus_dirty = true;
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
                    to_desktop->focus_dirty = true;
                }
            }
        }

        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


/* Reposition clients that no longer overlap any known monitor */
void surface_reflow_clients(surface_td *surface)
{
    cdlist_item_td *dnode;
    cdlist_item_td *dinitial;

    if (surface == NULL || surface->desktops == NULL) {
        return;
    }

    dnode = cdlist_head(surface->desktops);
    if (dnode == NULL) {
        return;
    }

    dinitial = dnode;
    do {
        desktop_td *desktop = (desktop_td *) cdlist_data(dnode);
        cdlist_item_td *cnode;
        cdlist_item_td *cinitial;

        if (desktop == NULL || desktop->stacking == NULL ||
                cdlist_size(desktop->stacking) == 0) {
            dnode = cdlist_next(dnode);
            continue;
        }

        cnode = cdlist_head(desktop->stacking);
        if (cnode == NULL) {
            dnode = cdlist_next(dnode);
            continue;
        }

        cinitial = cnode;
        do {
            client_td *client = (client_td *) cdlist_data(cnode);

            if (client != NULL) {
                /* Use the frame for decorated windows, the client window
                 * otherwise */
                xcb_window_t target =
                    (client_is_decorated(client) && client->frame != 0)
                    ? client->frame : client->window;

                int32_t cx = client->layout.geometry.cur.pos.x;
                int32_t cy = client->layout.geometry.cur.pos.y;
                uint32_t cw = client->layout.geometry.cur.dim.w;
                uint32_t ch = client->layout.geometry.cur.dim.h;
                bool still_on_a_monitor = false;

                /* A window overlapping two adjacent, still-connected
                 * monitors (a common, legitimate arrangement, e.g. a
                 * wide window straddling the seam between them) must
                 * not be "corrected" just because it is not fully
                 * inside any single one of them: only reposition a
                 * window that has landed with no overlap at all
                 * against any currently known monitor, e.g. because
                 * the one it used to be on was unplugged, or the
                 * combined layout changed shape around it (RandR does
                 * not require monitors to stay contiguous, so a
                 * disconnected one need not even have been at the
                 * edge of the old combined area). */
                for (uint32_t mi = 0u; mi < surface->monitor_count;
                        ++mi) {
                    const monitor_td *m = &surface->monitors[mi];

                    if (geom_intersection_area(cx, cy, cw, ch,
                                m->x, m->y, m->w, m->h) > 0u) {
                        still_on_a_monitor = true;
                        break;
                    }
                }

                if (!still_on_a_monitor) {
                    monitor_td target_monitor =
                        surface_monitor_for_point(surface,
                                cx + (int32_t) (cw / 2u),
                                cy + (int32_t) (ch / 2u));
                    int32_t mx0 = target_monitor.x;
                    int32_t my0 = target_monitor.y;
                    int32_t mx1 = mx0 + (int32_t) target_monitor.w;
                    int32_t my1 = my0 + (int32_t) target_monitor.h;

                    /* Minimum visible strip to keep on screen. */
                    int32_t margin = (int32_t)
                        ((surface->config->base.windows.move_step > 0u)
                         ? surface->config->base.windows.move_step
                         : 1u);
                    int32_t new_x = cx;
                    int32_t new_y = cy;

                    /* Clamp horizontally, within the resolved
                     * monitor rather than the whole combined
                     * surface */
                    if (new_x + (int32_t) cw < mx0 + margin) {
                        new_x = mx0 + margin - (int32_t) cw;
                    }
                    if (new_x > mx1 - margin) {
                        new_x = mx1 - margin;
                    }

                    /* Clamp vertically, within the resolved monitor */
                    if (new_y + (int32_t) ch < my0 + margin) {
                        new_y = my0 + margin - (int32_t) ch;
                    }
                    if (new_y > my1 - margin) {
                        new_y = my1 - margin;
                    }

                    if (new_x != cx || new_y != cy) {
                        uint32_t vals[2];
                        vals[0] = (uint32_t) new_x;
                        vals[1] = (uint32_t) new_y;

                        xcb_configure_window(surface->connection,
                                target,
                                XCB_CONFIG_WINDOW_X |
                                XCB_CONFIG_WINDOW_Y,
                                vals);

                        client->layout.geometry.cur.pos.x = new_x;
                        client->layout.geometry.cur.pos.y = new_y;
                        desktop->is_outdated = true;
                    }
                }
            }

            cnode = cdlist_next(cnode);
        } while (cnode != NULL && cnode != cinitial);

        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


/* Set the surface resolution */
int surface_action_set_resolution(surface_td *surface,
        struct dimensions_s resolution)
{
    xcb_randr_get_screen_resources_current_cookie_t res_cookie;
    xcb_randr_get_screen_resources_current_reply_t *res_reply;
    xcb_randr_mode_t target_mode;
    int nmodes;
    xcb_randr_mode_info_t *modes;
    xcb_randr_set_crtc_config_cookie_t cfg_cookie;
    xcb_randr_set_crtc_config_reply_t *cfg_reply;
    xcb_randr_output_t out_id;

    LOGGER_DEBUG("Setting resolution to %ux%u on surface %u",
                 resolution.w, resolution.h, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    if (!surface->randr.is_known) {
        LOGGER_WARNING("XRandR CRTC info not yet populated for" \
                       " surface %u; cannot set resolution",
                       surface->id);
        return 1;
    }

    if (resolution.w == 0 || resolution.h == 0) {
        LOGGER_ERROR("Invalid resolution %ux%u for surface %u",
                     resolution.w, resolution.h, surface->id);
        return 1;
    }

    target_mode = XCB_NONE;

    res_cookie = xcb_randr_get_screen_resources_current(
        surface->connection, surface->screen->root);

    res_reply = xcb_randr_get_screen_resources_current_reply(
        surface->connection, res_cookie, NULL);

    if (res_reply == NULL) {
        LOGGER_WARNING("Failed to query screen resources on" \
                       " surface %u", surface->id);
        return 1;
    }

    nmodes =
        xcb_randr_get_screen_resources_current_modes_length(res_reply);
    modes = xcb_randr_get_screen_resources_current_modes(res_reply);

    for (int mi = 0; mi < nmodes; ++mi) {
        if (modes[mi].width == (uint16_t)resolution.w &&
            modes[mi].height == (uint16_t)resolution.h) {
            target_mode = modes[mi].id;
            break;
        }
    }

    if (target_mode == XCB_NONE) {
        LOGGER_WARNING("No RandR mode found matching %ux%u on" \
                       " surface %u",
                       resolution.w, resolution.h, surface->id);
        free(res_reply);
        return 1;
    }

    out_id = (xcb_randr_output_t)surface->randr.output_id;

    cfg_cookie = xcb_randr_set_crtc_config(
        surface->connection,
        (xcb_randr_crtc_t)surface->randr.crtc_id,
        XCB_CURRENT_TIME,
        res_reply->config_timestamp,
        0, 0,
        target_mode,
        surface->randr.rotation,
        1u, &out_id);

    cfg_reply = xcb_randr_set_crtc_config_reply(
        surface->connection, cfg_cookie, NULL);

    if (cfg_reply == NULL ||
        cfg_reply->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
        LOGGER_WARNING("XRandR set-resolution request failed" \
                       " on surface %u (mode %u)",
                       surface->id, (unsigned int)target_mode);
        free(cfg_reply);
        free(res_reply);
        return 1;
    }

    surface->randr.mode_id = (uint32_t)target_mode;

    free(cfg_reply);
    free(res_reply);

    /* The resulting 'XCB_RANDR_SCREEN_CHANGE_NOTIFY' event will trigger
     * 'surface_refresh_workareas' via the event loop too; 'surface_
     * resize' and 'surface_refresh_monitors' are called proactively
     * here instead of waiting for that round-trip, to keep the frame
     * rate smooth and avoid a window where 'surface->monitors' still
     * reflects the pre-change layout */
    surface_resize(surface, resolution.w, resolution.h);
    surface_refresh_monitors(surface);
    surface->is_outdated = true;

    return 0;
}


/* Set the surface orientation */
int surface_action_set_orientation(surface_td *surface, int orientation)
{
    uint16_t rotation;
    xcb_randr_get_screen_resources_current_cookie_t res_cookie;
    xcb_randr_get_screen_resources_current_reply_t *res_reply;
    xcb_randr_set_crtc_config_cookie_t cfg_cookie;
    xcb_randr_set_crtc_config_reply_t *cfg_reply;
    xcb_randr_output_t out_id;

    LOGGER_DEBUG("Setting orientation %d on surface %u",
            orientation, surface->id);

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    if (!surface->randr.is_known) {
        LOGGER_WARNING("XRandR CRTC info not yet populated for"
                " surface %u; cannot set orientation", surface->id);
        return 1;
    }

    switch (orientation) {
        case 0:  rotation = (uint16_t) XCB_RANDR_ROTATION_ROTATE_0;   break;
        case 1:  rotation = (uint16_t) XCB_RANDR_ROTATION_ROTATE_90;  break;
        case 2:  rotation = (uint16_t) XCB_RANDR_ROTATION_ROTATE_180; break;
        case 3:  rotation = (uint16_t) XCB_RANDR_ROTATION_ROTATE_270; break;
        default:
            LOGGER_WARNING("Unknown orientation value %d for surface %u",
                    orientation, surface->id);
            return 1;
    }

    out_id = (xcb_randr_output_t) surface->randr.output_id;

    res_cookie = xcb_randr_get_screen_resources_current(
            surface->connection, surface->screen->root);
    res_reply = xcb_randr_get_screen_resources_current_reply(
            surface->connection, res_cookie, NULL);

    if (res_reply == NULL) {
        LOGGER_WARNING("Failed to query screen resources on"
                " surface %u", surface->id);
        return 1;
    }

    cfg_cookie = xcb_randr_set_crtc_config(
            surface->connection,
            (xcb_randr_crtc_t) surface->randr.crtc_id,
            XCB_CURRENT_TIME,
            res_reply->config_timestamp,
            0, 0,
            (xcb_randr_mode_t) surface->randr.mode_id,
            rotation,
            1u, &out_id);
    cfg_reply = xcb_randr_set_crtc_config_reply(
            surface->connection, cfg_cookie, NULL);

    free(res_reply);

    if (cfg_reply == NULL ||
            cfg_reply->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
        LOGGER_WARNING("XRandR set-orientation request failed" \
                " on surface %u (rotation %u)",
                surface->id, (unsigned int) rotation);
        free(cfg_reply);
        return 1;
    }

    surface->randr.rotation = rotation;
    free(cfg_reply);

    /* A rotation swaps width and height entirely, so 'surface->
     * monitors' is refreshed proactively here too, for the same
     * reason 'surface_action_set_resolution' does: to avoid a window
     * where it still reflects the pre-rotation layout while waiting
     * for the 'XCB_RANDR_SCREEN_CHANGE_NOTIFY' round-trip */
    surface_refresh_monitors(surface);
    surface->is_outdated = true;

    return 0;
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


/**
 * @brief One CRTC's own state, as it was immediately before
 *        'surface_action_apply_randr_profiles' changed it, so
 *        'surface_action_revert_randr_profiles' can restore exactly
 *        that afterward
 */
struct surface_randr_snapshot_s {
    xcb_randr_crtc_t crtc;
    /** @c XCB_NONE means this CRTC was off (driving nothing) before;
     *  reverting restores that, not any particular prior mode */
    xcb_randr_mode_t prior_mode;
    int16_t prior_x;
    int16_t prior_y;
    uint16_t prior_rotation;
    xcb_randr_output_t output_id;
};

/** Every CRTC 'surface_action_apply_randr_profiles' actually changed
 *  during its most recent snapshotting call, in application order */
static struct surface_randr_snapshot_s
    s_randr_snapshot[CONFIG_RANDR_MAX_OUTPUTS];

/** Number of valid entries in 's_randr_snapshot' */
static uint32_t s_randr_snapshot_count = 0u;

/** Whichever output was RandR's primary immediately before that same
 *  call, only meaningful when 's_randr_snapshot_primary_known' */
static xcb_randr_output_t s_randr_snapshot_prior_primary =
    (xcb_randr_output_t) XCB_NONE;

/** Whether 's_randr_snapshot_prior_primary' was actually captured (a
 *  failed query leaves it unusable, so reverting must not touch
 *  primary status rather than restore a value it never really had) */
static bool s_randr_snapshot_primary_known = false;

/** Surface 's_randr_snapshot' belongs to, so 'surface_action_revert_
 *  randr_profiles' (which takes no parameters of its own, called as
 *  it is straight from a dialog's cancel callback) knows which one's
 *  connection to revert on */
static surface_td *s_randr_snapshot_surface = NULL;


/**
 * @brief Find the RandR output whose own name matches a configured
 *        profile's, among those the screen currently reports
 *
 * Unlike the RandR 1.5 monitor list ('surface_refresh_monitors'
 * itself), where each entry's name is an X atom, the older per-output
 * API used here returns its name as plain bytes directly -- no atom
 * resolution needed.
 *
 * @param connection    XCB connection
 * @param res_reply     Already-fetched current screen resources
 * @param name          Output name to match (e.g. "HDMI-1"), compared
 *                      case-insensitively
 * @param out_output_id Receives the matching output, only when one
 *                      is found
 *
 * @return That output's own info (caller's to free), or @c NULL if
 *         none of the screen's outputs currently has this name
 *
 * @note Complexity: @e O(n), where @e n is the number of outputs the
 *       screen currently reports
 */
#if defined(__GNUC__) && !defined(__clang__)
/* Belt-and-suspenders alongside the single-declaration restructuring
 * below: two independently-structured attempts at satisfying
 * '-fanalyzer' by fully zeroing 'output_name' (at its own declaration
 * with '= {0}', and via an explicit 'memset' call) each made no
 * difference at all to this exact warning, and its own final event
 * carries no source location whatsoever for the read it claims is
 * uninitialized -- together, strong signs this is a known class of
 * '-fanalyzer' false positive around a loop containing an early
 * 'continue', not a real, traceable defect in this function.  Scoped
 * to only this one function, and only real GCC (never Clang, which
 * also defines '__GNUC__' for compatibility but does not implement
 * '-fanalyzer' or recognize this specific warning name at all), so
 * nothing here is silenced anywhere else in the project or under any
 * other compiler. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
#endif
static xcb_randr_get_output_info_reply_t *
s_surface_randr_find_output_by_name(xcb_connection_t *connection,
        xcb_randr_get_screen_resources_current_reply_t *res_reply,
        const char *name, xcb_randr_output_t *out_output_id)
{
    int output_count;
    xcb_randr_output_t *outputs;
    /* Declared once here, outside the loop, rather than once per
     * iteration inside it: '-fanalyzer' traced two separate
     * iterations reaching a loop-scoped declaration of this same
     * array (see this function's own history for the two prior,
     * differently-structured attempts at silencing it, both zeroing
     * the array at its own declaration point, that made no
     * difference at all) before reporting a "use of uninitialized
     * value" with no source location at all for the read itself --
     * itself a strong sign of a known class of '-fanalyzer' false
     * positive around a fixed array declared inside a loop with an
     * early 'continue', rather than a real, traceable read of
     * anything actually uninitialized.  A single declaration, reached
     * only once regardless of how many times the loop runs, removes
     * that whole shape entirely. */
    char output_name[CONFIG_RANDR_OUTPUT_NAME_LENGTH] = {0};

    output_count =
        xcb_randr_get_screen_resources_current_outputs_length(res_reply);
    outputs =
        xcb_randr_get_screen_resources_current_outputs(res_reply);

    for (int i = 0; i < output_count; ++i) {
        xcb_randr_get_output_info_cookie_t info_cookie;
        xcb_randr_get_output_info_reply_t *info_reply;
        int name_len;
        uint8_t *name_bytes;

        info_cookie = xcb_randr_get_output_info(connection, outputs[i],
                res_reply->config_timestamp);
        info_reply = xcb_randr_get_output_info_reply(connection,
                info_cookie, NULL);
        if (info_reply == NULL) {
            continue;
        }

        name_len = xcb_randr_get_output_info_name_length(info_reply);
        name_bytes = xcb_randr_get_output_info_name(info_reply);
        if (name_len < 0) {
            name_len = 0;
        }
        if ((size_t) name_len >= sizeof(output_name)) {
            name_len = (int) sizeof(output_name) - 1;
        }
        /* Re-zeroed on every iteration reusing this same array, so a
         * shorter name this time around can never leave a longer
         * previous iteration's own trailing bytes still in place past
         * 'name_len'. */
        memset(output_name, 0, sizeof(output_name));
        if (name_len > 0) {
            memcpy(output_name, name_bytes, (size_t) name_len);
        }

        if (strcasecmp(output_name, name) == 0) {
            *out_output_id = outputs[i];
            return info_reply;
        }
        free(info_reply);
    }

    return NULL;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif


/**
 * @brief Find a free CRTC compatible with a given output
 *
 * Only needed when the output has none assigned yet ('info->crtc' is
 * @c XCB_NONE): tries every CRTC XRandR lists as compatible with this
 * specific output, in the order given, and returns the first one
 * currently driving no output at all.
 *
 * @param connection XCB connection
 * @param info       That output's own info, already fetched
 * @param timestamp  Config timestamp from the same screen-resources
 *                   query @p info itself came from
 *
 * @return A free, compatible CRTC, or @c XCB_NONE if none is free
 *
 * @note Complexity: @e O(c), where @e c is the number of CRTCs
 *       compatible with this output
 */
static xcb_randr_crtc_t s_surface_randr_find_free_crtc(
        xcb_connection_t *connection,
        xcb_randr_get_output_info_reply_t *info,
        xcb_timestamp_t timestamp)
{
    int crtc_count;
    xcb_randr_crtc_t *crtcs;

    crtc_count = xcb_randr_get_output_info_crtcs_length(info);
    crtcs = xcb_randr_get_output_info_crtcs(info);

    for (int i = 0; i < crtc_count; ++i) {
        xcb_randr_get_crtc_info_cookie_t ci_cookie;
        xcb_randr_get_crtc_info_reply_t *ci_reply;
        bool is_free;

        ci_cookie = xcb_randr_get_crtc_info(connection, crtcs[i],
                timestamp);
        ci_reply = xcb_randr_get_crtc_info_reply(connection, ci_cookie,
                NULL);
        if (ci_reply == NULL) {
            continue;
        }
        is_free = ci_reply->num_outputs == 0u;
        free(ci_reply);

        if (is_free) {
            return crtcs[i];
        }
    }

    return (xcb_randr_crtc_t) XCB_NONE;
}


/**
 * @brief Find a RandR mode matching a given resolution
 *
 * Same lookup 'surface_action_set_resolution' does against the
 * screen's own mode list, extracted here so applying a per-output
 * profile can reuse the exact same technique.
 *
 * @param res_reply  Already-fetched current screen resources
 * @param resolution Resolution to match; either dimension @c 0 always
 *                   misses, since that means "not configured" rather
 *                   than a literal 0x0 mode to look for
 *
 * @return The matching mode, or @c XCB_NONE if none of the screen's
 *         modes has this exact resolution
 *
 * @note Complexity: @e O(n), where @e n is the number of modes the
 *       screen currently reports
 */
static xcb_randr_mode_t s_surface_randr_find_mode(
        xcb_randr_get_screen_resources_current_reply_t *res_reply,
        struct dimensions_s resolution)
{
    int nmodes;
    xcb_randr_mode_info_t *modes;

    if (resolution.w == 0u || resolution.h == 0u) {
        return (xcb_randr_mode_t) XCB_NONE;
    }

    nmodes =
        xcb_randr_get_screen_resources_current_modes_length(res_reply);
    modes = xcb_randr_get_screen_resources_current_modes(res_reply);

    for (int i = 0; i < nmodes; ++i) {
        if (modes[i].width == (uint16_t) resolution.w &&
                modes[i].height == (uint16_t) resolution.h) {
            return modes[i].id;
        }
    }

    return (xcb_randr_mode_t) XCB_NONE;
}


/**
 * @brief Save a CRTC's own current state into the next free
 *        's_randr_snapshot' slot, if there is room
 *
 * A no-op past 'CONFIG_RANDR_MAX_OUTPUTS' entries (cannot happen in
 * practice: at most one snapshot per configured profile, itself
 * already bounded to that same limit) or when @p take_snapshot is
 * @c false, so every call site can pass it unconditionally rather
 * than guarding each one individually.
 *
 * @param take_snapshot Whether snapshotting is active for this call
 *                      to 'surface_action_apply_randr_profiles' at all
 * @param crtc          CRTC being changed
 * @param output_id     Output it drives
 * @param prior_mode    Its mode immediately before the change,
 *                      @c XCB_NONE if it was off
 * @param prior_x       Its X position immediately before the change
 * @param prior_y       Its Y position immediately before the change
 * @param prior_rotation Its rotation immediately before the change
 *
 * @note Complexity: @e O(1)
 */
static void s_surface_randr_snapshot_save(bool take_snapshot,
        xcb_randr_crtc_t crtc, xcb_randr_output_t output_id,
        xcb_randr_mode_t prior_mode, int16_t prior_x, int16_t prior_y,
        uint16_t prior_rotation)
{
    struct surface_randr_snapshot_s *slot;

    if (!take_snapshot ||
            s_randr_snapshot_count >= (uint32_t) CONFIG_RANDR_MAX_OUTPUTS) {
        return;
    }

    slot = &s_randr_snapshot[s_randr_snapshot_count];
    slot->crtc = crtc;
    slot->output_id = output_id;
    slot->prior_mode = prior_mode;
    slot->prior_x = prior_x;
    slot->prior_y = prior_y;
    slot->prior_rotation = prior_rotation;
    ++s_randr_snapshot_count;
}


/**
 * @brief Turn an output's own CRTC off, blanking it
 *
 * The disabled-profile half of 's_surface_randr_apply_profile': a
 * plain 'xcb_randr_set_crtc_config' with no mode and no outputs
 * detaches this CRTC from the output entirely, physically disabling
 * it, the same as unplugging it (though the connector itself stays
 * electrically live, so the monitor may still report as connected).
 *
 * @param surface       Surface the output belongs to, for its
 *                      connection and for logging
 * @param res_reply     Already-fetched current screen resources
 * @param crtc          The output's own currently-assigned CRTC
 * @param output_id     Output @p crtc drives, for the snapshot only
 * @param name          Output name, for logging only
 * @param take_snapshot Whether to save this CRTC's state first (see
 *                      's_surface_randr_snapshot_save'), so
 *                      'surface_action_revert_randr_profiles' can
 *                      turn it back on exactly as it was
 *
 * @note Complexity: @e O(1)
 */
static bool s_surface_randr_blank_crtc(surface_td *surface,
        xcb_randr_get_screen_resources_current_reply_t *res_reply,
        xcb_randr_crtc_t crtc, xcb_randr_output_t output_id,
        const char *name, bool take_snapshot)
{
    xcb_randr_get_crtc_info_cookie_t ci_cookie;
    xcb_randr_get_crtc_info_reply_t *ci_reply;
    xcb_randr_set_crtc_config_cookie_t cfg_cookie;
    xcb_randr_set_crtc_config_reply_t *cfg_reply;
    bool ok;

    if (take_snapshot) {
        ci_cookie = xcb_randr_get_crtc_info(surface->connection, crtc,
                res_reply->config_timestamp);
        ci_reply = xcb_randr_get_crtc_info_reply(surface->connection,
                ci_cookie, NULL);
        if (ci_reply != NULL) {
            s_surface_randr_snapshot_save(true, crtc, output_id,
                    ci_reply->mode, ci_reply->x, ci_reply->y,
                    ci_reply->rotation);
        }
        free(ci_reply);
    }

    cfg_cookie = xcb_randr_set_crtc_config(surface->connection, crtc,
            XCB_CURRENT_TIME, res_reply->config_timestamp, 0, 0,
            (xcb_randr_mode_t) XCB_NONE,
            (uint16_t) XCB_RANDR_ROTATION_ROTATE_0, 0u, NULL);
    cfg_reply = xcb_randr_set_crtc_config_reply(surface->connection,
            cfg_cookie, NULL);

    ok = cfg_reply != NULL &&
        cfg_reply->status == XCB_RANDR_SET_CONFIG_SUCCESS;
    if (!ok) {
        LOGGER_WARNING("XRandR: failed to blank output '%s'" \
                " (its configured profile is disabled) on surface %u",
                name, surface->id);
    } else {
        LOGGER_NOTICE("XRandR: output '%s' blanked (its configured" \
                " profile is disabled)", name);
    }
    free(cfg_reply);
    return ok;
}


/**
 * @brief Clamp an @c int32_t coordinate to the @c int16_t range
 *        'xcb_randr_set_crtc_config' itself requires
 *
 * A configured position genuinely outside this range does not fit
 * any real display layout, but clamping instead of a plain cast
 * (which would silently wrap to an unrelated, even more nonsensical
 * value) keeps an absurd configured value from producing one.
 *
 * @param value       Value to clamp
 * @param axis        Axis name ("x" or "y"), for the log message only
 * @param output_name Output name, for the log message only
 *
 * @return @p value clamped to [@c INT16_MIN, @c INT16_MAX]
 *
 * @note Complexity: @e O(1)
 */
static int16_t s_surface_randr_clamp_position(int32_t value,
        const char *axis, const char *output_name)
{
    if (value < (int32_t) INT16_MIN || value > (int32_t) INT16_MAX) {
        LOGGER_WARNING("XRandR: configured position.%s=%d for output" \
                " '%s' is out of range; clamped to %d",
                axis, (int) value, output_name,
                (value < (int32_t) INT16_MIN)
                    ? (int) INT16_MIN : (int) INT16_MAX);
    }
    return (int16_t) ((value < (int32_t) INT16_MIN) ? INT16_MIN
            : (value > (int32_t) INT16_MAX) ? INT16_MAX : value);
}


/**
 * @brief Apply one configured, enabled RandR output profile to its
 *        matching, currently-connected output
 *
 * Reuses the output's current CRTC if it already has one, or claims
 * a free compatible one otherwise (see
 * 's_surface_randr_find_free_crtc').  Resolution is taken from the
 * profile if configured and a matching mode exists; otherwise the
 * CRTC's own already-active mode is kept, falling back to the
 * output's first preferred mode if it had none (a freshly-claimed
 * CRTC on an output with no prior mode of its own).  Position and
 * rotation always come straight from the profile.  'is_primary' is
 * applied as a separate, independent request afterward, since RandR
 * has no way to bundle it into the same one.
 *
 * @param surface   Surface the output belongs to, for its connection
 *                  and for logging
 * @param res_reply Already-fetched current screen resources, shared
 *                  across every profile one call batch applies
 * @param output_id Output this profile matched by name
 * @param info      That output's own info, already fetched
 * @param profile   Configured profile to apply
 *
 * @note Complexity: @e O(c), where @e c is the number of CRTCs
 *       compatible with this output, only when it has none active yet
 */
static bool s_surface_randr_apply_profile(surface_td *surface,
        xcb_randr_get_screen_resources_current_reply_t *res_reply,
        xcb_randr_output_t output_id,
        xcb_randr_get_output_info_reply_t *info,
        const struct config_randr_output_s *profile,
        xcb_randr_output_t current_primary, bool take_snapshot)
{
    xcb_randr_crtc_t crtc;
    xcb_randr_crtc_t prior_crtc;
    xcb_randr_mode_t mode;
    bool mode_was_configured;
    xcb_randr_get_crtc_info_cookie_t ci_cookie;
    xcb_randr_get_crtc_info_reply_t *ci_reply;
    xcb_randr_set_crtc_config_cookie_t cfg_cookie;
    xcb_randr_set_crtc_config_reply_t *cfg_reply;
    int16_t pos_x;
    int16_t pos_y;
    bool crtc_matches_current;
    bool changed = false;

    prior_crtc = info->crtc;
    crtc = (prior_crtc != (xcb_randr_crtc_t) XCB_NONE) ? prior_crtc
        : s_surface_randr_find_free_crtc(surface->connection, info,
                res_reply->config_timestamp);
    if (crtc == (xcb_randr_crtc_t) XCB_NONE) {
        LOGGER_WARNING("XRandR: no free CRTC compatible with output" \
                " '%s' on surface %u; cannot apply its profile",
                profile->name, surface->id);
        return false;
    }

    /* Fetched once up front, regardless of whether a resolution was
     * even configured: needed both for the "keep whatever mode is
     * already active" fallback below and, more importantly, to
     * compare the profile's desired state against this CRTC's actual
     * current one, so an already-matching profile issues no XRandR
     * write at all (see 'crtc_matches_current' below) rather than
     * reasserting an identical configuration on every reload. */
    ci_cookie = xcb_randr_get_crtc_info(surface->connection, crtc,
            res_reply->config_timestamp);
    ci_reply = xcb_randr_get_crtc_info_reply(surface->connection,
            ci_cookie, NULL);

    mode = s_surface_randr_find_mode(res_reply, profile->preferred_res);
    /* Whether the profile itself actually resolved to a specific
     * mode, as opposed to the "keep whatever is already active"
     * fallback taken below: 'crtc_matches_current' needs to know
     * this, since deliberately mirroring 'ci_reply->mode' back into
     * 'mode' just below would otherwise make that comparison match
     * by construction forever after the first real apply, hiding
     * every later change to this same profile's position or rotation
     * that arrives without ever specifying a resolution of its own
     * (see this function's own doc comment for the fuller
     * explanation) -- no resolution requested means no resolution
     * change to detect, so its comparison simply does not apply. */
    mode_was_configured = mode != (xcb_randr_mode_t) XCB_NONE;
    if (!mode_was_configured) {
        /* No resolution configured, or none of the screen's modes
         * matches it exactly: keep the CRTC's own already-active
         * mode instead of forcing a guess */
        mode = (ci_reply != NULL)
            ? ci_reply->mode : (xcb_randr_mode_t) XCB_NONE;

        if (mode == (xcb_randr_mode_t) XCB_NONE &&
                info->num_preferred > 0u) {
            xcb_randr_mode_t *info_modes =
                xcb_randr_get_output_info_modes(info);
            mode = info_modes[0];
        }
    }

    if (mode == (xcb_randr_mode_t) XCB_NONE) {
        LOGGER_WARNING("XRandR: no usable mode for output '%s' on" \
                " surface %u; cannot apply its profile",
                profile->name, surface->id);
        free(ci_reply);
        return false;
    }

    pos_x = s_surface_randr_clamp_position(profile->position.x, "x",
            profile->name);
    pos_y = s_surface_randr_clamp_position(profile->position.y, "y",
            profile->name);

    /* Only meaningful when this CRTC was already driving this same
     * output before this call (a freshly claimed, previously-free
     * CRTC is by definition a real change: it was driving nothing at
     * all); comparing mode (only when the profile actually configured
     * one; see 'mode_was_configured' above)/position/rotation against
     * its already-active configuration is what lets an unchanged
     * 'randr.json' profile skip the write entirely on every reload
     * instead of reasserting an identical configuration each time. */
    crtc_matches_current = (prior_crtc != (xcb_randr_crtc_t) XCB_NONE) &&
        ci_reply != NULL &&
        (!mode_was_configured || ci_reply->mode == mode) &&
        ci_reply->x == pos_x &&
        ci_reply->y == pos_y &&
        ci_reply->rotation == profile->rotation;

    if (!crtc_matches_current) {
        /* Saved from 'ci_reply' (this CRTC's real prior state) when
         * it already had one, or as "was off" when it did not (a
         * freshly claimed CRTC, prior_crtc == XCB_NONE): either way
         * exactly what 'surface_action_revert_randr_profiles' needs
         * to put back. */
        if (prior_crtc != (xcb_randr_crtc_t) XCB_NONE && ci_reply != NULL) {
            s_surface_randr_snapshot_save(take_snapshot, crtc, output_id,
                    ci_reply->mode, ci_reply->x, ci_reply->y,
                    ci_reply->rotation);
        } else {
            s_surface_randr_snapshot_save(take_snapshot, crtc, output_id,
                    (xcb_randr_mode_t) XCB_NONE, 0, 0,
                    (uint16_t) XCB_RANDR_ROTATION_ROTATE_0);
        }
    }
    free(ci_reply);

    if (!crtc_matches_current) {
        cfg_cookie = xcb_randr_set_crtc_config(surface->connection, crtc,
                XCB_CURRENT_TIME, res_reply->config_timestamp,
                pos_x, pos_y, mode,
                profile->rotation, 1u, &output_id);
        cfg_reply = xcb_randr_set_crtc_config_reply(surface->connection,
                cfg_cookie, NULL);

        if (cfg_reply == NULL ||
                cfg_reply->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
            LOGGER_WARNING("XRandR: failed to apply profile to" \
                    " output '%s' on surface %u",
                    profile->name, surface->id);
            free(cfg_reply);
            return false;
        }
        free(cfg_reply);

        LOGGER_NOTICE("XRandR: applied profile to output '%s' on" \
                " surface %u (mode=%u, pos=%d+%d, rot=%u)",
                profile->name, surface->id, (unsigned int) mode,
                profile->position.x, profile->position.y,
                (unsigned int) profile->rotation);
        changed = true;
    }

    if (profile->is_primary && output_id != current_primary) {
        xcb_randr_set_output_primary(surface->connection,
                surface->screen->root, output_id);
        LOGGER_NOTICE("XRandR: marked output '%s' as primary on" \
                " surface %u", profile->name, surface->id);
        changed = true;
    }

    return changed;
}


/* Apply every configured RandR output profile that matches a
 * currently-connected output */
bool surface_action_apply_randr_profiles(surface_td *surface,
        bool take_snapshot)
{
    xcb_randr_get_screen_resources_current_cookie_t res_cookie;
    xcb_randr_get_screen_resources_current_reply_t *res_reply;
    xcb_randr_get_output_primary_cookie_t primary_cookie;
    xcb_randr_get_output_primary_reply_t *primary_reply;
    xcb_randr_output_t current_primary;
    bool any_changed = false;

    /* Reset unconditionally, ahead of every early return below, so a
     * failed or skipped call never leaves a stale snapshot around for
     * 'surface_action_revert_randr_profiles' to act on later as
     * though it belonged to a change that never actually happened. */
    if (take_snapshot) {
        s_randr_snapshot_count = 0u;
        s_randr_snapshot_primary_known = false;
        s_randr_snapshot_surface = surface;
    }

    if (surface == NULL || surface->connection == NULL ||
            surface->screen == NULL || surface->config == NULL ||
            !surface->config->randr.is_enabled) {
        return false;
    }

    res_cookie = xcb_randr_get_screen_resources_current(
            surface->connection, surface->screen->root);
    res_reply = xcb_randr_get_screen_resources_current_reply(
            surface->connection, res_cookie, NULL);
    if (res_reply == NULL) {
        LOGGER_WARNING("XRandR: failed to query screen resources on" \
                " surface %u; cannot apply output profiles",
                surface->id);
        return false;
    }

    /* Fetched once up front, shared by every profile below (see
     * 's_surface_randr_apply_profile'), so an output already marked
     * primary skips 'xcb_randr_set_output_primary' too instead of
     * reissuing it every reload regardless of whether it would
     * actually change anything. */
    primary_cookie = xcb_randr_get_output_primary(surface->connection,
            surface->screen->root);
    primary_reply = xcb_randr_get_output_primary_reply(
            surface->connection, primary_cookie, NULL);
    current_primary = (primary_reply != NULL)
        ? primary_reply->output : (xcb_randr_output_t) XCB_NONE;
    free(primary_reply);

    if (take_snapshot) {
        s_randr_snapshot_prior_primary = current_primary;
        s_randr_snapshot_primary_known = true;
    }

    for (uint32_t i = 0u; i < surface->config->randr.output_count; ++i) {
        const struct config_randr_output_s *profile =
            &surface->config->randr.outputs[i];
        xcb_randr_output_t output_id = (xcb_randr_output_t) XCB_NONE;
        xcb_randr_get_output_info_reply_t *info;

        if (profile->name[0] == '\0') {
            continue;
        }

        info = s_surface_randr_find_output_by_name(surface->connection,
                res_reply, profile->name, &output_id);
        if (info == NULL) {
            LOGGER_DEBUG("XRandR: output '%s' (configured profile)" \
                    " is not currently connected on surface %u",
                    profile->name, surface->id);
            continue;
        }

        if (profile->is_enabled) {
            if (s_surface_randr_apply_profile(surface, res_reply,
                        output_id, info, profile, current_primary,
                        take_snapshot)) {
                any_changed = true;
            }
        } else if (info->crtc != (xcb_randr_crtc_t) XCB_NONE) {
            if (s_surface_randr_blank_crtc(surface, res_reply,
                        info->crtc, output_id, profile->name,
                        take_snapshot)) {
                any_changed = true;
            }
        }
        free(info);
    }

    free(res_reply);

    if (any_changed) {
        surface_refresh_monitors(surface);
        surface->is_outdated = true;
    } else {
        LOGGER_DEBUG("XRandR: every configured output profile on" \
                " surface %u already matches its output's current" \
                " state; nothing applied", surface->id);
    }

    return any_changed;
}


/* Undo the most recent snapshotting 'surface_action_apply_randr_
 * profiles' call */
void surface_action_revert_randr_profiles(void)
{
    surface_td *surface = s_randr_snapshot_surface;
    xcb_randr_get_screen_resources_current_cookie_t res_cookie;
    xcb_randr_get_screen_resources_current_reply_t *res_reply;

    /* Both checked, not just the CRTC snapshot count: a reload whose
     * only actual change was which output is primary (no CRTC
     * touched at all) would otherwise leave this function returning
     * immediately without ever reaching the primary-output revert
     * near the bottom, silently leaving that one change stuck. */
    if (surface == NULL || surface->connection == NULL ||
            (s_randr_snapshot_count == 0u &&
             !s_randr_snapshot_primary_known)) {
        s_randr_snapshot_count = 0u;
        s_randr_snapshot_primary_known = false;
        s_randr_snapshot_surface = NULL;
        return;
    }

    /* The config-timestamp 'xcb_randr_set_crtc_config' itself
     * requires (its second time argument) has to be one the server
     * actually issued, not 'XCB_CURRENT_TIME': the same reasoning
     * 's_surface_randr_apply_profile' and 's_surface_randr_
     * blank_crtc' already follow, both using a screen-resources
     * reply's own 'config_timestamp' rather than that constant. */
    res_cookie = xcb_randr_get_screen_resources_current(
            surface->connection, surface->screen->root);
    res_reply = xcb_randr_get_screen_resources_current_reply(
            surface->connection, res_cookie, NULL);
    if (res_reply == NULL) {
        LOGGER_WARNING("XRandR: failed to query screen resources on" \
                " surface %u; cannot revert output profiles",
                surface->id);
        s_randr_snapshot_count = 0u;
        s_randr_snapshot_primary_known = false;
        s_randr_snapshot_surface = NULL;
        return;
    }

    for (uint32_t i = 0u; i < s_randr_snapshot_count; ++i) {
        const struct surface_randr_snapshot_s *snap = &s_randr_snapshot[i];
        xcb_randr_set_crtc_config_cookie_t cfg_cookie;
        xcb_randr_set_crtc_config_reply_t *cfg_reply;
        bool was_off = snap->prior_mode == (xcb_randr_mode_t) XCB_NONE;

        cfg_cookie = xcb_randr_set_crtc_config(surface->connection,
                snap->crtc, XCB_CURRENT_TIME, res_reply->config_timestamp,
                was_off ? 0 : snap->prior_x,
                was_off ? 0 : snap->prior_y,
                was_off ? (xcb_randr_mode_t) XCB_NONE : snap->prior_mode,
                was_off ? (uint16_t) XCB_RANDR_ROTATION_ROTATE_0
                        : snap->prior_rotation,
                was_off ? 0u : 1u,
                was_off ? NULL : &snap->output_id);
        cfg_reply = xcb_randr_set_crtc_config_reply(surface->connection,
                cfg_cookie, NULL);

        if (cfg_reply == NULL ||
                cfg_reply->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
            LOGGER_WARNING("XRandR: failed to revert CRTC %u on" \
                    " surface %u to its prior state",
                    (unsigned int) snap->crtc, surface->id);
        } else {
            LOGGER_NOTICE("XRandR: reverted CRTC %u on surface %u" \
                    " to its prior state", (unsigned int) snap->crtc,
                    surface->id);
        }
        free(cfg_reply);
    }
    free(res_reply);

    if (s_randr_snapshot_primary_known) {
        xcb_randr_set_output_primary(surface->connection,
                surface->screen->root, s_randr_snapshot_prior_primary);
    }

    s_randr_snapshot_count = 0u;
    s_randr_snapshot_primary_known = false;
    s_randr_snapshot_surface = NULL;

    surface_refresh_monitors(surface);
    surface->is_outdated = true;
}

