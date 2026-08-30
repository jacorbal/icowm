/**
 * @file cmds/client/maximize.c
 *
 * @brief Client maximize/fullscreen command implementation
 *
 * One of the files @c cmds/client/ is made of.  Covers the per-axis
 * maximize state machine (horizontal, vertical, or both), and
 * re-filling an already-maximized client's geometry against a
 * workarea that has since changed.
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

/* Project includes */
#include <client.h>
#include <surface.h>
#include <utils/geom.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/focus.h>
#include <cmds/client/maximize.h>
#include <cmds/client/move.h>
#include <cmds/client/screen.h>
#include <cmds/client/state.h>
#include <cmds/client/workarea.h>
#include <utils/xcb/connection.h>
/**
 * @brief Hold a maximized size to whatever maximum the client declared
 *
 * ICCCM §4.1.2.3 has the window manager honor @c WM_NORMAL_HINTS
 * @c max_width and @c max_height, and nothing in EWMH exempts
 * maximization from that: a client stating a maximum expects it to
 * hold here too, which is why a dialog or a fixed-size utility being
 * maximized should stop at its limit rather than stretch past it.
 *
 * Only the maximum is applied, deliberately.  The minimum cannot bind,
 * the workarea being larger than it in any sane case, and the resize
 * increments are left alone: snapping a maximized window down to whole
 * character cells is defensible, but it is a different decision from
 * this one and would change the size of every maximized terminal on
 * the desktop.
 *
 * The hints describe the client's content, so the frame the
 * decoration adds is taken off before comparing and put back after.
 *
 * @param client Client whose hints apply
 * @param w      Frame width to hold, updated in place
 * @param h      Frame height to hold, updated in place
 *
 * @note Complexity: @e O(1)
 */
static void s_clamp_to_size_hints(const client_td *client,
        uint16_t *w, uint16_t *h)
{
    uint32_t deco_w;
    uint32_t deco_h;

    if (client == NULL || w == NULL || h == NULL ||
            !client->hints_icccm.size.is_valid) {
        return;
    }

    deco_w = (uint32_t) (client->layout.frame_extents.left +
            client->layout.frame_extents.right);
    deco_h = (uint32_t) (client->layout.frame_extents.top +
            client->layout.frame_extents.bottom);

    if (client->hints_icccm.size.max.w > 0u &&
            (uint32_t) *w > client->hints_icccm.size.max.w + deco_w) {
        *w = (uint16_t) (client->hints_icccm.size.max.w + deco_w);
    }
    if (client->hints_icccm.size.max.h > 0u &&
            (uint32_t) *h > client->hints_icccm.size.max.h + deco_h) {
        *h = (uint16_t) (client->hints_icccm.size.max.h + deco_h);
    }
}


/**
 * @brief Precondition checks shared by @a ccmd_client_maximize,
 *        @a ccmd_client_maximize_horz and
 *        @a ccmd_client_maximize_vert, restoring an iconified
 *        client and unshading a shaded one
 *        along the way
 *
 * @param client Client about to be maximized, on one axis or both
 *
 * @return @c true if the caller should proceed (the client is
 *         resizable, not fullscreen, and any prior iconified or
 *         shaded state has already been cleared); @c false if
 *         @p client is @c NULL or the maximize should be refused
 *         outright
 *
 * @note Complexity: @e O(1)
 */
static bool s_ccmd_maximize_precheck(client_td *client)
{
    if (client == NULL) {
        return false;
    }

    if (!client_is_resizable(client) || client_is_fullscreen(client)) {
        return false;
    }

    /* An iconified client's target window is unmapped and its
     * icon window stands in for it; maximizing it in place here
     * would map the frame back while the icon window is still up,
     * the same reasoning as the identical guard in
     * 'ccmd_client_shade' and 'ccmd_client_fullscreen' (state.c). */
    if (client_is_iconified(client)) {
        ccmd_client_restore(client);
    }

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    return true;
}


