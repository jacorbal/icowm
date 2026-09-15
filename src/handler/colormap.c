/**
 * @file handler/colormap.c
 *
 * @brief X @c COLORMAP_NOTIFY event handler
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
#include <stddef.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <surface/desktop.h>

/* Local includes */
#include <handler.h>
#include <handler/colormap.h>


/**
 * @brief What @a s_colormap_update_visit is looking for and doing
 */
struct s_colormap_ctx_s {
    const xcb_colormap_notify_event_t *event;   /**< Notification being
                                                     acted on */
    xcb_connection_t *connection;               /**< Connection to
                                                     install over */
    bool is_done;                               /**< Whether the owning
                                                     client was found */
};


/**
 * @brief Find whether @p window is one of a client's tracked
 *        @c WM_COLORMAP_WINDOWS entries
 *
 * @param client Client whose @c colormap_windows list is searched
 * @param window Window ID to search for
 *
 * @return Index into @p client->colormap_windows.windows on a match,
 *         or @c -1 if @p window is not one of them
 *
 * @note Complexity: @e O(n), where @e n is
 *       @p client->colormap_windows.count
 */
static int32_t s_client_colormap_window_index(const client_td *client,
        xcb_window_t window)
{
    for (uint32_t i = 0u; i < client->colormap_windows.count; ++i) {
        if (client->colormap_windows.windows[i] == window) {
            return (int32_t) i;
        }
    }

    return -1;
}


/**
 * @brief Update the client on this desktop that owns the named window
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_colormap_ctx_s being carried
 *
 * @note Stops acting once the owner is found, the whole walk still
 *       running: a visitor has no way to end one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_colormap_update_visit(desktop_td *desktop, void *data)
{
    struct s_colormap_ctx_s *const ctx = data;
    void *elem;

    if (ctx == NULL || ctx->is_done || desktop->clients == NULL) {
        return;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *const client = (client_td *) elem;
        int32_t idx;
        xcb_colormap_t new_id;

        if (client == NULL) {
            continue;
        }

        idx = s_client_colormap_window_index(client,
                ctx->event->window);
        if (idx < 0) {
            continue;
        }

        new_id = (ctx->event->state == XCB_COLORMAP_STATE_INSTALLED)
            ? ctx->event->colormap : (xcb_colormap_t) XCB_NONE;
        client->colormap_windows.colormap_ids[idx] = new_id;

        /* Only the currently focused client's colormaps are actually
         * installed anywhere ('ccmd_client_focus', in
         * 'cmds/client/focus.c'); for any other client this cached
         * update is all there is to do until it is focused again.
         * Installs the single updated one directly here rather than
         * calling that function again, which would also re-send
         * 'WM_TAKE_FOCUS' and clear urgency, neither warranted by
         * a colormap attribute change alone. */
        if (client_is_focused(client) &&
                new_id != (xcb_colormap_t) XCB_NONE) {
            xcb_install_colormap(ctx->connection, new_id);
        }
        ctx->is_done = true;
        return;
    }
}


/* Handle a 'COLORMAP_NOTIFY' event */
void handler_colormap_notify(xcb_connection_t *connection,
        list_td *surfaces, const xcb_colormap_notify_event_t *event)
{
    struct s_colormap_ctx_s ctx;

    if (event == NULL || surfaces == NULL) {
        return;
    }

    ctx.event = event;
    ctx.connection = connection;
    ctx.is_done = false;

    LOGGER_TRACE("Colormap notify event (window=0x%x, colormap=0x%x,"
            " new=%u, state=%u)", event->window,
            (unsigned int) event->colormap, (unsigned int) event->_new,
            (unsigned int) event->state);

    /* Not this project's root/support windows, and rare enough (see
     * 'client_props_refresh_colormap_windows', client/props.c, for why)
     * that a plain walk over every managed client, rather than
     * a dedicated lookup table keyed on colormap-list windows
     * specifically, costs nothing worth avoiding. */
    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        const surface_td *const surface =
            (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }

        surface_desktop_walk_all(surface, s_colormap_update_visit, &ctx);
        if (ctx.is_done) {
            return;
        }
    }
}
