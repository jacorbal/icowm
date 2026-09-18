/**
 * @file handler/focus.c
 *
 * @brief X @c PROPERTY_NOTIFY, @c FOCUS_IN, @c FOCUS_OUT, and
 *        @c MAPPING_NOTIFY event handlers
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
#include <stdlib.h>     /* free, NULL */
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
#include <utils/xcb/connection.h>

/* Command includes */
#include <cmds/client/state.h>

/* Policy includes */
#include <policy/focus.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/mouse/bind.h>
#include <input/mouse/event.h>

/* Render includes */
#include <render/desktop/background.h>
#include <render/outdate.h>
#include <render/stage.h>
#include <render/viewport/mesh.h>
#include <render/wmicon.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <stage/workarea.h>
#include <systray.h>
#include <systray/handle.h>
#include <wm.h>

/* Local includes */
#include <handler.h>
#include <handler/focus.h>


/** Deepest window nesting walked looking for a client's own window */
#define S_FOCUS_IN_MAX_DEPTH (16u)


/**
 * @brief Whether the X input focus is still on @p window or inside it
 *
 * A @c FocusIn is only news if it still describes the server's focus:
 * focus given to one window and at once to another yields a
 * @c FocusIn for the first after the window manager already made the
 * second active, and following that one would undo it.  Asked only
 * when a @c FocusIn names a client other than the active one, which
 * is rare, so the round trips it costs are not on any common path.
 *
 * @param connection XCB connection
 * @param window     Client window the @c FocusIn was for
 *
 * @return @c true if the focus is @p window or one of its descendants
 *
 * @note Complexity: @e O(d) round trips, where @e d is how deep inside
 *       @p window the focus is, at most @c S_FOCUS_IN_MAX_DEPTH
 */
static bool s_focus_in_is_current(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_get_input_focus_reply_t *focus_reply;
    xcb_window_t current;

    focus_reply = xcb_get_input_focus_reply(connection,
            xcb_get_input_focus(connection), NULL);
    if (focus_reply == NULL) {
        return false;
    }
    current = focus_reply->focus;
    free(focus_reply);

    for (uint32_t depth = 0u; depth < S_FOCUS_IN_MAX_DEPTH &&
            current != XCB_WINDOW_NONE &&
            current != (xcb_window_t) XCB_INPUT_FOCUS_POINTER_ROOT;
            ++depth) {
        xcb_query_tree_reply_t *tree_reply;

        if (current == window) {
            return true;
        }

        tree_reply = xcb_query_tree_reply(connection,
                xcb_query_tree(connection, current), NULL);
        if (tree_reply == NULL) {
            return false;
        }
        current = (tree_reply->parent == tree_reply->root)
            ? XCB_WINDOW_NONE : tree_reply->parent;
        free(tree_reply);
    }

    return false;
}


