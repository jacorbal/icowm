/**
 * @file surface/actions/client.c
 *
 * @brief Client show/hide, pinned transfer, and reflow for a surface
 *
 * One of the files @c surface/actions/ is made of; everything here
 * operates on a desktop's clients directly (as opposed to
 * @c surface/actions/randr.c's RandR output/CRTC/mode concerns, which
 * never touch client visibility directly).
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
#include <stdlib.h>     /* calloc, free */

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Commands includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/state.h>
#include <cmds/client/visibility.h>

/* Policy includes */
#include <policy/stacking.h>
#include <policy/focus.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <systray.h>

/* Surface includes */
#include <surface.h>
#include <surface/client.h>
#include <surface/desktop.h>
#include <surface/monitor.h>


/**
 * @brief What @a s_client_restack_visit carries between clients
 *
 * The window stacked just below this one, since each is placed relative
 * to its lower neighbour rather than to the top of everything.
 */
struct s_restack_ctx_s {
    xcb_window_t prev_target;   /**< Window stacked just below */
};

/**
 * @brief What @a s_client_reflow_visit needs beyond the client
 */
struct s_reflow_ctx_s {
    surface_td *surface;    /**< Surface whose monitors are consulted */
    desktop_td *desktop;    /**< Desktop to mark for redraw on a move */
};


/**
 * @brief What @a s_client_pinned_visit is gathering into, shared by its
 *        counting and filling passes
 *
 * @c out is @c NULL during the first, counting pass (nothing to write
 * yet, @c count just accumulates a total) and points at a freshly,
 * exactly sized allocation during the second, filling pass.
 */
struct s_pinned_ctx_s {
    client_td **out;        /**< @c NULL during the counting pass */
    int capacity;           /**< Slots @c out has (filling pass) */
    int count;              /**< How many have been put in so far */
};


/**
 * @brief Unmap one client as its desktop stops being shown
 *
 * @param client Client reached by the walk
 * @param data   The surface, as a @c surface_td pointer
 *
 * @note Complexity: @e O(1)
 */
static void s_client_hide_visit(client_td *client, void *data)
{
    const surface_td *const surface = data;

    if (surface == NULL) {
        return;
    }

    if (client != NULL &&
            !(client->properties.flags & CLIENT_FLAG_PIN)) {
        /* Only unmap and track events for clients whose windows are
         * currently mapped.  Hidden and iconified clients have already
         * had their windows unmapped by other code paths; issuing
         * another unmap would generate no 'UnmapNotify' events, yet
         * incrementing 'ignore_unmap' would leave the counter positive.
         * That residual count would then silently absorb the next
         * genuine 'UnmapNotify' (e.g., the app self-unmapping to go to
         * the system tray), preventing 'handler_window_unmap_notify' from
         * setting 'CLIENT_FLAG_HIDDEN' and breaking the systray restore
         * path in 'handler_message'. */
        if (!(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                !client_is_iconified(client)) {
            xcb_window_t target =
                (client_is_decorated(client) && client->frame != 0)
                ? client->frame
                : client->window;
            /* Two 'UnmapNotify' events arrive for the unmapped target:
             * one via the parent's 'SubstructureNotify' (event=parent,
             * window=target) and one via the target's 'StructureNotify'
             * (event=target, window=target).  An additional event
             * arrives for the titlebar via the frame's
             * 'SubstructureNotify'.  Desktop switches must not toggle
             * 'CLIENT_FLAG_HIDDEN': that flag represents an explicit
             * user/application hidden state, not temporary invisibility
             * on another desktop. */
            ccmd_client_unmap_decorated(client, target);
        }

        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_window_hide(client->icon_window);
            client->is_icon_mapped = false;
        }
    }
}


/**
 * @brief Map one client as its desktop starts being shown
 *
 * @param client Client reached by the walk
 * @param data   The surface, as a @c surface_td pointer
 *
 * @note Complexity: @e O(1)
 */
