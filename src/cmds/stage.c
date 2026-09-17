/**
 * @file cmds/stage.c
 *
 * @brief Implementation of actions related to screen stage management
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

/* Utils includes */
#include <utils/xcb/connection.h>

/* Menu includes */
#include <menu/notify/desktop.h>

/* Policy includes */
#include <policy/stacking.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <scratchpad.h>
#include <stage.h>
#include <stage/client.h>
#include <stage/desktop.h>
#include <stage/viewport.h>
#include <wm.h>

/* Command includes */
#include <cmds/client/move.h>
#include <cmds/client/screen.h>

/* Local includes */
#include <cmds/stage.h>


/**
 * @brief Client a viewport pan's per-client translate walk must leave
 *        untouched, since a drag in progress already handles its
 *        position (or its off-screen parking spot) through
 *        @c input/mouse/drag/pan.c instead; @c NULL whenever no drag is
 *        excluding one.
 */
static client_td *s_viewport_pan_excluded_client = NULL;


/**
 * @brief Show the desktop-switch notification for the current desktop
 *
 * Retrieves the desktop name from the active desktop and passes it to
 * the notify module together with the new desktop index.  Does nothing
 * when no config is available on the stage.
 *
 * @param stage Stage whose current desktop just became active
 *
 * @param cause What moved the view, deciding which of the two
 *              @c overlay settings gates the popup
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       the stage
 */
static void s_show_desktop_overlay(stage_td *stage,
        enum notify_desktop_cause_e cause)
{
    const desktop_td *desktop;

    if (stage == NULL || xcb_connection_get() == NULL ||
            stage->config == NULL) {
        return;
    }

    desktop = lookup_current_desktop(stage);
    notify_desktop_show(xcb_connection_get(), stage,
            stage->desktop_cur,
            (desktop != NULL) ? desktop->name : "", cause,
            stage->config);
}


/**
 * @brief Show the desktop overlay when a viewport pan actually moved
 *        the origin
 *
 * The overlay's label already appends the viewport page (see
 * @a notify_desktop_show, @c menu/notify/desktop.c), so the same one
 * the desktop switch uses reports a page jump without any widget of
 * its own.  Announcing only an actual move is what keeps a shortcut
 * aimed at the page already on screen, or a selection landing on a
 * client already visible, from flashing an overlay that says nothing
 * changed.
 *
 * @param stage  Stage whose current desktop was panned
 * @param before Viewport origin recorded before the pan
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       the stage
 */
static void s_show_viewport_overlay_on_move(stage_td *stage,
        struct position_s before)
{
    const desktop_td *desktop;

    if (stage == NULL) {
        return;
    }

    desktop = lookup_current_desktop(stage);
    if (desktop == NULL || (desktop->viewport_origin.x == before.x &&
                desktop->viewport_origin.y == before.y)) {
        return;
    }

    s_show_desktop_overlay(stage, NOTIFY_DESKTOP_CAUSE_VIEWPORT);
}


/**
 * @brief Switch a stage to the desktop in a given compass direction,
 *        in cyclic order
 *
 * Shared by @a scmd_stage_desktop_switch_north and its three siblings
 * below, which only differ in direction: which of
 * @a stage_desktop_select_north/south/east/west to call, and the log
 * message's wording.
 *
 * @param stage     Stage to switch
 * @param direction Compass direction to switch toward
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
static void s_switch_cyclic(stage_td *stage,
        enum compass_direction_e direction)
{
    uint32_t old_id;
    bool cycle;
    /* Initialized here, not left to the switch below: that switch
     * deliberately has no 'default:' so the compiler keeps checking
     * it covers every direction, which also means it cannot prove to
     * itself that one of its cases always runs */
    const char *direction_label = "unknown";

    if (stage == NULL) {
        return;
    }

    old_id = stage->desktop_cur;
    cycle = (stage->config != NULL)
        ? stage->config->desktops.wrap_at_bounds : true;

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
            " on stage %u", direction_label, stage->id);

    stage_client_hide_all(stage, old_id);
    switch (direction) {
    case COMPASS_NORTH:
        stage_desktop_select_north(stage, cycle);
        break;
    case COMPASS_SOUTH:
        stage_desktop_select_south(stage, cycle);
        break;
    case COMPASS_EAST:
        stage_desktop_select_east(stage, cycle);
        break;
    case COMPASS_WEST:
        stage_desktop_select_west(stage, cycle);
        break;
    }

    if (stage->desktop_cur != old_id) {
        stage_client_pinned_transfer_all(stage,
                stage->desktop_cur);
        stage_client_show_all(stage, stage->desktop_cur);
        s_show_desktop_overlay(stage, NOTIFY_DESKTOP_CAUSE_SWITCH);
        stage->is_outdated = true;
    } else {
        /* No switch happened; restore visibility */
        stage_client_show_all(stage, old_id);
    }
}


