/**
 * @file input/mouse/drag/warp.c
 *
 * @brief Desktop warp triggered by holding a drag against a screen
 *        edge
 *
 * One of the files @c input/mouse/drag/ is made of;
 * see @c drag/internal.h for why.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* NULL */
#include <stdlib.h>     /* free, malloc */
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/icon.h>

/* Command includes */
#include <cmds/client/transient.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <surface.h>
#include <wm.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Menu includes */
#include <menu/notify/desktop.h>

/* Local includes */
#include <input/mouse/drag/icon.h>
#include <policy/focus.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/overlay.h>
#include <input/mouse/drag/outline.h>
#include <input/mouse/drag/warp.h>
#include <utils/xcb/connection.h>


/**
 * @brief Resolve which desktop a warp in the pending direction lands
 *        on
 *
 * @param surface Surface the warp happens on
 * @param cycle   Whether the surface wraps around at its bounds
 *
 * @return The desktop to warp to, or @c NULL when there is none in
 *         that direction
 *
 * @note Complexity: @e O(1)
 */
static desktop_td *s_warp_target_desktop(surface_td *surface,
        bool cycle)
{
    const uint32_t old_desktop_id = surface->desktop_cur;
    /* Initialized here, not left to the switch below.  That switch
     * deliberately has no 'default:' so the compiler keeps checking
     * it against every direction, which also means it cannot prove
     * to itself that one of its cases always runs */
    desktop_td *new_desktop = NULL;

    switch (s_drag.warp_direction) {
    case COMPASS_NORTH:
        new_desktop = surface_desktop_north(surface, old_desktop_id,
                cycle);
        break;
    case COMPASS_SOUTH:
        new_desktop = surface_desktop_south(surface, old_desktop_id,
                cycle);
        break;
    case COMPASS_EAST:
        new_desktop = surface_desktop_east(surface, old_desktop_id,
                cycle);
        break;
    case COMPASS_WEST:
        new_desktop = surface_desktop_west(surface, old_desktop_id,
                cycle);
        break;
    }

    return new_desktop;
}


/**
 * @brief Move the dragged client, and its transient family, across
 *
 * @param old_desktop Desktop being left, which may be @c NULL
 * @param new_desktop Desktop being entered
 *
 * @note Complexity: @e O(f), where @e f is the size of the client's
 *       transient family
 */
static void s_warp_move_family(desktop_td *old_desktop,
        desktop_td *new_desktop)
{
    /* Move the dragged client itself to the new desktop without
     * touching its mapped state at all: unlike a normal desktop
     * switch, it must stay visible and uninterrupted throughout the
     * whole warp, not hidden with the rest of the old desktop's
     * clients below. */
    if (old_desktop != NULL) {
        (void) desktop_action_client_rem(old_desktop, s_drag.client);
        /* Deliberately never clears 'old_desktop->client_active_id'
         * here, even though it now names a client no longer actually
         * on that desktop: left stale like this, exactly like it
         * already is whenever a desktop's active client simply
         * closes while some other desktop is the one currently
         * shown, is precisely what tells 'surface_clients_show'
         * (surface/actions.c) to have 'client_focus_fallback' guess
         * a reasonable replacement once the user switches back,
         * rather than relinquishing focus outright the way a
         * 'client_active_id' that was 0 to begin with would.  Actually
         * clearing it here would collapse that same distinction this
         * whole session already built 'client_focus_fallback' itself
         * around, right back into the exact bug that whole thing was
         * written to fix in the first place. */
    }
    (void) desktop_action_client_add(new_desktop, s_drag.client);
    /* The dragged client is, by construction, always the one the
     * user is actively engaged with right now; 'new_desktop' itself
     * has no way to already know that on its own, so without this it
     * would keep rendering whichever client was its last
     * genuinely active one instead, active-window highlight included,
     * as soon as the drag settles there. */
    new_desktop->client_active_id = s_drag.client->id;
    new_desktop->is_focus_dirty = true;

    /* And said in the focus order too, which is what
     * 'surface_clients_show' consults when the switch below settles:
     * it works out a desktop's focus by walking that order rather
     * than reading 'client_active_id', so a client that had only just
     * been added here would sit at the far end of it, as the least
     * recently used thing on a desktop it has been on for an instant,
     * and lose the focus this line just gave it. */
    focus_order_to_top(s_drag.client);

    /* 'desktop_action_client_rem'/'_add' above only move the client
     * between each desktop's stacking list and lookup table;
     * neither one touches the client's recorded 'desktop_id'
     * (unlike 'desktop_action_client_send', the normal "send to another
     * desktop" path, which does).  Left stale here, anything that reads
     * a client's desktop from that field directly instead of from
     * whichever desktop's stacking list it is actually in (the window
     * list menu's per-desktop grouping foremost among them) would
     * keep showing the just-warped client under the desktop it left, or
     * drop it from view entirely, even though the warp itself already
     * moved it correctly everywhere else. */
    s_drag.client->desktop_id = new_desktop->id;

    /* Every other member of 's_drag.client''s transient family
     * (a "save changes?" prompt still open on it, say, or the parent
     * window it belongs to) moves along with it here too, the same
     * way 'enact_desktop_client_send' and 'hi_handle_net_wm_desktop'
     * ('enact/desktop.c', 'handler/ewmh.c') already keep a family
     * together across an explicit desktop send; a warp is just
     * another way for a client to end up on a different desktop, and
     * should not strand the rest of its family behind on the old one.
     * Deliberately only the data move (desktop membership, stacking
     * list, 'desktop_id'): the pointer-following visual drag below
     * (position, overlay, pointer warp) is inherently about the one
     * specific window actually held under the cursor and does not
     * apply to a family member the user is not physically
     * dragging. */
    if (old_desktop != NULL) {
        client_td *const top =
            ccmd_client_transient_top_parent(s_drag.client);

        if (top != NULL) {
            size_t count;
            client_td **siblings;

            /* 'top' itself is never part of the snapshot just below
             * ('ccmd_client_transient_family_snapshot' always
             * excludes it, being the family's reference point),
             * so it needs its move here first, but only when it
             * both is not the very client already being dragged
             * (handled by the visual drag below already) and is
             * actually registered on 'old_desktop' to begin with (a
             * pinned top parent stays registered under whichever
             * desktop it was originally on forever; see
             * 'ccmd_client_bring_family' in
             * 'cmds/client/transient.c' for why that distinction
             * matters, and
             * moving it off of a desktop it never really left would
             * be exactly the same class of bug that comment
             * describes). */
            if (top != s_drag.client &&
                    wm_get_client_desktop(top) == old_desktop) {
                (void) desktop_action_client_move(old_desktop,
                        new_desktop, top);
            }

            siblings = ccmd_client_transient_family_snapshot(
                    old_desktop, top, &count);
            if (siblings != NULL) {
                for (size_t i = 0; i < count; i++) {
                    if (siblings[i] != s_drag.client) {
                        (void) desktop_action_client_move(old_desktop,
                                new_desktop, siblings[i]);
                    }
                }

                free(siblings);
            }
        }
    }
}


