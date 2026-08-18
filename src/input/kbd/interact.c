/**
 * @file input/kbd/interact.c
 *
 * @brief Direct keyboard interaction with the currently focused
 *        client: program launch, move, and resize bindings
 *
 * Split out of @c input/kbd/event.c, launching a program and moving or
 * resizing the active client are direct, immediate reactions to
 * a single keypress, independent from the cycle-menu, dialog, and
 * open-menu key handling and the generic client-action dispatch that
 * remain there.  @a ik_get_active_client is the one piece of state
 * lookup genuinely shared between the two files.
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

/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/geom.h>

/* Command includes */
#include <cmds/client/geom.h>
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <lifecycle.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Menu includes */
#include <menu/dialog/run.h>

/* Local includes */
#include <input/kbd/internal.h>


/* Active-client lookup helper */

/**
 * @brief Resolve the currently focused client on a surface
 *
 * Looks up the current desktop for @p surface and returns the active
 * client on that desktop.  Optionally returns the owning surface and
 * desktop pointers through @p cs_out and @p cd_out.
 *
 * @param surface  Surface to query
 * @param surfaces Full surface list (for @a lookup_find_client)
 * @param cs_out   Receives the client's owning surface (may be null)
 * @param cd_out   Receives the client's owning desktop (may be null)
 *
 * @return Active client, or @c NULL when none is focused
 *
 * @note Complexity: @e O(n) for the client list walk
 */
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
 * @return The target outer frame size for the selected axis after
 *         applying keyboard resize semantics and clamping via
 *         @a geom_clamp_dim.
 *
 * @note With this, it's honored @c WM_NORMAL_HINTS increments when
 *       available, ensuring that keyboard resizing respects the
 *       client's preferred resize granularity.
 */
