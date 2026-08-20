/**
 * @file input/mouse/drag/warp.c
 *
 * @brief Desktop warp triggered by holding a drag against a screen
 *        edge
 *
 * Split out of what used to be a single, flat @c input/mouse/drag.c;
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
#include <stdio.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/icon.h>

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
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/overlay.h>
#include <input/mouse/drag/outline.h>
#include <input/mouse/drag/warp.h>


/* Track whether the pointer is held against a warp-eligible screen
 * edge, and schedule (or keep, or cancel) the pending desktop-warp
 * countdown accordingly; see the header's own doc comment for the
 * full reasoning */
void drag_warp_edge_check(int16_t root_x)
{
    const surface_td *surface;
    bool at_left;
    bool at_right;

    if (s_drag.client == NULL) {
        s_drag.warp_pending = false;
        return;
    }

    surface = wm_get_surface_by_id(s_drag.client->screen_id);
    if (surface == NULL || surface->config == NULL ||
            !surface->config->desktops.warp_on_edge_drag ||
            surface->desktop_count <= 1u) {
        s_drag.warp_pending = false;
        return;
    }

    at_left = root_x <= 0;
    at_right = (int32_t) root_x >= (int32_t) s_drag.screen_w - 1;

    if (!at_left && !at_right) {
        s_drag.warp_pending = false;
        return;
    }

    if (s_drag.warp_pending && s_drag.warp_is_left == at_left) {
        /* Same edge still held: let the existing countdown keep
         * running rather than restarting it on every motion event. */
        return;
    }

    s_drag.warp_pending = true;
    s_drag.warp_is_left = at_left;
    if (clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due) == 0) {
        s_drag.warp_due.tv_nsec +=
            (long) WM_DESKTOP_WARP_DELAY_MS * 1000000L;
        if (s_drag.warp_due.tv_nsec >= 1000000000L) {
            s_drag.warp_due.tv_sec += 1;
            s_drag.warp_due.tv_nsec -= 1000000000L;
        }
    } else {
        /* Could not read the clock to schedule the countdown; safer
         * to not warp at all than to warp immediately on every edge
         * touch. */
        s_drag.warp_pending = false;
    }
}


/* Milliseconds until a pointer held against a warp-eligible screen
 * edge is due to switch desktops */