static void s_client_show_visit(client_td *client, void *data)
{
    const surface_td *const surface = data;

    if (surface == NULL) {
        return;
    }

    if (client != NULL &&
            !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
            !client_is_iconified(client)) {
        xcb_window_t target =
            (client_is_decorated(client) && client->frame != 0)
            ? client->frame
            : client->window;
        if (client->titlebar != 0) {
            xcb_window_show(client->titlebar);
        }
        xcb_window_show(target);
        /* A shaded client's content window must stay unmapped until an
         * explicit unshade: mapping it here regardless would put it
         * back on screen, sized to whatever tiny remnant its shaded
         * frame currently allows, while every other part of this
         * project still believes it is shaded, and while its real input
         * focus target (revert-to Parent) is still whatever
         * 'ccmd_client_shade' last left it at.  This state split
         * (mapped at the X server, still shaded to the WM) is a genuine
         * bug on its own regardless of the exact downstream
         * consequence; it also lines up, in practice, with switching
         * away from and back to a shaded client's desktop leaving
         * keyboard input dead until that client is refocused or
         * closed. */
        if (target != client->window &&
                !client_is_shaded(client)) {
            xcb_window_show(client->window);
        }
    } else if (client != NULL &&
            client_is_iconified(client) &&
            client->icon_window != 0) {
        xcb_window_t tray_below;

        xcb_window_show(client->icon_window);
        /* Icons stay lower than the tray even within the shared 'below'
         * layer, "stuck to the desktop"; see 'ccmd_client_iconify' for
         * the fuller explanation of why an unqualified 'below' with no
         * sibling is not enough to guarantee that on its. */
        tray_below = systray_below_window();
        if (tray_below != XCB_WINDOW_NONE) {
            xcb_window_stack_below(client->icon_window, tray_below);
        } else {
            xcb_window_lower(client->icon_window);
        }
        client->is_icon_mapped = true;
    }
}


/**
 * @brief Stack one client directly above whichever came before it
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_restack_ctx_s this walk carries
 *
 * @note Complexity: @e O(1)
 */
static void s_client_restack_visit(client_td *client, void *data)
{
    struct s_restack_ctx_s *const restack_ctx = data;
    xcb_window_t target;

    if (client == NULL || restack_ctx == NULL ||
            (client->properties.flags & CLIENT_FLAG_HIDDEN) ||
            client_is_iconified(client)) {
        return;
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame : client->window;

    if (restack_ctx->prev_target != XCB_WINDOW_NONE) {
        xcb_window_stack_above(target, restack_ctx->prev_target);
    } else {
        xcb_window_raise(target);
    }
    restack_ctx->prev_target = target;
}


/**
 * @brief Gather one client if it is pinned
 *
 * Shared by @a surface_client_pinned_transfer_all's counting pass
 * (@c out left @c NULL, nothing to write yet, @c count just accumulates
 * a total) and its filling pass (@c out pointing at a freshly, exactly
 * sized allocation).
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_pinned_ctx_s being filled
 *
 * @note Complexity: @e O(1)
 */
static void s_client_pinned_visit(client_td *client, void *data)
{
    struct s_pinned_ctx_s *const pinned_ctx = data;

    if (client == NULL || pinned_ctx == NULL ||
            !client_is_pinned(client)) {
        return;
    }

    if (pinned_ctx->out != NULL) {
        if (pinned_ctx->count >= pinned_ctx->capacity) {
            return;
        }

        pinned_ctx->out[pinned_ctx->count] = client;
    }

    pinned_ctx->count++;
}


/**
 * @brief Bring one client back onto a monitor after a layout change
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_reflow_ctx_s this walk carries
 *
 * @note Complexity: @e O(m), where @e m is the number of monitors on
 *       the surface
 */
static void s_client_reflow_visit(client_td *client, void *data)
{
    struct s_reflow_ctx_s *const reflow_ctx = data;
    surface_td *const surface =
        (reflow_ctx != NULL) ? reflow_ctx->surface : NULL;
    desktop_td *const desktop =
        (reflow_ctx != NULL) ? reflow_ctx->desktop : NULL;

    if (surface == NULL || desktop == NULL) {
        return;
    }

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

        /* A window overlapping two adjacent, still-connected monitors
         * (a common, legitimate arrangement, e.g., a wide window
         * straddling the seam between them) must not be "corrected"
         * just because it is not fully inside any single one of them:
         * only reposition a window that has landed with no overlap at
         * all against any currently known monitor, e.g., because the
         * one it used to be on was unplugged, or the combined layout
         * changed shape around it (RandR does not require monitors to
         * stay contiguous, so a disconnected one need not even have
         * been at the edge of the old combined area). */
        for (uint32_t mi = 0u; mi < surface->monitor_count; ++mi) {
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
                xcb_window_move(target, new_x, new_y);

                client->layout.geometry.cur.pos.x = new_x;
                client->layout.geometry.cur.pos.y = new_y;
                client->is_outdated = true;
                desktop->is_outdated = true;
            }
        } /* ! if (!still_on_a_monitor) */
    } /* ! if (!client) */
}