static uint32_t s_kb_resize_axis_target(const client_td *client,
        uint32_t step, bool horizontal, uint32_t cur_frame, bool grow)
{
    uint32_t ext_a;
    uint32_t ext_b;
    uint32_t cur_inner;
    int32_t base_i;
    int32_t min_i;
    int32_t inc_i;
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
    if (!client->size_hints.valid) {
        int32_t resize_step = (step > 0u) ? (int32_t) step : 1;
        uint32_t floor_frame = ext_a + ext_b + WM_MIN_WINDOW_DIMENSION;
        uint32_t clamped;

        target = (grow)
            ? (int32_t) cur_frame + resize_step
            : (int32_t) cur_frame - resize_step;
        clamped = geom_clamp_dim(target);
        /* A decorated client's own frame extents ('ext_a'/'ext_b', the
         * border plus, on the vertical axis, the titlebar) are fixed
         * regardless of how small its content shrinks: floored here so
         * the titlebar in particular can never itself shrink away or
         * disappear, no matter how far a resize keeps pushing this
         * axis; 'geom_clamp_dim' alone has no client in scope to know
         * this frame carries a titlebar at all, only ever floors to
         * a content-sized minimum on its own. */
        return (uint16_t) ((clamped > floor_frame)
                ? clamped : floor_frame);
    }

    if (horizontal) {
        base_i = client->size_hints.base_w;
        min_i = client->size_hints.min_w;
        inc_i = client->size_hints.inc_w;
    } else {
        base_i = client->size_hints.base_h;
        min_i = client->size_hints.min_h;
        inc_i = client->size_hints.inc_h;
    }

    if (inc_i > 1) {
        /* ICCCM §4.1.2.3: when 'BASE_SIZE' is absent, 'MIN_SIZE' serves
         * as the base for the increment grid */
        uint32_t base = (base_i > 0)
            ? (uint32_t) base_i
            : ((min_i > 0) ? (uint32_t) min_i : 0u);
        uint32_t inc = (uint32_t) inc_i;
        /* The client's own true floor, in units of 'inc' above 'base':
         * its own 'min_w'/'min_h' if it provides one larger than the
         * one-unit default (a client is free to demand more than one
         * row/column at all times), or 'WM_MIN_WINDOW_DIMENSION_UNITS'
         * (defs/client.h) otherwise.  Never all the way down to 'base'
         * itself, which without an explicit 'min_w'/'min_h' of the
         * client's own leaves no floor at all ('cur_inner' below would
         * allow shrinking to exactly 'base', 0 units). */
        uint32_t floor_inner = base +
            WM_MIN_WINDOW_DIMENSION_UNITS * inc;
        uint32_t over;
        uint32_t snapped;
        uint32_t target_inner;

        if (min_i > 0 && (uint32_t) min_i > floor_inner) {
            floor_inner = (uint32_t) min_i;
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

        return geom_clamp_dim((int32_t) (target_inner + ext_a + ext_b));
    }

    frame_floor = ext_a + ext_b + WM_MIN_WINDOW_DIMENSION;

    target = (grow)
        ? (int32_t) cur_frame + (int32_t) ((step > 0u) ? step : 1u)
        : (int32_t) cur_frame - (int32_t) ((step > 0u) ? step : 1u);
    frame_clamped = geom_clamp_dim(target);
    /* Same reasoning as the '!client->size_hints.valid' branch
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
 * @param new_x  New frame X position (screen-relative)
 * @param new_y  New frame Y position (screen-relative)
 * @param new_w  New frame width
 * @param new_h  New frame height
 *
 * @note This function flushes the XCB connection before returning.
 */
static void s_kbd_resize_apply(client_td *client,
        int32_t new_x, int32_t new_y, uint32_t new_w, uint32_t new_h)
{
    bool pos_changed;
    uint16_t mask;
    uint32_t values[4];
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
     * dimensions.  Re-applying 'client_constrain_size' here would snap
     * the values a second time and could produce a size different from
     * what the position correction ('new_y += old_h - new_h') was
     * computed for, causing the top edge of the window to shift by the
     * wrong amount on 'RESIZE_UP'. */
    pos_changed = (new_x != client->layout.geometry.cur.pos.x ||
                   new_y != client->layout.geometry.cur.pos.y);

    /* Apply the new geometry to the correct X window.  Decorated
     * clients are reparented into a frame; undecorated clients are
     * direct children of the root. */
    target_win = (client->frame != 0 && client_is_decorated(client))
        ? client->frame : client->window;

    if (pos_changed) {
        mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
               XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[0] = (uint32_t) new_x;
        values[1] = (uint32_t) new_y;
        values[2] = new_w;
        values[3] = new_h;
    } else {
        mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[0] = new_w;
        values[1] = new_h;
    }
    xcb_configure_window(client->connection, target_win, mask, values);

    /* Update the stored geometry after configuring X so that
     * 'client_sync_decoration_layout' and the synthetic
     * 'ConfigureNotify' both see the final values */
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    client->layout.geometry.cur.dim.w = new_w;
    client->layout.geometry.cur.dim.h = new_h;

    /* Reposition and resize the inner window and titlebar to match the
     * new frame dimensions (no-op for undecorated clients) */
    client_sync_decoration_layout(client);

    /* ICCCM §4.2.3: send a synthetic 'ConfigureNotify' with
     * screen-relative coordinates so the application always knows its
     * true on-screen position and content-area size, regardless of
     * reparenting. */
    client_send_synthetic_configure_notify(client->connection, client);

    /* Force a repaint AFTER the synthetic 'ConfigureNotify' so the
     * application draws at the correct screen-relative geometry.
     * Placing the 'Expose' here ensures it arrives in the client's
     * event queue after both the xcb_configure_window (from
     * 'client_sync_decoration_layout') and the synthetic
     * 'ConfigureNotify', giving programs that rely on size and position
     * before their 'Expose' handler runs the correct geometry. */
    xcb_clear_area(client->connection, 1, client->window, 0, 0, 0, 0);

    xcb_flush(client->connection);

    /* Mark the desktop as needing a repaint so frame decorations are
     * refreshed at the correct new dimensions */
    wm_request_client_redraw(client);
}


/* Program launch dispatch */

/**
 * @brief Launch a configured program for the given binding type
 *
 * Maps each @c KEYBIND_LAUNCH_* constant to its program string from the
 * configuration and calls @a lifecycle_dispatch_launch.
 *
 * @param btype   Keyboard binding type (one of the @c KEYBIND_LAUNCH_*
 *                constants)
 * @param surface Current surface passed to @a lifecycle_dispatch_launch
 * @param config  Active configuration holding the program paths
 */
void ik_handle_launch(enum wm_keybind_type_e btype,
        surface_td *surface, const config_td *config)
{
    const char *program = NULL;

    switch (btype) {
        /* To avoid warnings from the compiler, ALL cases must be here */
        case KEYBIND_NONE:
        case KEYBIND_WM_SCRATCHPAD_TOGGLE:
        case KEYBIND_DESKTOP_NEXT:
        case KEYBIND_DESKTOP_PREV:
        case KEYBIND_CLIENT_ICONIFY:
        case KEYBIND_CLIENT_HIDE:
        case KEYBIND_CLIENT_CLOSE:
        case KEYBIND_CLIENT_KILL:
        case KEYBIND_CLIENT_MAXIMIZE:
        case KEYBIND_CLIENT_CENTER:
        case KEYBIND_CLIENT_MOVE_NEXT_MONITOR:
        case KEYBIND_CLIENT_SHADE:
        case KEYBIND_CLIENT_FULLSCREEN:
        case KEYBIND_CLIENT_PIN:
        case KEYBIND_CLIENT_INFO:
        case KEYBIND_CLIENT_TOGGLE_DECORATION:
        case KEYBIND_CLIENT_CYCLE_LAYER:
        case KEYBIND_CLIENT_CYCLE_NEXT:
        case KEYBIND_CLIENT_CYCLE_PREV:
        case KEYBIND_DESKTOP_ICON_NEXT:
        case KEYBIND_DESKTOP_ICON_PREV:
        case KEYBIND_CLIENT_MOVE_LEFT:
        case KEYBIND_CLIENT_MOVE_RIGHT:
        case KEYBIND_CLIENT_MOVE_UP:
        case KEYBIND_CLIENT_MOVE_DOWN:
        case KEYBIND_CLIENT_MOVE_TOP_LEFT:
        case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
        case KEYBIND_CLIENT_RESIZE_LEFT:
        case KEYBIND_CLIENT_RESIZE_RIGHT:
        case KEYBIND_CLIENT_RESIZE_UP:
        case KEYBIND_CLIENT_RESIZE_DOWN:
        case KEYBIND_DESKTOP_SHOW:
        case KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL:
        case KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL:
        case KEYBIND_DESKTOP_CLIENTS_REARRANGE:
        case KEYBIND_DESKTOP_GOTO_0:
        case KEYBIND_DESKTOP_GOTO_1:
        case KEYBIND_DESKTOP_GOTO_2:
        case KEYBIND_DESKTOP_GOTO_3:
        case KEYBIND_DESKTOP_GOTO_4:
        case KEYBIND_DESKTOP_GOTO_5:
        case KEYBIND_DESKTOP_GOTO_6:
        case KEYBIND_DESKTOP_GOTO_7:
        case KEYBIND_DESKTOP_GOTO_8:
        case KEYBIND_DESKTOP_GOTO_9:
        case KEYBIND_DESKTOP_ADD:
        case KEYBIND_DESKTOP_REMOVE:
        case KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE:
        case KEYBIND_WM_ROOT_MENU:
        case KEYBIND_WM_SEARCH_WINDOWS:
        case KEYBIND_WM_WINDOWS_MENU:
        case KEYBIND_CLIENT_WINDOW_MENU:
        case KEYBIND_WM_REDRAW:
        case KEYBIND_WM_RELOAD:
        case KEYBIND_WM_QUIT:
        case KEYBIND_WM_SHORTCUTS_LIST:
        case KEYBIND_WM_EMERGENCY_EXIT:
        case KEYBIND_WM_FORTUNE:
            return;

        case KEYBIND_LAUNCH_TERMINAL:
            program = config->base.programs.terminal;
            break;
        case KEYBIND_LAUNCH_LAUNCHER:
            if (config->base.prompt.is_enabled) {
                run_init(surface->connection, surface, config);
                return;
            }
            program = config->base.programs.launcher;
            break;
        case KEYBIND_LAUNCH_FILE_MANAGER:
            program = config->base.programs.file_manager;
            break;
        case KEYBIND_LAUNCH_WEB_BROWSER:
            program = config->base.programs.web_browser;
            break;
        case KEYBIND_LAUNCH_EDITOR:
            program = config->base.programs.editor;
            break;
    }

    lifecycle_dispatch_launch(surface, program, NULL);
}


/* Keyboard move dispatch */

/**
 * @brief Move the focused client by keyboard
 *
 * Resolves the active client and computes a new position based on
 * @p btype: relative steps (@c KEYBIND_CLIENT_MOVE_LEFT / @c RIGHT /
 * @c UP / @c DOWN) or absolute corner snaps
 * (@c KEYBIND_CLIENT_MOVE_TOP_LEFT...).
 *
 * @param btype    Keyboard binding type (one of the
 *                 @c KEYBIND_CLIENT_MOVE_* constants)
 * @param surface  Current surface
 * @param surfaces Full surface list
 * @param config   Active configuration (for the move step size)
 */
void ik_handle_move(enum wm_keybind_type_e btype,
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
     * consistent with 'MOUSEBIND_MOVE' (input/mouse/event/press.c)
     * and the window context menu's own 'can_move'
     * (menu/context/wincmenu.c);
     * a client maximized on just one axis is still free to move,
     * since only one axis is pinned to the workarea edge. */
    if (client_is_maximized(client) || client_is_fullscreen(client) ||
            client_is_locked(client)) {
        return;
    }

    move_step = (int32_t) ((config->base.windows.move_step > 0u)
            ? config->base.windows.move_step : 1u);
    new_x = client->layout.geometry.cur.pos.x;
    new_y = client->layout.geometry.cur.pos.y;

    /* The corner destinations below need the workarea of whichever
     * monitor 'client' actually sits on, not the whole surface's own
     * raw dimensions: on a multi-monitor surface, the latter would
     * send "top-right" to the far edge of the last monitor rather
     * than the current one's, and either one alone would still tuck
     * the client under a panel or the tray reserving space at that
     * same edge.  Falls back to the whole-surface computation this
     * function already used, unchanged, whenever a monitor or
     * desktop cannot be resolved for 'client' at all. */
    have_workarea = ccmd_client_monitor_workarea(client,
            &wa_x, &wa_y, &wa_w, &wa_h);

    max_x = (have_workarea)
        ? wa_x + (int32_t) wa_w -
          (int32_t) client->layout.geometry.cur.dim.w
        : ((cs != NULL)
            ? (int32_t) cs->properties.dim.w -
              (int32_t) client->layout.geometry.cur.dim.w
            : new_x);
    max_y = (have_workarea)
        ? wa_y + (int32_t) wa_h -
          (int32_t) client->layout.geometry.cur.dim.h
        : ((cs != NULL)
            ? (int32_t) cs->properties.dim.h -
              (int32_t) client->layout.geometry.cur.dim.h
            : new_y);

    switch (btype) {
        /* To avoid warnings from the compiler, ALL cases must be here */
        case KEYBIND_NONE:
        case KEYBIND_WM_SCRATCHPAD_TOGGLE:
        case KEYBIND_DESKTOP_NEXT:
        case KEYBIND_DESKTOP_PREV:
        case KEYBIND_CLIENT_ICONIFY:
        case KEYBIND_CLIENT_HIDE:
        case KEYBIND_CLIENT_CLOSE:
        case KEYBIND_CLIENT_KILL:
        case KEYBIND_CLIENT_MAXIMIZE:
        case KEYBIND_CLIENT_CENTER:
        case KEYBIND_CLIENT_MOVE_NEXT_MONITOR:
        case KEYBIND_CLIENT_SHADE:
        case KEYBIND_CLIENT_FULLSCREEN:
        case KEYBIND_CLIENT_PIN:
        case KEYBIND_CLIENT_INFO:
        case KEYBIND_CLIENT_TOGGLE_DECORATION:
        case KEYBIND_CLIENT_CYCLE_LAYER:
        case KEYBIND_CLIENT_CYCLE_NEXT:
        case KEYBIND_CLIENT_CYCLE_PREV:
        case KEYBIND_DESKTOP_ICON_NEXT:
        case KEYBIND_DESKTOP_ICON_PREV:
        case KEYBIND_LAUNCH_TERMINAL:
        case KEYBIND_LAUNCH_LAUNCHER:
        case KEYBIND_LAUNCH_FILE_MANAGER:
        case KEYBIND_LAUNCH_WEB_BROWSER:
        case KEYBIND_LAUNCH_EDITOR:
        case KEYBIND_CLIENT_RESIZE_LEFT:
        case KEYBIND_CLIENT_RESIZE_RIGHT:
        case KEYBIND_CLIENT_RESIZE_UP:
        case KEYBIND_CLIENT_RESIZE_DOWN:
        case KEYBIND_DESKTOP_SHOW:
        case KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL:
        case KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL:
        case KEYBIND_DESKTOP_CLIENTS_REARRANGE:
        case KEYBIND_DESKTOP_GOTO_0:
        case KEYBIND_DESKTOP_GOTO_1:
        case KEYBIND_DESKTOP_GOTO_2:
        case KEYBIND_DESKTOP_GOTO_3:
        case KEYBIND_DESKTOP_GOTO_4:
        case KEYBIND_DESKTOP_GOTO_5:
        case KEYBIND_DESKTOP_GOTO_6:
        case KEYBIND_DESKTOP_GOTO_7:
        case KEYBIND_DESKTOP_GOTO_8:
        case KEYBIND_DESKTOP_GOTO_9:
        case KEYBIND_DESKTOP_ADD:
        case KEYBIND_DESKTOP_REMOVE:
        case KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE:
        case KEYBIND_WM_ROOT_MENU:
        case KEYBIND_WM_SEARCH_WINDOWS:
        case KEYBIND_WM_WINDOWS_MENU:
        case KEYBIND_CLIENT_WINDOW_MENU:
        case KEYBIND_WM_REDRAW:
        case KEYBIND_WM_RELOAD:
        case KEYBIND_WM_QUIT:
        case KEYBIND_WM_SHORTCUTS_LIST:
        case KEYBIND_WM_EMERGENCY_EXIT:
        case KEYBIND_WM_FORTUNE:
            return;

        case KEYBIND_CLIENT_MOVE_LEFT:
            new_x -= move_step;
            break;
        case KEYBIND_CLIENT_MOVE_RIGHT:
            new_x += move_step;
            break;
        case KEYBIND_CLIENT_MOVE_UP:
            new_y -= move_step;
            break;
        case KEYBIND_CLIENT_MOVE_DOWN:
            new_y += move_step;
            break;
        case KEYBIND_CLIENT_MOVE_TOP_LEFT:
            new_x = (have_workarea) ? wa_x : 0;
            new_y = (have_workarea) ? wa_y : 0;
            break;
        case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
            new_x = max_x;
            new_y = (have_workarea) ? wa_y : 0;
            break;
        case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
            new_x = (have_workarea) ? wa_x : 0;
            new_y = max_y;
            break;
        case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
            new_x = max_x;
            new_y = max_y;
            break;
    }

    enact_client_move(client, new_x, new_y);
}


/* Keyboard resize dispatch */

/**
 * @brief Resize the focused client by keyboard
 *
 * Resolves the active client, checks that it is resizable and not in
 * a state that prevents resizing (fullscreen, maximized), and applies
 * an increment-aware size change in the direction indicated by
 * @p btype.  @c Left / @c Up shrink from the right/bottom edge;
 * @c Right / @c Down grow that edge.
 *
 * @param btype    Keyboard binding type (one of the
 *                 @c KEYBIND_CLIENT_RESIZE_* constants)
 * @param surface  Current surface
 * @param surfaces Full surface list
 * @param config   Active configuration (for the resize step size)
 */
void ik_handle_resize(enum wm_keybind_type_e btype,
        surface_td *surface, list_td *surfaces,
        const config_td *config)
{
    client_td *client;
    uint32_t resize_step;
    int32_t new_x;
    int32_t new_y;
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

    /* Refuse to resize clients in a fixed-size state entirely
     * ("Maximized windows can't be moved or resized", Karp, O'Reilly,
     * & Mott, 2005, 'Windows XP in a Nutshell', 2nd ed., ch. 2);
     * a client maximized on just one axis still allows resizing its
     * free axis below (see the per-direction axis-lock checks further
     * down), the same way a mouse border drag does (see
     * 'drag_start_resize_axis_locked' in 'input/mouse/drag.c'). */
    if (client_is_fullscreen(client) || client_is_maximized(client)) {
        return;
    }

    /* The maximized axis of a horizontal-only or vertical-only
     * maximized client is snapped exactly to its workarea edge, so it
     * has nothing left to grow or shrink by keyboard either; only the
     * still-free axis (the other 'KEYBIND_CLIENT_RESIZE_*' pair) keeps
     * working normally. */
    if ((btype == KEYBIND_CLIENT_RESIZE_LEFT ||
                btype == KEYBIND_CLIENT_RESIZE_RIGHT) &&
            client_is_maximized_horz(client)) {
        return;
    }
    if ((btype == KEYBIND_CLIENT_RESIZE_UP ||
                btype == KEYBIND_CLIENT_RESIZE_DOWN) &&
            client_is_maximized_vert(client)) {
        return;
    }

    resize_step = (config->base.windows.resize_step > 0u)
        ? config->base.windows.resize_step : 1u;
    new_x = client->layout.geometry.cur.pos.x;
    new_y = client->layout.geometry.cur.pos.y;

    /* Operate in frame space (outer dimensions including decoration
     * extents).  's_kbd_resize_apply' converts to inner space
     * internally when applying size hints. */
    old_w = client->layout.geometry.cur.dim.w;
    old_h = client_is_shaded(client)
        ? client->layout.geometry.old.dim.h
        : client->layout.geometry.cur.dim.h;
    new_w = (int32_t) old_w;
    new_h = (int32_t) old_h;

    switch (btype) {
        /* To avoid warnings from the compiler, ALL cases must be here */
        case KEYBIND_NONE:
        case KEYBIND_WM_SCRATCHPAD_TOGGLE:
        case KEYBIND_DESKTOP_NEXT:
        case KEYBIND_DESKTOP_PREV:
        case KEYBIND_CLIENT_ICONIFY:
        case KEYBIND_CLIENT_HIDE:
        case KEYBIND_CLIENT_CLOSE:
        case KEYBIND_CLIENT_KILL:
        case KEYBIND_CLIENT_MAXIMIZE:
        case KEYBIND_CLIENT_CENTER:
        case KEYBIND_CLIENT_MOVE_NEXT_MONITOR:
        case KEYBIND_CLIENT_SHADE:
        case KEYBIND_CLIENT_FULLSCREEN:
        case KEYBIND_CLIENT_PIN:
        case KEYBIND_CLIENT_INFO:
        case KEYBIND_CLIENT_TOGGLE_DECORATION:
        case KEYBIND_CLIENT_CYCLE_LAYER:
        case KEYBIND_CLIENT_CYCLE_NEXT:
        case KEYBIND_CLIENT_CYCLE_PREV:
        case KEYBIND_DESKTOP_ICON_NEXT:
        case KEYBIND_DESKTOP_ICON_PREV:
        case KEYBIND_LAUNCH_TERMINAL:
        case KEYBIND_LAUNCH_LAUNCHER:
        case KEYBIND_LAUNCH_FILE_MANAGER:
        case KEYBIND_LAUNCH_WEB_BROWSER:
        case KEYBIND_LAUNCH_EDITOR:
        case KEYBIND_CLIENT_MOVE_LEFT:
        case KEYBIND_CLIENT_MOVE_RIGHT:
        case KEYBIND_CLIENT_MOVE_UP:
        case KEYBIND_CLIENT_MOVE_DOWN:
        case KEYBIND_CLIENT_MOVE_TOP_LEFT:
        case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
        case KEYBIND_DESKTOP_SHOW:
        case KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL:
        case KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL:
        case KEYBIND_DESKTOP_CLIENTS_REARRANGE:
        case KEYBIND_DESKTOP_GOTO_0:
        case KEYBIND_DESKTOP_GOTO_1:
        case KEYBIND_DESKTOP_GOTO_2:
        case KEYBIND_DESKTOP_GOTO_3:
        case KEYBIND_DESKTOP_GOTO_4:
        case KEYBIND_DESKTOP_GOTO_5:
        case KEYBIND_DESKTOP_GOTO_6:
        case KEYBIND_DESKTOP_GOTO_7:
        case KEYBIND_DESKTOP_GOTO_8:
        case KEYBIND_DESKTOP_GOTO_9:
        case KEYBIND_DESKTOP_ADD:
        case KEYBIND_DESKTOP_REMOVE:
        case KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE:
        case KEYBIND_WM_ROOT_MENU:
        case KEYBIND_WM_SEARCH_WINDOWS:
        case KEYBIND_WM_WINDOWS_MENU:
        case KEYBIND_CLIENT_WINDOW_MENU:
        case KEYBIND_WM_REDRAW:
        case KEYBIND_WM_RELOAD:
        case KEYBIND_WM_QUIT:
        case KEYBIND_WM_SHORTCUTS_LIST:
        case KEYBIND_WM_EMERGENCY_EXIT:
        case KEYBIND_WM_FORTUNE:
            return;

        case KEYBIND_CLIENT_RESIZE_LEFT:
            new_w = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, true, old_w, false);
            aspect_h = (uint32_t) new_h;
            client_clamp_aspect_ratio(client, (uint32_t) new_w, &aspect_h);
            new_h = (int32_t) aspect_h;
            new_x += (int32_t) old_w - new_w;
            break;
        case KEYBIND_CLIENT_RESIZE_RIGHT:
            new_w = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, true, old_w, true);
            aspect_h = (uint32_t) new_h;
            client_clamp_aspect_ratio(client, (uint32_t) new_w, &aspect_h);
            new_h = (int32_t) aspect_h;
            break;
        case KEYBIND_CLIENT_RESIZE_UP:
            new_h = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, false, old_h, false);
            aspect_h = (uint32_t) new_h;
            client_clamp_aspect_ratio(client, (uint32_t) new_w, &aspect_h);
            new_h = (int32_t) aspect_h;
            new_y += (int32_t) old_h - new_h;
            break;
        case KEYBIND_CLIENT_RESIZE_DOWN:
            new_h = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, false, old_h, true);
            aspect_h = (uint32_t) new_h;
            client_clamp_aspect_ratio(client, (uint32_t) new_w, &aspect_h);
            new_h = (int32_t) aspect_h;
            break;
    }

    s_kbd_resize_apply(client,
            new_x, new_y,
            geom_clamp_dim(new_w),
            geom_clamp_dim(new_h));
}

