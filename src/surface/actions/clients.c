/**
 * @file surface/actions/clients.c
 *
 * @brief Client show/hide, sticky transfer, and reflow for a surface
 *
 * Split out of what used to be a single, flat @c surface/actions.c;
 * everything here operates on a desktop's own clients directly (as
 * opposed to @c surface/actions/randr.c's own RandR output/CRTC/mode
 * concerns, which never touch client visibility directly).
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <systray.h>

/* Command includes */
#include <cmds/client/basic.h>

/* Unmap all non-sticky clients on the specified desktop */
void surface_clients_hide(surface_td *surface, uint32_t desktop_id)
{
    desktop_td *desktop;
    cdlist_item_td *node;
    const cdlist_item_td *initial;

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
        client_td *const client = (client_td *) cdlist_data(node);
        if (client != NULL &&
                !(client->properties.flags & CLIENT_FLAG_PIN)) {
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
                client->ignore.unmap += 2u;
                if (client->titlebar != 0) {
                    client->ignore.unmap += 1u;
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
    const cdlist_item_td *initial;
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
        client_td *const client = (client_td *) cdlist_data(node);
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
             * 'ccmd_client_iconify' for the fuller explanation of why
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
     * top of the X11 stacking order when all windows are shown.  Every
     * window after the first is raised relative to the one just placed
     * (sibling + above), not to the absolute top of the whole stack: an
     * unqualified 'above' claims the very top every time, so with more
     * than one window this would momentarily place each one over
     * literally everything else (including the icons and tray already
     * pushed to 'below' just above) until the next iteration covered it
     * again, visible as a rapid, distracting flash on every desktop
     * switch with more than a couple of windows on it. */
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
    focus_target = NULL;
    if (desktop->client_active_id != 0) {
        client_td *c = desktop_find_client_by_id(desktop,
                desktop->client_active_id);

        if (c != NULL && !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                !client_is_shaded(c) &&
                c->properties.state !=
                    (uint16_t) CLIENT_STATE_ICONIFIED &&
                (c->properties.flags & CLIENT_FLAG_FOCUSABLE)) {
            focus_target = c;
        }
    }


    /* When the desktop's own remembered active client could not be
     * restored above, only let 'client_focus_fallback' guess another
     * reasonable visible, focusable client on this same desktop (see
     * its own doc comment, cmds/client/basic.h, for the exact
     * criteria, skip-taskbar exclusion included) when a
     * 'client_active_id' genuinely existed to begin with, i.e.,
     * someone really had focused something on this desktop before;
     * relinquish focus to 'PointerRoot' directly otherwise, without
     * ever guessing, e.g., for a desktop whose only client is a pinned
     * window merely visible there on loan from wherever it actually
     * got focused. */
    if (focus_target != NULL) {
        desktop->client_active_id = focus_target->id;
        desktop->focus_dirty = true;
        /* 'ccmd_client_focus', not a bare 'xcb_set_input_focus': the
         * exact same ICCCM/EWMH sequence every other focus-granting
         * path in this project already goes through (see its own
         * doc comment) — honoring the client's own 'WM_HINTS' input
         * model, sending 'WM_TAKE_FOCUS' for a Locally- or Globally-
         * Active client that relies on it to actually accept focus
         * internally, and publishing '_NET_WM_STATE_FOCUSED' so a
         * pager or taskbar agrees with the X server about who just
         * got focus back.  A raw 'SetInputFocus' here previously
         * skipped all three, leaving this one specific path (desktop
         * switch) the only place a restored client could hold real
         * keyboard focus while every EWMH-aware tool still thought
         * otherwise, and the only place a Locally/Globally-Active
         * client's own focus ring or cursor never actually reappeared
         * on switching back to it. */
        ccmd_client_focus(focus_target);
        (void) desktop_action_client_send_front(desktop, focus_target);
    } else if (desktop->client_active_id != 0) {
        client_focus_fallback(desktop, surface, NULL);
    } else {
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
    const cdlist_item_td *dinitial;
    cdlist_item_td *cnode;
    const cdlist_item_td *cinitial;
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
                client_td *const c = (client_td *) cdlist_data(cnode);

                if (c != NULL && client_is_pinned(c) &&
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
void surface_clients_reflow(surface_td *surface)
{
    cdlist_item_td *dnode;
    const cdlist_item_td *dinitial;

    if (surface == NULL || surface->desktops == NULL) {
        return;
    }

    dnode = cdlist_head(surface->desktops);
    if (dnode == NULL) {
        return;
    }

    dinitial = dnode;
    do {
        desktop_td *const desktop = (desktop_td *) cdlist_data(dnode);
        cdlist_item_td *cnode;
        const cdlist_item_td *cinitial;

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
            client_td *const client = (client_td *) cdlist_data(cnode);

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
                 * monitors (a common, legitimate arrangement, e.g.,
                 * a wide window straddling the seam between them) must
                 * not be "corrected" just because it is not fully
                 * inside any single one of them: only reposition
                 * a window that has landed with no overlap at all
                 * against any currently known monitor, e.g., because
                 * the one it used to be on was unplugged, or the
                 * combined layout changed shape around it (RandR does
                 * not require monitors to stay contiguous, so
                 * a disconnected one need not even have been at the
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
                                (struct position_s) {
                                    cx + (int32_t) (cw / 2u),
                                    cy + (int32_t) (ch / 2u) });
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
                        client->is_outdated = true;
                        desktop->is_outdated = true;
                    }
                }
            }

            cnode = cdlist_next(cnode);
        } while (cnode != NULL && cnode != cinitial);

        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}
