/**
 * @file cmds/surface.c
 *
 * @brief Implementation of actions related to screen surface management
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

/* Type includes */
#include <types/direction.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Utils includes */
#include <utils/xcb/connection.h>

/* Menu includes */
#include <menu/notify/desktop.h>

/* Command includes */
#include <cmds/client/move.h>
#include <cmds/client/screen.h>

/* Policy includes */
#include <policy/stacking.h>

/* Defs includes */
#include <defs/config.h>  /* CONFIG_MAX_SCREENS */

/* Local includes */
#include <cmds/surface.h>


/**
 * @brief Show the desktop-switch notification for the current desktop
 *
 * Retrieves the desktop name from the active desktop and passes it to
 * the notify module together with the new desktop index.  Does nothing
 * when no config is available on the surface.
 *
 * @param surface Surface whose current desktop just became active
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       the surface
 */
static void s_show_desktop_overlay(surface_td *surface)
{
    const desktop_td *desktop;

    if (surface == NULL || xcb_connection_get() == NULL ||
            surface->config == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    notify_desktop_show(xcb_connection_get(), surface,
            surface->desktop_cur,
            (desktop != NULL) ? desktop->name : "",
            surface->config);
}


/**
 * @brief Switch a surface to the desktop in a given compass direction,
 *        in cyclic order
 *
 * Shared by @a scmd_surface_desktop_switch_north and its three siblings
 * below, which only differ in direction: which of
 * @c surface_desktop_select_north/south/east/west to call, and the log
 * message's wording.
 *
 * @param surface   Surface to switch
 * @param direction Compass direction to switch toward
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
static void s_switch_cyclic(surface_td *surface,
        enum compass_direction_e direction)
{
    uint32_t old_id;
    bool cycle;
    /* Initialized here, not left to the switch below: that switch
     * deliberately has no 'default:' so the compiler keeps checking
     * it covers every direction, which also means it cannot prove to
     * itself that one of its cases always runs */
    const char *direction_label = "unknown";

    if (surface == NULL) {
        return;
    }

    old_id = surface->desktop_cur;
    cycle = (surface->config != NULL)
        ? surface->config->desktops.wrap_at_bounds : true;

    switch (direction) {
    case COMPASS_NORTH:
        direction_label = "north";
        break;
    case COMPASS_SOUTH:
        direction_label = "south";
        break;
    case COMPASS_EAST:
        direction_label = "east";
        break;
    case COMPASS_WEST:
        direction_label = "west";
        break;
    }
    LOGGER_DEBUG("Switching to the desktop %s of the current one" \
            " on surface %u", direction_label, surface->id);

    surface_clients_hide(surface, old_id);
    switch (direction) {
    case COMPASS_NORTH:
        surface_desktop_select_north(surface, cycle);
        break;
    case COMPASS_SOUTH:
        surface_desktop_select_south(surface, cycle);
        break;
    case COMPASS_EAST:
        surface_desktop_select_east(surface, cycle);
        break;
    case COMPASS_WEST:
        surface_desktop_select_west(surface, cycle);
        break;
    }

    if (surface->desktop_cur != old_id) {
        surface_clients_pinned_transfer_all(surface,
                surface->desktop_cur);
        surface_clients_show(surface, surface->desktop_cur);
        s_show_desktop_overlay(surface);
        surface->is_outdated = true;
    } else {
        /* No switch happened; restore visibility */
        surface_clients_show(surface, old_id);
    }
}


/* Switch to another desktop */
void scmd_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id)
{
    uint32_t old_id;

    if (surface == NULL) {
        return;
    }

    old_id = surface->desktop_cur;
    if (desktop_id == old_id) {
        return;     /* Already on this desktop */
    }

    LOGGER_DEBUG("Switching desktop: %u to %u on surface %u",
            old_id, desktop_id, surface->id);

    surface_clients_hide(surface, old_id);
    if (surface_desktop_select(surface, desktop_id) != 0) {
        surface_clients_show(surface, old_id);
        return;
    }
    surface_clients_pinned_transfer_all(surface, desktop_id);
    surface_clients_show(surface, desktop_id);

    s_show_desktop_overlay(surface);

    surface->is_outdated = true;
}


