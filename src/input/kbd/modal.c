/**
 * @file input/kbd/modal.c
 *
 * @brief Keyboard modal move and resize implementation
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
#include <xcb/xcb_keysyms.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <enact.h>
#include <render/surface.h>
#include <utils/geom.h>

/* Local includes */
#include <input/kbd/modal.h>




/* Internal mode and edge enumerations */
typedef enum {
    KBD_MODAL_NONE,
    KBD_MODAL_MOVING,
    KBD_MODAL_RESIZING
} s_mode_e;

typedef enum {
    KBD_EDGE_NONE,
    KBD_EDGE_TOP,
    KBD_EDGE_BOTTOM,
    KBD_EDGE_LEFT,
    KBD_EDGE_RIGHT
} s_edge_e;


/* Static state */
static s_mode_e s_mode = KBD_MODAL_NONE;
static s_edge_e s_edge = KBD_EDGE_NONE;
static xcb_connection_t *s_conn = NULL;
static client_td *s_client = NULL;
static int32_t s_saved_x = 0;
static int32_t s_saved_y = 0;
static uint32_t s_saved_w = 0u;
static uint32_t s_saved_h = 0u;


/**
 * @brief Release the active keyboard grab and flush the connection
 */
static void s_release_grab(void)
{
    if (s_conn != NULL) {
        xcb_ungrab_keyboard(s_conn, XCB_CURRENT_TIME);
        xcb_flush(s_conn);
    }
}


/**
 * @brief Restore client geometry to its saved values
 *
 * Used on ESC cancel.  The restore is applied through the same
 * function as normal geometry changes
 */
static void s_restore_geometry(void)
{
    if (s_client == NULL) {
        return;
    }
    if (s_mode == KBD_MODAL_MOVING) {
        enact_client_move(s_client, s_saved_x, s_saved_y);
    } else {
        enact_client_resize(s_client,
                s_saved_x, s_saved_y, s_saved_w, s_saved_h);
    }
}


/**
 * @brief Exit modal mode, releasing the keyboard grab and clearing state
 */
static void s_exit_modal(void)
{
    s_release_grab();
    s_mode = KBD_MODAL_NONE;
    s_edge = KBD_EDGE_NONE;
    s_conn = NULL;
    s_client = NULL;
    s_saved_x = 0;
    s_saved_y = 0;
    s_saved_w = 0u;
    s_saved_h = 0u;
}


/**
 * @brief Handle one key press while in move modal mode
 *
 * Applies a position delta for arrow keys, confirms on @c Return, and
 * cancels (restoring original position) on @c Escape.
 *
 * @param keysym    X keysym of the pressed key
 * @param move_step Distance in pixels to move per key press
 */
static void s_handle_move_key(xcb_keysym_t keysym, int32_t move_step)
{
    int32_t x;
    int32_t y;

    if (keysym == KS_RETURN || keysym == KS_KP_ENTER) {
        s_exit_modal();
        return;
    }

    if (keysym == KS_ESCAPE) {
        s_restore_geometry();
        s_exit_modal();
        return;
    }

    if (s_client == NULL) {
        return;
    }

    x = s_client->layout.geometry.cur.pos.x;
    y = s_client->layout.geometry.cur.pos.y;

    if (keysym == KS_LEFT) {
        x -= move_step;
    } else if (keysym == KS_RIGHT) {
        x += move_step;
    } else if (keysym == KS_UP) {
        y -= move_step;
    } else if (keysym == KS_DOWN) {
        y += move_step;
    } else {
        return;
    }

    enact_client_move(s_client, x, y);
}


/**
 * @brief Compute new geometry for one resize key press on a given edge
 *
 * Grows or shrinks the window by @p step pixels along the active edge,
 * updating @p nx, @p ny, @p nw and @p nh in-place.
 *
 * @param keysym X keysym of the pressed key
 * @param step   Resize step in pixels
 * @param nx     Frame X (in/out)
 * @param ny     Frame Y (in/out)
 * @param nw     Frame width (in/out)
 * @param nh     Frame height (in/out)
 */
static void s_compute_resize(xcb_keysym_t keysym, int32_t step,
        int32_t *nx, int32_t *ny, int32_t *nw, int32_t *nh)
{
    switch (s_edge) {
        case KBD_EDGE_TOP:
            if (keysym == KS_UP) {
                /* grow from top: move y up, increase height */
                *ny -= step;
                *nh += step;
            } else if (keysym == KS_DOWN) {
                /* shrink from top: move y down, decrease height */
                *ny += step;
                *nh -= step;
            }
            break;
        case KBD_EDGE_BOTTOM:
            if (keysym == KS_DOWN) {
                *nh += step;
            } else if (keysym == KS_UP) {
                *nh -= step;
            }
            break;
        case KBD_EDGE_LEFT:
            if (keysym == KS_LEFT) {
                /* grow from left: move x left, increase width */
                *nx -= step;
                *nw += step;
            } else if (keysym == KS_RIGHT) {
                /* shrink from left: move x right, decrease width */
                *nx += step;
                *nw -= step;
            }
            break;
        case KBD_EDGE_RIGHT:
            if (keysym == KS_RIGHT) {
                *nw += step;
            } else if (keysym == KS_LEFT) {
                *nw -= step;
            }
            break;
        case KBD_EDGE_NONE:
            break;
    }
}


