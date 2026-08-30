/**
 * @file input/kbd/interact.c
 *
 * @brief Direct keyboard interaction with the focused client, that is,
 *        the program launch, move and resize bindings
 *
 * One of the files @c input/kbd/ is made of.  Launching a program and
 * moving or resizing the active client are immediate reactions to
 * a single keypress, independent from the cycle menu, the dialogs, the
 * open-menu key handling and the generic client action dispatch.
 * @a ik_get_active_client is the one piece of state lookup genuinely
 * shared between the two files.
 *
 * @see @c input/kbd/internal.h
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
#include <time.h>       /* clock_gettime, struct timespec, NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/geom.h>
#include <utils/time/clock.h>

/* Default initial values */
#include <defs/kbd.h>

/* Command includes */
#include <cmds/client/move.h>
#include <cmds/client/workarea.h>
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <cctl/launch.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Menu includes */
#include <menu/dialog/run.h>

/* Local includes */
#include <input/kbd/internal.h>
#include <utils/xcb/connection.h>


/**
 * @brief When @a ik_handle_launch last actually dispatched a program
 *        launch, or the zero value from static initialization before
 *        the first one
 *
 * Shared across every @c KEYBIND_LAUNCH_* binding rather than kept per
 * binding.  The goal is bounding how fast this window manager itself
 * hands off new processes overall, not tracking each binding on its
 * own, and a single held key is by far the common case this exists for
 * regardless.
 */
static struct timespec s_last_launch;


/* Keyboard resize helpers */

/**
 * @brief Compute a keyboard resize target for one axis
 *
 * Calculates the next frame size for either the horizontal or vertical
 * axis when resizing a @c client_td via keyboard, taking into account
 * frame extents and window manager size hints such as base size,
 * minimum size and resize increment.  When valid size hints are
 * present, the inner size is snapped to the nearest increment starting
 * from the base (or minimum) size; otherwise a fixed keyboard resize
 * step is applied.
 *
 * @param client     Pointer to the client whose geometry is being
 *                   resized; may be null, in which case @p cur_frame is
 *                   returned
 * @param step       Amount of pixels to resize every step
 * @param horizontal @c true to operate on the horizontal axis (width),
 *                   @c false for the vertical axis (height)
 * @param cur_frame  Current outer frame size (including extents) for
 *                   the selected axis
 * @param grow       @c true to grow (increase) the size, @c false to
 *                   shrink (decrease) it
 *
 * @return The target outer frame size for the selected axis, after
 *         the keyboard resize semantics have been applied and
 *         @a geom_dim_clamp has clamped the result
 *
 * @note Honors the @c WM_NORMAL_HINTS increments where a client
 *       declares them, so that keyboard resizing respects the
 *       granularity that client asked for
 * @note Complexity: @e O(1)
 */
