/**
 * @file handler/focus.c
 *
 * @brief X @c PROPERTY_NOTIFY, @c FOCUS_IN, and @c MAPPING_NOTIFY event
 *        handlers
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
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Rules includes */
#include <rules.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <render/desktop.h>
#include <render/outdate.h>
#include <render/surface.h>
#include <render/wmicon.h>
#include <surface.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/mouse.h>

/* Command includes */
#include <cmds/state.h>

/* Project includes */
#include <lookup.h>
#include <wm.h>

/* Local includes */
#include <handler.h>


/**
 * @brief Recompute the work area for every desktop on a surface
 *
 * Iterates over all desktops belonging to the given surface and updates
 * each one's work area based on the surface's current dimensions,
 * accounting for reserved space such as docks or panels.
 *
 * @param surface Pointer to the target surface
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
static void s_handler_refresh_workareas(surface_td *surface)
{
    if (surface == NULL) {
        return;
    }

    for (uint32_t did = 0u; did < surface->desktop_count; ++did) {
        desktop_td *d = surface_desktop_get(surface, did);

        if (d != NULL) {
            desktop_update_workarea(d,
                    surface->properties.dim.w,
                    surface->properties.dim.h);
        }
    }
}


/* Handle a 'PROPERTY_NOTIFY' event */
void handler_property_notify(wm_td *wm, xcb_connection_t *connection,
        list_td *surfaces, xcb_property_notify_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    xcb_ewmh_get_extents_reply_t strut;
    xcb_ewmh_wm_strut_partial_t partial;
    xcb_atom_t wm_window_role = XCB_ATOM_NONE;
    xcb_atom_t motif_hints_atom = XCB_ATOM_NONE;
    xcb_get_property_cookie_t motif_ck;
    xcb_get_property_reply_t *motif_r;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in property handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Property notify event (window=0x%x, atom=%u)",
            event->window, event->atom);

    if (event->state == XCB_PROPERTY_DELETE) {
        return;
    }

    /* A root window's own property changing, not a managed client's:
     * 'lookup_find_client' below would never find one for it, so this
     * has to be checked first, before that early return discards the
     * event.  Recognizes only the specific properties a wallpaper tool
     * might set (see 'desktop_property_is_background_pixmap' in
     * render/desktop.c), rather than invalidating the cached
     * background pixmap on every root property change regardless of
     * which one it was; many of those, including ones icowm's own
     * EWMH state syncing writes to the root window itself, have
     * nothing to do with the background pixmap at all. */
    for (list_item_td *snode = list_head(surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *s = (surface_td *) list_data(snode);

        if (s == NULL || s->screen == NULL ||
                event->window != s->screen->root) {
            continue;
        }
        if (desktop_property_is_background_pixmap(connection,
                    event->atom)) {
            desktop_invalidate_background_pixmap_cache();
            surface_render_current_desktop_repaint(s);
        }
        return;
    }

    desktop = NULL;
    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client == NULL) {
        return;
    }

    /* Re-read 'WM_NORMAL_HINTS' whenever the application updates them.
     * Some applications set their final 'base_size' and 'resize_inc'
     * values after startup (e.g., once the font and UI chrome are
     * initialized), so the hints read at 'MAP_REQUEST' time may already
     * be stale by the time the first keyboard resize is attempted.
     * Keeping the stored size hints current ensures that
     * 's_kb_resize_axis_target' and 'client_constrain_size' compute
     * a target height that lies exactly on the application's current
     * increment grid, preventing the spurious ConfigureRequest that
     * otherwise causes 'RESIZE_UP' to also shrink the bottom edge. */
    if (event->atom == XCB_ATOM_WM_NORMAL_HINTS) {
        client_props_refresh_normal_hints(client);
        return;
    }

    if (event->atom == XCB_ATOM_WM_NAME ||
            (client->ewmh != NULL &&
             event->atom == client->ewmh->_NET_WM_NAME)) {
        client_props_refresh_name(client);

        if (surface != NULL && desktop != NULL) {
            (void) rules_apply(wm, client, &surface, &desktop,
                    RULES_TRIGGER_PROPERTY);
        }

        /* The client's own titlebar text is what actually changed:
         * without marking the client itself outdated too, the render
         * pass's per-client skip check ('client->is_outdated' in
         * 's_desktop_render_one_client', render/desktop.c) means its
         * titlebar keeps showing the old title until some unrelated
         * event (focus change, move, resize...) happens to mark that
         * client outdated for a different reason -- rather than
         * updating the moment this property notify itself arrives. */
        wm_outdate_client(client);
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
        return;
    }

    if (event->atom == XCB_ATOM_WM_ICON_NAME ||
            (client->ewmh != NULL &&
             event->atom == client->ewmh->_NET_WM_ICON_NAME)) {
        client_props_refresh_icon_name(client);
        wm_outdate_client(client);
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
        return;
    }

    /* '_NET_WM_ICON' (the icon image itself, unlike '_NET_WM_ICON_NAME'
     * above, which is only its taskbar label text) has no cached data
     * of its own to refresh here: 'wmicon_draw' (see render/wmicon.h)
     * always reads the property fresh on a cache miss, so all that is
     * needed is throwing away whatever it cached from the property's
     * old value, which would otherwise keep being reused (that is the
     * entire point of the cache) even though it no longer matches what
     * the application just published.  'WM_HINTS' is included here
     * too: 'wmicon_draw' falls back to its own 'icon_pixmap'/
     * 'icon_mask' fields when '_NET_WM_ICON' is absent (see
     * render/wmicon.c), so a client updating those at runtime needs
     * the exact same cache invalidation, even though most of
     * 'WM_HINTS' otherwise unrelated to icons (input model, urgency,
     * window group) is not itself re-read here. */
    if ((client->ewmh != NULL &&
                event->atom == client->ewmh->_NET_WM_ICON) ||
            event->atom == XCB_ATOM_WM_HINTS) {
        wmicon_invalidate(client->connection, &client->icon_pixmap_cache);
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
        return;
    }

    wm_window_role = atom_intern(client->connection, "WM_WINDOW_ROLE",
            true);

    if (event->atom == XCB_ATOM_WM_CLASS ||
            event->atom == wm_window_role) {
        if (event->atom == wm_window_role) {
            client_props_refresh_role(client);
        }
        if (surface != NULL && desktop != NULL &&
                rules_apply(wm, client, &surface, &desktop,
                    RULES_TRIGGER_PROPERTY)) {
            wm_outdate_surface(surface);
            wm_outdate_desktop(desktop);
        }
        return;
    }

    /* Applications that dynamically toggle their own decoration request
     * (e.g., 'Xpad') do so by re-setting '_MOTIF_WM_HINTS' at runtime
     * and rely on the window manager noticing the change; the same
     * freaking de-facto hint 'client_manage' already reads once at
     * initial map time (see there for the field layout), just applied
     * live here whenever it actually changes. */
    motif_hints_atom = atom_intern(client->connection, "_MOTIF_WM_HINTS",
            true);
    if (motif_hints_atom != XCB_ATOM_NONE &&
            event->atom == motif_hints_atom) {
        motif_ck = xcb_get_property(client->connection, 0,
                client->window, motif_hints_atom, motif_hints_atom,
                0, 5);
        motif_r = xcb_get_property_reply(client->connection, motif_ck,
                NULL);
        if (motif_r != NULL) {
            if (motif_r->format == 32 &&
                    xcb_get_property_value_length(motif_r) >=
                        (int) (3u * sizeof(uint32_t))) {
                const uint32_t *motif_vals = (const uint32_t *)
                    xcb_get_property_value(motif_r);
                uint32_t motif_flags = motif_vals[0];
                uint32_t motif_decorations = motif_vals[2];
                bool wants_decorated = client_is_decorated(client);

                if ((motif_flags & 0x2u) != 0u) {
                    if (motif_decorations == 0u) {
                        wants_decorated = false;
                    } else if (client->theme != NULL &&
                            client->theme->window.is_decorated) {
                        wants_decorated = true;
                    }
                }

                if (wants_decorated != client_is_decorated(client)) {
                    LOGGER_DEBUG("'_MOTIF_WM_HINTS' changed for" \
                            " window=0x%x; toggling decoration to" \
                            " decorated=%d", client->window,
                            (int) wants_decorated);
                    wcmd_client_toggle_decoration(client);
                }
            }
            free(motif_r);
        }
        return;
    }

    if (client->ewmh != NULL &&
            (event->atom == client->ewmh->_NET_WM_STRUT_PARTIAL ||
             event->atom == client->ewmh->_NET_WM_STRUT)) {
        memset(&strut, 0, sizeof(strut));
        memset(&partial, 0, sizeof(partial));
        if (xcb_ewmh_get_wm_strut_partial_reply(client->ewmh,
                    xcb_ewmh_get_wm_strut_partial(client->ewmh,
                            client->window),
                    &partial, NULL)) {
            client->layout.strut_partial.sides.left =
                (int32_t) partial.left;
            client->layout.strut_partial.sides.right =
                (int32_t) partial.right;
            client->layout.strut_partial.sides.top =
                (int32_t) partial.top;
            client->layout.strut_partial.sides.bottom =
                (int32_t) partial.bottom;
            client->layout.strut_partial.start.left =
                (int32_t) partial.left_start_y;
            client->layout.strut_partial.start.right =
                (int32_t) partial.right_start_y;
            client->layout.strut_partial.start.top =
                (int32_t) partial.top_start_x;
            client->layout.strut_partial.start.bottom =
                (int32_t) partial.bottom_start_x;
            client->layout.strut_partial.end.left =
                (int32_t) partial.left_end_y;
            client->layout.strut_partial.end.right =
                (int32_t) partial.right_end_y;
            client->layout.strut_partial.end.top =
                (int32_t) partial.top_end_x;
            client->layout.strut_partial.end.bottom =
                (int32_t) partial.bottom_end_x;
        } else if (xcb_ewmh_get_wm_strut_reply(client->ewmh,
                    xcb_ewmh_get_wm_strut(client->ewmh, client->window),
                    &strut, NULL)) {
            client->layout.strut_partial.sides.left =
                (int32_t) strut.left;
            client->layout.strut_partial.sides.right =
                (int32_t) strut.right;
            client->layout.strut_partial.sides.top =
                (int32_t) strut.top;
            client->layout.strut_partial.sides.bottom =
                (int32_t) strut.bottom;
        }

        s_handler_refresh_workareas(surface);

        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
        return;
    }

    if (surface != NULL && desktop != NULL &&
            rules_apply(wm, client, &surface, &desktop,
                RULES_TRIGGER_PROPERTY)) {
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
    }
}


