/**
 * @file surface/actions/clients.c
 *
 * @brief Client show/hide, sticky transfer, and reflow for a surface
 *
 * One of the files @c surface/actions/ is made of;
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
#include <policy/focus.h>
#include <surface.h>
#include <systray.h>

/* Command includes */
#include <cmds/client/focus.h>
#include <cmds/client/state.h>
#include <cmds/client/visibility.h>

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
                    !client_is_iconified(client)) {
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
                ccmd_client_unmap_decorated(client, surface->connection,
                        target);
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

    if (surface == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, desktop_id);
    if (desktop == NULL) {
        return;
    }

    /* An empty desktop has nothing to map, but must still fall
     * through to the focus-restoration tail below rather than
     * returning here: leaving early skipped that unconditionally,
     * including its own 'PointerRoot' fallback for a desktop with
     * nothing to inherit focus from, so real input focus was left
     * wherever hiding the previous desktop's clients had already
     * put it, unreverted, for as long as this one stayed empty.
     * Landing on 'None' this way (rather than 'PointerRoot' or a
     * genuine client) leaves every keyboard shortcut dead, since
     * 'None' delivers key events nowhere at all, until something
     * else happens to reassert real focus on returning to whichever
     * desktop still has a client on it. */
    if (desktop->stacking != NULL && cdlist_size(desktop->stacking) > 0) {
        node = cdlist_head(desktop->stacking);
        if (node != NULL) {
            initial = node;
            do {
                client_td *const client = (client_td *) cdlist_data(node);
                if (client != NULL &&
                        !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                        !client_is_iconified(client)) {
                    xcb_window_t target =
                        (client_is_decorated(client) && client->frame != 0)
                        ? client->frame
                        : client->window;
                    if (client->titlebar != 0) {
                        xcb_map_window(surface->connection,
                                client->titlebar);
                    }
                    xcb_map_window(surface->connection, target);
                    /* A shaded client's own content window must stay
                     * unmapped until an explicit unshade: mapping it
                     * here regardless (as this used to) puts it back
                     * on screen, sized to whatever tiny remnant its
                     * shaded frame currently allows, while every
                     * other part of this project still believes it
                     * is shaded, and while its own real input focus
                     * target (revert-to Parent) is still whatever
                     * ccmd_client_shade last left it at.  This state
                     * split (mapped at the X server, still shaded to
                     * the WM) is a genuine bug on its own regardless
                     * of the exact downstream consequence; it also
                     * lines up, in practice, with switching away
                     * from and back to a shaded client's own desktop
                     * leaving keyboard input dead until that client
                     * is refocused or closed. */
                    if (target != client->window &&
                            !client_is_shaded(client)) {
                        xcb_map_window(surface->connection,
                                client->window);
                    }
                } else if (client != NULL &&
                        client_is_iconified(client) &&
                        client->icon_window != 0) {
                    xcb_window_t tray_below;

                    xcb_map_window(surface->connection,
                            client->icon_window);
                    /* Icons stay lower than the tray even within the
                     * shared 'below' layer, "stuck to the desktop";
                     * see 'ccmd_client_iconify' for the fuller
                     * explanation of why an unqualified 'below' with
                     * no sibling is not enough to guarantee that on
                     * its own. */
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
                                (const uint32_t[]) {
                                XCB_STACK_MODE_BELOW });
                    }
                    client->is_icon_mapped = true;
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }
    }

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
    node = (desktop->stacking != NULL)
        ? cdlist_head(desktop->stacking) : NULL;
    if (node != NULL) {
        xcb_window_t prev_tgt = XCB_WINDOW_NONE;

        initial = node;
        do {
            client_td *c = (client_td *) cdlist_data(node);
            if (c != NULL &&
                    !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    !client_is_iconified(c)) {
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

    /* Focus is worked out here rather than remembered.  The most
     * recently focused client on this desktop that may still hold
     * focus is asked for, which is what Openbox does on every switch:
     * it keeps no per-desktop record of who was active, only one
     * order over every client, and filters it.
     *
     * Keeping such a record is what went wrong before.  It could name
     * a client since iconified, hidden, or carried off by being
     * pinned, and each of those left the desktop pointing at
     * something no longer eligible, noticed only on returning.  An
     * order cannot go stale that way: a client that stops qualifying
     * is passed over and the next one down is right by construction.
     *
     * 'client_focus_fallback' relinquishes to 'PointerRoot' itself
     * when nothing at all qualifies, which is the genuinely empty
     * desktop and nothing else. */
    client_focus_fallback(desktop, surface, NULL);

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
                    from_desktop->is_focus_dirty = true;
                }

                (void) desktop_action_client_move(from_desktop,
                        to_desktop, sticky[i]);

                /* Preserve focus: if this sticky client was the active
                 * window on the source desktop, make it active on the
                 * destination desktop so 'surface_clients_show'
                 * restores input focus to it.
                 *
                 * Nothing is done to the focus order, and nothing
                 * needs to be: moving between desktops does not touch
                 * it, so this client arrives holding exactly the
                 * place it already had.  That is the whole reason the
                 * order is one list rather than one per desktop. */
                if (was_active) {
                    to_desktop->client_active_id = sticky[i]->id;
                    to_desktop->is_focus_dirty = true;
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
                /* Use the frame for decorated windows, the client
                 * window otherwise */
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