/* Handle a 'PROPERTY_NOTIFY' event */
void handler_property_notify(const wm_td *wm,
        xcb_connection_t *connection,
        list_td *stages, xcb_property_notify_event_t *event)
{
    client_td *client;
    stage_td *stage;
    desktop_td *desktop;
    xcb_ewmh_get_extents_reply_t strut;
    xcb_ewmh_wm_strut_partial_t partial;
    xcb_atom_t wm_window_role = XCB_ATOM_NONE;
    xcb_atom_t motif_hints_atom = XCB_ATOM_NONE;
    xcb_atom_t colormap_windows_atom = XCB_ATOM_NONE;
    xcb_get_property_cookie_t motif_ck;
    xcb_ewmh_connection_t *const ewmh = xcb_ewmh_connection_get();

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

    /* A root window's property changing, not a managed client's:
     * 'lookup_find_client' below would never find one for it, so this
     * has to be checked first, before that early return discards the
     * event.  Recognizes only the specific properties a wallpaper tool
     * might set (see 'render_desktop_background_property_is_pixmap' in
     * render/desktop/background.c), rather than invalidating the cached
     * background pixmap on every root property change regardless of
     * which one it was; many of those, including ones icowm's EWMH
     * state syncing writes to the root window itself,
     * have nothing to do with the background pixmap at all. */
    for (list_item_td *snode = list_head(stages); snode != NULL;
            snode = list_next(snode)) {
        stage_td *const s = (stage_td *) list_data(snode);

        if (s == NULL || s->screen == NULL ||
                event->window != s->screen->root) {
            continue;
        }
        if (render_desktop_background_property_is_pixmap(connection,
                    event->atom)) {
            render_desktop_background_cache_invalidate();
            /* An external tool just took the root window over, or just
             * let go of it; either way whichever mesh tile is cached no
             * longer describes what belongs on screen */
            viewport_mesh_cache_invalidate();
            stage_render_current_desktop_repaint(s);
        }
        return;
    }

    /* Before the managed-client lookup below, and unconditionally, for
     * the same reason 'systray_handle_destroy' is in
     * 'handler_window_destroy_notify': a docked systray icon is never
     * a managed client, so that lookup would never find one for it.
     * The property actually changes on the icon's own window, not on
     * the tray window, since 'systray_protocol_dock' sets
     * 'XCB_EVENT_MASK_PROPERTY_CHANGE' on the icon itself rather than
     * on 's_tray.window'; checking 'systray_owns_window' here, as the
     * client-message and expose handlers correctly do for events
     * addressed to the tray window itself, would never match.
     * 'systray_handle_property_notify' is a no-op for any window that
     * is not currently docked. */
    systray_handle_property_notify(wm, event);

    desktop = NULL;
    client = lookup_find_client(stages, event->window,
            &stage, &desktop);
    if (client == NULL) {
        return;
    }

    /* Re-read 'WM_NORMAL_HINTS' whenever the application updates them.
     * Some applications set their final 'base_size' and 'resize_inc'
     * values after startup (e.g., once the font and UI chrome are
     * initialized), so the hints read at 'MAP_REQUEST' time may already
     * be stale by the time the first keyboard resize is attempted.
     * Keeping the stored size hints current ensures that
     * 's_kb_resize_axis_target' and 'client_size_constrain' compute
     * a target height that lies exactly on the application's current
     * increment grid, preventing the spurious ConfigureRequest that
     * otherwise causes 'RESIZE_UP' to also shrink the bottom edge. */
    if (event->atom == XCB_ATOM_WM_NORMAL_HINTS) {
        client_props_refresh_normal_hints(client);
        return;
    }

    if (event->atom == XCB_ATOM_WM_NAME ||
            (ewmh != NULL &&
             event->atom == ewmh->_NET_WM_NAME)) {
        const bool name_changed = client_props_refresh_name(client);
        bool rule_acted = false;

        if (stage != NULL && desktop != NULL) {
            rule_acted = rules_apply(wm, client, &stage, &desktop,
                    RULES_TRIGGER_PROPERTY);
        }

        /* Either reason is enough on its own.  Without the first,
         * a client rewriting the very same title, which some do every
         * few seconds, has its whole titlebar cleared and drawn again
         * to arrive at the text already on it, and the clear is seen;
         * without the second, a rule that moved the client on this very
         * notify would leave the screen showing where it used to be. */
        if (!name_changed && !rule_acted) {
            return;
        }

        /* The client's titlebar text is what actually changed: without
         * marking the client itself outdated too, the render pass's
         * per-client skip check ('client->is_outdated' in
         * 's_desktop_render_one_client', render/desktop.c) means its
         * titlebar keeps showing the old title until some unrelated
         * event (focus change, move, resize...) happens to mark that
         * client outdated for a different reason, rather than updating
         * the moment this property notify itself arrives. */
        wm_outdate_client(client);
        wm_outdate_stage(stage);
        wm_outdate_desktop(desktop);
        return;
    }

    if (event->atom == XCB_ATOM_WM_ICON_NAME ||
            (ewmh != NULL &&
             event->atom == ewmh->_NET_WM_ICON_NAME)) {
        /* Rewritten in the same breath as the title by the clients that
         * rewrite it at all, so it needs the same guard: an icon name
         * that has not moved is nothing to redraw for */
        if (!client_props_refresh_icon_name(client)) {
            return;
        }

        wm_outdate_client(client);
        wm_outdate_stage(stage);
        wm_outdate_desktop(desktop);
        return;
    }

    /* '_NET_WM_ICON' (the icon image itself, unlike '_NET_WM_ICON_NAME'
     * above, which is only its taskbar label text) has no cached data
     * of its own to refresh here: 'wmicon_draw' (see 'render/wmicon.h')
     * always reads the property fresh on a cache miss, so all that is
     * needed is throwing away whatever it cached from the property's
     * old value, which would otherwise keep being reused (that is the
     * entire point of the cache) even though it no longer matches what
     * the application just published.  'WM_HINTS' is included here too:
     * 'wmicon_draw' falls back to its 'icon_pixmap'/ 'icon_mask' fields
     * when '_NET_WM_ICON' is absent (see 'render/wmicon.c'), so
     * a client updating those at runtime needs the exact same cache
     * invalidation, even though most of 'WM_HINTS' otherwise unrelated
     * to icons (input model, urgency, window group) is not itself
     * re-read here. */
    if ((ewmh != NULL &&
                event->atom == ewmh->_NET_WM_ICON) ||
            event->atom == XCB_ATOM_WM_HINTS) {
        wmicon_invalidate(xcb_connection_get(), &client->icon_pixmap_cache);
        wm_outdate_client(client);
        wm_outdate_stage(stage);
        wm_outdate_desktop(desktop);
        return;
    }

    wm_window_role = atom_intern(xcb_connection_get(),
            "WM_WINDOW_ROLE", true);

    if (event->atom == XCB_ATOM_WM_CLASS ||
            event->atom == wm_window_role) {
        if (event->atom == wm_window_role) {
            client_props_refresh_role(client);
        }
        if (stage != NULL && desktop != NULL &&
                rules_apply(wm, client, &stage, &desktop,
                    RULES_TRIGGER_PROPERTY)) {
            wm_outdate_client(client);
            wm_outdate_stage(stage);
            wm_outdate_desktop(desktop);
        }
        return;
    }

    /* Applications that dynamically toggle their decoration request
     * (e.g., Xpad) do so by re-setting '_MOTIF_WM_HINTS' at runtime and
     * rely on the window manager noticing the change; the same freaking
     * de-facto hint 'client_init' already reads once at initial map
     * time (see there for the field layout), just applied live here
     * whenever it actually changes. */
    motif_hints_atom = atom_intern(xcb_connection_get(),
            "_MOTIF_WM_HINTS", true);
    if (motif_hints_atom != XCB_ATOM_NONE &&
            event->atom == motif_hints_atom) {
        xcb_get_property_reply_t *motif_r;

        motif_ck = xcb_get_property(xcb_connection_get(), 0,
                client->window, motif_hints_atom, motif_hints_atom,
                0, 5);
        motif_r = xcb_get_property_reply(xcb_connection_get(), motif_ck,
                NULL);
        if (motif_r != NULL) {
            if (motif_r->format == 32 &&
                    xcb_get_property_value_length(motif_r) >=
                        (int) (3u * sizeof(uint32_t))) {
                const uint32_t *motif_vals = (const uint32_t *)
                    xcb_get_property_value(motif_r);
                uint32_t motif_flags = motif_vals[0];
                bool wants_decorated = client_is_decorated(client);

                if ((motif_flags & 0x2u) != 0u) {
                    uint32_t motif_decorations = motif_vals[2];

                    if (motif_decorations == 0u) {
                        wants_decorated = false;
                    } else if (client->config != NULL &&
                            client->config->theme.window.is_decorated) {
                        wants_decorated = true;
                    }
                }

                if (wants_decorated != client_is_decorated(client)) {
                    LOGGER_DEBUG("'_MOTIF_WM_HINTS' changed for" \
                            " window=0x%x; toggling decoration to" \
                            " decorated=%d", client->window,
                            (int) wants_decorated);
                    ccmd_client_toggle_decorate(client);
                }
            }
            free(motif_r);
        }
        return;
    }

    if (ewmh != NULL &&
            (event->atom ==
                 ewmh->_NET_WM_STRUT_PARTIAL ||
             event->atom == ewmh->_NET_WM_STRUT)) {
        memset(&strut, 0, sizeof(strut));
        memset(&partial, 0, sizeof(partial));
        if (xcb_ewmh_get_wm_strut_partial_reply(ewmh,
                    xcb_ewmh_get_wm_strut_partial(ewmh,
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
        } else if (xcb_ewmh_get_wm_strut_reply(ewmh,
                    xcb_ewmh_get_wm_strut(ewmh,
                            client->window),
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

        stage_workarea_refresh_all(stage);

        wm_outdate_stage(stage);
        wm_outdate_desktop(desktop);
        return;
    }

    /* ICCCM §4.1.8: the client changed its priority list of subwindows
     * wanting their colormap installed on colormap focus; re-read it
     * and re-subscribe 'ColormapChangeMask' on whichever set it names
     * now (a window dropped from the list keeps whatever mask it
     * already had, since nothing else in this
     * project relies on it being cleared again afterward). */
    colormap_windows_atom = atom_intern(xcb_connection_get(),
            "WM_COLORMAP_WINDOWS", true);
    if (colormap_windows_atom != XCB_ATOM_NONE &&
            event->atom == colormap_windows_atom) {
        client_props_refresh_colormap_windows(client);
        client_subscribe_colormap_windows(xcb_connection_get(), client);
        return;
    }

    if (stage != NULL && desktop != NULL &&
            rules_apply(wm, client, &stage, &desktop,
                RULES_TRIGGER_PROPERTY)) {
        wm_outdate_client(client);
        wm_outdate_stage(stage);
        wm_outdate_desktop(desktop);
    }
}


/* Handle a 'FOCUS_IN' event */
void handler_focus_in(xcb_connection_t *connection,
        list_td *stages, const xcb_focus_in_event_t *event)
{
    client_td *client;
    stage_td *stage = NULL;
    desktop_td *desktop = NULL;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in focus handler", L_NARG);
        return;
    }

    if (mouse_enter_focus_is_active()) {
        mouse_enter_focus_clear();
    }

    /* Only a real move of the focus: the ones a keyboard grab starting
     * or ending produces, the cycle menu's among them, say nothing
     * about where it now is, and one for the window under the pointer
     * is the server following the pointer while the focus itself sits
     * on the root */
    if ((event->mode != XCB_NOTIFY_MODE_NORMAL &&
                event->mode != XCB_NOTIFY_MODE_WHILE_GRABBED) ||
            event->detail == XCB_NOTIFY_DETAIL_POINTER ||
            event->detail == XCB_NOTIFY_DETAIL_POINTER_ROOT ||
            event->detail == XCB_NOTIFY_DETAIL_NONE) {
        return;
    }

    /* Only a client's own window, never its frame or titlebar, and only
     * one the window manager does not already count as active, which
     * is every focus it gave itself */
    client = lookup_find_client(stages, event->event, &stage, &desktop);
    if (client == NULL || client->window != event->event ||
            stage == NULL || desktop == NULL ||
            desktop->client_active_id == client->id ||
            !s_focus_in_is_current(connection, client->window)) {
        return;
    }

    focus_adopt(stages, stage, desktop, client);
}


/* Handle a 'FOCUS_OUT' event */
void handler_focus_out(const wm_td *wm, xcb_focus_out_event_t *event)
{
    stage_td *stage = NULL;

    if (wm == NULL || event == NULL) {
        return;
    }

    if ((event->mode == XCB_NOTIFY_MODE_NORMAL ||
                event->mode == XCB_NOTIFY_MODE_WHILE_GRABBED) &&
            lookup_find_client(wm_stages(wm), event->event,
                    &stage, NULL) != NULL &&
            stage != NULL) {
        stage->is_outdated = true;
    }
}


/* Handle a 'MAPPING_NOTIFY' event */
void handler_mapping_notify(xcb_key_symbols_t *keysyms,
        list_td *stages, xcb_mapping_notify_event_t *event,
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

    connection = xcb_connection_get();

    xcb_refresh_keyboard_mapping(keysyms, event);

    if (connection != NULL && stages != NULL) {
        for (list_item_td *node = list_head(stages); node != NULL;
                node = list_next(node)) {
            stage_td *stage = (stage_td *) list_data(node);
            if (stage == NULL || stage->screen == NULL) {
                continue;
            }
            xcb_ungrab_key(connection,
                    (xcb_keycode_t) XCB_GRAB_ANY,
                    stage->screen->root,
                    (uint16_t) XCB_MOD_MASK_ANY);
        }

    }

    keyboard_load(stages, keysyms, cfg);

    if (event->request == XCB_MAPPING_MODIFIER &&
            connection != NULL && stages != NULL) {
        for (list_item_td *node = list_head(stages); node != NULL;
                node = list_next(node)) {
            stage_td *stage = (stage_td *) list_data(node);
            if (stage == NULL || stage->screen == NULL) {
                continue;
            }
            xcb_ungrab_button(connection,
                    (uint8_t) XCB_BUTTON_INDEX_ANY,
                    stage->screen->root,
                    (uint16_t) XCB_MOD_MASK_ANY);
        }

        mouse_load(stages, cfg);
    }
}