/**
 * @brief Pan one already-panned client's icon window by the same delta,
 *        when @c viewport.pan-icons asks for it
 *
 * A no-op whenever the client has no @c config to read the flag from,
 * the flag itself is off (the default), there is no icon window at
 * all yet, that window is not currently the one on screen
 * (@a is_icon_mapped false, e.g., the client is not iconified right
 * now), or @c icon_pos is still the unset sentinel (@c -1, @c -1).
 *
 * That sentinel check tests both components together rather than
 * either one being negative, since a legitimately panned icon can
 * end up with a negative @c icon_pos.x or @c icon_pos.y the moment it
 * sits on a viewport page west or north of the one the desktop
 * itself now shows; treating that as 'unset' would silently stop
 * this same icon from following any further pan.  Otherwise shifts
 * the saved @c icon_pos by @c delta and reconfigures the real icon
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
            !client->config->base.viewport.pan_icons ||
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


/**
 * @brief Per-client callback for @a s_viewport_pan, translating one
 *        client's stored geometry and its on-screen window together
 *
 * Skips any client stuck to the screen rather than the desktop's own
 * pannable canvas (@a client_is_sticky), leaving it exactly where it
 * already sits, as well as whichever single client (if any)
 * @a scmd_stage_viewport_drag_exclude last named.
 *
 * Every other client, maximized or fullscreen included, moves by the
 * same delta as the desktop's own viewport origin: both @c cur and
 * @c old halves of its saved geometry shift together, so a later
 * unmaximize or unshade restores it to where this pan left it rather
 * than to where it sat before.  Reaches the real window directly
 * through @a ccmd_client_apply_geometry rather than
 * @a ccmd_client_move, since that higher-level wrapper refuses to touch
 * a maximized or fullscreen client at all.  Finally hands off to
 * @a s_viewport_translate_icon, which decides on its own whether this
 * client's icon should pan along too.
 *
 * @param client Client the walk is currently visiting; never null
 * @param data   The @c struct position_s pixel delta to add to
 *               @p client's position, cast back from @c void*
 *
 * @note Complexity: @e O(1)
 */
