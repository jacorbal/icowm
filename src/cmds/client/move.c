/**
 * @file cmds/client/move.c
 *
 * @brief Client positioning command implementation
 *
 * One of the files @c cmds/client/ is made of.  Covers moving a client
 * (including to a specific monitor, or centering it), plus
 * @a ccmd_client_apply_geometry, the single shared XCB call every
 * geometry-changing operation across move, resize, and maximize alike
 * funnels through.
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

/* Type includes */
#include <types/direction.h>
#include <types/pair.h>

/* Utils includes */
#include <utils/xcb/connection.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/move.h>
#include <cmds/client/screen.h>
#include <cmds/client/workarea.h>


/**
 * @brief Move a client to the monitor lying in a given direction
 *
 * Shared by the four public entry points below, which differ from one
 * another in nothing but the direction they ask for.
 *
 * A surface with a single monitor has nowhere to move to, and so does
 * one where the search comes back with the monitor the client is
 * already on, which is what @a surface_monitor_direction returns when
 * there is no neighbor that way.
 *
 * @param client    Client to move; may be null
 * @param direction Which way to look for a neighboring monitor
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors on
 *       the client's surface, scanned to turn the neighbor's
 *       coordinates back into the index @a ccmd_client_move_to_monitor
 *       takes
 */
static void s_move_to_monitor_toward(client_td *client,
        enum compass_direction_e direction)
{
    surface_td *surface = NULL;
    monitor_td cur_monitor;
    monitor_td target_monitor;

    if (client == NULL) {
        return;
    }

    if (!ccmd_client_monitor(client, &surface, &cur_monitor) ||
            surface == NULL || surface->monitor_count <= 1u) {
        return;
    }

    target_monitor = surface_monitor_direction(surface, cur_monitor,
            direction);
    if (target_monitor.x == cur_monitor.x &&
            target_monitor.y == cur_monitor.y) {
        /* No neighbor that way; nothing to move to */
        return;
    }

    for (uint32_t i = 0u; i < surface->monitor_count; ++i) {
        if (surface->monitors[i].x == target_monitor.x &&
                surface->monitors[i].y == target_monitor.y) {
            ccmd_client_move_to_monitor(client, i);
            return;
        }
    }
}


/* Apply a client's geometry to its target window in a single XCB call */
void ccmd_client_apply_geometry(client_td *client,
        xcb_window_t target, uint16_t mask,
        int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    uint32_t values[5];
    uint32_t num = 0;

    if (client == NULL || xcb_connection_get() == NULL ||
            target == XCB_WINDOW_NONE) {
        return;
    }

    if (mask & (uint16_t) XCB_CONFIG_WINDOW_X) {
        values[num++] = (uint32_t) x;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_Y) {
        values[num++] = (uint32_t) y;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_WIDTH) {
        values[num++] = w;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_HEIGHT) {
        values[num++] = h;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        values[num++] = border_width;
    }

    /* Recorded so 'handler_configure_notify' can tell the server's
     * echo of this very request apart from the echo of an earlier one
     * that arrived late; see 'requested_pos' in client/layout.h */
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_X) {
        client->layout.requested_pos.x = x;
        client->layout.has_requested_pos = true;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_Y) {
        client->layout.requested_pos.y = y;
        client->layout.has_requested_pos = true;
    }

    xcb_configure_window(xcb_connection_get(), target, mask, values);
}


/* Move the client to a new position */
void ccmd_client_move(client_td *client, struct position_s pos)
{
    xcb_window_t target;

    if (client == NULL || client_is_maximized(client) ||
            client_is_fullscreen(client)) {
        return;
    }

    target = ccmd_target_win(client);
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y,
            pos.x, pos.y, 0u, 0u, 0u);
    client->layout.geometry.cur.pos.x = pos.x;
    client->layout.geometry.cur.pos.y = pos.y;
    client->has_rule_position_locked = false;
}


