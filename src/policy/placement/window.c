/**
 * @file policy/placement/window.c
 *
 * @brief Window placement policy implementation
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/pair.h>

/* Command includes */
#include <cmds/client/transient.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <defs/placement.h>
#include <policy/placement/manual.h>
#include <policy/placement/monitor.h>
#include <policy/placement/rect.h>
#include <policy/placement/score.h>
#include <policy/placement/smart.h>
#include <policy/placement/window.h>
#include <utils/xcb/window.h>


/* Cascade sequence: how many clients this policy has placed so
 * far, since the window manager itself started; each new client
 * offsets one step further along the cascade, wrapping back to
 * the top-left corner once it runs past the configured maximum
 * step count (used in place_window_apply_cascade) */
static uint32_t s_cascade_seq = 0;


/**
 * @brief Offset placement coordinates according to client gravity and
 *        clamp
 *
 * Adjusts the requested top-left placement coordinates so the client's
 * frame is positioned relative to its configured gravity point (e.g.,
 * centering or anchoring the frame by its edge or corner instead of its
 * top-left corner).  After applying the gravity offset, the resulting
 * position is clamped so the frame stays fully within the surface
 * bounds, preferring to keep it at the near edge when it does not fit.
 *
 * @param surface Pointer to the surface providing the placement bounds
 * @param client  Pointer to the client whose gravity and frame size are
 *                used
 * @param x       Pointer to the X coordinate to adjust and clamp in
 *                place
 * @param y       Pointer to the Y coordinate to adjust and clamp in
 *                place
 *
 * @note Complexity: @e O(1)
 */
static void s_place_window_apply_gravity(const surface_td *surface,
        const client_td *client,
        int32_t *restrict x, int32_t *restrict y)
{
    int32_t nx;
    int32_t ny;
    uint32_t sw;
    uint32_t sh;
    uint32_t fw;
    uint32_t fh;

    if (surface == NULL || client == NULL || x == NULL || y == NULL) {
        return;
    }

    nx = *x;
    ny = *y;
    sw = surface->properties.dim.w;
    sh = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;

    if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_NORTH_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_EAST) {
        nx -= (int32_t) fw;
    } else if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_NORTH ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_CENTER ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH) {
        nx -= (int32_t) (fw / 2u);
    }

    if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_WEST) {
        ny -= (int32_t) fh;
    } else if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_CENTER ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_WEST) {
        ny -= (int32_t) (fh / 2u);
    }

    if (nx < 0) {
        nx = 0;
    } else if ((uint32_t) nx + fw > sw) {
        nx = (sw > fw) ? (int32_t) (sw - fw) : 0;
    }
    if (ny < 0) {
        ny = 0;
    } else if ((uint32_t) ny + fh > sh) {
        ny = (sh > fh) ? (int32_t) (sh - fh) : 0;
    }

    *x = nx;
    *y = ny;
}


/**
 * @brief Center a client over its ICCCM §4.1.2.6 @c WM_TRANSIENT_FOR
 *        parent, clamped to that parent's monitor, and configure its
 *        window
 *
 * On success, this fully places the client (configures its window and
 * updates its stored geometry) and returns @c true, so
 * @a place_window_apply has nothing further to do.
 *
 * @param surface Surface @p client is on
 * @param client  Client being placed
 * @param wa      Surface-wide workarea, for the fallback monitor
 *                clip below
 *
 * @return @c true if @p client was transient and got placed here
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (see @c lookup_find_client)
 *
 * @see A no-op, returning @c false, when @p client is not transient for
 *      anything, or its declared parent's geometry could not be
 *      resolved at all (neither an already-managed client entry nor
 *      a raw @a xcb_get_geometry reply
 */
static bool s_place_window_transient_centered(const wm_td *wm,
        surface_td *surface, client_td *client,
        struct geometry_s wa)
{
    uint32_t fw;
    uint32_t fh;
    int32_t new_x = 0;
    int32_t new_y = 0;
    bool placed_as_transient = false;
    xcb_window_t target;
    client_td *parent;
    struct geometry_s t_wa;
    struct dimensions_s screen;
    struct dimensions_s t_sz;
    xcb_connection_t *connection = wm_connection(wm);

    if (client->transient_for == XCB_WINDOW_NONE) {
        return false;
    }

    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    parent = NULL;