/* Unmap all non-pinned clients on the specified desktop */
void surface_client_hide_all(surface_td *surface, uint32_t desktop_id)
{
    const desktop_td *desktop;

    if (surface == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, desktop_id);
    if (desktop == NULL) {
        return;
    }

    stacking_walk(desktop, s_client_hide_visit, surface);
}


/* Map all visible (non-hidden, non-iconified) clients on the specified
 * desktop */
void surface_client_show_all(surface_td *surface, uint32_t desktop_id)
{
    desktop_td *desktop;
    struct s_restack_ctx_s restack_ctx;

    if (surface == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, desktop_id);
    if (desktop == NULL) {
        return;
    }

    /* An empty desktop has nothing to map, but must still fall through
     * to the focus-restoration tail below rather than returning here:
     * leaving early skipped that unconditionally, including its
     * 'PointerRoot' fallback for a desktop with nothing to inherit
     * focus from, so real input focus was left wherever hiding the
     * previous desktop's clients had already put it, unreverted, for as
     * long as this one stayed empty.  Landing on 'None' this way
     * (rather than 'PointerRoot' or a genuine client) leaves every
     * keyboard shortcut dead, since 'None' delivers key events nowhere
     * at all, until something else happens to reassert real focus on
     * returning to whichever
     * desktop still has a client on it. */
    stacking_walk(desktop, s_client_show_visit, surface);

    /* Restore Z-order: iterate from head (bottom) to tail (top),
     * raising each window so the tail (topmost client) ends up at the
     * top of the X11 stacking order when all windows are shown.  Every
     * window after the first is raised relative to the one just placed
     * (sibling + above), not to the absolute top of the stack.  An
     * unqualified 'above' claims the very top every time, so with more
     * than one window this would momentarily place each one over
     * literally everything else (including the icons and tray already
     * pushed to 'below' just above) until the next iteration covered it
     * again, visible as a rapid, distracting flash on every desktop
     * switch with more than a couple of windows on it. */
    restack_ctx.prev_target = XCB_WINDOW_NONE;
    stacking_walk(desktop, s_client_restack_visit, &restack_ctx);

    /* The walk above puts the windows in the order they are stacked
     * among themselves, which says nothing about layers.  A client kept
     * below or above its neighbours is a property of the client, not of
     * where it sits in that order.  Re-imposed here, so that showing
     * a desktop leaves its layers as they were.
     *
     * Done for every caller rather than at the desktop switch alone.
     * Nothing did it before, and a window put below stayed wherever
     * this walk left it until some later action happened to enforce
     * layers again, which is why the wrong stacking was seen after
     * a drag between desktops but corrected itself as soon as anything
     * was selected. */
    ccmd_desktop_enforce_layers(desktop);

    /* Focus is worked out here rather than remembered.  The most
     * recently focused client on this desktop that may still hold focus
     * is asked for, which is what Openbox does on every switch: it
     * keeps no per-desktop record of who was active, only one order
     * over every client, and filters it.
     *
     * Keeping such a record is what went wrong before.  It could name
     * a client since iconified, hidden, or carried off by being pinned,
     * and each of those left the desktop pointing at something no
     * longer eligible, noticed only on returning.  An order cannot go
     * stale that way: a client that stops qualifying is passed over and
     * the next one down is right by construction.
     *
     * 'client_focus_fallback' relinquishes to 'PointerRoot' itself when
     * nothing at all qualifies, which is the genuinely empty desktop
     * and nothing else. */
    client_focus_fallback(desktop, surface, NULL);

    desktop->is_outdated = true;

}