/* Handle a 'FOCUS_IN' event */
void handler_focus_in(xcb_connection_t *connection,
        list_td *surfaces, xcb_focus_in_event_t *event)
{
    (void) connection;
    (void) surfaces;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in focus handler", L_NARG);
        return;
    }

    if (mouse_enter_focus_is_active()) {
        mouse_enter_focus_clear();
    }
}


/* Handle a 'MAPPING_NOTIFY' event */
void handler_mapping_notify(xcb_key_symbols_t *keysyms,
        list_td *surfaces, xcb_mapping_notify_event_t *event,
        const config_td *cfg)
{
    xcb_connection_t *connection = NULL;

    if (keysyms == NULL || event == NULL || cfg == NULL) {
        LOGGER_ERROR("Received null pointer in mapping notify handler",
                L_NARG);
        return;
    }

    if (event->request == XCB_MAPPING_POINTER) {
        return;
    }

    LOGGER_TRACE("Mapping notify (request=%u); refreshing grabs",
            (unsigned int) event->request);

    /* Obtain the XCB connection from the first surface */
    if (surfaces != NULL) {
        list_item_td *head = list_head(surfaces);
        if (head != NULL) {
            surface_td *first = (surface_td *) list_data(head);
            if (first != NULL) {
                connection = first->connection;
            }
        }
    }

    xcb_refresh_keyboard_mapping(keysyms, event);

    if (connection != NULL && surfaces != NULL) {
        for (list_item_td *node = list_head(surfaces); node != NULL;
                node = list_next(node)) {
            surface_td *surface = (surface_td *) list_data(node);
            if (surface == NULL || surface->screen == NULL) {
                continue;
            }
            xcb_ungrab_key(connection,
                    (xcb_keycode_t) XCB_GRAB_ANY,
                    surface->screen->root,
                    (uint16_t) XCB_MOD_MASK_ANY);
        }

        xcb_flush(connection);
    }

    keyboard_load(surfaces, keysyms, cfg);

    if (event->request == XCB_MAPPING_MODIFIER &&
            connection != NULL && surfaces != NULL) {
        for (list_item_td *node = list_head(surfaces); node != NULL;
                node = list_next(node)) {
            surface_td *surface = (surface_td *) list_data(node);
            if (surface == NULL || surface->screen == NULL) {
                continue;
            }
            xcb_ungrab_button(connection,
                    (uint8_t) XCB_BUTTON_INDEX_ANY,
                    surface->screen->root,
                    (uint16_t) XCB_MOD_MASK_ANY);
        }

        xcb_flush(connection);
        mouse_load(surfaces, cfg);
    }
}