int drag_warp_ms_remaining(void)
{
    if (!s_drag.warp_pending) {
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
    uint32_t right_edge_x;
    int16_t new_root_x;
    int32_t new_window_x;
    bool cycle;
    bool is_icon;
    bool show_geom;

    if (connection == NULL || !s_drag.warp_pending ||
            drag_warp_ms_remaining() > 0) {
        return;
    }

    s_drag.warp_pending = false;

    if (s_drag.client == NULL ||
            s_drag.operation != CLIENT_OPERATION_MOVING ||
            (s_drag.drag_window != XCB_WINDOW_NONE &&
                s_drag.drag_window != s_drag.client->icon_window)) {
        /* Not (or no longer) a plain window move or icon move; nothing
         * to warp for, as a resize never sets 'warp_pending' in the
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

    new_desktop = s_drag.warp_is_left
        ? surface_desktop_prev(surface, old_desktop_id, cycle)
        : surface_desktop_next(surface, old_desktop_id, cycle);
    if (new_desktop == NULL || new_desktop->id == old_desktop_id) {
        /* Already at the end and 'cycle' is off: nothing to warp to. */
        return;
    }

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
         * already is whenever a desktop's own active client simply
         * closes while some other desktop is the one currently
         * shown, is precisely what tells 'surface_clients_show'
         * (surface/actions.c) to have 'client_focus_fallback' guess
         * a reasonable replacement once the person switches back,
         * rather than relinquishing focus outright the way a
         * 'client_active_id' that was 0 to begin with would.  Actually
         * clearing it here would collapse that same distinction this
         * whole session already built 'client_focus_fallback' itself
         * around, right back into the exact bug that whole thing was
         * written to fix in the first place. */
    }
    (void) desktop_action_client_add(new_desktop, s_drag.client);
    /* The dragged client is, by construction, always the one the
     * person is actively engaged with right now; 'new_desktop' itself
     * has no way to already know that on its own, so without this it
     * would keep rendering whichever client was its own last
     * genuinely active one instead, active-window highlight included,
     * as soon as the drag settles there. */
    new_desktop->client_active_id = s_drag.client->id;
    new_desktop->focus_dirty = true;

    /* 'desktop_action_client_rem'/'_add' above only move the client
     * between each desktop's own stacking list and lookup table;
     * neither one touches the client's own recorded 'desktop_id'
     * (unlike 'desktop_action_client_send', the normal "send to another
     * desktop" path, which does).  Left stale here, anything that reads
     * a client's desktop from that field directly instead of from
     * whichever desktop's stacking list it is actually in (the window
     * list menu's own per-desktop grouping foremost among them) would
     * keep showing the just-warped client under the desktop it left, or
     * drop it from view entirely, even though the warp itself already
     * moved it correctly everywhere else. */
    s_drag.client->desktop_id = new_desktop->id;

    surface->desktop_cur = new_desktop->id;
    surface_clients_hide(surface, old_desktop_id);
    surface_clients_show(surface, new_desktop->id);
    surface->is_outdated = true;

    /* Same desktop-switch notification a normal (non-warp) switch
     * shows (see 's_show_desktop_overlay' in cmds/surface.c, whose own
     * thin wrapper over this same call this mirrors): without it, a
     * warp is the one way to switch desktops that never shows which
     * one just became active. */
    notify_desktop_show(surface->connection, surface,
            surface->desktop_cur, new_desktop->name, surface->config);

    /* Reposition the pointer to the opposite edge, one pixel in from
     * it rather than exactly on it, so the very next motion notify
     * does not immediately re-arm another warp back the way it just
     * came from.  'right_edge_x' clamps to INT16_MAX before the
     * final cast: 'screen_w' (uint32_t, no compile-time bound of its
     * own) is not guaranteed to fit int16_t on an extreme multi-
     * monitor surface, and this pointer position is sent to the X
     * server as one, via xcb_warp_pointer below. */
    right_edge_x = (s_drag.screen_w > 1u) ? (s_drag.screen_w - 2u) : 0u;
    new_root_x = s_drag.warp_is_left
        ? (int16_t) ((right_edge_x > (uint32_t) INT16_MAX)
                ? INT16_MAX : right_edge_x)
        : (int16_t) 1;

    /* Move the dragged window or icon by the exact same delta the
     * pointer itself is about to jump, so it stays under the cursor
     * across the warp instead of being left behind on the old desktop's
     * own edge.  Shifting 'client_cur_x' (the position 'drag_update'
     * last actually applied, which already folds in any edge-snapping)
     * is what 'pointer_start_x'/'client_start_x' being left untouched
     * below relies on.
     *
     * With both of those unchanged, the very next real motion notify's
     * own 'new_x = client_start_x + (root_x - pointer_start_x)' is
     * a plain linear function of 'root_x', so it naturally reflects the
     * same shift automatically, for adjusting either baseline here
     * instead would cancel that shift back out (the bug an earlier
     * version of this function actually had, i.e, shifting
     * 'pointer_start_x' to compensate for the pointer jump made the
     * computed position identical before and after the warp, keeping
     * the dragged window or icon pinned at its old spot rather than
     * following the pointer to the new one). */
    new_window_x = s_drag.client_cur_x +
        ((int32_t) new_root_x - (int32_t) s_drag.last_root_x);

    s_drag.client_cur_x = new_window_x;

    if (is_icon) {
        uint32_t vals[2];

        show_geom = s_drag.client->config_base != NULL &&
            s_drag.client->config_base->icons.show_geom;

        vals[0] = (uint32_t) new_window_x;
        vals[1] = (uint32_t) s_drag.client_cur_y;
        xcb_configure_window(connection, s_drag.client->icon_window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
    } else {
        show_geom = s_drag.client->config_base != NULL &&
            s_drag.client->config_base->windows.show_geom;

        if (s_drag.solid_drag) {
            enact_client_move(s_drag.client,
                    (struct position_s) { new_window_x,
                        s_drag.client_cur_y });
        } else {
            /* Same reasoning as the geometry overlay just below: left
             * untouched here, the outline would stay drawn wherever it
             * was right before the warp, on the old desktop's own
             * edge, until whatever real motion notify happens to come
             * next, rather than following the pointer across
             * immediately.  Width/height stay 'client_start_w'/'_h'
             * (never 'client_cur_w'/'_h'), the same as 'drag_update''s
             * own MOVING branch, since this whole function only ever
             * runs for a plain move, never a resize (see the early
             * 'CLIENT_OPERATION_MOVING' guard above), so the size
             * itself never actually changes here at all. */
            drag_outline_move(connection, (struct geometry_s) {
                        { new_window_x, s_drag.client_cur_y },
                        { s_drag.client_start_w,
                            s_drag.client_start_h } });
        }
    }

    /* Same geometry overlay 'drag_update' keeps current on every real
     * motion notify.  Without this, it would stay painted at the
     * position the window (or icon) had right before the warp (on the
     * old desktop's own edge) until whatever real pointer motion
     * happens to come next, rather than following it across
     * immediately. */
    if (show_geom) {
        char geom_buf[24];

        (void) snprintf(geom_buf, sizeof(geom_buf), "%+d%+d",
                (int) new_window_x, (int) s_drag.client_cur_y);
        drag_overlay_show(connection, is_icon, (struct geometry_s) {
                    { new_window_x, s_drag.client_cur_y },
                    { is_icon
                        ? (uint16_t) WM_ICON_SQUARE_SIZE
                        : s_drag.client_start_w,
                      is_icon
                        ? drag_icon_height(s_drag.client)
                        : s_drag.client_start_h } },
                geom_buf);
    }

    xcb_warp_pointer(connection, XCB_NONE, surface->screen->root,
            0, 0, 0, 0, new_root_x, s_drag.last_root_y);

    s_drag.last_root_x = new_root_x;
    s_drag.desktop = new_desktop;

    xcb_flush(connection);
}
