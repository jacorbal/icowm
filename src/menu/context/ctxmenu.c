/**
 * @file menu/context/ctxmenu.c
 *
 * @brief Generic context menu library: window lifecycle and top-level
 *        state queries
 *
 * Split by competency into @c ctxmenu/layout.c (row geometry and
 * hit-testing), @c ctxmenu/redraw.c (painting), @c ctxmenu/select.c
 * (selection and activation), @c ctxmenu/handle.c (raw event
 * handling for a single window), and @c ctxmenu/tree.c (dispatch
 * across a submenu window tree), leaving this file with only what
 * belongs to no single one of those: creating and destroying the
 * menu window itself, and querying whether it is open.
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
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/layout.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/* Create and show a context menu window */
void ctxmenu_show(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *state,
        struct position_s pos, const config_td *config)
{
    uint32_t mask;
    uint32_t values[4];
    uint32_t stk[1];
    int16_t clamped_x;
    int16_t clamped_y;
    int32_t max_x;
    int32_t max_y;
    struct geometry_s work;
    const desktop_td *desktop;
    size_t entry_count;

    if (connection == NULL || surface == NULL || state == NULL ||
            config == NULL || state->entries == NULL ||
            state->entry_count <= 0) {
        return;
    }

    /* Copied into a 'size_t' local right after the guard above proved
     * it positive: GCC's allocation-size analysis cannot otherwise
     * see past the 'ctxmenu_close' and 'ctxmenu_width_compute' calls
     * between here and the 'calloc' below to know
     * 'state->entry_count' is
     * still positive at that point, since either call could in
     * principle modify the struct through the same pointer, so
     * without this it falls back to assuming the field's entire
     * signed range is possible there. */
    entry_count = (size_t) state->entry_count;

    ctxmenu_close(state);

    state->surface = surface;
    state->config = config;
    state->selected = -1;
    state->last_motion_y = -1;

    state->width = ctxmenu_width_compute(connection, state->entries,
            state->entry_count, config);

    /* Cache each row's top-Y offset so 'ctxmenu_entry_top_y' and
     * 'ctxmenu_entry_at_y' need not re-walk 'entries' on every repaint,
     * click, or motion event; 'entry_top_y' is left 'NULL' (and both
     * helpers fall back to an 'O(n)' walk) if 'calloc' fails */
    state->entry_top_y = (int32_t *) calloc(entry_count,
            sizeof(int32_t));
    state->height = ctxmenu_layout_build(state);

    /* Use the active desktop's work area to clamp position */
    work = (struct geometry_s) { { 0, 0 }, surface->properties.dim };
    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop != NULL) {
        work = desktop->workarea;
    }

    max_x = (int32_t) ((uint32_t) work.pos.x + work.dim.w) -
        (int32_t) state->width;
    max_y = (int32_t) ((uint32_t) work.pos.y + work.dim.h) -
        (int32_t) state->height;

    clamped_x = (int16_t) pos.x;
    clamped_y = (int16_t) pos.y;
    if (clamped_x > (int16_t) max_x) { clamped_x = (int16_t) max_x; }
    if (clamped_y > (int16_t) max_y) { clamped_y = (int16_t) max_y; }
    if (clamped_x < (int16_t) work.pos.x) {
        clamped_x = (int16_t) work.pos.x;
    }
    if (clamped_y < (int16_t) work.pos.y) {
        clamped_y = (int16_t) work.pos.y;
    }

    state->origin_x = clamped_x;
    state->origin_y = clamped_y;

    state->window = xcb_generate_id(connection);
    mask = XCB_CW_BACK_PIXEL        |
           XCB_CW_BORDER_PIXEL      |
           XCB_CW_OVERRIDE_REDIRECT |
           XCB_CW_EVENT_MASK;
    values[0] = config->theme.menu.unselected.color.background;
    values[1] = config->theme.menu.border.color;
    values[2] = 1;  /* override_redirect: prevent WM from managing it */
    values[3] = XCB_EVENT_MASK_EXPOSURE     |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_POINTER_MOTION;

    xcb_create_window(connection, XCB_COPY_FROM_PARENT,
            state->window, surface->screen->root,
            clamped_x, clamped_y,
            state->width, state->height,
            (uint16_t) config->theme.menu.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* The whole menu window's opacity, distinct from any one
     * row's font/color/border, since '_NET_WM_WINDOW_OPACITY' is
     * a per-window property, not a per-row one; see the doc comment
     * on 'config_theme_s.menu.opacity' (config.h) */
    atom_set_window_opacity(connection, state->window,
            config_theme_opacity_to_raw(config->theme.menu.opacity));

    /* Raise to the top */
    stk[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, state->window,
            XCB_CONFIG_WINDOW_STACK_MODE, stk);

    xcb_map_window(connection, state->window);

    /* Grab keyboard and pointer for the root menu only (not submenus).
     * The keyboard grab redirects all key events (including 'Escape')
     * to the window manager so the menu can be dismissed without the
     * focused application consuming those keys first.  The pointer grab
     * ensures that clicks outside the menu hierarchy are seen by the WM
     * even when an application holds an active pointer grab. */
    if (state->parent == NULL) {
        xcb_grab_keyboard(connection,
                0,                      /* owner_events */
                surface->screen->root,
                XCB_CURRENT_TIME,
                XCB_GRAB_MODE_ASYNC,    /* pointer events unaffected */
                XCB_GRAB_MODE_ASYNC);   /* delivered asynchronously */
        xcb_grab_pointer(connection,
                1,                      /* owner_events: report events
                                           normally to whichever window
                                           in the menu hierarchy the
                                           pointer is actually over
                                           (needed so 'MotionNotify'
                                           events are delivered with the
                                           correct 'event' window and
                                           window-relative coordinates,
                                           enabling hover highlight) */
                surface->screen->root,
                XCB_EVENT_MASK_BUTTON_PRESS   |
                XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_POINTER_MOTION,
                XCB_GRAB_MODE_ASYNC,
                XCB_GRAB_MODE_ASYNC,
                XCB_NONE,               /* confine to no window */
                XCB_NONE,               /* no cursor override */
                XCB_CURRENT_TIME);
    }

}


/* Close a context menu and its entire descendant chain */
void ctxmenu_close(ctxmenu_state_td *state)
{
    if (state == NULL) {
        return;
    }

    /* Close children first */
    if (state->child != NULL) {
        ctxmenu_close(state->child);
        state->child = NULL;
    }

    if (xcb_connection_get() != NULL && state->window != XCB_WINDOW_NONE) {
        xcb_window_destroy(state->window);
    }

    /* Always release keyboard and pointer grabs when the root menu
     * closes, even if the window was already gone.  This prevents stale
     * grabs from blocking further input when a race condition or early
     * destroy leaves 'window' as 'XCB_WINDOW_NONE' before close. */
    if (state->parent == NULL && xcb_connection_get() != NULL) {
        xcb_ungrab_keyboard(xcb_connection_get(), XCB_CURRENT_TIME);
        xcb_ungrab_pointer(xcb_connection_get(), XCB_CURRENT_TIME);
    }

    if (xcb_connection_get() != NULL) {
    }

    state->window = XCB_WINDOW_NONE;
    state->selected = -1;
    state->width = 0;
    state->height = 0;
    state->surface = NULL;
    state->config = NULL;

    free(state->entry_top_y);
    state->entry_top_y = NULL;
}


/* Query whether the context menu is currently open */
bool ctxmenu_is_open(const ctxmenu_state_td *state)
{
    return state != NULL && state->window != XCB_WINDOW_NONE;
}