/* Center the client on its current screen */
void ccmd_client_center(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    int32_t mx = 0;
    int32_t my = 0;
    int32_t x;
    int32_t y;
    xcb_window_t target;

    if (client == NULL || client_is_maximized(client) ||
            client_is_fullscreen(client)) {
        return;
    }

    /* Centers within the workarea of whichever monitor 'client'
     * currently sits on, not its raw dimensions: consistent with every
     * other quick-position command in this project (maximize, smart
     * placement, transient centering, and now the keyboard's own corner
     * moves in 'ik_handle_move', input/kbd/interact.c), none of which
     * would tuck a client under a panel or the tray reserving space at
     * that same edge. */
    if (!ccmd_client_resolve_workarea(client, &mx, &my, &sw, &sh) &&
            !ccmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    target = ccmd_target_win(client);
    x = ((int32_t) sw - (int32_t) client->layout.geometry.cur.dim.w) / 2;
    y = ((int32_t) sh - (int32_t) client->layout.geometry.cur.dim.h) / 2;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    x += mx;
    y += my;

    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
            (uint16_t) XCB_CONFIG_WINDOW_Y,
            x, y, 0u, 0u, 0u);
    client->layout.geometry.cur.pos.x = x;
    client->layout.geometry.cur.pos.y = y;
    client->has_rule_position_locked = false;
}


/* Move the client to a specific monitor on its own surface */
void ccmd_client_move_to_monitor(client_td *client,
        uint32_t monitor_index)
{
    surface_td *surface = NULL;
    monitor_td cur_monitor;
    monitor_td target_monitor;
    xcb_window_t target;
    int32_t new_x;
    int32_t new_y;
    uint32_t idx;

    if (client == NULL) {
        return;
    }

    if (!ccmd_client_monitor(client, &surface, &cur_monitor) ||
            surface == NULL || surface->monitor_count == 0u) {
        return;
    }

    idx = monitor_index;
    if (idx >= surface->monitor_count) {
        LOGGER_WARNING("Move-to-monitor targets monitor %u, which" \
                " does not exist on surface %u (%u monitor(s));" \
                " falling back to monitor 0", monitor_index,
                surface->id, surface->monitor_count);
        idx = 0u;
    }
    target_monitor = surface->monitors[idx];

    if (target_monitor.x == cur_monitor.x &&
            target_monitor.y == cur_monitor.y) {
        return;
    }

    new_x = client->layout.geometry.cur.pos.x -
        cur_monitor.x + target_monitor.x;
    new_y = client->layout.geometry.cur.pos.y -
        cur_monitor.y + target_monitor.y;

    /* Clamp so the window stays fully on the target monitor even if it
     * is smaller than the one the client came from */
    if (new_x < target_monitor.x) {
        new_x = target_monitor.x;
    } else if ((uint32_t) (new_x - target_monitor.x) +
            client->layout.geometry.cur.dim.w > target_monitor.w) {
        new_x = (target_monitor.w > client->layout.geometry.cur.dim.w)
            ? target_monitor.x + (int32_t) (target_monitor.w -
                    client->layout.geometry.cur.dim.w)
            : target_monitor.x;
    }
    if (new_y < target_monitor.y) {
        new_y = target_monitor.y;
    } else if ((uint32_t) (new_y - target_monitor.y) +
            client->layout.geometry.cur.dim.h > target_monitor.h) {
        new_y = (target_monitor.h > client->layout.geometry.cur.dim.h)
            ? target_monitor.y + (int32_t) (target_monitor.h -
                    client->layout.geometry.cur.dim.h)
            : target_monitor.y;
    }

    target = ccmd_target_win(client);
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y,
            new_x, new_y, 0u, 0u, 0u);
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    client->has_rule_position_locked = false;
    wm_request_client_redraw(client);
}


/* Move a client to the monitor to its north */
void ccmd_client_move_to_monitor_north(client_td *client)
{
    s_move_to_monitor_toward(client, COMPASS_NORTH);
}


/* Move a client to the monitor to its south */
void ccmd_client_move_to_monitor_south(client_td *client)
{
    s_move_to_monitor_toward(client, COMPASS_SOUTH);
}


/* Move a client to the monitor to its east */
void ccmd_client_move_to_monitor_east(client_td *client)
{
    s_move_to_monitor_toward(client, COMPASS_EAST);
}


/* Move a client to the monitor to its west */
void ccmd_client_move_to_monitor_west(client_td *client)
{
    s_move_to_monitor_toward(client, COMPASS_WEST);
}