/**
 * @brief Undo maximization on whichever axes the request names
 *
 * @param client    Client to demote
 * @param dir       Which axes: 0 both, 1 horizontal, 2 vertical
 * @param target    Window the geometry is applied to
 * @param horz_now  Whether it is maximized horizontally already
 * @param vert_now  Whether it is maximized vertically already
 *
 * @return @c true when the request was a demotion and is now done
 *
 * @note Complexity: @e O(1)
 */
static bool s_ccmd_maximize_demote(client_td *client, int dir,
        xcb_window_t target, bool horz_now, bool vert_now)
{
    if (dir == 0 && horz_now && vert_now) {
        client_geometry_restore(client);
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                client->layout.geometry.cur.pos.x,
                client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h, 0u);
        client->properties.state &= (uint16_t) ~CLIENT_STATE_MAXIMIZED;
        if (client->frame != 0) {
            client_decoration_layout_sync(client);
        }
        ccmd_client_sync_states(client);
        wm_request_client_redraw(client);
        return true;
    }

    /* Demote this single axis alone, restoring it from the saved
     * pre-maximize geometry and leaving the other axis exactly as it
     * currently is: fully maximized demotes to the other axis alone,
     * and this axis alone demotes to normal. Only reachable for a
     * single-axis 'dir'; 'dir == 0' either already returned above
     * (both axes maximized) or falls through to maximizing both
     * below regardless of any single axis's current state. */
    if (dir != 0 && ((dir == 1 && horz_now) || (dir == 2 && vert_now))) {
        if (dir == 1) {
            client->layout.geometry.cur.pos.x =
                client->layout.geometry.old.pos.x;
            client->layout.geometry.cur.dim.w =
                client->layout.geometry.old.dim.w;
            ccmd_client_apply_geometry(client, target,
                    (uint16_t) XCB_CONFIG_WINDOW_X |
                        (uint16_t) XCB_CONFIG_WINDOW_WIDTH,
                    client->layout.geometry.cur.pos.x, 0,
                    client->layout.geometry.cur.dim.w, 0u, 0u);
            client->properties.state &=
                (uint16_t) ~CLIENT_STATE_MAXIMIZED_HORZ;
        } else {
            client->layout.geometry.cur.pos.y =
                client->layout.geometry.old.pos.y;
            client->layout.geometry.cur.dim.h =
                client->layout.geometry.old.dim.h;
            ccmd_client_apply_geometry(client, target,
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                        (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                    0, client->layout.geometry.cur.pos.y,
                    0u, client->layout.geometry.cur.dim.h, 0u);
            client->properties.state &=
                (uint16_t) ~CLIENT_STATE_MAXIMIZED_VERT;
        }
        if (client->frame != 0) {
            client_decoration_layout_sync(client);
        }
        ccmd_client_sync_states(client);
        wm_request_client_redraw(client);
        return true;
    }

    return false;
}


