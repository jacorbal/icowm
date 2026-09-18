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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <cmds/client/move.h>
#include <config.h>
#include <enact.h>
#include <enact/client.h>
#include <logger.h>
#include <render/stage.h>
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
static struct geometry_s s_saved_geom = { { 0, 0 }, { 0u, 0u } };


/**
 * @brief Release the active keyboard grab and flush the connection
 */
static void s_grab_release(void)
{
    if (s_conn != NULL) {
        xcb_ungrab_keyboard(s_conn, XCB_CURRENT_TIME);
        xcb_flush(s_conn);
    }
}


/**
 * @brief Restore client geometry to its saved values
 *
 * Used on @c Escape cancel.  The restore is applied through the same
 * function as normal geometry changes.
 */
static void s_geometry_restore(void)
{
    if (s_client == NULL) {
        return;
    }
    if (s_mode == KBD_MODAL_MOVING) {
        enact_client_move(s_client, s_saved_geom.pos);
    } else {
        enact_client_resize(s_client, s_saved_geom);
    }
}


/**
 * @brief Exit modal mode, releasing the keyboard grab and clearing
 *        state
 */
static void s_modal_exit(void)
{
    s_grab_release();
    s_mode = KBD_MODAL_NONE;
    s_edge = KBD_EDGE_NONE;
    s_conn = NULL;
    s_client = NULL;
    s_saved_geom.pos.x = 0;
    s_saved_geom.pos.y = 0;
    s_saved_geom.dim.w = 0u;
    s_saved_geom.dim.h = 0u;
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
        /* One real, notifying move now that the session actually
         * ends, the one call withheld from every quiet key-by-key
         * step along the way (see ccmd_client_move_track's own
         * comment for why), so the client's belief about where it
         * sits on screen is never left stale. */
        if (s_client != NULL) {
            enact_client_move(s_client,
                    s_client->layout.geometry.cur.pos);
        }
        s_modal_exit();
        return;
    }

    if (keysym == KS_ESCAPE) {
        s_geometry_restore();
        s_modal_exit();
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

    /* Quiet on purpose for every intermediate key press, same
     * reasoning as a mouse drag's own live steps (see
     * ccmd_client_move_track's own comment); s_handle_move_key's
     * Return branch above sends the one real, notifying move once
     * the session actually ends. */
    ccmd_client_move_track(s_client, (struct position_s) { x, y });
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
static void s_resize_compute(xcb_keysym_t keysym, int32_t step,
        int32_t *restrict nx, int32_t *restrict ny,
        int32_t *restrict nw, int32_t *restrict nh)
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
 * The first arrow key both picks the active edge and, in that same
 * press, already grows or shrinks the window along it; every arrow
 * press after that keeps resizing along whichever edge is currently
 * active.  Return confirms and Escape restores the original geometry
 * and cancels.
 *
 * @param keysym      X keysym of the pressed key
 * @param resize_step Distance in pixels to resize per key press
 */
static void s_handle_resize_key(xcb_keysym_t keysym,
        int32_t resize_step)
{
    int32_t nx;
    int32_t ny;
    int32_t nw;
    int32_t nh;

    if (keysym == KS_RETURN || keysym == KS_KP_ENTER) {
        s_modal_exit();
        return;
    }

    if (keysym == KS_ESCAPE) {
        s_geometry_restore();
        s_modal_exit();
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

        /* A client maximized on just one axis has nothing free to
         * resize along the other: refuse picking an edge on the locked
         * axis outright, resetting back to no active edge rather than
         * leaving it set to one that will never actually move anything,
         * so a later press on the still-free axis is free to start over
         * as a genuine first press of its own.  The same axis-lock
         * every other interactive resize entry point in this project
         * already applies (mouse border drag via
         * 'drag_start_resize_axis_locked', input/mouse/drag.c;
         * per-keypress resize in 's_kbd_resize_axis_target',
         * input/kbd/interact.c). */
        if ((s_edge == KBD_EDGE_TOP || s_edge == KBD_EDGE_BOTTOM) &&
                client_is_maximized_vert(s_client)) {
            s_edge = KBD_EDGE_NONE;
            return;
        }
        if ((s_edge == KBD_EDGE_LEFT || s_edge == KBD_EDGE_RIGHT) &&
                client_is_maximized_horz(s_client)) {
            s_edge = KBD_EDGE_NONE;
            return;
        }
    }

    nx = s_client->layout.geometry.cur.pos.x;
    ny = s_client->layout.geometry.cur.pos.y;
    nw = (int32_t) s_client->layout.geometry.cur.dim.w;
    nh = (int32_t) s_client->layout.geometry.cur.dim.h;

    s_resize_compute(keysym, resize_step, &nx, &ny, &nw, &nh);

    if (nw < 1) {
        nw = 1;
    }
    if (nh < 1) {
        nh = 1;
    }

    enact_client_resize(s_client, (struct geometry_s) {
                { nx, ny },
                { geom_dim_clamp(nw), geom_dim_clamp(nh) } });
}


/**
 * @brief Enter a keyboard modal session (move or resize) for a client
 *
 * Shared by @c kbd_modal_move_start and @c kbd_modal_resize_start
 * below, which only differ in which @c s_mode_e the session enters.
 *
 * @param connection XCB connection
 * @param stage      Stage @p client is on, for the keyboard grab's
 *                   own root window
 * @param client Client entering the modal session
 * @param mode   @c KBD_MODAL_MOVING or @c KBD_MODAL_RESIZING
 *
 * @note Complexity: @e O(1)
 */
static void s_modal_enter(xcb_connection_t *connection,
        stage_td *stage, client_td *client, s_mode_e mode)
{
    xcb_window_t root_win;
    xcb_grab_keyboard_cookie_t cookie;
    xcb_grab_keyboard_reply_t *reply;

    if (connection == NULL || client == NULL || stage == NULL) {
        return;
    }

    root_win = (stage->screen != NULL)
        ? stage->screen->root
        : XCB_WINDOW_NONE;

    if (root_win == XCB_WINDOW_NONE) {
        return;
    }

    cookie = xcb_grab_keyboard(connection,
            0,
            root_win,
            XCB_CURRENT_TIME,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC);
    reply = xcb_grab_keyboard_reply(connection, cookie, NULL);

    if (reply == NULL || reply->status != XCB_GRAB_STATUS_SUCCESS) {
        LOGGER_WARNING("xcb_grab_keyboard failed for modal session," \
                " status=%d",
                (reply != NULL) ? (int) reply->status : -1);
        free(reply);
        return;
    }
    free(reply);

    s_conn = connection;
    s_client = client;
    s_saved_geom = client->layout.geometry.cur;
    s_edge = KBD_EDGE_NONE;
    s_mode = mode;
}


/* Check whether a keyboard modal mode is active */
bool kbd_modal_is_active(void)
{
    return s_mode != KBD_MODAL_NONE;
}


/* Enter keyboard modal move mode for the given client */
void kbd_modal_move_start(xcb_connection_t *connection,
        stage_td *stage, client_td *client)
{
    s_modal_enter(connection, stage, client, KBD_MODAL_MOVING);
}


/* Enter keyboard modal resize mode for the given client */
void kbd_modal_resize_start(xcb_connection_t *connection,
        stage_td *stage, client_td *client)
{
    s_modal_enter(connection, stage, client, KBD_MODAL_RESIZING);
}


/* Dispatch a key press while a keyboard modal session is active */
bool kbd_modal_handle_keypress(xcb_connection_t *connection,
        stage_td *stage, xcb_keysym_t keysym,
        const config_td *config)
{
    int32_t move_step;
    int32_t resize_step;

    (void) connection;
    (void) stage;

    if (s_mode == KBD_MODAL_NONE) {
        return false;
    }

    move_step =
        (config != NULL && config->base.windows.move_step > 0u)
            ? (int32_t) config->base.windows.move_step : 1;
    resize_step =
        (config != NULL && config->base.windows.resize_step > 0u)
            ? (int32_t) config->base.windows.resize_step : 1;

    if (s_mode == KBD_MODAL_MOVING) {
        s_handle_move_key(keysym, move_step);
    } else {
        s_handle_resize_key(keysym, resize_step);
    }

    return true;
}
