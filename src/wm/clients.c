/**
 * @file wm/clients.c
 *
 * @brief Private helpers shared across @c wm sub-modules implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>
#include <stddef.h>     /* NULL */

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <cmds/client/ewmh.h>
#include <desktop.h>
#include <policy/stacking.h>
#include <surface.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Local includes */
#include <wm/internal.h>


/**
 * @brief Release a client back to bare X, undoing this window
 *        manager's management of it, without touching its
 *        window at all beyond reparenting it
 *
 * @c client_destroy (client.h) destroys @c frame/@c titlebar/
 * @c icon_window (every one of them genuinely owned by this window
 * manager) and, unless a caller already zeroed it first, @c window
 * itself too: correct when @c window is already gone (the two
 * existing callers that zero it first, both in handler/map.c, do so
 * specifically because theirs already is, one from a real
 * @c DestroyNotify, the other from a client that never finished
 * being adopted), but not here: every client reaching this function
 * is still fully alive, mid-session, with @p client_destroy about to
 * run on it moments later as this whole window manager instance
 * itself is torn down (@a s_wm_cleanup), not the client.  A window
 * manager exiting, being replaced, or reloading must never take a
 * user's running applications down with it.
 *
 * Reparented back to @p client's root window, at its current
 * absolute on-screen position (recovered from the frame's
 * position plus its frame extents, since @p client's window
 * sits at that fixed offset inside its frame, e.g.,
 * @a ci_create_decorations, cmds/client/state.c), rather than left
 * inside a frame this window manager is about to destroy right along
 * with everything else: 'X' does not auto-reparent a window's
 * children out from under it, so the reparenting itself, not just the
 * zeroing below, is what keeps @p client's window from
 * disappearing along with its frame the moment this instance's
 * cleanup destroys that frame.  A no-op for an already-undecorated
 * client (@c frame already 0), whose window already sits
 * directly under root with nothing to undo.
 *
 * @param client Client to release
 *
 * @note No-op if @p client, its connection, or its window is
 *       already gone
 * @note Complexity: @e O(1)
 */
static void s_client_unmanage(client_td *client)
{
    if (client == NULL || xcb_connection_get() == NULL ||
            client->window == 0u) {
        return;
    }

    /* Map the window back before letting go of it.  This window
     * manager unmaps routinely, every client on a desktop that is not
     * the current one and every iconified client among them, and a
     * window left unmapped once nobody is managing it is lost.  It is
     * still there, its process still running, but no
     * 'MapRequest' will ever be sent for it again, so neither the
     * user nor the next window manager has any way to bring it
     * back.
     *
     * ICCCM asks for exactly this of a window manager giving up its
     * clients, so that another may adopt them in a sane state
     * (Scheifler and Gettys, 1994, "Inter-Client Communication
     * Conventions Manual", v2.0, §4.1.4).
     *
     * Strictly after the reparenting below, never before it: 'X'
     * unmaps a mapped window itself as the first step of reparenting
     * it, so a map issued ahead of that would simply be undone again
     * (X Consortium, 1994, "X Window System Protocol", v11 R6,
     * ReparentWindow). */
    if (client->frame != 0u) {
        int16_t abs_x = (int16_t) (client->layout.geometry.cur.pos.x +
                (int32_t) client->layout.frame_extents.left);
        int16_t abs_y = (int16_t) (client->layout.geometry.cur.pos.y +
                (int32_t) client->layout.frame_extents.top);

        xcb_window_reparent(client->window,
                client->parent_id, abs_x, abs_y);

        /* Handed back to its true parent by hand, so the save set
         * no longer has anything to do for it; leaving it in would
         * only have this connection's eventual close-down reparent
         * it a second time for nothing (Scheifler and Gettys, 1994,
         * "Inter-Client Communication Conventions Manual", v2.0,
         * §4.1.2). */
        xcb_window_save_set(client->window, false);
    }

    xcb_window_show(client->window);
    ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL, XCB_NONE);

    /* 'client_destroy' only ever destroys 'window' itself when this
     * is still non-zero; every other field it destroys ('frame',
     * 'titlebar', 'icon_window') is untouched here, since those are
     * genuinely this window manager's resources, correctly torn
     * down along with the rest of it. */
    client->window = 0u;
}


/**
 * @brief Release one client back to the X server
 *
 * @param client Client reached by the walk
 * @param data   Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_client_unmanage_visit(client_td *client, void *data)
{
    (void) data;

    s_client_unmanage(client);
}


/**
 * @brief What @a s_client_action_visit is counting and calling
 */
struct s_client_action_ctx_s {
    void (*action)(client_td *client, void *userdata);
                            /**< Called per client; may be @c NULL */
    void *userdata;         /**< Handed to @p action untouched */
    uint32_t count;         /**< How many clients have been reached */
};


/**
 * @brief Reach every client one desktop shows
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_client_action_ctx_s being carried
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_client_action_visit(desktop_td *desktop, void *data)
{
    struct s_client_action_ctx_s *const ctx = data;
    void *elem;

    if (ctx == NULL || desktop->clients == NULL) {
        return;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *const client = (client_td *) elem;

        if (client != NULL) {
            ctx->count++;
            if (ctx->action != NULL) {
                ctx->action(client, ctx->userdata);
            }
        }
    }
}


/* Visit every managed client, on every surface and desktop */
uint32_t wm_for_each_client(const wm_td *wm,
        void (*action)(client_td *client,
            void *userdata), void *userdata)
{
    struct s_client_action_ctx_s ctx;
    list_td *const surfaces = wm_surfaces(wm);

    if (surfaces == NULL) {
        return 0u;
    }

    ctx.action = action;
    ctx.userdata = userdata;
    ctx.count = 0u;

    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        const surface_td *const surface = (surface_td *) list_data(snode);

        if (surface != NULL) {
            surface_desktops_walk(surface, s_client_action_visit, &ctx);
        }
    }

    return ctx.count;
}


/**
 * @brief Release every client, on every desktop of every managed
 *        surface, back to bare X before this whole instance's
 *        teardown destroys the window manager's resources
 *
 * @param wm Window manager instance
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       clients across every desktop of every managed surface
 */
void wm_all_clients_unmanage(const wm_td *wm)
{
    list_td *const surfaces = wm_surfaces(wm);
    list_item_td *snode;

    if (surfaces == NULL) {
        return;
    }

    for (snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *const surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;
        const cdlist_item_td *dinitial;

        if (surface == NULL || surface->desktops == NULL) {
            continue;
        }

        dnode = cdlist_head(surface->desktops);
        if (dnode == NULL) {
            continue;
        }

        dinitial = dnode;
        do {
            const desktop_td *const desktop =
                (desktop_td *) cdlist_data(dnode);

            stacking_walk(desktop, s_client_unmanage_visit, NULL);
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial);
    } /* ! for (snode) */
}
