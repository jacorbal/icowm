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
#include <scratchpad.h>
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
 * now), or 'icon_pos' is still the unset sentinel (@c -1, @c -1).
 * That sentinel check tests both components together rather than
 * either one being negative, since a legitimately panned icon can
 * end up with a negative 'icon_pos.x' or 'icon_pos.y' the moment it
 * sits on a viewport page west or north of the one the desktop
 * itself now shows; treating that as 'unset' would silently stop
 * this same icon from following any further pan.  Otherwise shifts
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
            (client->icon_pos.x == -1 && client->icon_pos.y == -1)) {
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
 * this clamp is applied.  Also the one place, once an actual pan is
 * confirmed, that reaches @a scratchpad_notice_viewport_panned, so
 * every path that ever moves a viewport (an edge-hover pan, a
 * background or window drag crossing an edge, a keybind, an IPC
 * command) hides the scratchpad the same way, without each needing
 * its own separate call.
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
 *
 * @see @a scratchpad_notice_viewport_panned
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

    /* A visible scratchpad is just another non-sticky client the walk
     * above already shifted away from its own configured edge, same
     * as everything else on 'desktop'; hiding it here, rather than
     * leaving it wherever that shift left it, is what keeps it from
     * ever being seen drifted out of its own zone.  A no-op with no
     * current scratchpad client, one already hidden, or one that
     * belongs to some other desktop. */
    scratchpad_notice_viewport_panned(desktop);
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


/* Pan the current desktop's viewport by 'viewport.move-step' pixels */
void scmd_surface_viewport_pan_step(surface_td *surface,
        enum compass_direction_e direction)
{
    desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    uint32_t step;
    struct position_s origin;

    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    step = (surface->config != NULL) ? surface->config->base
        .viewport.move_step : 0u;
    s_surface_viewport_dims(surface, &columns, &rows);
    origin = desktop->viewport_origin;

    switch (direction) {
    case COMPASS_NORTH:
        origin.y -= (int32_t) step;
        break;
    case COMPASS_SOUTH:
        origin.y += (int32_t) step;
        break;
    case COMPASS_EAST:
        origin.x += (int32_t) step;
        break;
    case COMPASS_WEST:
        origin.x -= (int32_t) step;
        break;
    }

    s_viewport_apply_origin(surface, desktop, columns, rows, origin);
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


/**
 * @brief Convert a desktop-space (virtual-canvas) position into its
 *        configured viewport page, the exact inverse of the per-axis
 *        multiply @a scmd_surface_viewport_goto already does to land
 *        on a page's own origin
 *
 * @param canvas_pos Position within @p desktop's virtual canvas
 * @param desktop    Desktop whose per-page dimensions to divide by
 * @param col_out    Resulting zero-based column, updated in place
 * @param row_out    Resulting zero-based row, updated in place
 *
 * @note Complexity: @e O(1)
 */
static void s_viewport_page_for_canvas_pos(struct position_s canvas_pos,
        const desktop_td *desktop, uint32_t *col_out, uint32_t *row_out)
{
    int32_t col = canvas_pos.x / (int32_t) desktop->geometry.dim.w;
    int32_t row = canvas_pos.y / (int32_t) desktop->geometry.dim.h;

    *col_out = (col < 0) ? 0u : (uint32_t) col;
    *row_out = (row < 0) ? 0u : (uint32_t) row;
}


/* Report the viewport page a client currently sits on */
bool scmd_surface_viewport_client_page(const surface_td *surface,
        const desktop_td *desktop, const client_td *client,
        uint32_t *col_out, uint32_t *row_out)
{
    uint32_t columns;
    uint32_t rows;
    struct position_s canvas_pos;

    if (surface == NULL || desktop == NULL || client == NULL ||
            col_out == NULL || row_out == NULL) {
        return false;
    }

    s_surface_viewport_dims(surface, &columns, &rows);
    if (columns <= 1u && rows <= 1u) {
        return false;
    }

    canvas_pos.x = client->layout.geometry.cur.pos.x +
        desktop->viewport_origin.x;
    canvas_pos.y = client->layout.geometry.cur.pos.y +
        desktop->viewport_origin.y;
    s_viewport_page_for_canvas_pos(canvas_pos, desktop, col_out, row_out);
    return true;
}


/* Report the viewport page a desktop's viewport currently shows */
bool scmd_surface_viewport_desktop_page(const surface_td *surface,
        const desktop_td *desktop, uint32_t *col_out, uint32_t *row_out)
{
    uint32_t columns;
    uint32_t rows;

    if (surface == NULL || desktop == NULL ||
            col_out == NULL || row_out == NULL) {
        return false;
    }

    s_surface_viewport_dims(surface, &columns, &rows);
    if (columns <= 1u && rows <= 1u) {
        return false;
    }

    s_viewport_page_for_canvas_pos(desktop->viewport_origin, desktop,
            col_out, row_out);
    return true;
}


/**
 * @brief Guarantee a client's own recorded position sits within its
 *        desktop's actual pannable canvas, moving it back in if not
 *
 * A defensive backstop, not a normal code path: nothing here is
 * supposed to ever leave a client's @c layout.geometry.cur.pos outside
 * @p desktop's virtual canvas (@c columns times @c rows whole
 * screens), but relying solely on every drag/pan/warp calculation
 * always being perfect would leave a client permanently unreachable,
 * even via the search menu, the moment any one of them is not.  Only
 * ever moves @p client when it is genuinely outside the whole
 * canvas, never merely off the currently panned-to page, so a client
 * legitimately sitting on some other, valid page is left exactly
 * where it is for @a scmd_surface_viewport_center_on_client to pan to
 * instead.
 *
 * @param surface Surface owning @p desktop
 * @param desktop Desktop whose pannable canvas @p client must sit
 *                within
 * @param client  Client whose recorded position is clamped in place
 *
 * @note Complexity: @e O(1)
 */
static void s_viewport_clamp_client_to_canvas(surface_td *surface,
        const desktop_td *desktop, client_td *client)
{
    uint32_t columns;
    uint32_t rows;
    int32_t canvas_w;
    int32_t canvas_h;
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
    int32_t clamped_x;
    int32_t clamped_y;
    xcb_window_t target;

    s_surface_viewport_dims(surface, &columns, &rows);
    canvas_w = (int32_t) columns * (int32_t) desktop->geometry.dim.w;
    canvas_h = (int32_t) rows * (int32_t) desktop->geometry.dim.h;

    /* A client is allowed to sit anywhere its own top-left corner
     * still leaves at least one pixel of it inside the canvas, on
     * either axis, rather than requiring the whole window to fit: the
     * canvas bound itself is the only thing being enforced here, not
     * a re-centering. */
    min_x = -(int32_t) client->layout.geometry.cur.dim.w + 1;
    min_y = -(int32_t) client->layout.geometry.cur.dim.h + 1;
    max_x = canvas_w - 1;
    max_y = canvas_h - 1;

    clamped_x = client->layout.geometry.cur.pos.x;
    clamped_y = client->layout.geometry.cur.pos.y;
    clamped_x = (clamped_x < min_x) ? min_x : clamped_x;
    clamped_x = (clamped_x > max_x) ? max_x : clamped_x;
    clamped_y = (clamped_y < min_y) ? min_y : clamped_y;
    clamped_y = (clamped_y > max_y) ? max_y : clamped_y;

    if (clamped_x == client->layout.geometry.cur.pos.x &&
            clamped_y == client->layout.geometry.cur.pos.y) {
        return;     /* Already within the canvas: nothing to recover */
    }

    LOGGER_WARNING("Client 0x%08x ('%s') sat outside desktop %u's" \
            " canvas at %d,%d; clamped back to %d,%d",
            client->id, client->info.name, desktop->id,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y, clamped_x, clamped_y);

    client->layout.geometry.cur.pos.x = clamped_x;
    client->layout.geometry.cur.pos.y = clamped_y;
    client->layout.geometry.old.pos.x = clamped_x;
    client->layout.geometry.old.pos.y = clamped_y;

    target = ccmd_target_win(client);
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X | (uint16_t) XCB_CONFIG_WINDOW_Y,
            clamped_x, clamped_y, 0u, 0u, 0u);
}


/* Pan the current desktop's viewport, if needed, to bring a client
 * not currently visible into view, centered */
void scmd_surface_viewport_center_on_client(surface_td *surface,
        client_td *client)
{
    desktop_td *desktop;
    struct geometry_s screen;
    struct geometry_s win;
    struct position_s canvas_pos;
    int32_t new_x;
    int32_t new_y;

    if (surface == NULL || client == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    /* See this function's own doc comment on
     * 's_viewport_clamp_client_to_canvas': guarantees 'client' is
     * reachable by panning at all before the centering math below
     * ever runs, rather than relying solely on every drag/pan/warp
     * calculation elsewhere always being perfect. */
    s_viewport_clamp_client_to_canvas(surface, desktop, client);

    screen = desktop->geometry;
    win = client->layout.geometry.cur;

    if (win.pos.x + (int32_t) win.dim.w > screen.pos.x &&
            win.pos.x < screen.pos.x + (int32_t) screen.dim.w &&
            win.pos.y + (int32_t) win.dim.h > screen.pos.y &&
            win.pos.y < screen.pos.y + (int32_t) screen.dim.h) {
        /* Already at least partly visible on the current page: leave
         * the viewport exactly where it is rather than nudge it just
         * to perfect this client's centering. */
        return;
    }

    canvas_pos.x = win.pos.x + desktop->viewport_origin.x;
    canvas_pos.y = win.pos.y + desktop->viewport_origin.y;
    new_x = canvas_pos.x + (int32_t) win.dim.w / 2 -
        screen.pos.x - (int32_t) screen.dim.w / 2;
    new_y = canvas_pos.y + (int32_t) win.dim.h / 2 -
        screen.pos.y - (int32_t) screen.dim.h / 2;
    scmd_surface_viewport_set(surface, new_x, new_y);
}