/**
 * @brief Work out where the pointer lands on the opposite edge
 *
 * @param out_x Receives the root X the pointer warps to
 * @param out_y Receives the root Y the same
 *
 * @note Complexity: @e O(1)
 */
static void s_warp_pointer_target(int16_t *out_x, int16_t *out_y)
{
    uint32_t opposite_edge;
    int16_t new_root_x;
    int16_t new_root_y;
    bool is_horizontal;

    /* Reposition the pointer to the opposite edge, one pixel in from
     * it rather than exactly on it, so the very next motion notify
     * does not immediately re-arm another warp back the way it just
     * came from, on whichever one of the two axes 'warp_direction'
     * actually warped along; the other axis' pointer coordinate
     * passes through unchanged.  'opposite_edge' clamps to INT16_MAX
     * before the final cast: 'screen_w'/'screen_h' (uint32_t, no
     * compile-time bound) are not guaranteed to fit
     * int16_t on an extreme multi-monitor surface, and this pointer
     * position is sent to the X server as one, via 'xcb_warp_pointer'
     * below. */
    is_horizontal = (s_drag.warp_direction == COMPASS_EAST ||
            s_drag.warp_direction == COMPASS_WEST);

    if (is_horizontal) {
        opposite_edge =
            (s_drag.screen_w > 1u) ? (s_drag.screen_w - 2u) : 0u;
        new_root_x = (s_drag.warp_direction == COMPASS_WEST)
            ? (int16_t) ((opposite_edge > (uint32_t) INT16_MAX)
                    ? INT16_MAX : opposite_edge)
            : (int16_t) 1;
        new_root_y = s_drag.last_root_y;
    } else {
        opposite_edge =
            (s_drag.screen_h > 1u) ? (s_drag.screen_h - 2u) : 0u;
        new_root_y = (s_drag.warp_direction == COMPASS_NORTH)
            ? (int16_t) ((opposite_edge > (uint32_t) INT16_MAX)
                    ? INT16_MAX : opposite_edge)
            : (int16_t) 1;
        new_root_x = s_drag.last_root_x;
    }

    *out_x = new_root_x;
    *out_y = new_root_y;
}


/**
 * @brief Move the dragged window or icon by the pointer's delta
 *
 * Keeps whatever is being dragged under the cursor across the warp,
 * and the geometry overlay with it.
 *
 * @param connection XCB connection
 * @param is_icon    Whether an icon window is being dragged
 * @param new_root_x Root X the pointer is warping to
 * @param new_root_y Root Y the same
 *
 * @note Complexity: @e O(1)
 */