/* Move all pinned clients from every other desktop to the target
 * desktop */
void surface_client_pinned_transfer_all(surface_td *surface,
        uint32_t to_id)
{
    cdlist_item_td *dnode;
    const cdlist_item_td *dinitial;
    desktop_td *to_desktop;
    desktop_td *from_desktop;

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
                stacking_count(from_desktop) > 0u) {
            /* Collected before any of them is moved.  Moving one takes
             * it off this desktop, and a walk that moved as it went
             * would be reading a set it was itself changing.  Counted
             * first, then gathered into an allocation sized to the
             * count: the same two passes 's_family_snapshot' makes
             * ('cmds/client/transient.c'), and for the same reason.
             * A fixed array would drop whatever pinned windows did not
             * fit, and drop them silently, leaving some following the
             * desktop change and others left behind. */
            struct s_pinned_ctx_s pinned_ctx;
            client_td **pinned;
            int n;

            pinned_ctx.out = NULL;
            pinned_ctx.capacity = 0;
            pinned_ctx.count = 0;
            stacking_walk(from_desktop, s_client_pinned_visit,
                    &pinned_ctx);
            n = pinned_ctx.count;

            pinned = (n > 0) ? calloc((size_t) n, sizeof(*pinned)) : NULL;
            if (pinned != NULL) {
                pinned_ctx.out = pinned;
                pinned_ctx.capacity = n;
                pinned_ctx.count = 0;
                stacking_walk(from_desktop, s_client_pinned_visit,
                        &pinned_ctx);

                /* Walked from the last collected to the first, and the
                 * collection above ran from the bottom of the stack
                 * upward.  Each client sent to the bottom below lands
                 * under the one sent before it, so taking them in
                 * collection order would leave them stacked the wrong
                 * way round, and swapped again on the next desktop
                 * change: two pinned windows would trade places every
                 * time. */
                for (int i = n - 1; i >= 0; --i) {
                    bool was_active =
                        (from_desktop->client_active_id ==
                                pinned[i]->id);
                    if (was_active) {
                        from_desktop->client_active_id = 0;
                        from_desktop->is_focus_dirty = true;
                    }

                    (void) desktop_action_client_move(from_desktop,
                            to_desktop, pinned[i]);
                    ccmd_publish_wm_desktop(pinned[i], to_desktop->id);

                    /* Arriving at the bottom unless it was the window
                     * being worked in.  'desktop_action_client_move'
                     * adds to the top, which is right for a window the
                     * user deliberately sent elsewhere but not for one
                     * that is merely following them: a pinned window
                     * nobody had touched climbed over whatever they did
                     * have open, on every desktop change.
                     *
                     * The focused one still arrives on top, since
                     * putting the window being worked in underneath
                     * everything would interrupt that work just as
                     * badly in the other direction. */
                    if (!was_active) {
                        (void) desktop_action_client_send_back(
                                to_desktop, pinned[i]);
                    }

                    /* Preserve focus: if this pinned client was the
                     * active window on the source desktop, make it
                     * active on the destination desktop so
                     * 'surface_client_show_all' restores input focus to
                     * it.
                     *
                     * Nothing is done to the focus order, and nothing
                     * needs to be: moving between desktops does not
                     * touch it, so this client arrives holding exactly
                     * the place it already had.  That is the whole
                     * reason the order is one list rather than one per
                     * desktop. */
                    if (was_active) {
                        to_desktop->client_active_id = pinned[i]->id;
                        to_desktop->is_focus_dirty = true;
                    }
                }
            }

            free(pinned);
        }

        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


/* Reposition clients that no longer overlap any known monitor */
void surface_client_reflow_all(surface_td *surface)
{
    struct s_reflow_ctx_s reflow_ctx;
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

        reflow_ctx.surface = surface;
        reflow_ctx.desktop = desktop;
        stacking_walk(desktop, s_client_reflow_visit, &reflow_ctx);

        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}