/* Switch to the desktop north of the current one */
void scmd_surface_desktop_switch_north(surface_td *surface)
{
    s_switch_cyclic(surface, COMPASS_NORTH);
}


/* Switch to the desktop south of the current one */
void scmd_surface_desktop_switch_south(surface_td *surface)
{
    s_switch_cyclic(surface, COMPASS_SOUTH);
}


/* Switch to the desktop east of the current one */
void scmd_surface_desktop_switch_east(surface_td *surface)
{
    s_switch_cyclic(surface, COMPASS_EAST);
}


/* Switch to the desktop west of the current one */
void scmd_surface_desktop_switch_west(surface_td *surface)
{
    s_switch_cyclic(surface, COMPASS_WEST);
}


/**
 * @brief Pan one already-panned client's icon window by the same
 *        delta, when @c icons.follow-viewport asks for it
 *
 * A no-op whenever the client has no @c config to read the flag from,
 * the flag itself is off (the default), there is no icon window at
 * all yet, that window is not currently the one on screen (@a
 * is_icon_mapped false, e.g., the client is not iconified right
 * now), or 'icon_pos' is still the unset sentinel (@c -1, @c -1):
 * exactly the same 'icon_pos' guard @a s_icon_position_choose and its
 * callers in @c cmds/client/icon.c already rely on.  Otherwise shifts
 * the saved 'icon_pos' by 'delta' and reconfigures the real icon
 * window to match, mirroring how @a s_viewport_translate_visit itself
 * moves the client's own window.
 *
 * @param client Client whose icon, if visibly mapped, should pan too
 * @param delta  Pixel delta to add to 'icon_pos'
 *
 * @note Complexity: @e O(1)
 */
static void s_viewport_translate_icon(client_td *client,
        const struct position_s *delta)
{
    if (client->config == NULL ||
            !client->config->base.icons.follow_viewport ||
            client->icon_window == 0u || !client->is_icon_mapped ||
            client->icon_pos.x < 0 || client->icon_pos.y < 0) {
        return;
    }

    client->icon_pos.x = (int16_t) (client->icon_pos.x + delta->x);
    client->icon_pos.y = (int16_t) (client->icon_pos.y + delta->y);

    ccmd_client_apply_geometry(client, client->icon_window,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y,
            client->icon_pos.x, client->icon_pos.y, 0u, 0u, 0u);
}


/** Client a viewport pan's per-client translate walk must leave
 * untouched, since a drag in progress already handles its position
 * (or its off-screen parking spot) through 'input/mouse/drag/pan.c'
 * instead; @c NULL whenever no drag is excluding one. */
static client_td *s_viewport_pan_excluded_client = NULL;


/**
 * @brief Per-client callback for @a s_viewport_pan, translating one
 *        client's stored geometry and its on-screen window together
 *
 * Skips any client stuck to the screen rather than the desktop's own
 * pannable canvas ('client_is_sticky'), leaving it exactly where it
 * already sits, as well as whichever single client (if any)
 * @a scmd_surface_viewport_drag_exclude last named.  Every other
 * client, maximized or fullscreen included, moves by the same delta
 * as the desktop's own viewport origin: both 'cur' and 'old' halves
 * of its saved geometry shift together, so a later unmaximize or
 * unshade restores it to where this pan left it rather than to where
 * it sat before.  Reaches the real window directly through
 * 'ccmd_client_apply_geometry' rather than 'ccmd_client_move', since
 * that higher-level wrapper refuses to touch a maximized or
 * fullscreen client at all.  Finally hands off to @a
 * s_viewport_translate_icon, which decides on its own whether this
 * client's icon should pan along too.
 *
 * @param client Client the walk is currently visiting; never @c NULL
 * @param data   The 'struct position_s' pixel delta to add to
 *               'client's position, cast back from 'void *'
 *
 * @note Complexity: @e O(1)
 */
