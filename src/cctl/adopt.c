/**
 * @file cctl/adopt.c
 *
 * @brief Window manager startup scan implementation
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
#include <stdlib.h>     /* NULL, free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Default initial values */
#include <defs/desktop.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <rules.h>
#include <surface.h>
#include <wm.h>

/* Render includes */
#include <render/outdate.h>

/* Local includes */
#include <cctl/adopt.h>


/* Adopt all pre-existing mapped windows at window manager startup */
void cctl_adopt_scan(const wm_td *wm)
{
    xcb_connection_t *connection = wm_connection(wm);
    xcb_ewmh_connection_t *ewmh = wm_ewmh(wm);
    config_td *config = wm_config(wm);

    if (wm == NULL) {
        return;
    }

    LOGGER_DEBUG("Scanning for pre-existing mapped windows", L_NARG);

    for (list_item_td *node = list_head(wm_surfaces(wm));
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        xcb_query_tree_cookie_t qt_cookie;
        xcb_query_tree_reply_t *qt_reply;
        xcb_window_t *children;
        int nchildren;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        LOGGER_TRACE("Querying window tree for surface %u" \
                " (root %#x)", surface->id, surface->screen->root);

        qt_cookie = xcb_query_tree(connection,
                surface->screen->root);
        qt_reply = xcb_query_tree_reply(connection,
                qt_cookie, NULL);
        if (qt_reply == NULL) {
            LOGGER_WARNING("Failed to query window tree for surface %u",
                    surface->id);
            continue;
        }

        children = xcb_query_tree_children(qt_reply);
        nchildren = xcb_query_tree_children_length(qt_reply);

        LOGGER_TRACE("Found %d child window(s) on surface %u",
                nchildren, surface->id);

        for (int i = 0; i < nchildren; ++i) {
            xcb_get_window_attributes_cookie_t ac;
            xcb_get_window_attributes_reply_t *ar;

            ac = xcb_get_window_attributes(connection, children[i]);
            ar = xcb_get_window_attributes_reply(
                    connection, ac, NULL);

            if (ar == NULL) {
                LOGGER_TRACE("Failed to get attributes for window %#x;" \
                        " skipping", children[i]);
                continue;
            }

            if (!ar->override_redirect &&
                    ar->map_state == XCB_MAP_STATE_VIEWABLE) {
                desktop_td *desktop =
                    lookup_current_desktop(surface);
                if (desktop != NULL) {
                    client_td *const client = client_init(
                            connection, ewmh,
                            children[i], &config->theme,
                            &config->base,
                            &config->a11y);
                    if (client != NULL) {
                        /* ReparentWindow on an already-mapped window
                         * generates an 'UnmapNotify'.  Absorb it so
                         * handler_unmap_notify does not wrongly unmap
                         * the new frame. */
                        if (client->frame != 0) {
                            client->ignore_unmap++;
                        }

                        client->screen_id = surface->id;
                        client->desktop_id = desktop->id;
                        desktop_action_client_add(desktop, client);
                        if (ewmh != NULL) {
                            uint32_t did =
                                (client->properties.flags &
                                 CLIENT_FLAG_PIN)
                                ? WM_DESKTOP_ID_ALL : desktop->id;
                            xcb_change_property(connection,
                                    XCB_PROP_MODE_REPLACE,
                                    client->window,
                                    ewmh->_NET_WM_DESKTOP,
                                    XCB_ATOM_CARDINAL, 32, 1, &did);
                        }

                        /* Apply the same rules a client mapped after
                         * this window manager started would already
                         * get (handler_map_notify, handler/map.c):
                         * without this, a rule assigning a desktop,
                         * geometry, layer, or flag to some client
                         * only ever took effect for one launched
                         * fresh, silently skipping any window still
                         * open from before this window manager's own
                         * restart, e.g., surviving a crash or an
                         * intentional reload via 'exec'. */
                        if (rules_apply(wm, client, &surface, &desktop,
                                    RULES_TRIGGER_MAP)) {
                            wm_outdate_surface(surface);
                            wm_outdate_desktop(desktop);
                        }

                        /* ICCCM §4.2.3: inform the client of its
                         * screen-relative geometry now that the frame
                         * has been positioned.  Without this the client
                         * only knows the coordinates that were set
                         * before the window manager started, which are
                         * frame-relative rather than screen-relative. */
                        if (client->frame != 0 &&
                                client_is_decorated(client)) {
                            client_send_synthetic_configure_notify(
                                    connection, client);
                        }

                        surface->is_outdated = true;
                        LOGGER_DEBUG("Adopted pre-existing window %#x" \
                                " on surface %u desktop %u",
                                children[i], surface->id, desktop->id);
                    }
                }
            }

            free(ar);
        }

        surface_refresh_workareas(surface);
        free(qt_reply);
    }

    xcb_flush(connection);
    LOGGER_DEBUG("Finished scanning for pre-existing windows", L_NARG);
}