static void s_warp_move_dragged(xcb_connection_t *connection,
        bool is_icon, int16_t new_root_x, int16_t new_root_y)
{
    int32_t new_window_x;
    int32_t new_window_y;
    bool show_geom;

    /* Move the dragged window or icon by the exact same delta the
     * pointer itself is about to jump, so it stays under the cursor
     * across the warp instead of being left behind on the old desktop's
     * edge.  Shifting 'client_cur.pos.x'/'.pos.y' (the position
     * 'drag_update' last actually applied, which already folds in any
     * edge-snapping) is what 'pointer_start_x'/'pointer_start_y'/
     * 'client_start.pos.x'/'.pos.y' being left untouched below relies
     * on.
     *
     * With those unchanged, the very next real motion notify's
     * 'new_x = client_start.pos.x + (root_x - pointer_start_x)' (and
     * its 'y' counterpart) is a plain linear function of 'root_x'/
     * 'root_y', so it naturally reflects the same shift automatically,
     * for adjusting either baseline here instead would cancel that
     * shift back out (the bug an earlier version of this function
     * actually had, i.e, shifting 'pointer_start_x' to compensate for
     * the pointer jump made the computed position identical before and
     * after the warp, keeping the dragged window or icon pinned at its
     * old spot rather than following the pointer to the new one).  The
     * axis 'warp_direction' did not warp along shifts by exactly zero
     * here, since 'new_root_x' and 'new_root_y' above already equal
     * 'last_root_x' and 'last_root_y' on that axis, so this same
     * pair of assignments is correct
     * unconditionally, without needing its 'is_horizontal' branch
     * too. */
    new_window_x = s_drag.client_cur.pos.x +
        ((int32_t) new_root_x - (int32_t) s_drag.last_root_x);
    new_window_y = s_drag.client_cur.pos.y +
        ((int32_t) new_root_y - (int32_t) s_drag.last_root_y);

    s_drag.client_cur.pos.x = new_window_x;
    s_drag.client_cur.pos.y = new_window_y;

    if (is_icon) {
        uint32_t vals[2];

        show_geom = s_drag.client->config != NULL &&
            s_drag.client->config->base.icons.show_geom;

        vals[0] = (uint32_t) new_window_x;
        vals[1] = (uint32_t) s_drag.client_cur.pos.y;
        xcb_configure_window(connection, s_drag.client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
    } else {
        show_geom = s_drag.client->config != NULL &&
            s_drag.client->config->base.windows.show_geom;

        if (s_drag.is_solid_drag) {
            enact_client_move(s_drag.client,
                    (struct position_s) { new_window_x,
                        s_drag.client_cur.pos.y });
        } else {
            /* Same reasoning as the geometry overlay just below.  Left
             * untouched here, the outline would stay drawn wherever it
             * was right before the warp, on the old desktop's
             * edge, until whatever real motion notify happens to come
             * next, rather than following the pointer across
             * immediately.  Width/height stay 'client_start.dim.w'/
             * '.h' (never 'client_cur.dim.w'/'.h'), the same as
             * 'drag_update''s MOVING branch, since this whole
             * function only ever runs for a plain move, never
             * a resize (see the early 'CLIENT_OPERATION_MOVING' guard
             * above), so the size itself never actually changes here
             * at all. */
            drag_outline_move(connection, (struct geometry_s) {
                        { new_window_x, s_drag.client_cur.pos.y },
                        { s_drag.client_start.dim.w,
                            s_drag.client_start.dim.h } });
        }
    }

    /* Same geometry overlay 'drag_update' keeps current on every real
     * motion notify.  Without this, it would stay painted at the
     * position the window (or icon) had right before the warp (on the
     * old desktop's edge) until whatever real pointer motion
     * happens to come next, rather than following it across
     * immediately. */
    if (show_geom) {
        char geom_buf[24];

        (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                (int) new_window_x, (int) s_drag.client_cur.pos.y);
        drag_overlay_show(connection, is_icon, (struct geometry_s) {
                    { new_window_x, s_drag.client_cur.pos.y },
                    { is_icon
                        ? (uint16_t) WM_ICON_SQUARE_SIZE
                        : s_drag.client_start.dim.w,
                      is_icon
                        ? drag_icon_height(s_drag.client)
                        : s_drag.client_start.dim.h } },
                geom_buf);
    }
}


/* Track whether the pointer is held against a warp-eligible screen
 * edge, and schedule (or keep, or cancel) the pending desktop-warp
 * countdown accordingly; see the header's doc comment for the
 * full reasoning */
void drag_warp_edge_check(int16_t root_x, int16_t root_y)
{
    const surface_td *surface;
    bool at_left;
    bool at_right;
    bool at_top;
    bool at_bottom;
    enum compass_direction_e direction;

    if (s_drag.client == NULL) {
        s_drag.is_warp_pending = false;
        return;
    }

    surface = wm_get_surface_by_id(s_drag.client->screen_id);
    if (surface == NULL || surface->config == NULL ||
            !surface->config->desktops.warp_on_edge_drag ||
            surface->desktop_count <= 1u) {
        s_drag.is_warp_pending = false;
        return;
    }

    at_left = root_x <= 0;
    at_right = (int32_t) root_x >= (int32_t) s_drag.screen_w - 1;
    at_top = root_y <= 0;
    at_bottom = (int32_t) root_y >= (int32_t) s_drag.screen_h - 1;

    /* A screen corner holds two edges at once; the horizontal one
     * wins, matching whichever edge this same check already
     * preferred before a vertical one existed at all. */
    if (at_left) {
        direction = COMPASS_WEST;
    } else if (at_right) {
        direction = COMPASS_EAST;
    } else if (at_top) {
        direction = COMPASS_NORTH;
    } else if (at_bottom) {
        direction = COMPASS_SOUTH;
    } else {
        s_drag.is_warp_pending = false;
        return;
    }

    if (s_drag.is_warp_pending && s_drag.warp_direction == direction) {
        /* Same edge still held: let the existing countdown keep
         * running rather than restarting it on every motion event. */
        return;
    }

    s_drag.is_warp_pending = true;
    s_drag.warp_direction = direction;
    if (clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due) == 0) {
        clock_add_ms(&s_drag.warp_due, WM_DESKTOP_WARP_DELAY_MS);
    } else {
        /* Could not read the clock to schedule the countdown; safer
         * to not warp at all than to warp immediately on every edge
         * touch. */
        s_drag.is_warp_pending = false;
    }
}


