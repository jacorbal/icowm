/**
 * @file cmds/client/move.c
 *
 * @brief Client positioning command implementation
 *
 * One of the files @c cmds/client/ is made of.  Covers moving a
 * client (including to a specific monitor, or centering it), plus
 * @a ccmd_client_apply_geometry, the single shared XCB call every
 * geometry-changing operation across move, resize, and maximize
 * alike funnels through.
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
#include <utils/xcb/connection.h>


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
 * @param client    Client to move; may be @c NULL
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


/**
 * @brief Apply a client's geometry to its target window in a single
 *        XCB call
 *
 * Openbox's real answer to configuring a window's geometry
 * (confirmed directly against its source, @c client_configure in
 * @c client.c): one shared function every geometry-changing operation
 * funnels through, rather than each one building its
 * @c xcb_configure_window values array by hand.  @c XCB_CONFIG_WINDOW_*
 * bit values themselves fix the order @c xcb_configure_window's
 * values array must list whichever fields @p mask selects in (@c X
 * @c <@c Y @c <@c WIDTH @c <@c HEIGHT @c <@c BORDER_WIDTH, confirmed
 * directly against @c xproto.h), the exact ordering every one of this
 * function's former call sites had to get right by hand, on its
 * own, every single time; this function gets it right once.
 *
 * Deliberately narrow in scope: only the single
 * @c xcb_configure_window call itself, nothing about updating
 * @p client's tracked
 * @c layout.geometry.cur fields to match, which stays each caller's
 * own concern, since which fields to track, and anything else a
 * caller needs alongside such as clearing
 * @c has_rule_position_locked, genuinely varies from one call site
 * to the next in ways a
 * single shared function covering both would only obscure.
 *
 * @param client       Client whose target window to configure
 * @param target       Window to configure; @a ccmd_target_win's
 *                      result, the frame for a decorated client or
 *                      the bare content window otherwise
 * @param mask         Bitwise OR of whichever @c XCB_CONFIG_WINDOW_X/
 *                      @c _Y/@c _WIDTH/@c _HEIGHT/@c _BORDER_WIDTH
 *                      bits are actually changing; a field whose
 *                      bit is not set here is never read at all,
 *                      whatever @p x/@p y/@p w/@p h/@p border_width
 *                      themselves happen to hold
 * @param x            New X position, only applied when
 *                     @c XCB_CONFIG_WINDOW_X is set in @p mask
 * @param y            New Y position, only applied when
 *                     @c XCB_CONFIG_WINDOW_Y is set in @p mask
 * @param w            New width, only applied when
 *                     @c XCB_CONFIG_WINDOW_WIDTH is set in @p mask
 * @param h            New height, only applied when
 *                     @c XCB_CONFIG_WINDOW_HEIGHT is set in @p mask
 * @param border_width New native border width, only applied if
 *                      @c XCB_CONFIG_WINDOW_BORDER_WIDTH is set in
 *                      @p mask
 *
 * @note A null @p client, one with no connection, or a @p target of
 *       @c XCB_WINDOW_NONE is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_apply_geometry(const client_td *client,
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

    xcb_configure_window(xcb_connection_get(), target, mask, values);
}


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
     * currently sits on, not its raw dimensions: consistent with
     * every other quick-position command in this project (maximize,
     * smart placement, transient centering, and now the keyboard's
     * own corner moves in 'ik_handle_move', input/kbd/interact.c),
     * none of which would tuck a client under a panel or the tray
     * reserving space at that same edge. */
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


void ccmd_client_move_to_monitor(client_td *client, uint32_t monitor_index)
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

    /* Clamp so the window stays fully on the target monitor even if
     * it is smaller than the one the client came from */
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
