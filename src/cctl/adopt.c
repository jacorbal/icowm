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
#include <stdlib.h>     /* NULL, calloc, free */

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

/* Utils includes */
#include <utils/xcb/reply.h>

/* Local includes */
#include <cctl/adopt.h>


/**
 * @brief Take one pre-existing window under management
 *
 * Everything a window mapped after this window manager started would
 * already have gone through, done here for one that was open before
 * it: the client is created, put on the surface's current desktop,
 * told which desktop that is, and run past the rules.
 *
 * @param wm      Window manager instance
 * @param surface Surface the window is on
 * @param window  Window to adopt
 *
 * @note The nested checks are early returns here rather than one
 *       deeper level each, which is the whole reason this is its
 *       function
 * @note Complexity: @e O(n), where @e n is the number of rules
 */
static void s_adopt_one_window(const wm_td *wm, surface_td *surface,
        xcb_window_t window)
{
    xcb_connection_t *const connection = wm_connection(wm);
    xcb_ewmh_connection_t *const ewmh = wm_ewmh(wm);
    const config_td *const config = wm_config(wm);
    desktop_td *desktop = lookup_current_desktop(surface);
    client_td *client;

    if (desktop == NULL) {
        return;
    }

    client = client_init(connection, ewmh, window, config);
    if (client == NULL) {
        return;
    }

    /* ReparentWindow on an already-mapped window generates an
     * 'UnmapNotify'.  Absorb it so handler_unmap_notify does not
     * wrongly unmap the new frame. */
    if (client->frame != 0) {
        client->ignore.unmap++;
    }

    client->screen_id = surface->id;
    client->desktop_id = desktop->id;
    desktop_action_client_add(desktop, client);
    if (ewmh != NULL) {
        uint32_t did = (client->properties.flags & CLIENT_FLAG_PIN)
            ? WM_DESKTOP_ID_ALL : desktop->id;

        xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                client->window, ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &did);
    }

    /* Apply the same rules a client mapped after this window manager
     * started would already get ('handler_map_notify', handler/map.c):
     * without this, a rule assigning a desktop, geometry, layer, or
     * flag to some client only ever took effect for one launched
     * fresh, silently skipping any window still open from before this
     * window manager's restart, e.g., surviving a crash or an
     * intentional reload via 'exec'. */
    if (rules_apply(wm, client, &surface, &desktop,
                RULES_TRIGGER_MAP)) {
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
    }

    /* ICCCM §4.2.3: inform the client of its screen-relative geometry
     * now that the frame has been positioned.  Without this the client
     * only knows the coordinates that were set before the window
     * manager started, which are frame-relative rather than
     * screen-relative. */
    if (client->frame != 0 && client_is_decorated(client)) {
        client_send_synthetic_configure_notify(connection, client);
    }

    surface->is_outdated = true;
    LOGGER_DEBUG("Adopted pre-existing window %#x on surface %u" \
            " desktop %u", window, surface->id, desktop->id);
}


/**
 * @brief Adopt every window already mapped on one surface
 *
 * Asks the root window for its children, then asks for all their
 * attributes before awaiting any answer, so the scan costs one round
 * trip rather than one per window: a session being adopted holds as
 * many windows as the user had open, and over a remote display that
 * difference is the whole of the startup delay.
 *
 * @param wm      Window manager instance
 * @param surface Surface to scan
 *
 * @note Complexity: @e O(n), where @e n is the number of windows
 *       already on @p surface
 */
static void s_adopt_scan_surface(const wm_td *wm, surface_td *surface)
{
    xcb_connection_t *const connection = wm_connection(wm);
    xcb_query_tree_cookie_t qt_cookie;
    xcb_get_window_attributes_cookie_t *cookies;
    xcb_query_tree_reply_t *qt_reply;
    xcb_generic_error_t *qt_error = NULL;
    xcb_window_t *children;
    int nchildren;

    LOGGER_TRACE("Querying window tree for surface %u" \
            " (root %#x)", surface->id, surface->screen->root);

    qt_cookie = xcb_query_tree(connection,
            surface->screen->root);
    qt_reply = xcb_query_tree_reply(connection,
            qt_cookie, &qt_error);
    if (qt_reply == NULL) {
        xcb_reply_log_error(qt_error, "a root window's own tree");
        LOGGER_WARNING("Failed to query window tree for surface %u",
                surface->id);
        return;
    }

    cookies = NULL;
    children = xcb_query_tree_children(qt_reply);
    nchildren = xcb_query_tree_children_length(qt_reply);

    LOGGER_TRACE("Found %d child window(s) on surface %u",
            nchildren, surface->id);

    /* Every child's attributes are asked for before any answer is
     * awaited, so that the whole scan costs one round trip to the
     * server rather than one per window already on the screen.
     * A session being adopted holds as many windows as the user
     * had open, and over a remote display that difference is the
     * whole of the startup delay.
     *
     * Allocated rather than kept on the stack: 'nchildren' is
     * whatever the root window happens to hold, which is not a
     * number this can bound.  Failing to allocate is not fatal,
     * only slower, so the scan falls back to asking one at a
     * time. */
    cookies = calloc((size_t) nchildren, sizeof(*cookies));
    if (cookies != NULL) {
        for (int i = 0; i < nchildren; ++i) {
            cookies[i] = xcb_get_window_attributes(connection,
                    children[i]);
        }
    }

    for (int i = 0; i < nchildren; ++i) {
        xcb_get_window_attributes_reply_t *ar;

        ar = xcb_get_window_attributes_reply(connection,
                (cookies != NULL)
                    ? cookies[i]
                    : xcb_get_window_attributes(connection,
                            children[i]),
                NULL);

        if (ar == NULL) {
            LOGGER_TRACE("Failed to get attributes for window %#x;" \
                    " skipping", children[i]);
            continue;
        }

        if (!ar->override_redirect &&
                ar->map_state == XCB_MAP_STATE_VIEWABLE) {
            s_adopt_one_window(wm, surface, children[i]);
        }

        free(ar);
    }

    free(cookies);
    surface_refresh_workareas(surface);
    free(qt_reply);
}


/* Adopt all pre-existing mapped windows at window manager startup */
void cctl_adopt_scan(const wm_td *wm)
{
    xcb_connection_t *const connection = wm_connection(wm);

    if (wm == NULL) {
        return;
    }

    LOGGER_DEBUG("Scanning for pre-existing mapped windows", L_NARG);

    for (list_item_td *node = list_head(wm_surfaces(wm));
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        s_adopt_scan_surface(wm, surface);
    }

    xcb_flush(connection);
    LOGGER_DEBUG("Finished scanning for pre-existing windows", L_NARG);
}