/**
 * @brief Maximize a client on one axis or both, or restore/demote/
 *        complete depending on its current maximize state
 *
 * The shared implementation behind @a ccmd_client_maximize,
 * @a ccmd_client_maximize_horz and @a ccmd_client_maximize_vert,
 * each now a
 * thin wrapper passing its fixed @p dir; matches Openbox's
 * @c client_maximize (@c client.c), which takes the identical @p dir
 * convention for the identical reason: one function, one place the
 * demote/complete/fresh-maximize decision is made, rather than the
 * same three-way branch (see below) duplicated once per axis.
 *
 * Each axis is a bit of its own in @c properties.state, the way EWMH
 * holds them, so "is the horizontal axis maximized" is one bit test,
 * which @a client_is_maximized_horz makes.
 *
 * @p dir @c == @c 0, both axes, has the two cases Openbox's
 * top-level toggle does: already maximized in both directions, so
 * restore; anything else, so maximize both.  @p dir @c == @c 1 or
 * @c 2, one axis, has three: demote this axis alone if it is
 * currently maximized, restoring it from @c layout.geometry.old and
 * leaving the other exactly as it is; fold this axis in from the
 * workarea if the other is already maximized; or maximize this axis
 * alone otherwise, saving the pre-maximize geometry first unless some
 * maximized state already holds it.
 *
 * @param client Client to maximize
 * @param dir    @c 0 for both axes, @c 1 for horizontal only, @c 2
 *               for vertical only
 *
 * @note A null @p client, or a failing
 *       @a s_ccmd_maximize_precheck, is a silent no-op
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_maximize_dir(client_td *client, int dir)
{
    int32_t mx = 0;
    int32_t my = 0;
    uint16_t sw = 0;
    uint16_t sh = 0;
    xcb_window_t target;
    const desktop_td *own_desktop;
    bool is_active;
    uint32_t border;
    bool want_horz = (dir == 0 || dir == 1);
    bool horz_now;
    bool vert_now;

    if (!s_ccmd_maximize_precheck(client)) {
        return;
    }

    horz_now = client_is_maximized_horz(client);
    vert_now = client_is_maximized_vert(client);
    target = ccmd_target_win(client);

    /* Toggle: both axes fully maximized already restores to normal;
     * any other current state (normal, or maximized on just one
     * axis) falls through to maximizing both below instead,
     * overriding whatever partial state was there. */
    if (s_ccmd_maximize_demote(client, dir, target, horz_now,
                vert_now)) {
        return;
    }

    if (!ccmd_client_resolve_workarea(client, &mx, &my, &sw, &sh) &&
            !ccmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    /* Per the X11 protocol (ConfigureWindow), 'x'/'y' name a window's
     * own top-left corner including its native border, if any, drawn
     * growing rightward/downward from there: the full on-screen
     * footprint of a client with one reaches all the way to
     * 'x + 2 * border + w', 2 * border wider/taller than 'w' alone
     * (equivalently for height).  'client_border_width' is 0 for a
     * decorated client, so this only ever actually shrinks the
     * target for an undecorated one; kept within the workarea/
     * monitor rect 'sw'/'sh' just resolved above, rather than
     * spilling its border past its right/bottom edge. */
    own_desktop = wm_get_client_desktop(client);
    is_active = own_desktop != NULL &&
        own_desktop->client_active_id == client->id;
    border = 2u * client_border_width(client, is_active, false);
    sw = (uint16_t) ((sw > border) ? sw - border : 0u);
    sh = (uint16_t) ((sh > border) ? sh - border : 0u);

    s_clamp_to_size_hints(client, &sw, &sh);


    /* Complete this single axis to full maximize: the other axis is
     * already the one currently maximized, so fold this one in from
     * the workarea without disturbing it.  Only reachable for a
     * single-axis 'dir'. */
    if (dir == 1 && vert_now) {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH,
                mx, 0, sw, 0u, 0u);
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.dim.w = sw;
        client->properties.state |=
            (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
        if (client->frame != 0) {
            client_decoration_layout_sync(client);
        }
        ccmd_client_sync_states(client);
        wm_request_client_redraw(client);
        return;
    }
    if (dir == 2 && horz_now) {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                0, my, 0u, sh, 0u);
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.h = sh;
        client->properties.state |=
            (uint16_t) CLIENT_STATE_MAXIMIZED_VERT;
        if (client->frame != 0) {
            client_decoration_layout_sync(client);
        }
        ccmd_client_sync_states(client);
        wm_request_client_redraw(client);
        return;
    }

    /* Maximize fresh: either both axes at once ('dir == 0', which can
     * only still reach here with neither axis currently fully
     * maximized), or this single axis alone with the other left
     * exactly as it is.  Only remember the geometry to restore to if
     * it is not already a maximized state's geometry, or restoring
     * later would land at whichever partial-maximize size happened
     * to be current instead of the window's true original one. */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    if (dir == 0) {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                mx, my, sw, sh, 0u);
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.w = sw;
        client->layout.geometry.cur.dim.h = sh;
        client->properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    } else if (want_horz) {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH,
                mx, client->layout.geometry.cur.pos.y, sw, 0u, 0u);
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.dim.w = sw;
        client->properties.state |=
            (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    } else {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                client->layout.geometry.cur.pos.x, my, 0u, sh, 0u);
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.h = sh;
        client->properties.state |=
            (uint16_t) CLIENT_STATE_MAXIMIZED_VERT;
    }

    if (client->frame != 0) {
        client_decoration_layout_sync(client);
    }
    ccmd_client_sync_states(client);
    wm_request_client_redraw(client);
}