static uint32_t s_kb_resize_axis_target(const client_td *client,
        uint32_t step, bool horizontal, uint32_t cur_frame, bool grow)
{
    uint32_t ext_a;
    uint32_t ext_b;
    uint32_t cur_inner;
    uint32_t base_i;
    uint32_t min_i;
    uint32_t inc_i;
    int32_t target;
    uint32_t frame_floor;
    uint32_t frame_clamped;

    if (client == NULL) {
        return cur_frame;
    }

    if (horizontal) {
        ext_a = (uint32_t) client->layout.frame_extents.left;
        ext_b = (uint32_t) client->layout.frame_extents.right;
    } else {
        ext_a = (uint32_t) client->layout.frame_extents.top;
        ext_b = (uint32_t) client->layout.frame_extents.bottom;
    }

    cur_inner = (cur_frame > ext_a + ext_b)
        ? cur_frame - ext_a - ext_b : 0u;
    if (!client->hints_icccm.size.is_valid) {
        int32_t resize_step = (step > 0u) ? (int32_t) step : 1;
        uint32_t floor_frame = ext_a + ext_b + WM_MIN_WINDOW_DIMENSION;
        uint32_t clamped;

        target = (grow)
            ? (int32_t) cur_frame + resize_step
            : (int32_t) cur_frame - resize_step;
        clamped = geom_dim_clamp(target);
        /* A decorated client's frame extents ('ext_a'/'ext_b', the
         * border plus, on the vertical axis, the titlebar) are fixed
         * regardless of how small its content shrinks: floored here so
         * the titlebar in particular can never itself shrink away or
         * disappear, no matter how far a resize keeps pushing this
         * axis; 'geom_dim_clamp' alone has no client in scope to know
         * this frame carries a titlebar at all, only ever floors to
         * a content-sized minimum on its own. */
        return (uint16_t) ((clamped > floor_frame)
                ? clamped : floor_frame);
    }

    if (horizontal) {
        base_i = client->hints_icccm.size.base.w;
        min_i = client->hints_icccm.size.min.w;
        inc_i = client->hints_icccm.size.inc.w;
    } else {
        base_i = client->hints_icccm.size.base.h;
        min_i = client->hints_icccm.size.min.h;
        inc_i = client->hints_icccm.size.inc.h;
    }

    if (inc_i > 1) {
        /* Per ICCCM §4.1.2.3, when 'BASE_SIZE' is absent it is
         * 'MIN_SIZE' that serves as the base for the increment
         * grid */
        uint32_t base = (base_i > 0) ? base_i : ((min_i > 0) ? min_i : 0u);
        uint32_t inc = inc_i;
        /* The client's true floor, in units of 'inc' above 'base':
         * its 'min_w'/'min_h' if it provides one larger than the
         * one-unit default (a client is free to demand more than one
         * row/column at all times), or 'WM_MIN_WINDOW_DIMENSION_UNITS'
         * (defs/client.h) otherwise.  Never all the way down to 'base'
         * itself, which without an explicit 'min_w'/'min_h' of the
         * client's leaves no floor at all ('cur_inner' below would
         * allow shrinking to exactly 'base', 0 units). */
        uint32_t floor_inner = base +
            WM_MIN_WINDOW_DIMENSION_UNITS * inc;
        uint32_t over;
        uint32_t snapped;
        uint32_t target_inner;

        if (min_i > 0 && min_i > floor_inner) {
            floor_inner = min_i;
        }
        if (cur_inner < floor_inner) {
            cur_inner = floor_inner;
        }

        over = (cur_inner > base) ? (cur_inner - base) : 0u;
        snapped = base + (over / inc) * inc;

        if (grow) {
            target_inner = snapped + inc;
        } else {
            target_inner = (snapped > floor_inner)
                ? (snapped - inc) : floor_inner;
        }

        return geom_dim_clamp((int32_t) (target_inner + ext_a + ext_b));
    }

    frame_floor = ext_a + ext_b + WM_MIN_WINDOW_DIMENSION;

    target = (grow)
        ? (int32_t) cur_frame + (int32_t) ((step > 0u) ? step : 1u)
        : (int32_t) cur_frame - (int32_t) ((step > 0u) ? step : 1u);
    frame_clamped = geom_dim_clamp(target);
    /* Same reasoning as the '!client->hints_icccm.size.is_valid' branch
     * above: a client with hints but no resize-increment of its
     * own still has fixed frame extents to protect. */
    return (uint16_t) ((frame_clamped > frame_floor)
            ? frame_clamped : frame_floor);
}


/**
 * @brief Directly apply a keyboard resize to a client
 *
 * Applies the resize synchronously without going through the event
 * queue.  This mirrors the interactive (mouse-drag) resize path so that
 * both input methods share identical behavior.  The geometry is
 * constrained per-axis, applied to the correct X window (frame for
 * decorated clients, content window for undecorated clients), and
 * followed by a synthetic @c ConfigureNotify so the application learns
 * its new geometry immediately.
 *
 * @param client Pointer to the client to resize
 * @param geom   New frame position and dimensions (screen-relative)
 *
 * @note This function flushes the XCB connection before returning
 */
static void s_kbd_resize_apply(client_td *client,
        struct geometry_s geom)
{
    bool pos_changed;
    uint16_t mask;
    xcb_window_t target_win;

    if (client == NULL) {
        return;
    }