    /* ICCCM: 'WM_TRANSIENT_FOR' set to the root window means this
     * dialog is transient for its whole application group, not one
     * specific window ("Window Managers should decide" how to handle
     * this, per the spec's wording); prefer
     * centering over whichever currently-mapped sibling shares the
     * same group leader as this client, falling through to the
     * ordinary geometry-based fallback further below (which, for the
     * root window specifically, ends up centering on screen) when no
     * such sibling is currently mapped. */
    if (client->transient_for == surface->screen->root) {
        xcb_window_t leader = client_group_leader(client);

        if (leader != XCB_WINDOW_NONE) {
            desktop_td *desktop =
                surface_desktop_get(surface, surface->desktop_cur);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const sibling = (client_td *) elem;

                    if (sibling != client &&
                            client_group_leader(sibling) == leader &&
                            !client_is_iconified(sibling)) {
                        parent = sibling;
                        break;
                    }
                }
            }
        } /* ! if (leader) */
    }

    /* Prefer the WM's stored frame geometry over
     * 'xcb_get_geometry': after reparenting the parent's inner
     * window lives inside the frame, so 'xcb_get_geometry' would
     * return its position relative to the frame (left, top); not
     * the frame's root-relative screen position.  Using the stored
     * geometry correctly centers the dialog wherever the parent
     * window is on screen. */
    if (parent == NULL) {
        parent = lookup_find_client(wm_surfaces(wm),
                client->transient_for, NULL, NULL);
    }
    if (parent != NULL) {
        int32_t px = parent->layout.geometry.cur.pos.x;
        int32_t py = parent->layout.geometry.cur.pos.y;
        uint32_t pw = parent->layout.geometry.cur.dim.w;
        uint32_t ph = parent->layout.geometry.cur.dim.h;

        new_x = px + ((int32_t) pw - (int32_t) fw) / 2;
        new_y = py + ((int32_t) ph - (int32_t) fh) / 2;
        placed_as_transient = true;
    } else {
        /* Parent not yet managed (or unmanaged window): fall back
         * to 'xcb_get_geometry' on the declared transient-for
         * window */
        xcb_get_geometry_cookie_t pgc;
        xcb_get_geometry_reply_t *pgr;
        pgc = xcb_get_geometry(connection, client->transient_for);
        pgr = xcb_get_geometry_reply(connection, pgc, NULL);
        if (pgr != NULL) {
            new_x = (int32_t) pgr->x +
                    ((int32_t) pgr->width - (int32_t) fw) / 2;
            new_y = (int32_t) pgr->y +
                    ((int32_t) pgr->height - (int32_t) fh) / 2;
            free(pgr);
            placed_as_transient = true;
        }
    }

    if (!placed_as_transient) {
        return false;
    }

    /* Resolved from the dialog's proposed center, not the
     * pointer: it is meant to sit with its parent, wherever
     * that is, regardless of where the pointer happens to be
     * right now. */
    screen.w = surface->properties.dim.w;
    screen.h = surface->properties.dim.h;
    placement_clip_to_monitor(surface, &wa, &screen,
            surface_monitor_for_point(surface,
                    (struct position_s) {
                        new_x + (int32_t) (fw / 2u),
                        new_y + (int32_t) (fh / 2u) }),
            &t_wa, &t_sz);

    if (new_x < t_wa.pos.x) { new_x = t_wa.pos.x; }
    if (new_y < t_wa.pos.y) { new_y = t_wa.pos.y; }
    if ((uint32_t) new_x + fw > t_sz.w) {
        new_x = (t_sz.w > fw) ? (int32_t) (t_sz.w - fw) : t_wa.pos.x;
    }
    if ((uint32_t) new_y + fh > t_sz.h) {
        new_y = (t_sz.h > fh) ? (int32_t) (t_sz.h - fh) : t_wa.pos.y;
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame : client->window;
    xcb_window_move(target, new_x, new_y);
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    return true;
}


/**
 * @brief Apply gravity, clamp to the workarea, and move the client to
 *        its final resolved position
 *
 * Shared final step of every placement policy: adjusts for window
 * gravity, clamps so the title bar never ends up above the workarea or
 * the physical screen edge, then issues the actual @c ConfigureWindow
 *
 * @param surface Surface the client lives on
 * @param client  Client being placed
 * @param wa_pos  Workarea origin, unclipped to any single monitor
 * @param new_pos Policy-resolved position, before gravity/clamping
 *
 * @note Complexity: @e O(1)
 */
static void s_place_window_finalize(const surface_td *surface,
        client_td *client, struct position_s wa_pos,
        struct position_s new_pos)
{
    xcb_window_t target;

    s_place_window_apply_gravity(surface, client,
            &new_pos.x, &new_pos.y);