static void s_viewport_translate_visit(client_td *client, void *data)
{
    const struct position_s *delta = (const struct position_s *) data;
    xcb_window_t target;

    if (client_is_sticky(client) || client == s_viewport_pan_excluded_client) {
        return;
    }

    client->layout.geometry.cur.pos.x += delta->x;
    client->layout.geometry.cur.pos.y += delta->y;
    client->layout.geometry.old.pos.x += delta->x;
    client->layout.geometry.old.pos.y += delta->y;

    target = ccmd_target_win(client);
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y, 0u, 0u, 0u);

    s_viewport_translate_icon(client, delta);
}


/**
 * @brief Read the configured viewport size for @p surface's screen
 *
 * A surface with no @c config, or an @c id past
 * @c CONFIG_MAX_SCREENS, reports the physical screen size back (a 1x1
 * viewport), the same fallback every other reader of this field
 * already falls back to.
 *
 * @param surface     Surface to read the viewport size for
 * @param columns_out Where the configured viewport width, in whole
 *                    screens, is written; never @c NULL
 * @param rows_out    Where the configured viewport height, in whole
 *                    screens, is written; never @c NULL
 *
 * @note Complexity: @e O(1)
 */
static void s_surface_viewport_dims(const surface_td *surface,
        uint32_t *columns_out, uint32_t *rows_out)
{
    *columns_out = 1u;
    *rows_out = 1u;

    if (surface->config != NULL &&
            surface->id < (uint32_t) CONFIG_MAX_SCREENS) {
        *columns_out = surface->config->base.screens[surface->id]
            .viewport.columns;
        *rows_out = surface->config->base.screens[surface->id]
            .viewport.rows;
    }
}


/**
 * @brief Clamp a requested viewport origin to the pannable area and,
 *        if it differs from the current one, translate every
 *        non-sticky client on @p desktop by the resulting delta
 *
 * Shared by @a s_viewport_pan and @a scmd_surface_viewport_set, which
 * only differ in how each arrives at the requested @p origin before
 * this clamp is applied.
 *
 * @param surface Surface owning @p desktop, marked outdated when the
 *                origin actually changes
 * @param desktop Desktop whose viewport is being repositioned
 * @param columns Configured viewport width, in whole screens
 * @param rows    Configured viewport height, in whole screens
 * @param origin  Requested new viewport origin, in pixels, not yet
 *                clamped to the pannable area
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_viewport_apply_origin(surface_td *surface,
        desktop_td *desktop, uint32_t columns, uint32_t rows,
        struct position_s origin)
{
    int32_t max_x;
    int32_t max_y;
    struct position_s delta;

    max_x = (int32_t) (columns - 1u) * (int32_t) desktop->geometry.dim.w;
    max_y = (int32_t) (rows - 1u) * (int32_t) desktop->geometry.dim.h;
    origin.x = (origin.x < 0) ? 0 : origin.x;
    origin.y = (origin.y < 0) ? 0 : origin.y;
    origin.x = (origin.x > max_x) ? max_x : origin.x;
    origin.y = (origin.y > max_y) ? max_y : origin.y;

    if (origin.x == desktop->viewport_origin.x &&
            origin.y == desktop->viewport_origin.y) {
        return;     /* Already at the requested origin */
    }

    delta.x = desktop->viewport_origin.x - origin.x;
    delta.y = desktop->viewport_origin.y - origin.y;
    stacking_walk(desktop, s_viewport_translate_visit, &delta);
    desktop->viewport_origin = origin;
    surface->is_outdated = true;
}