/* Milliseconds until a pointer held against a warp-eligible screen
 * edge is due to switch desktops */
int drag_warp_ms_remaining(void)
{
    if (!s_drag.is_warp_pending) {
        return -1;
    }

    return (int) clock_ms_until(&s_drag.warp_due);
}


/* Perform the pending warp, if due */
void drag_warp_tick(xcb_connection_t *connection)
{
    surface_td *surface;
    desktop_td *old_desktop;
    desktop_td *new_desktop;
    uint32_t old_desktop_id;
    int16_t new_root_x;
    int16_t new_root_y;
    bool cycle;
    bool is_icon;

    if (connection == NULL || !s_drag.is_warp_pending ||
            drag_warp_ms_remaining() > 0) {
        return;
    }

    s_drag.is_warp_pending = false;

    if (s_drag.client == NULL ||
            s_drag.operation != CLIENT_OPERATION_MOVING ||
            (s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window != s_drag.client->icon_window)) {
        /* Not (or no longer) a plain window move or icon move; nothing
         * to warp for, as a resize never sets 'is_warp_pending' in the
         * first place (see 'drag_warp_edge_check'), but this still
         * guards against it having somehow become stale. */
        return;
    }

    is_icon = s_drag.drag_window != XCB_WINDOW_NONE;

    surface = wm_get_surface_by_id(s_drag.client->screen_id);
    if (surface == NULL || surface->screen == NULL ||
            surface->config == NULL ||
            !surface->config->desktops.warp_on_edge_drag ||
            surface->desktop_count <= 1u) {
        return;
    }

    old_desktop_id = surface->desktop_cur;
    old_desktop = surface_desktop_get(surface, old_desktop_id);
    cycle = surface->config->desktops.wrap_at_bounds;

    new_desktop = s_warp_target_desktop(surface, cycle);
    if (new_desktop == NULL || new_desktop->id == old_desktop_id) {
        /* Already at the end and 'cycle' is off: nothing to warp to. */
        return;
    }

    s_warp_move_family(old_desktop, new_desktop);

    surface->desktop_cur = new_desktop->id;
    surface_clients_hide(surface, old_desktop_id);
    surface_clients_show(surface, new_desktop->id);
    surface->is_outdated = true;

    /* Same desktop-switch notification a normal (non-warp) switch
     * shows (see 's_show_desktop_overlay' in cmds/surface.c, whose
     * thin wrapper over this same call this mirrors).  Without it, a
     * warp is the one way to switch desktops that never shows which
     * one just became active. */
    notify_desktop_show(xcb_connection_get(), surface,
            surface->desktop_cur, new_desktop->name, surface->config);

    s_warp_pointer_target(&new_root_x, &new_root_y);

    s_warp_move_dragged(connection, is_icon, new_root_x, new_root_y);

    xcb_warp_pointer(connection, XCB_NONE, surface->screen->root,
            0, 0, 0, 0, new_root_x, new_root_y);

    s_drag.last_root_x = new_root_x;
    s_drag.last_root_y = new_root_y;
    s_drag.desktop = new_desktop;

}
