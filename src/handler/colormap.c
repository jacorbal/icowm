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

/* Local includes */
#include <handler.h>


/**
 * @brief Find whether @p window is one of a client's own tracked
 *        @c WM_COLORMAP_WINDOWS entries
 *
 * @param client Client whose own @c colormap_windows list is searched
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


/* Handle a 'COLORMAP_NOTIFY' event */
void handler_colormap_notify(xcb_connection_t *connection,
        list_td *surfaces, const xcb_colormap_notify_event_t *event)
{
    if (event == NULL || surfaces == NULL) {
        return;
    }

    LOGGER_TRACE("Colormap notify event (window=0x%x, colormap=0x%x,"
            " new=%u, state=%u)", event->window,
            (unsigned int) event->colormap, (unsigned int) event->_new,
            (unsigned int) event->state);

    /* Not this project's own root/support windows, and rare enough
     * (see 'client_props_refresh_colormap_windows', client/props.c,
     * for why) that a plain walk over every managed client, rather
     * than a dedicated lookup table keyed on colormap-list windows
     * specifically, costs nothing worth avoiding. */
    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *const surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;

        if (surface == NULL) {
            continue;
        }

        cdlist_foreach(surface->desktops, dnode) {
            desktop_td *const desktop = (desktop_td *) cdlist_data(dnode);
            void *elem;

            if (desktop == NULL || desktop->clients == NULL) {
                continue;
            }

            ohtbl_foreach(desktop->clients, elem) {
                client_td *const client = (client_td *) elem;
                int32_t idx;
                xcb_colormap_t new_id;

                if (client == NULL) {
                    continue;
                }

                idx = s_client_colormap_window_index(client,
                        event->window);
                if (idx < 0) {
                    continue;
                }

                new_id = (event->state == XCB_COLORMAP_STATE_INSTALLED)
                    ? event->colormap : (xcb_colormap_t) XCB_NONE;
                client->colormap_windows.colormap_ids[idx] = new_id;

                /* Only the currently focused client's own colormaps
                 * are actually installed anywhere ('ccmd_client_focus',
                 * cmds/client/focus.c); for any other client this
                 * cached update is all there is to do until it is
                 * focused again.  Installs the single updated one
                 * directly here rather than calling that function
                 * again, which would also re-send 'WM_TAKE_FOCUS' and
                 * clear urgency, neither warranted by a colormap
                 * attribute change alone. */
                if (client_is_focused(client) &&
                        new_id != (xcb_colormap_t) XCB_NONE) {
                    xcb_install_colormap(connection, new_id);
                }
                return;
            }
        }
    }
}