/**
 * @brief Pan the current desktop's viewport by one whole screen in a
 *        given compass direction, clamped at the edges of the
 *        pannable area
 *
 * Shared by @a scmd_surface_viewport_pan_north and its three siblings
 * below, which only differ in direction.  Unlike
 * @a s_switch_cyclic, this never wraps around and never changes
 * @p surface's current desktop: it only moves where within that one
 * desktop the physical screen is looking, translating every non-sticky
 * client the opposite way so their positions on screen stay put
 * relative to the desktop's virtual canvas.
 *
 * @param surface   Surface to pan
 * @param direction Compass direction to pan toward
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
static void s_viewport_pan(surface_td *surface,
        enum compass_direction_e direction)
{
    desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    struct position_s origin;

    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    s_surface_viewport_dims(surface, &columns, &rows);
    origin = desktop->viewport_origin;

    switch (direction) {
    case COMPASS_NORTH:
        origin.y -= (int32_t) desktop->geometry.dim.h;
        break;
    case COMPASS_SOUTH:
        origin.y += (int32_t) desktop->geometry.dim.h;
        break;
    case COMPASS_EAST:
        origin.x += (int32_t) desktop->geometry.dim.w;
        break;
    case COMPASS_WEST:
        origin.x -= (int32_t) desktop->geometry.dim.w;
        break;
    }

    s_viewport_apply_origin(surface, desktop, columns, rows, origin);
}


/* Set or clear the one client the translate walk of every future pan
 * must leave untouched, because a drag already owns its position */
void scmd_surface_viewport_drag_exclude(client_td *client)
{
    s_viewport_pan_excluded_client = client;
}


/* Whether the current desktop's viewport still has room to pan one
 * more screen toward a given compass direction */
bool scmd_surface_viewport_pan_available(surface_td *surface,
        enum compass_direction_e direction)
{
    const desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    int32_t max_x;
    int32_t max_y;
    /* Initialized here, not left to the switch below, matching
     * 's_warp_target_desktop' (input/mouse/drag/warp.c): that switch
     * deliberately has no 'default:' so the compiler keeps checking it
     * against every direction, which also means it cannot prove to
     * itself that one of its cases always runs. */
    bool available = false;

    if (surface == NULL) {
        return false;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return false;
    }

    s_surface_viewport_dims(surface, &columns, &rows);
    max_x = (int32_t) (columns - 1u) * (int32_t) desktop->geometry.dim.w;
    max_y = (int32_t) (rows - 1u) * (int32_t) desktop->geometry.dim.h;

    switch (direction) {
    case COMPASS_NORTH:
        available = desktop->viewport_origin.y > 0;
        break;
    case COMPASS_SOUTH:
        available = desktop->viewport_origin.y < max_y;
        break;
    case COMPASS_EAST:
        available = desktop->viewport_origin.x < max_x;
        break;
    case COMPASS_WEST:
        available = desktop->viewport_origin.x > 0;
        break;
    }

    return available;
}


/* Pan the current desktop's viewport one screen north */
void scmd_surface_viewport_pan_north(surface_td *surface)
{
    s_viewport_pan(surface, COMPASS_NORTH);
}


/* Pan the current desktop's viewport one screen south */
void scmd_surface_viewport_pan_south(surface_td *surface)
{
    s_viewport_pan(surface, COMPASS_SOUTH);
}


/* Pan the current desktop's viewport one screen east */
void scmd_surface_viewport_pan_east(surface_td *surface)
{
    s_viewport_pan(surface, COMPASS_EAST);
}


/* Pan the current desktop's viewport one screen west */
void scmd_surface_viewport_pan_west(surface_td *surface)
{
    s_viewport_pan(surface, COMPASS_WEST);
}


/* Move the current desktop's viewport straight to an absolute origin */
void scmd_surface_viewport_set(surface_td *surface, int32_t x, int32_t y)
{
    desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    struct position_s origin;

    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    s_surface_viewport_dims(surface, &columns, &rows);
    origin.x = x;
    origin.y = y;
    s_viewport_apply_origin(surface, desktop, columns, rows, origin);
}


/* Jump the current desktop's viewport straight to one of its
 * configured pages, addressed by a single linear index */
void scmd_surface_viewport_goto(surface_td *surface, uint32_t page)
{
    const desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    uint32_t col;
    uint32_t row;

    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    s_surface_viewport_dims(surface, &columns, &rows);
    if (page >= columns * rows) {
        return;
    }

    col = page % columns;
    row = page / columns;
    scmd_surface_viewport_set(surface,
            (int32_t) (col * desktop->geometry.dim.w),
            (int32_t) (row * desktop->geometry.dim.h));
}