/**
 * @brief Re-fill an already-maximized client's geometry against
 *        its current workarea
 *
 * A maximized client's geometry, grown or shrunk in place, is
 * only ever right immediately after actually maximizing it: anything
 * that later changes what its workarea resolves to (a panel
 * mapped or unmapped, @c desktops.margins reloaded, or the surface's
 * own strutless-maximization mode,
 * @a surface_action_toggle_strutless_maximize,
 * surface.h, toggled) leaves it still filling wherever the OLD
 * workarea was, not the new one, until something re-applies its
 * maximize geometry from scratch.  This does exactly that: resolved
 * against @a ccmd_client_resolve_workarea (the same resolution
 * @a ccmd_client_maximize itself already uses), so the client ends up
 * exactly refilling the workarea as it now stands, the same as if it
 * had only just been maximized.
 *
 * Only the axis (or axes) @p client's @c properties.state
 * actually names gets touched: a client maximized on one axis alone
 * keeps its other axis exactly as it already was, rather than
 * growing it to fill the workarea too and silently turning a
 * horizontal- or vertical-only maximize into a full one.
 *
 * @param client Client to re-fill; a no-op unless it is currently
 *               maximized on at least one axis
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_refill_maximized(client_td *client)
{
    int32_t mx = 0;
    int32_t my = 0;
    uint16_t sw;
    uint16_t sh;
    const desktop_td *own_desktop;
    bool is_active;
    xcb_window_t target;
    bool touch_x;
    bool touch_y;
    uint32_t border;
    uint16_t mask;

    if (client == NULL || !client_is_maximized_any(client)) {
        return;
    }

    if (!ccmd_client_resolve_workarea(client, &mx, &my, &sw, &sh)) {
        return;
    }

    own_desktop = wm_get_client_desktop(client);
    is_active = own_desktop != NULL &&
        own_desktop->client_active_id == client->id;

    target = ccmd_target_win(client);
    touch_x = client_is_maximized_horz(client);
    touch_y = client_is_maximized_vert(client);
    border = 2u * client_border_width(client, is_active, false);
    mask = 0u;

    sw = (uint16_t) ((sw > border) ? sw - border : 0u);
    sh = (uint16_t) ((sh > border) ? sh - border : 0u);
    s_clamp_to_size_hints(client, &sw, &sh);

    if (touch_x) {
        mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH;
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.dim.w = sw;
    }
    if (touch_y) {
        mask |= XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT;
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.h = sh;
    }

    if (mask != 0u) {
        ccmd_client_apply_geometry(client, target, mask,
                client->layout.geometry.cur.pos.x,
                client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h, 0u);
    }

    if (client->frame != 0) {
        client_decoration_layout_sync(client);
    }

    client_send_synthetic_configure_notify(xcb_connection_get(), client);
    wm_request_client_redraw(client);
}


void ccmd_client_maximize_horz(client_td *client)
{
    s_ccmd_client_maximize_dir(client, 1);
}


void ccmd_client_maximize_vert(client_td *client)
{
    s_ccmd_client_maximize_dir(client, 2);
}


void ccmd_client_demote_axis_state(client_td *client, int dir)
{
    if (client == NULL) {
        return;
    }

    client->properties.state &= (dir == 1)
        ? (uint16_t) ~CLIENT_STATE_MAXIMIZED_HORZ
        : (uint16_t) ~CLIENT_STATE_MAXIMIZED_VERT;
    ccmd_client_sync_states(client);
}


void ccmd_client_promote_axis_state(client_td *client, int dir)
{
    if (client == NULL) {
        return;
    }

    client->properties.state |= (dir == 1)
        ? (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ
        : (uint16_t) CLIENT_STATE_MAXIMIZED_VERT;
    ccmd_client_sync_states(client);
}


void ccmd_client_maximize(client_td *client)
{
    s_ccmd_client_maximize_dir(client, 0);
}