static void s_viewport_translate_visit(client_td *client, void *data)
{
    const struct position_s *delta = (const struct position_s *) data;
    xcb_window_t target;

    if (client_is_sticky(client) ||
            client == s_viewport_pan_excluded_client) {
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
 * @brief Clamp a requested viewport origin to the pannable area and,
 *        if it differs from the current one, translate every
 *        non-sticky client on @p desktop by the resulting delta
 *
 * Shared by @a s_viewport_pan and @a scmd_stage_viewport_set, which
 * only differ in how each arrives at the requested @p origin before
 * this clamp is applied.  Also the one place, once an actual pan is
 * confirmed, that reaches @a scratchpad_notice_viewport_panned, so
 * every path that ever moves a viewport (an edge-hover pan,
 * a background or window drag crossing an edge, a keybind, an IPC
 * command) hides the scratchpad the same way, without each needing its
 * own separate call.
 *
 * @param stage Stage owning @p desktop, marked outdated when the
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
static void s_viewport_apply_origin(stage_td *stage,
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
    stage->is_outdated = true;

    /* A visible scratchpad is just another non-sticky client the walk
     * above already shifted away from its own configured edge, same as
     * everything else on 'desktop'; hiding it here, rather than leaving
     * it wherever that shift left it, is what keeps it from ever being
     * seen drifted out of its own zone.  A no-op with no current
     * scratchpad client, one already hidden, or one that belongs to
     * some other desktop. */
    scratchpad_notice_viewport_panned(desktop);
}


/**
 * @brief Pan the current desktop's viewport by one whole screen in
 *        a given compass direction, clamped at the edges of the
 *        pannable area
 *
 * Shared by @a scmd_stage_viewport_pan_north and its three siblings
 * below, which only differ in direction.
 *
 * Unlike @a s_switch_cyclic, this never wraps around and never changes
 * @p stage's current desktop: it only moves where within that one
 * desktop the physical screen is looking, translating every non-sticky
 * client the opposite way so their positions on screen stay put
 * relative to the desktop's virtual canvas.
 *
 * @param stage     Stage to pan
 * @param direction Compass direction to pan toward
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       current desktop
 */
static void s_viewport_pan(stage_td *stage,
        enum compass_direction_e direction)
{
    desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    struct position_s origin;
    struct position_s origin_before;

    if (stage == NULL) {
        return;
    }

    desktop = lookup_current_desktop(stage);
    if (desktop == NULL) {
        return;
    }

    stage_viewport_dims(stage, &columns, &rows);
    origin_before = desktop->viewport_origin;
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

    s_viewport_apply_origin(stage, desktop, columns, rows, origin);
    /* A whole-page move is a jump, the same kind of change a desktop
     * switch is, so it announces itself; the pixel-sized steps of
     * 's_viewport_pan_step' deliberately do not */
    s_show_viewport_overlay_on_move(stage, origin_before);
}


/**
 * @brief Remainder of @p value over @p modulus, always in
 *        @c [0, @p modulus)
 *
 * C's @c % truncates toward zero, so a negative @p value yields a
 * negative remainder; one modulus added back folds it forward.
 *
 * @param value   Value to reduce; may be negative
 * @param modulus Modulus to reduce it by; never zero
 *
 * @return Remainder in @c [0, @p modulus)
 *
 * @note Complexity: @e O(1)
 */
static int32_t s_positive_remainder(int32_t value, int32_t modulus)
{
    const int32_t remainder = value % modulus;

    return (remainder < 0) ? remainder + modulus : remainder;
}


/**
 * @brief Convert a desktop-space (virtual-canvas) position into its
 *        configured viewport page, the exact inverse of the per-axis
 *        multiply @a scmd_stage_viewport_goto already does to land
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
    int32_t col;
    int32_t row;

    /* A desktop whose geometry has not been resolved yet would divide
     * by zero here, which is undefined behavior rather than a
     * recoverable error, and every caller can do something sensible
     * with page zero */
    if (desktop->geometry.dim.w == 0u || desktop->geometry.dim.h == 0u) {
        *col_out = 0u;
        *row_out = 0u;
        return;
    }

    col = canvas_pos.x / (int32_t) desktop->geometry.dim.w;
    row = canvas_pos.y / (int32_t) desktop->geometry.dim.h;

    *col_out = (col < 0) ? 0u : (uint32_t) col;
    *row_out = (row < 0) ? 0u : (uint32_t) row;
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
 * even via the search menu, the moment any one of them is not.
 *
 * Only ever moves @p client when it is genuinely outside the whole
 * canvas, never merely off the currently panned-to page, so a client
 * legitimately sitting on some other, valid page is left exactly where
 * it is for @a scmd_stage_viewport_center_on_client to pan to
 * instead.
 *
 * @param stage   Stage owning @p desktop
 * @param desktop Desktop whose pannable canvas @p client must sit
 *                within
 * @param client Client whose recorded position is clamped in place
 *
 * @note Complexity: @e O(1)
 */
static void s_viewport_clamp_client_to_canvas(stage_td *stage,
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

    stage_viewport_dims(stage, &columns, &rows);
    canvas_w = (int32_t) columns * (int32_t) desktop->geometry.dim.w;
    canvas_h = (int32_t) rows * (int32_t) desktop->geometry.dim.h;

    /* A client is allowed to sit anywhere its own top-left corner still
     * leaves at least one pixel of it inside the canvas, on either
     * axis, rather than requiring the whole window to fit: the canvas
     * bound itself is the only thing being enforced here, not
     * a re-centering. */
    min_x = -(int32_t) client->layout.geometry.cur.dim.w + 1;
    min_y = -(int32_t) client->layout.geometry.cur.dim.h + 1;
    max_x = canvas_w - 1;
    max_y = canvas_h - 1;

    /* Converted to canvas coordinates before being bounded, and back
     * afterwards.  A client's recorded position is relative to what is
     * on screen, not to the canvas: 's_viewport_translate_visit' above
     * shifts every non-sticky one of them by the pan delta, so the same
     * window reads a different position from every viewport origin.
     * Bounding the screen-relative value directly against a canvas-wide
     * limit would drag every client on some other page back toward the
     * current one the moment the origin left zero, which is exactly
     * what this function's own contract above says it must never
     * do. */
    clamped_x = client->layout.geometry.cur.pos.x +
        desktop->viewport_origin.x;
    clamped_y = client->layout.geometry.cur.pos.y +
        desktop->viewport_origin.y;
    clamped_x = (clamped_x < min_x) ? min_x : clamped_x;
    clamped_x = (clamped_x > max_x) ? max_x : clamped_x;
    clamped_y = (clamped_y < min_y) ? min_y : clamped_y;
    clamped_y = (clamped_y > max_y) ? max_y : clamped_y;
    clamped_x -= desktop->viewport_origin.x;
    clamped_y -= desktop->viewport_origin.y;

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


/**
 * @brief Guarantee a client's own recorded position sits within the
 *        one page @p desktop is actually showing right now, moving it
 *        back in if not
 *
 * Unlike @a s_viewport_clamp_client_to_canvas, whose "at least one
 * pixel still inside" tolerance only makes sense together with the
 * pan to that exact page its own caller
 * (@a scmd_stage_viewport_center_on_client) always follows it with:
 * a client left with merely one pixel inside the whole, possibly
 * still multi-page canvas is nowhere near genuinely visible on
 * whichever page happens to be on screen right now, and nothing calls
 * this expecting to pan afterward, since a reload may have stranded
 * clients across several different vanished pages at once and there
 * is only the one page left to show them all on.
 *
 * Screen-relative coordinates are bound directly against one screen's
 * width and height, with no canvas or viewport-origin arithmetic
 * needed at all: a client's own recorded position already reads
 * relative to whichever page is currently on screen, by the same
 * convention @a s_viewport_translate_visit's own delta shift relies
 * on, so that screen-relative value is already the one page's own
 * coordinate space.  The bound itself is the client's whole size, not
 * a single pixel of it either, for the same reason: a corner just
 * barely touching the page is no more genuinely visible here than
 * one just barely touching the whole canvas would be.
 *
 * @param desktop Desktop whose currently shown page @p client must
 *                sit within
 * @param client Client whose recorded position is clamped in place
 *
 * @note Complexity: @e O(1)
 */
static void s_viewport_clamp_client_to_current_page(
        const desktop_td *desktop, client_td *client)
{
    int32_t width = (int32_t) client->layout.geometry.cur.dim.w;
    int32_t height = (int32_t) client->layout.geometry.cur.dim.h;
    int32_t page_w = (int32_t) desktop->geometry.dim.w;
    int32_t page_h = (int32_t) desktop->geometry.dim.h;
    int32_t max_x;
    int32_t max_y;
    int32_t clamped_x;
    int32_t clamped_y;
    xcb_window_t target;

    /* The whole client, not merely one pixel of it: unlike
     * 's_viewport_clamp_client_to_canvas', where the "just one pixel"
     * tolerance is only ever genuinely visible because centering on
     * a client always pans to its exact page right afterward, nothing
     * here promises that, so "technically inside" is not enough.
     * A client wider or taller than the page itself is left flush
     * against 0 on whichever axis does not fit, rather than clamped
     * to a negative maximum that would push it off the opposite edge
     * instead. */
    max_x = (width < page_w) ? page_w - width : 0;
    max_y = (height < page_h) ? page_h - height : 0;

    clamped_x = client->layout.geometry.cur.pos.x;
    clamped_y = client->layout.geometry.cur.pos.y;
    clamped_x = (clamped_x < 0) ? 0 : clamped_x;
    clamped_x = (clamped_x > max_x) ? max_x : clamped_x;
    clamped_y = (clamped_y < 0) ? 0 : clamped_y;
    clamped_y = (clamped_y > max_y) ? max_y : clamped_y;

    if (clamped_x == client->layout.geometry.cur.pos.x &&
            clamped_y == client->layout.geometry.cur.pos.y) {
        return;     /* Already on the current page: nothing to do */
    }

    LOGGER_WARNING("Client 0x%08x ('%s') sat off desktop %u's" \
            " currently shown page at %d,%d; clamped back to %d,%d",
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


/* Switch to another desktop */
void scmd_stage_desktop_switch(stage_td *stage,
        uint32_t desktop_id)
{
    uint32_t old_id;

    if (stage == NULL) {
        return;
    }

    old_id = stage->desktop_cur;
    if (desktop_id == old_id) {
        return;     /* Already on this desktop */
    }

    LOGGER_DEBUG("Switching desktop: %u to %u on stage %u",
            old_id, desktop_id, stage->id);

    stage_client_hide_all(stage, old_id);
    if (stage_desktop_select(stage, desktop_id) != 0) {
        stage_client_show_all(stage, old_id);
        return;
    }
    stage_client_pinned_transfer_all(stage, desktop_id);
    stage_client_show_all(stage, desktop_id);

    s_show_desktop_overlay(stage, NOTIFY_DESKTOP_CAUSE_SWITCH);

    stage->is_outdated = true;
}


/* Switch to the desktop north of the current one */
void scmd_stage_desktop_switch_north(stage_td *stage)
{
    s_switch_cyclic(stage, COMPASS_NORTH);
}


/* Switch to the desktop south of the current one */
void scmd_stage_desktop_switch_south(stage_td *stage)
{
    s_switch_cyclic(stage, COMPASS_SOUTH);
}


/* Switch to the desktop east of the current one */
void scmd_stage_desktop_switch_east(stage_td *stage)
{
    s_switch_cyclic(stage, COMPASS_EAST);
}


/* Switch to the desktop west of the current one */
void scmd_stage_desktop_switch_west(stage_td *stage)
{
    s_switch_cyclic(stage, COMPASS_WEST);
}


/* Set or clear the one client the translate walk of every future pan
 * must leave untouched, because a drag already owns its position */
void scmd_stage_viewport_drag_exclude(client_td *client)
{
    s_viewport_pan_excluded_client = client;
}


/* Whether the current desktop's viewport still has room to pan one
 * more screen toward a given compass direction */
bool scmd_stage_viewport_pan_available(stage_td *stage,
        enum compass_direction_e direction)
{
    const desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    int32_t max_x;
    int32_t max_y;
    /* Initialized here, not left to the switch below, matching
     * 's_warp_target_desktop' ('input/mouse/drag/warp.c'): that switch
     * deliberately has no 'default:' so the compiler keeps checking it
     * against every direction, which also means it cannot prove to
     * itself that one of its cases always runs. */
    bool available = false;

    if (stage == NULL) {
        return false;
    }

    desktop = lookup_current_desktop(stage);
    if (desktop == NULL) {
        return false;
    }

    stage_viewport_dims(stage, &columns, &rows);
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
void scmd_stage_viewport_pan_north(stage_td *stage)
{
    s_viewport_pan(stage, COMPASS_NORTH);
}


/* Pan the current desktop's viewport one screen south */
void scmd_stage_viewport_pan_south(stage_td *stage)
{
    s_viewport_pan(stage, COMPASS_SOUTH);
}


/* Pan the current desktop's viewport one screen east */
void scmd_stage_viewport_pan_east(stage_td *stage)
{
    s_viewport_pan(stage, COMPASS_EAST);
}


/* Pan the current desktop's viewport one screen west */
void scmd_stage_viewport_pan_west(stage_td *stage)
{
    s_viewport_pan(stage, COMPASS_WEST);
}


/* Pan the current desktop's viewport by 'viewport.pan-step' pixels */
void scmd_stage_viewport_pan_step(stage_td *stage,
        enum compass_direction_e direction)
{
    desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    uint32_t step;
    struct position_s origin;

    if (stage == NULL) {
        return;
    }

    desktop = lookup_current_desktop(stage);
    if (desktop == NULL) {
        return;
    }

    step = (stage->config != NULL) ? stage->config->base
        .viewport.pan_step : 0u;
    stage_viewport_dims(stage, &columns, &rows);
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

    s_viewport_apply_origin(stage, desktop, columns, rows, origin);
}


/* Move the current desktop's viewport straight to an absolute origin */
void scmd_stage_viewport_set(stage_td *stage,
        int32_t x, int32_t y)
{
    desktop_td *desktop;
    uint32_t columns;
    uint32_t rows;
    struct position_s origin;

    if (stage == NULL) {
        return;
    }

    desktop = lookup_current_desktop(stage);
    if (desktop == NULL) {
        return;
    }

    stage_viewport_dims(stage, &columns, &rows);
    origin.x = x;
    origin.y = y;
    s_viewport_apply_origin(stage, desktop, columns, rows, origin);
}


/**
 * @brief What @a s_viewport_reclamp_visit needs for each client it is
 *        handed
 */
struct s_reclamp_ctx_s {
    const desktop_td *desktop;   /**< Desktop being re-clamped */
};


/**
 * @brief Bring one client back onto its desktop's currently shown
 *        page if a just-shrunk viewport left it off it
 *
 * A @a stacking_walk visitor wrapping @a s_viewport_clamp_client_to_
 * current_page, called once per client on
 * @a scmd_stage_viewport_reclamp's own desktop.
 *
 * @param client Client to clamp back onto the current page if needed
 * @param data   This desktop's own @c s_reclamp_ctx_s
 *
 * @note Complexity: @e O(1)
 */
static void s_viewport_reclamp_visit(client_td *client, void *data)
{
    const struct s_reclamp_ctx_s *const ctx = data;

    s_viewport_clamp_client_to_current_page(ctx->desktop, client);
}


/* Re-clamp a desktop's own current viewport origin against its
 * stage's presently configured viewport size */
void scmd_stage_viewport_reclamp(stage_td *stage,
        desktop_td *desktop)
{
    uint32_t columns;
    uint32_t rows;
    struct s_reclamp_ctx_s ctx;

    if (stage == NULL || desktop == NULL) {
        return;
    }

    stage_viewport_dims(stage, &columns, &rows);

    /* The desktop's own pan position first, so a page it was actually
     * showing that no longer exists lands back on one that does; only
     * then, individual clients: with the origin already corrected,
     * every client still on the desktop is clamped onto this one,
     * now-current page directly, screen-relative coordinates needing
     * no origin arithmetic of their own to do that (see
     * 's_viewport_clamp_client_to_current_page''s own comment on
     * exactly why that, and not 's_viewport_clamp_client_to_canvas',
     * is the one this needs). */
    s_viewport_apply_origin(stage, desktop, columns, rows,
            desktop->viewport_origin);

    /* A client already sitting on the page now being shown, even one
     * that was never the page being shown before the reclamp above,
     * is left exactly where it is: only a client left off it, on some
     * other page the shrink also removed, is moved. */
    ctx.desktop = desktop;
    stacking_walk(desktop, s_viewport_reclamp_visit, &ctx);
}


/* Jump the current desktop's viewport straight to one of its
 * configured pages, addressed by a single linear index */
void scmd_stage_viewport_goto(stage_td *stage, uint32_t page)
{
    const desktop_td *desktop;
    struct position_s origin_before;
    uint32_t columns;
    uint32_t rows;
    uint32_t col;
    uint32_t row;

    if (stage == NULL) {
        return;
    }

    desktop = lookup_current_desktop(stage);
    if (desktop == NULL) {
        return;
    }

    stage_viewport_dims(stage, &columns, &rows);
    if (page >= columns * rows) {
        return;
    }

    col = page % columns;
    row = page / columns;
    origin_before = desktop->viewport_origin;
    scmd_stage_viewport_set(stage,
            (int32_t) (col * desktop->geometry.dim.w),
            (int32_t) (row * desktop->geometry.dim.h));
    s_show_viewport_overlay_on_move(stage, origin_before);
}


/* Move a client to a given page of the current desktop's viewport,
 * keeping its position within that page */
void scmd_stage_viewport_client_send_to_page(stage_td *stage,
        client_td *client, uint32_t col, uint32_t row)
{
    desktop_td *desktop;
    struct position_s canvas_pos;
    int32_t page_w;
    int32_t page_h;
    int32_t new_x;
    int32_t new_y;
    xcb_window_t target;

    if (stage == NULL || client == NULL || client_is_sticky(client)) {
        return;
    }

    desktop = lookup_current_desktop(stage);
    if (desktop == NULL) {
        return;
    }

    page_w = (int32_t) desktop->geometry.dim.w;
    page_h = (int32_t) desktop->geometry.dim.h;
    if (page_w <= 0 || page_h <= 0) {
        return;
    }

    /* The offset within whichever page the client sits on now is kept
     * as it is, so a window near a page's top-left corner lands near
     * the new page's top-left corner rather than being re-placed */
    canvas_pos.x = client->layout.geometry.cur.pos.x +
        desktop->viewport_origin.x;
    canvas_pos.y = client->layout.geometry.cur.pos.y +
        desktop->viewport_origin.y;
    new_x = (int32_t) col * page_w + s_positive_remainder(canvas_pos.x,
            page_w) - desktop->viewport_origin.x;
    new_y = (int32_t) row * page_h + s_positive_remainder(canvas_pos.y,
            page_h) - desktop->viewport_origin.y;

    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    client->layout.geometry.old.pos.x = new_x;
    client->layout.geometry.old.pos.y = new_y;

    target = ccmd_target_win(client);
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y,
            new_x, new_y, 0u, 0u, 0u);

    /* The move above repositions the content window, invisibly for an
     * iconified client: unmapped, it is the icon window alone that is
     * actually on screen, tracking its own separate on-screen position
     * ('icon_pos') rather than 'layout.geometry.cur.pos'.  Left
     * untouched, the icon would stay exactly where it was regardless
     * of which page it was just sent to, the same "nothing visibly
     * happened" gap 's_viewport_translate_icon' already exists to
     * avoid for an actual pan; this follows its exact same math,
     * moved by the destination page instead of a pan delta. */
    if (client->is_icon_mapped && client->icon_window != 0) {
        struct position_s icon_canvas_pos;
        int32_t new_icon_x;
        int32_t new_icon_y;

        icon_canvas_pos.x = client->icon_pos.x +
            desktop->viewport_origin.x;
        icon_canvas_pos.y = client->icon_pos.y +
            desktop->viewport_origin.y;
        new_icon_x = (int32_t) col * page_w +
            s_positive_remainder(icon_canvas_pos.x, page_w) -
            desktop->viewport_origin.x;
        new_icon_y = (int32_t) row * page_h +
            s_positive_remainder(icon_canvas_pos.y, page_h) -
            desktop->viewport_origin.y;

        client->icon_pos.x = (int16_t) new_icon_x;
        client->icon_pos.y = (int16_t) new_icon_y;
        ccmd_client_apply_geometry(client, client->icon_window,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y,
                new_icon_x, new_icon_y, 0u, 0u, 0u);
    }

    stage->is_outdated = true;
}


/* Report the viewport page a client currently sits on */
bool scmd_stage_viewport_client_page(const stage_td *stage,
        const desktop_td *desktop, const client_td *client,
        uint32_t *col_out, uint32_t *row_out)
{
    uint32_t columns;
    uint32_t rows;
    struct position_s canvas_pos;

    if (stage == NULL || desktop == NULL || client == NULL ||
            col_out == NULL || row_out == NULL) {
        return false;
    }

    stage_viewport_dims(stage, &columns, &rows);
    if (columns <= 1u && rows <= 1u) {
        return false;
    }

    /* A sticky client belongs to no one page: it is excluded from the
     * pan translation ('s_viewport_translate_visit' above), so its
     * stored position is where it sits on screen rather than a point
     * on the canvas, and it is in view from every origin.  Answering
     * with the page its corner happens to land on would have callers
     * send the user somewhere for a window already in front of them. */
    if (client_is_sticky(client)) {
        return false;
    }

    canvas_pos.x = client->layout.geometry.cur.pos.x +
        desktop->viewport_origin.x;
    canvas_pos.y = client->layout.geometry.cur.pos.y +
        desktop->viewport_origin.y;
    s_viewport_page_for_canvas_pos(canvas_pos, desktop,
            col_out, row_out);
    return true;
}


/* Report the viewport page a desktop's viewport currently shows */
bool scmd_stage_viewport_desktop_page(const stage_td *stage,
        const desktop_td *desktop, uint32_t *col_out, uint32_t *row_out)
{
    struct position_s centre;
    uint32_t columns;
    uint32_t rows;

    if (stage == NULL || desktop == NULL ||
            col_out == NULL || row_out == NULL) {
        return false;
    }

    stage_viewport_dims(stage, &columns, &rows);
    if (columns <= 1u && rows <= 1u) {
        return false;
    }

    /* Measured from the middle of what is on screen, not from its
     * corner.  Panning leaves the origin at any pixel, so a view
     * straddling two pages would otherwise report whichever one holds
     * its top-left corner even when that is the sliver and the other
     * one fills the screen.  On an origin sitting exactly on a page,
     * which is where every 'go-to' and whole-page move lands, the two
     * agree: 'col * w + w / 2' divided by 'w' is 'col'. */
    centre.x = desktop->viewport_origin.x +
        (int32_t) desktop->geometry.dim.w / 2;
    centre.y = desktop->viewport_origin.y +
        (int32_t) desktop->geometry.dim.h / 2;
    s_viewport_page_for_canvas_pos(centre, desktop, col_out, row_out);
    return true;
}


/* Pan the current desktop's viewport, if needed, to bring a client
 * not currently visible into view, centered */
void scmd_stage_viewport_center_on_client(stage_td *stage,
        client_td *client)
{
    desktop_td *desktop;
    struct geometry_s screen;
    struct geometry_s win;
    struct position_s canvas_pos;
    struct position_s origin_before;
    uint32_t client_col;
    uint32_t client_row;
    uint32_t page_col;
    uint32_t page_row;
    int32_t frame_w;
    int32_t frame_h;
    int32_t new_x;
    int32_t new_y;

    if (stage == NULL || client == NULL) {
        return;
    }

    desktop = lookup_current_desktop(stage);
    if (desktop == NULL) {
        return;
    }

    /* Every calculation below reads 'client' position against this
     * desktop's own viewport origin, which only means anything for a
     * client that is on this desktop.  A client belonging to another
     * one has its position expressed against that desktop's origin
     * instead, so the arithmetic would pan somewhere arbitrary and,
     * worse, the clamp just below would write a corrected position
     * back onto a window this desktop has no business moving.
     *
     * Callers that switch desktops before asking, the search box and
     * the cycle menu among them, satisfy this by construction.
     * 'focus_apply' (policy/focus.c) does not: the window list and
     * the '_NET_ACTIVE_WINDOW' handler both reach it with a client
     * that may still be elsewhere. */
    if (wm_get_client_desktop(client) != desktop) {
        return;
    }

    /* See this function's own doc comment on
     * 's_viewport_clamp_client_to_canvas': guarantees 'client' is
     * reachable by panning at all before the centering math below
     * ever runs, rather than relying solely on every drag/pan/warp
     * calculation elsewhere always being perfect. */
    s_viewport_clamp_client_to_canvas(stage, desktop, client);

    screen = desktop->geometry;
    win = client->layout.geometry.cur;

    /* The whole window, decoration included.  'geometry.cur.pos' is
     * already the frame's own corner ('ccmd_target_win', in
     * cmds/client/screen.c, configures the frame for a decorated
     * client), but 'dim' is the content's, so the titlebar and the
     * borders have to be added back or every calculation below is off
     * by them.  Both are zero on an undecorated client. */
    frame_w = (int32_t) win.dim.w +
        (int32_t) client->layout.frame_extents.left +
        (int32_t) client->layout.frame_extents.right;
    frame_h = (int32_t) win.dim.h +
        (int32_t) client->layout.frame_extents.top +
        (int32_t) client->layout.frame_extents.bottom;

    /* Whether the client is on the page being shown, not whether some
     * pixel of it happens to overlap it.  A window pressed against a
     * page's edge shows a sliver of its frame on the neighboring page,
     * and an overlap test would call that "already visible" and leave
     * the viewport where it is, which is exactly the case this is
     * asked about.  The page a window belongs to is where its own
     * corner falls, the same rule "Send to page" and the per-page
     * rearrange use. */
    /* A sticky client is on screen from every origin, so there is
     * never anywhere to pan to for it.  Asked separately because the
     * page lookup below now reports no page at all for one, and a
     * missing page must not read as "somewhere else". */
    if (client_is_sticky(client)) {
        return;
    }

    if (scmd_stage_viewport_client_page(stage, desktop, client,
                &client_col, &client_row) &&
            scmd_stage_viewport_desktop_page(stage, desktop,
                &page_col, &page_row) &&
            client_col == page_col && client_row == page_row) {
        return;
    }

    canvas_pos.x = win.pos.x + desktop->viewport_origin.x;
    canvas_pos.y = win.pos.y + desktop->viewport_origin.y;
    new_x = canvas_pos.x + frame_w / 2 -
        screen.pos.x - (int32_t) screen.dim.w / 2;
    new_y = canvas_pos.y + frame_h / 2 -
        screen.pos.y - (int32_t) screen.dim.h / 2;
    origin_before = desktop->viewport_origin;
    scmd_stage_viewport_set(stage, new_x, new_y);
    s_show_viewport_overlay_on_move(stage, origin_before);
}