    /* Final safety: gravity adjustments must not push the title bar
     * above the workarea top or above the physical screen edge */
    if (new_pos.y < wa_pos.y) { new_pos.y = wa_pos.y; }
    if (new_pos.x < wa_pos.x) { new_pos.x = wa_pos.x; }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_window_move(target, new_pos.x, new_pos.y);
    client->layout.geometry.cur.pos = new_pos;
}


/* Place the client following the cascade policy, unconditionally */
void place_window_apply_cascade(const wm_td *wm,
        surface_td *surface, client_td *client)
{
    const uint32_t cascade_step = 24u;
    uint32_t max_steps;
    uint32_t fw;
    uint32_t fh;
    struct geometry_s wa;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    int32_t new_x;
    int32_t new_y;

    if (wm == NULL || wm_config(wm) == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    placement_workarea(wm, surface, client, &wa, &mon_wa, &mon_sz);

    max_steps = (mon_sz.w > fw) ? (mon_sz.w - fw) / cascade_step : 1u;
    if (mon_sz.h > fh) {
        uint32_t my = (mon_sz.h - fh) / cascade_step;

        if (my < max_steps) {
            max_steps = my;
        }
    }

    if (max_steps == 0u) {
        max_steps = 1u;
    }

    /* Cascade starts at the workarea origin, not at (0, 0), so the
     * title bar is never hidden behind a panel or dock */
    new_x = mon_wa.pos.x +
        (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
    new_y = mon_wa.pos.y +
        (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
    s_cascade_seq++;

    s_place_window_finalize(surface, client, wa.pos,
            (struct position_s) { new_x, new_y });
}


/* Apply the configured placement policy to a newly mapped client */
void place_window_apply(const wm_td *wm,
        surface_td *surface, client_td *client)
{
    const uint32_t cascade_step = 24u;
    xcb_query_pointer_cookie_t pointer_cookie;
    struct dimensions_s screen;
    uint32_t fw;
    uint32_t fh;
    struct position_s wa_pos;
    struct dimensions_s wa_dim;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    struct geometry_s wa_geom;
    int32_t new_x;
    int32_t new_y;
    enum config_placement_policy_e policy;
    desktop_td *desktop;
    xcb_window_t leader;
    bool placed_as_sibling;
    bool ignore_junk_origin_hint;
    xcb_connection_t *connection = wm_connection(wm);
    config_td *config = wm_config(wm);

    if (wm == NULL || config == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    screen.w = surface->properties.dim.w;
    screen.h = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    policy = config->base.windows.placement_policy;
    leader = client_group_leader(client);
    placed_as_sibling = false;

    /* Determine the usable workarea (respects panel struts).
     * Fall back to the full screen dimensions when no workarea is
     * set */
    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop != NULL && desktop->workarea.dim.w > 0u &&
            desktop->workarea.dim.h > 0u) {
        wa_pos = desktop->workarea.pos;
        wa_dim = desktop->workarea.dim;
    } else {
        wa_pos.x = 0;
        wa_pos.y = 0;
        wa_dim = screen;
    }

    /* A splash screen goes in the middle of the work area.  EWMH does
     * not require this, saying only what the type means, but it is
     * what every toolkit that offers a splash does of its own accord
     * and what the person expects to see: a start-up screen cascaded
     * into a corner alongside ordinary windows looks like a mistake.
     *
     * Placed ahead of the honored-position branch below, and not
     * merely ahead of the transient centering: a splash routinely
     * computes its own centre and asks for it through 'PPosition',
     * which that branch obeys, so a splash never reached this at all
     * while it sat after.  What it asks for is a guess at where the
     * middle is, made without knowing the work area or which monitor
     * it will land on, and this knows both.
     *
     * Ahead of the transient centering too, since a splash may be
     * transient for something and its own screen is the frame that
     * matters for it rather than whatever window spawned it. */
    if (client->properties.type == (uint16_t) CLIENT_TYPE_SPLASH) {
        struct position_s centered;

        centered.x = wa_pos.x + ((int32_t) wa_dim.w -
                (int32_t) client->layout.geometry.cur.dim.w) / 2;
        centered.y = wa_pos.y + ((int32_t) wa_dim.h -
                (int32_t) client->layout.geometry.cur.dim.h) / 2;

        /* Placed without going through 's_place_window_finalize',
         * which applies the client's own window gravity to whatever
         * position it is handed.  That is right for a position the
         * client asked for, which is stated in terms of its gravity,
         * and wrong for this one: the middle worked out here is
         * already where the window goes, so a splash declaring
         * centre gravity had half its width taken off again and
         * landed left of centre. */
        xcb_configure_window(connection,
                (client_is_decorated(client) && client->frame != 0)
                    ? client->frame : client->window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                (const uint32_t[]) { (uint32_t) centered.x,
                    (uint32_t) centered.y });
        client->layout.geometry.cur.pos = centered;
        return;
    }

    /* ICCCM 4.1.2.3: a client-requested position takes priority over
     * every placement policy below, including the transient-centering
     * convenience immediately following this: an explicit position
     * request is the client's most specific, deliberate statement
     * of where it wants to appear, ahead of any convenience default
     * this window manager would otherwise pick on its behalf.
     *
     * Exception: a transient window (one with 'WM_TRANSIENT_FOR' set)
     * requesting exactly (0, 0) is not honored here.  In practice
     * this combination is essentially never a deliberate placement
     * choice on a dialog's part; it is toolkit boilerplate left over
     * from a default 'PPosition'/'USPosition' hint nobody meant to
     * set to a specific value, and honoring it verbatim pins every
     * such dialog to the screen's top-left corner instead of the
     * transient-centered position ICCCM §4.1.2.6 recommends
     * immediately below.  A window that genuinely wants (0, 0) is
     * vanishingly rare among transients specifically, so this narrow
     * exception costs nothing for any other client while fixing that
     * one common, confusing case (a "save changes?"-style prompt
     * landing at the screen corner instead of over its parent). */
    ignore_junk_origin_hint = client->transient_for != XCB_WINDOW_NONE &&
        client->hints_icccm.size.req_pos.x == 0 &&
        client->hints_icccm.size.req_pos.y == 0;
    if (client->hints_icccm.size.has_position &&
            !ignore_junk_origin_hint) {
        s_place_window_finalize(surface, client, wa_pos,
                client->hints_icccm.size.req_pos);
        return;
    }

    /* ICCCM §4.1.2.6: center transient dialogs over their parent */
    if (s_place_window_transient_centered(wm, surface, client,
                (struct geometry_s) { wa_pos, wa_dim })) {
        return;
    }

    /* Cluster windows of the same application: if another currently
     * mapped (non-iconified) client on this desktop shares the same
     * 'WM_CLIENT_LEADER'/'WM_HINTS' group as 'client', place the new
     * window offset from the group instead of running the configured
     * placement policy, so related windows stay visually together.
     * Gated by 'windows.placement.group-related' since not everyone
     * wants this: it can be turned off in configuration.
     *
     * The offset scales with how many siblings already exist (not just
     * whichever one 'ohtbl_foreach' happens to visit first) so that
     * a 3rd, 4th,... window of the same group each land at a further,
     * distinct position instead of every one of them after the 2nd
     * piling up on exactly the same spot as the 2nd. */
    if (leader != XCB_WINDOW_NONE && desktop != NULL &&
            desktop->clients != NULL &&
            config->base.windows.group_related) {
        void *elem;
        client_td *anchor = NULL;
        uint32_t sibling_count = 0u;

        ohtbl_foreach(desktop->clients, elem) {
            client_td *const sibling = (client_td *) elem;

            if (sibling == client ||
                    client_group_leader(sibling) != leader ||
                    client_is_iconified(sibling)) {
                continue;
            }

            sibling_count++;
            anchor = sibling;
        }

        if (anchor != NULL) {
            new_x = anchor->layout.geometry.cur.pos.x +
                (int32_t) (cascade_step * sibling_count);
            new_y = anchor->layout.geometry.cur.pos.y +
                (int32_t) (cascade_step * sibling_count);
            placed_as_sibling = true;
        }
    }

    /* Clip the workarea (and the screen bound used for edge-clamping)
     * down to whichever physical monitor 'windows.placement.monitor'
     * resolves to, on a surface made of more than one: cascade,
     * centered, and under-mouse below all score or clamp against these
     * two, and without this they would do so against the whole combined
     * area instead of one monitor.  Falls back to the unclipped values
     * (identical to previous behavior) when there is only one monitor
     * or clipping would leave nothing to place into. */
    wa_geom.pos = wa_pos;
    wa_geom.dim = wa_dim;
    placement_clip_to_monitor(surface, &wa_geom, &screen,
            placement_reference_monitor(wm, surface, client,
                    config->base.windows.monitor_policy),
            &mon_wa, &mon_sz);

    if (placed_as_sibling) {
        struct geometry_s s_wa;
        struct dimensions_s s_sz;

        /* Resolved from the offset position next to the anchor sibling,
         * not the pointer: a related window is meant to stay with its
         * group, wherever that is. */
        placement_clip_to_monitor(surface, &wa_geom, &screen,
                surface_monitor_for_point(surface,
                        (struct position_s) {
                            new_x + (int32_t) (fw / 2u),
                            new_y + (int32_t) (fh / 2u) }),
                &s_wa, &s_sz);

        /* Clamp to the workarea/screen the same way the cascade policy
         * below does, so a sibling near the edge does not push the new
         * window off-screen */
        if (new_x < s_wa.pos.x) { new_x = s_wa.pos.x; }
        if (new_y < s_wa.pos.y) { new_y = s_wa.pos.y; }
        if ((uint32_t) new_x + fw > s_sz.w) {
            new_x = (s_sz.w > fw) ? (int32_t) (s_sz.w - fw) : s_wa.pos.x;
        }
        if ((uint32_t) new_y + fh > s_sz.h) {
            new_y = (s_sz.h > fh) ? (int32_t) (s_sz.h - fh) : s_wa.pos.y;
        }
    } else if (policy == CONFIG_PLACEMENT_POLICY_SMART &&
            place_window_smart(wm, surface, client, &new_x, &new_y)) {
        /* Placement chosen by smart scan */
    } else if (policy == CONFIG_PLACEMENT_POLICY_CASCADE ||
            policy == CONFIG_PLACEMENT_POLICY_SMART) {
        place_window_apply_cascade(wm, surface, client);
        return;
    } else if (policy == CONFIG_PLACEMENT_POLICY_CENTERED) {
        /* Center on the workarea, not on the full screen. */
        new_x = mon_wa.pos.x +
            ((int32_t) mon_wa.dim.w - (int32_t) fw) / 2;
        new_y = mon_wa.pos.y +
            ((int32_t) mon_wa.dim.h - (int32_t) fh) / 2;
        if (new_x < mon_wa.pos.x) { new_x = mon_wa.pos.x; }
        if (new_y < mon_wa.pos.y) { new_y = mon_wa.pos.y; }
    } else if (policy == CONFIG_PLACEMENT_POLICY_MANUAL &&
            place_window_manual(wm, surface, client, &new_x, &new_y)) {
        /* Placement the person will be asked to confirm or move, sat
         * meanwhile wherever the smart scan chose */
    } else if (policy == CONFIG_PLACEMENT_POLICY_MANUAL) {
        /* Same cascade fallback the smart policy takes when its
         * scan finds nothing free.  Reached by way of the branch just
         * above, which already marked this client as one to ask about,
         * so the question is still put; only the position it starts
         * from differs. */
        place_window_apply_cascade(wm, surface, client);
        return;
    } else if (policy == CONFIG_PLACEMENT_POLICY_UNDER_MOUSE) {
        xcb_query_pointer_reply_t *pointer_reply;

        pointer_cookie = xcb_query_pointer(connection,
                surface->screen->root);
        pointer_reply = xcb_query_pointer_reply(connection,
                pointer_cookie, NULL);

        if (pointer_reply == NULL) {
            LOGGER_WARNING("Failed to query pointer for" \
                    " 'under-mouse' placement; keeping" \
                    " X-server-assigned position", L_NARG);
            return;
        }

        new_x = (int32_t) pointer_reply->root_x - (int32_t) (fw / 2u);
        new_y = (int32_t) pointer_reply->root_y - (int32_t) (fh / 2u);
        if (new_x < mon_wa.pos.x) {
            new_x = mon_wa.pos.x;
        } else if ((uint32_t) new_x + fw > mon_sz.w) {
            new_x = (mon_sz.w > fw)
                ? (int32_t) (mon_sz.w - fw)
                : mon_wa.pos.x;
        }
        if (new_y < mon_wa.pos.y) {
            new_y = mon_wa.pos.y;
        } else if ((uint32_t) new_y + fh > mon_sz.h) {
            new_y = (mon_sz.h > fh)
                ? (int32_t) (mon_sz.h - fh)
                : mon_wa.pos.y;
        }

        free(pointer_reply);
    } else {
        /* "none" or unknown: keep the X-server-assigned position unless
         * the frame title bar would be hidden above the workarea top
         * (e.g., behind a panel) or above the physical screen edge */
        new_x = client->layout.geometry.cur.pos.x;
        new_y = client->layout.geometry.cur.pos.y;

        if (new_x < 0) {
            new_x = 0;
        }

        if (new_y < wa_pos.y) {
            new_y = wa_pos.y;
        }

        if (new_x == client->layout.geometry.cur.pos.x &&
                new_y == client->layout.geometry.cur.pos.y) {
            return;
        }
    }

    s_place_window_finalize(surface, client, wa_pos,
            (struct position_s) { new_x, new_y });
}