/**
 * @brief Handle one key press while in resize modal mode
 *
 * The first arrow key sets the active edge.  Subsequent arrow presses
 * grow or shrink the window along that edge.  Return confirms and
 * Escape restores the original geometry and cancels.
 *
 * @param keysym      X keysym of the pressed key
 * @param resize_step Distance in pixels to resize per key press
 */
static void s_handle_resize_key(xcb_keysym_t keysym, int32_t resize_step)
{
    int32_t nx;
    int32_t ny;
    int32_t nw;
    int32_t nh;

    if (keysym == KS_RETURN || keysym == KS_KP_ENTER) {
        s_exit_modal();
        return;
    }

    if (keysym == KS_ESCAPE) {
        s_restore_geometry();
        s_exit_modal();
        return;
    }

    if (s_client == NULL) {
        return;
    }

    /* First arrow press determines the active edge */
    if (s_edge == KBD_EDGE_NONE) {
        if (keysym == KS_UP) {
            s_edge = KBD_EDGE_TOP;
        } else if (keysym == KS_DOWN) {
            s_edge = KBD_EDGE_BOTTOM;
        } else if (keysym == KS_LEFT) {
            s_edge = KBD_EDGE_LEFT;
        } else if (keysym == KS_RIGHT) {
            s_edge = KBD_EDGE_RIGHT;
        } else {
            return;
        }
    }

    nx = s_client->layout.geometry.cur.pos.x;
    ny = s_client->layout.geometry.cur.pos.y;
    nw = (int32_t) s_client->layout.geometry.cur.dim.w;
    nh = (int32_t) s_client->layout.geometry.cur.dim.h;

    s_compute_resize(keysym, resize_step, &nx, &ny, &nw, &nh);

    if (nw < 1) {
        nw = 1;
    }
    if (nh < 1) {
        nh = 1;
    }

    enact_client_resize(s_client, nx, ny,
            geom_clamp_dim(nw), geom_clamp_dim(nh));
}


/* Check whether a keyboard modal mode is active */
bool kbd_modal_is_active(void)
{
    return s_mode != KBD_MODAL_NONE;
}


/**
 * @brief Enter a keyboard modal session (move or resize) for a client
 *
 * Shared by @c kbd_modal_move_start and @c kbd_modal_resize_start
 * below, which only differ in which @c s_mode_e the session enters.
 *
 * @param connection XCB connection
 * @param surface    Surface @p client is on, for the keyboard grab's
 *                   own root window
 * @param client     Client entering the modal session
 * @param mode       @c KBD_MODAL_MOVING or @c KBD_MODAL_RESIZING
 *
 * @note Complexity: @e O(1)
 */
static void s_enter_modal(xcb_connection_t *connection,
        surface_td *surface, client_td *client, s_mode_e mode)
{
    xcb_window_t root_win;

    if (connection == NULL || client == NULL || surface == NULL) {
        return;
    }

    s_conn = connection;
    s_client = client;
    s_saved_x = client->layout.geometry.cur.pos.x;
    s_saved_y = client->layout.geometry.cur.pos.y;
    s_saved_w = client->layout.geometry.cur.dim.w;
    s_saved_h = client->layout.geometry.cur.dim.h;
    s_edge = KBD_EDGE_NONE;
    s_mode = mode;

    root_win = (surface->screen != NULL)
        ? surface->screen->root
        : XCB_WINDOW_NONE;

    if (root_win != XCB_WINDOW_NONE) {
        xcb_grab_keyboard(connection,
                0,
                root_win,
                XCB_CURRENT_TIME,
                XCB_GRAB_MODE_ASYNC,
                XCB_GRAB_MODE_ASYNC);
        xcb_flush(connection);
    }
}


/* Enter keyboard modal move mode for the given client */
void kbd_modal_move_start(xcb_connection_t *connection,
        surface_td *surface, client_td *client)
{
    s_enter_modal(connection, surface, client, KBD_MODAL_MOVING);
}


/* Enter keyboard modal resize mode for the given client */
void kbd_modal_resize_start(xcb_connection_t *connection,
        surface_td *surface, client_td *client)
{
    s_enter_modal(connection, surface, client, KBD_MODAL_RESIZING);
}


/* Dispatch a key press while a keyboard modal session is active */
bool kbd_modal_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config)
{
    int32_t move_step;
    int32_t resize_step;

    (void) connection;
    (void) surface;

    if (s_mode == KBD_MODAL_NONE) {
        return false;
    }

    move_step = (config != NULL && config->base.windows.move_step > 0u)
        ? (int32_t) config->base.windows.move_step : 1;
    resize_step = (config != NULL && config->base.windows.resize_step > 0u)
        ? (int32_t) config->base.windows.resize_step : 1;

    if (s_mode == KBD_MODAL_MOVING) {
        s_handle_move_key(keysym, move_step);
    } else {
        s_handle_resize_key(keysym, resize_step);
    }

    return true;
}