    /* A shaded client shows only the titlebar; restore the full window
     * before applying the new dimensions */
    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    /* The caller ('ik_handle_resize' via 's_kb_resize_axis_target')
     * already produced fully snapped, increment-aligned frame
     * dimensions.  Re-applying 'client_size_constrain' here would snap
     * the values a second time and could produce a size different from
     * what the position correction ('new_y += old_h - new_h') was
     * computed for, causing the top edge of the window to shift by the
     * wrong amount on 'RESIZE_UP'. */
    pos_changed = (geom.pos.x != client->layout.geometry.cur.pos.x ||
                   geom.pos.y != client->layout.geometry.cur.pos.y);

    /* Apply the new geometry to the correct X window.  Decorated
     * clients are reparented into a frame; undecorated clients are
     * direct children of the root. */
    target_win = (client->frame != 0 && client_is_decorated(client))
        ? client->frame : client->window;

    if (pos_changed) {
        mask = (uint16_t) XCB_CONFIG_WINDOW_X |
            (uint16_t) XCB_CONFIG_WINDOW_Y |
            (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
            (uint16_t) XCB_CONFIG_WINDOW_HEIGHT;
    } else {
        mask = (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
            (uint16_t) XCB_CONFIG_WINDOW_HEIGHT;
    }
    ccmd_client_apply_geometry(client, target_win, mask,
            geom.pos.x, geom.pos.y, geom.dim.w, geom.dim.h, 0u);

    /* Update the stored geometry after configuring X so that
     * 'client_decoration_layout_sync' and the synthetic
     * 'ConfigureNotify' both see the final values */
    client->layout.geometry.cur = geom;

    /* Reposition and resize the inner window and titlebar to match the
     * new frame dimensions (no-op for undecorated clients) */
    client_decoration_layout_sync(client);

    /* ICCCM §4.2.3: send a synthetic 'ConfigureNotify' with
     * screen-relative coordinates so the application always knows its
     * true on-screen position and content-area size, regardless of
     * reparenting. */
    client_send_synthetic_configure_notify(xcb_connection_get(), client);

    /* Force a repaint AFTER the synthetic 'ConfigureNotify' so the
     * application draws at the correct screen-relative geometry.
     * Placing the 'Expose' here ensures it arrives in the client's
     * event queue after both the xcb_configure_window (from
     * 'client_decoration_layout_sync') and the synthetic
     * 'ConfigureNotify', giving programs that rely on size and position
     * before their 'Expose' handler runs the correct geometry. */
    xcb_clear_area(xcb_connection_get(), 1, client->window, 0, 0, 0, 0);


    /* Mark the desktop as needing a repaint so frame decorations are
     * refreshed at the correct new dimensions */
    wm_request_client_redraw(client);
}


/* Program launch dispatch */

/**
 * @brief Whether at least @c KBD_LAUNCH_MIN_INTERVAL_MS has passed
 *        since @a s_last_launch, updating @a s_last_launch to now when
 *        it has
 *
 * @return @c true if this launch may proceed
 *
 * @note Complexity: @e O(1)
 */
static bool s_launch_pace_ok(void)
{
    struct timespec now;

    if (s_last_launch.tv_sec != 0 &&
            clock_ms_since(&s_last_launch) <
                (long) KBD_LAUNCH_MIN_INTERVAL_MS) {
        return false;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
        s_last_launch = now;
    }
    return true;
}


/* Resolve the currently focused client on a surface */
client_td *ik_get_active_client(surface_td *surface,
        list_td *surfaces,
        surface_td **cs_out,
        desktop_td **cd_out)
{
    desktop_td *desktop;

    if (cs_out != NULL) { *cs_out = NULL; }
    if (cd_out != NULL) { *cd_out = NULL; }

    if (surface == NULL) {
        return NULL;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL || desktop->client_active_id == 0) {
        return NULL;
    }

    return lookup_find_client(surfaces, desktop->client_active_id,
            cs_out, cd_out);
}


/* Launch a configured program for the given binding type */
void ik_handle_launch(enum ik_launch_e program,
        surface_td *surface, const config_td *config)
{
    const char *path = NULL;

    switch (program) {
        case IK_LAUNCH_TERMINAL:
            path = config->base.programs.terminal;
            break;
        case IK_LAUNCH_LAUNCHER:
            if (config->base.prompt.is_enabled) {
                run_init(xcb_connection_get(), surface, config);
                return;
            }
            path = config->base.programs.launcher;
            break;
        case IK_LAUNCH_FILE_MANAGER:
            path = config->base.programs.file_manager;
            break;
        case IK_LAUNCH_WEB_BROWSER:
            path = config->base.programs.web_browser;
            break;
        case IK_LAUNCH_EDITOR:
            path = config->base.programs.editor;
            break;
    }

    if (!s_launch_pace_ok()) {
        return;
    }

    cctl_launch_dispatch(surface, path, NULL);
}


/* Move the focused client by keyboard */
void ik_handle_move(enum ik_move_e direction,
        surface_td *surface, list_td *surfaces,
        const config_td *config)
{
    surface_td *cs = NULL;
    client_td *client;
    int32_t move_step;
    int32_t new_x;
    int32_t new_y;
    int32_t max_x;
    int32_t max_y;
    int32_t border;
    int32_t wa_x = 0;
    int32_t wa_y = 0;
    uint16_t wa_w = 0;
    uint16_t wa_h = 0;
    bool have_workarea;

    client = ik_get_active_client(surface, surfaces, &cs, NULL);
    if (client == NULL) {
        return;
    }

    /* A fully maximized or fullscreen client cannot be moved at all,
     * consistent with 'MOUSEBIND_MOVE' (input/mouse/event/press.c) and
     * the window context menu's 'can_move' (see
     * 'menu/context/wincmenu.c').  A client maximized on just one axis
     * is still free to move, since only one axis is pinned to the
     * workarea edge. */
    if (client_is_maximized(client) || client_is_fullscreen(client) ||
            client_is_locked(client)) {
        return;
    }

    move_step = (int32_t) ((config->base.windows.move_step > 0u)
            ? config->base.windows.move_step : 1u);
    new_x = client->layout.geometry.cur.pos.x;
    new_y = client->layout.geometry.cur.pos.y;

    /* The corner destinations below need the workarea of whichever
     * monitor 'client' actually sits on, not the whole surface's
     * raw dimensions: on a multi-monitor surface, the latter would send
     * "top-right" to the far edge of the last monitor rather than the
     * current one's, and either one alone would still tuck the client
     * under a panel or the tray reserving space at that same edge.
     * Falls back to the whole-surface computation this function already
     * used, unchanged, whenever a monitor or desktop cannot be resolved
     * for 'client' at all. */
    have_workarea = ccmd_client_resolve_workarea(client,
            &wa_x, &wa_y, &wa_w, &wa_h);

    /* The border counts against the space available.  A window's
     * recorded width and height cover the frame alone, while the
     * X window it sits in occupies that plus a border on each side, so
     * a right or bottom edge worked out from the width by itself sat
     * one border past where it was meant to and pushed that much of the
     * window off the work area.
     *
     * Only the far edges are affected: the left and top ones are the
     * window's position, which the border grows away from rather
     * than into. */
    border = 2 * (int32_t) client_border_width(client, true, false);

    max_x = (have_workarea)
        ? wa_x + (int32_t) wa_w -
          (int32_t) client->layout.geometry.cur.dim.w - border
        : ((cs != NULL)
            ? (int32_t) cs->properties.dim.w -
              (int32_t) client->layout.geometry.cur.dim.w - border
            : new_x);
    max_y = (have_workarea)
        ? wa_y + (int32_t) wa_h -
          (int32_t) client->layout.geometry.cur.dim.h - border
        : ((cs != NULL)
            ? (int32_t) cs->properties.dim.h -
              (int32_t) client->layout.geometry.cur.dim.h - border
            : new_y);

    switch (direction) {
        case IK_MOVE_LEFT:
            new_x -= move_step;
            break;
        case IK_MOVE_RIGHT:
            new_x += move_step;
            break;
        case IK_MOVE_UP:
            new_y -= move_step;
            break;
        case IK_MOVE_DOWN:
            new_y += move_step;
            break;
        case IK_MOVE_TOP_LEFT:
            new_x = (have_workarea) ? wa_x : 0;
            new_y = (have_workarea) ? wa_y : 0;
            break;
        case IK_MOVE_TOP_RIGHT:
            new_x = max_x;
            new_y = (have_workarea) ? wa_y : 0;
            break;
        case IK_MOVE_BOTTOM_LEFT:
            new_x = (have_workarea) ? wa_x : 0;
            new_y = max_y;
            break;
        case IK_MOVE_BOTTOM_RIGHT:
            new_x = max_x;
            new_y = max_y;
            break;
    }

    enact_client_move(client, (struct position_s) { new_x, new_y });
}


/* Resize the focused client by keyboard */
void ik_handle_resize(enum ik_resize_e edge,
        surface_td *surface, list_td *surfaces,
        const config_td *config)
{
    client_td *client;
    uint32_t resize_step;
    struct position_s new_pos;
    uint32_t old_w;
    uint32_t old_h;
    int32_t new_w;
    int32_t new_h;
    uint32_t aspect_h;

    client = ik_get_active_client(surface, surfaces, NULL, NULL);
    if (client == NULL || !client_is_resizable(client) ||
            client_is_locked(client)) {
        return;
    }

    /* Refuse to resize clients in a fixed-size state entirely; a client
     * maximized on just one axis still allows resizing its free axis
     * below (see the per-direction axis-lock checks further down), the
     * same way a mouse border drag does (see
     * 'drag_start_resize_axis_locked' in 'input/mouse/drag.c'). */
    if (client_is_fullscreen(client) || client_is_maximized(client)) {
        return;
    }

    /* The maximized axis of a horizontal-only or vertical-only
     * maximized client is snapped exactly to its workarea edge, so it
     * has nothing left to grow or shrink by keyboard either.
     * Only the still-free axis, the other pair of edges, keeps working
     * normally. */
    if ((edge == IK_RESIZE_LEFT || edge == IK_RESIZE_RIGHT) &&
            client_is_maximized_horz(client)) {
        return;
    }
    if ((edge == IK_RESIZE_UP || edge == IK_RESIZE_DOWN) &&
            client_is_maximized_vert(client)) {
        return;
    }

    resize_step = (config->base.windows.resize_step > 0u)
        ? config->base.windows.resize_step : 1u;
    new_pos.x = client->layout.geometry.cur.pos.x;
    new_pos.y = client->layout.geometry.cur.pos.y;

    /* Operate in frame space (outer dimensions including decoration
     * extents).  's_kbd_resize_apply' converts to inner space
     * internally when applying size hints. */
    old_w = client->layout.geometry.cur.dim.w;
    old_h = client_is_shaded(client)
        ? client->layout.geometry.old.dim.h
        : client->layout.geometry.cur.dim.h;
    new_w = (int32_t) old_w;
    new_h = (int32_t) old_h;

    switch (edge) {
        case IK_RESIZE_LEFT:
            new_w = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, true, old_w, false);
            aspect_h = (uint32_t) new_h;
            client_aspect_ratio_clamp(client, (uint32_t) new_w,
                    &aspect_h);
            new_h = (int32_t) aspect_h;
            new_pos.x += (int32_t) old_w - new_w;
            break;
        case IK_RESIZE_RIGHT:
            new_w = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, true, old_w, true);
            aspect_h = (uint32_t) new_h;
            client_aspect_ratio_clamp(client, (uint32_t) new_w,
                    &aspect_h);
            new_h = (int32_t) aspect_h;
            break;
        case IK_RESIZE_UP:
            new_h = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, false, old_h, false);
            aspect_h = (uint32_t) new_h;
            client_aspect_ratio_clamp(client, (uint32_t) new_w,
                    &aspect_h);
            new_h = (int32_t) aspect_h;
            new_pos.y += (int32_t) old_h - new_h;
            break;
        case IK_RESIZE_DOWN:
            new_h = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, false, old_h, true);
            aspect_h = (uint32_t) new_h;
            client_aspect_ratio_clamp(client, (uint32_t) new_w,
                    &aspect_h);
            new_h = (int32_t) aspect_h;
            break;
    }

    s_kbd_resize_apply(client, (struct geometry_s) {
                new_pos,
                { geom_dim_clamp(new_w), geom_dim_clamp(new_h) } });
}
