/**
 * @file client.c
 *
 * @brief Client structure management
 *
 * Owns allocation, initialization, adoption (manage), update and
 * destruction of @c client_td instances.  X11 property reading lives in
 * @c client/props.c; geometry and decoration helpers in
 * @c client/geom.c; outgoing event senders in @c client/event.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* NULL, free, calloc */
#include <string.h>     /* memcpy, memset, snprintf */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/sync.h>

/* Utils includes */
#include <utils/safe/safemem.h>
#include <utils/safe/safestr.h>
#include <utils/safe/safeflg.h>
#include <utils/xcb/atom.h>

/* Type includes */
#include <types/pair.h>

/* Command includes */
#include <cmds/client/basic.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/client.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <logger.h>
#include <render/wmicon.h>
#include <scratchpad.h>
#include <wm.h>

/* Input includes */
#include <input/mouse.h>

/* Local includes */
#include <client/internal.h>


/**
 * @brief Free every heap-owned client field
 *
 * @param client Client whose owned buffers should be released
 */
static void s_client_heap_fields_release(client_td *client)
{
    if (client == NULL) {
        return;
    }

    safe_free_var((void **) &client->info.name,
            (void **) &client->info.visible_name,
            (void **) &client->info.role_name,
            (void **) &client->info.class_name[0],
            (void **) &client->info.class_name[1],
            (void **) &client->icon_info.icon_name,
            (void **) &client->icon_info.visible_icon_name,
            (void **) &client->icon_info.icons,
            (void **) &client->process.command,
            SAFE_FREE_VAR_END);
}


/**
 * @brief Initialize the common non-zero client defaults
 *
 * @param client        Client structure to initialize
 * @param connection    XCB connection
 * @param ewmh          EWMH connection
 * @param theme         Theme configuration
 * @param config_base   Base configuration
 * @param a11y          Accessibility (a11y) configuration
 */
static void s_client_init_common(client_td *client,
        xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        struct config_theme_s *theme,
        const struct config_base_s *config_base,
        const struct config_a11y_s *a11y)
{
    if (client == NULL) {
        return;
    }

    client->connection = connection;
    client->ewmh = ewmh;
    client->theme = theme;
    client->config_base = config_base;
    client->a11y = a11y;
    client->process.pid = -1;
    client->wm_input_hint = true;
    client->icon_x = -1;
    client->icon_y = -1;
    client->layout.gravity =
        (uint16_t) ((config_base != NULL)
                ? config_base->windows.gravity
                : CONFIG_GRAVITY_NORTH_WEST);
    client->properties.flags =
        CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_RESIZABLE;
    client->properties.type = CLIENT_TYPE_NORMAL;
    client->properties.state = CLIENT_STATE_NORMAL;
    client->properties.layer = CLIENT_LAYER_NORMAL;
    client->properties.operation = CLIENT_OPERATION_IDLE;
    client->properties.focusing = CLIENT_FOCUSING_UNFOCUSED;
    /* Force the render pass to apply the real border width at least
     * once, regardless of what that value turns out to be
     * (vid. 'ri_render_client' in 'render/desktop.c') */
    client->last_border_width = UINT32_MAX;
    ci_set_decoration_defaults(client, theme);
}


/**
 * @brief Duplicate a client title into both visible-name buffers
 *
 * @param client Client to update
 * @param name   Source title string
 */
static void s_client_display_name_set(client_td *client,
        const char *name)
{
    if (client == NULL || name == NULL || name[0] == '\0') {
        return;
    }

    safe_strncpy(client->info.name, name, CONFIG_MAX_LENGTH_NAME - 1);
    safe_strncpy(client->info.visible_name, name,
            CONFIG_MAX_LENGTH_NAME - 1);
}




/* Destroy the specified client and free associated resources */
void client_destroy(client_td *client)
{
    if (client == NULL) {
        return;
    }

    scratchpad_notice_client_destroyed(client);

    LOGGER_DEBUG("Destroying client %p (window %#x, name '%s')",
            (void *) client, client->window, client->info.name);

    /* Destroy the XCB window representation and flush the output buffer
     * to ensure the request is processed */
    if (client->connection != NULL && client->window != 0) {
        xcb_destroy_window(client->connection, client->window);
        xcb_flush(client->connection);
    }

    /* Release the '_NET_WM_SYNC_REQUEST' alarm, if any: it is
     * a server-side resource owned by the window manager's own
     * connection (unlike the counter it watches, which belongs to the
     * client and is not ours to destroy), so it is not freed
     * automatically when the client window above is destroyed */
    if (client->connection != NULL && client->sync_alarm != 0u) {
        xcb_sync_destroy_alarm(client->connection,
                (xcb_sync_alarm_t) client->sync_alarm);
    }

    /* Destroy decorations if any */
    if (client->connection != NULL && client->titlebar != 0) {
        xcb_destroy_window(client->connection, client->titlebar);
    }
    if (client->connection != NULL && client->icon_window != 0) {
        xcb_destroy_window(client->connection, client->icon_window);
    }
    /* Frees the cached '_NET_WM_ICON' Picture built by 'wmicon_draw'
     * (see render/wmicon.h), if any; a no-op if nothing was ever
     * cached, e.g., a client that never had 'theme.icon.show-pixmaps'
     * draw anything for it in the first place */
    wmicon_invalidate(client->connection, &client->icon_pixmap_cache);
    if (client->connection != NULL && client->frame != 0) {
        xcb_destroy_window(client->connection, client->frame);
    }

    /* Free all allocated string buffers */
    s_client_heap_fields_release(client);

    /* Free the client structure itself */
    free(client);
}


/* Refresh a client's own user-time from a genuine input event that
 * just reached it */
void client_update_user_time(client_td *client, uint32_t time)
{
    if (client == NULL) {
        return;
    }

    if (client_user_time_is_newer(time, client->user_time)) {
        client->user_time = time;
    }
}


/* Apply a client's own themed border color and width to its own
 * window, honoring 'border_override' when set */
void client_border_apply(client_td *client, bool use_active_style)
{
    uint32_t color;
    uint32_t width;
    uint8_t opacity_percent;

    if (client == NULL || client->connection == NULL ||
            client->theme == NULL || client_is_fullscreen(client) ||
            (client_is_decorated(client) && client->frame != 0)) {
        return;
    }

    if (client->border_override.is_set) {
        color = client->border_override.color;
        width = client->border_override.width;
    } else if (use_active_style) {
        color = client->theme->window.active.border.color;
        width = client->theme->window.active.border.width;
    } else {
        color = client->theme->window.inactive.border.color;
        width = client->theme->window.inactive.border.width;
    }

    if (use_active_style) {
        opacity_percent = (client->opacity_override.is_set_active)
            ? client->opacity_override.active
            : client->theme->window.active.opacity;
    } else {
        opacity_percent = (client->opacity_override.is_set_inactive)
            ? client->opacity_override.inactive
            : client->theme->window.inactive.opacity;
    }

    /* Accessibility: never let the focus indicator go thinner than
     * 'a11y.focus-indicator.min-border-width', regardless of
     * what the theme itself specifies */
    if (client->a11y != NULL &&
            width < client->a11y->focus_indicator
                .min_border_width) {
        width = client->a11y->focus_indicator.min_border_width;
    }

    xcb_change_window_attributes(client->connection, client->window,
            XCB_CW_BORDER_PIXEL, &color);
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH,
            (const uint32_t[]) { width });
    atom_set_window_opacity(client->connection, client->window,
            config_theme_opacity_to_raw(opacity_percent));
}


/**
 * @brief Read 'WM_PROTOCOLS' and set up '_NET_WM_SYNC_REQUEST' support
 *
 * Interns 'WM_DELETE_WINDOW', 'WM_TAKE_FOCUS', and '_NET_WM_PING',
 * caches which of those (plus '_NET_WM_SYNC_REQUEST') the window
 * advertises support for, and, when '_NET_WM_SYNC_REQUEST' is both
 * advertised and the XSync extension is available, reads the
 * client-set counter and creates the alarm watching it (see
 * 'ccmd_client_resize' and 'handler_sync_event' for how that alarm is
 * consumed later).
 *
 * @param connection XCB connection
 * @param ewmh       EWMH connection, for 'WM_PROTOCOLS' and
 *                   '_NET_WM_SYNC_REQUEST_COUNTER'
 * @param window     Window being adopted
 * @param client     Client being initialized; its protocol-support
 *                   flags, 'sync_counter', and 'sync_alarm' fields
 *                   are set here
 *
 * @note Complexity: @e O(n), where @e n is the number of protocols
 *       'WM_PROTOCOLS' advertises
 */
static void s_client_read_wm_protocols(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        client_td *client)
{
    xcb_atom_t wm_delete_atom;
    xcb_atom_t wm_take_focus_atom;
    xcb_atom_t net_wm_ping_atom;
    xcb_icccm_get_wm_protocols_reply_t proto;

    /* Read 'WM_PROTOCOLS': cache 'WM_DELETE_WINDOW', 'WM_TAKE_FOCUS',
     * and '_NET_WM_PING' support */
    wm_delete_atom = atom_intern(connection, "WM_DELETE_WINDOW", true);
    wm_take_focus_atom = atom_intern(connection, "WM_TAKE_FOCUS", true);
    net_wm_ping_atom = atom_intern(connection, "_NET_WM_PING", true);

    client->wm_delete_atom = wm_delete_atom;
    client->has_wm_delete_window = false;
    client->wm_take_focus_atom = wm_take_focus_atom;
    client->has_wm_take_focus = false;
    client->has_net_wm_ping = false;
    client->has_net_wm_sync_request = false;

    memset(&proto, 0, sizeof(proto));
    if (xcb_icccm_get_wm_protocols_reply(connection,
                xcb_icccm_get_wm_protocols(connection, window,
                    ewmh->WM_PROTOCOLS),
                &proto, NULL)) {
        for (uint32_t pi = 0; pi < proto.atoms_len; ++pi) {
            if (proto.atoms[pi] == wm_delete_atom) {
                client->has_wm_delete_window = true;
            } else if (proto.atoms[pi] == wm_take_focus_atom) {
                client->has_wm_take_focus = true;
            } else if (proto.atoms[pi] == net_wm_ping_atom) {
                client->has_net_wm_ping = true;
            } else if (proto.atoms[pi] == ewmh->_NET_WM_SYNC_REQUEST) {
                client->has_net_wm_sync_request = true;
            }
        }
        xcb_icccm_get_wm_protocols_reply_wipe(&proto);
    }

    /* '_NET_WM_SYNC_REQUEST': per the EWMH protocol, the CLIENT (not
     * the window manager) creates the XSync counter and advertises its
     * XID via the '_NET_WM_SYNC_REQUEST_COUNTER' property on its own
     * window; the window manager only reads that property and creates
     * an alarm watching the client's counter for positive transitions,
     * so it is notified ('AlarmNotify') whenever the client advances it
     * after finishing a redraw (see 'ccmd_client_resize' and
     * 'handler_sync_event').  These are unchecked requests, matching
     * the rest of this function, so an unsupported/misbehaving client
     * or server at worst leaves 'has_net_wm_sync_request' effectively
     * unusable, not a crash. */
    client->sync_counter = 0u;
    client->sync_alarm = 0u;
    if (client->has_net_wm_sync_request && wm_sync_is_available()) {
        xcb_get_property_cookie_t counter_cookie;
        xcb_get_property_reply_t *counter_reply;

        counter_cookie = xcb_get_property(connection, 0, window,
                ewmh->_NET_WM_SYNC_REQUEST_COUNTER, XCB_ATOM_CARDINAL,
                0, 1);
        counter_reply = xcb_get_property_reply(connection,
                counter_cookie, NULL);
        if (counter_reply != NULL) {
            if (counter_reply->format == 32 &&
                    xcb_get_property_value_length(counter_reply) >=
                        (int) sizeof(uint32_t)) {
                client->sync_counter = *(uint32_t *)
                    xcb_get_property_value(counter_reply);
            }
            free(counter_reply);
        }

        if (client->sync_counter != 0u) {
            uint32_t alarm_values[7];

            client->sync_alarm = xcb_generate_id(connection);
            /* Per the XSync value-list order (ascending 'CA_*' bit pos.):
             * 'COUNTER', 'VALUE_TYPE', 'VALUE', 'TEST_TYPE', 'DELTA'.
             * 'VALUE' and 'DELTA' are each a 64-bit 'INT64' (hi-word,
             * then lo word), not a single 'CARD32'.  Omitting 'VALUE'
             * entirely and treating 'DELTA' as one word (an earlier
             * version of this code did both) leaves the value-list
             * shorter than what the request's own mask calls for, which
             * the server rejects; the alarm XID above then never
             * actually exists server-side, so it can never fire, and
             * every resize silently falls back to only ever applying
             * once every 'WM_SYNC_MAX_WAIT_TICKS' attempts instead of
             * being acknowledged promptly. */
            alarm_values[0] = client->sync_counter;         /* COUNTER */
            alarm_values[1] = (uint32_t) XCB_SYNC_VALUETYPE_RELATIVE;
                                                            /* VALUE_TYPE */
            alarm_values[2] = 0u;                           /* VALUE.hi */
            alarm_values[3] = 0u;                           /* VALUE.lo */
            alarm_values[4] =
                (uint32_t) XCB_SYNC_TESTTYPE_POSITIVE_TRANSITION;
                                                            /* TEST_TYPE */
            alarm_values[5] = 0u;                           /* DELTA.hi */
            alarm_values[6] = 1u;                           /* DELTA.lo */
            xcb_sync_create_alarm(connection,
                    (xcb_sync_alarm_t) client->sync_alarm,
                    (uint32_t) (XCB_SYNC_CA_COUNTER |
                            XCB_SYNC_CA_VALUE_TYPE |
                            XCB_SYNC_CA_VALUE |
                            XCB_SYNC_CA_TEST_TYPE |
                            XCB_SYNC_CA_DELTA),
                    alarm_values);
            LOGGER_DEBUG("Enabled '_NET_WM_SYNC_REQUEST' for" \
                    " window=0x%x (counter=0x%x, alarm=0x%x)", window,
                    client->sync_counter, client->sync_alarm);
        } else {
            /* Client advertised the protocol but never actually set
             * its counter property; treat it as unsupported rather
             * than sending requests nobody will ever answer */
            LOGGER_DEBUG("window=0x%x advertised" \
                    " '_NET_WM_SYNC_REQUEST' but never set its" \
                    " counter property; treating it as unsupported",
                    window);
            client->has_net_wm_sync_request = false;
        }
    }
}


/**
 * @brief Read 'WM_HINTS', 'WM_CLIENT_LEADER', and 'WM_TRANSIENT_FOR'
 *
 * @c WM_HINTS supplies the input model, initial iconic state, window
 * group, and urgency; @c WM_CLIENT_LEADER (ICCCM §5.1) and
 * @c WM_HINTS' own window group together let @c client_group_leader
 * and @c place_apply cluster windows belonging to the same
 * application; @c WM_TRANSIENT_FOR identifies dialogs and their
 * parent.
 *
 * @param connection XCB connection
 * @param window     Window being adopted
 * @param client     Client being initialized; every field these three
 *                   properties feed is set here
 *
 * @note Complexity: @e O(1)
 */
static void s_client_read_wm_hints_and_leader(xcb_connection_t *connection,
        xcb_window_t window, client_td *client)
{
    xcb_atom_t client_leader_atom;
    xcb_get_property_cookie_t hints_cookie;
    xcb_get_property_cookie_t client_leader_cookie;
    xcb_get_property_cookie_t transient_cookie;
    xcb_icccm_wm_hints_t wm_hints;
    xcb_window_t transient = XCB_WINDOW_NONE;

    /* None of the three properties below depends on either of the
     * other two, so every request is sent up front, before any reply
     * is awaited; this leaves the server free to work on all three at
     * once instead of only ever seeing the next one after this
     * process has already finished handling the previous reply.
     * 'atom_intern' below is itself cached (see 'utils/xcb/atom.c'),
     * so this holds exactly on every call after the very first one in
     * a session; only that first, cold-cache call briefly blocks
     * between sending the first and third request here, resolving
     * 'WM_CLIENT_LEADER' once for the rest of the session's own
     * lifetime. */
    hints_cookie = xcb_icccm_get_wm_hints(connection, window);

    client_leader_atom = atom_intern(connection, "WM_CLIENT_LEADER", true);
    client_leader_cookie = (client_leader_atom != XCB_ATOM_NONE)
        ? xcb_get_property(connection, 0, window,
                client_leader_atom, XCB_ATOM_WINDOW, 0, 1)
        : (xcb_get_property_cookie_t) { 0 };

    transient_cookie = xcb_icccm_get_wm_transient_for(connection, window);

    /* Read 'WM_HINTS': input model and window group */
    memset(&wm_hints, 0, sizeof(wm_hints));
    if (xcb_icccm_get_wm_hints_reply(connection, hints_cookie,
                &wm_hints, NULL)) {
        if (wm_hints.flags & XCB_ICCCM_WM_HINT_INPUT) {
            client->wm_input_hint = (wm_hints.input != 0);
        }
        if (wm_hints.flags & XCB_ICCCM_WM_HINT_STATE &&
                wm_hints.initial_state == XCB_ICCCM_WM_STATE_ICONIC) {
            client->initial_iconic = true;
        }
        if (wm_hints.flags & XCB_ICCCM_WM_HINT_WINDOW_GROUP) {
            client->group_leader = wm_hints.window_group;
        }
        if (wm_hints.flags & XCB_ICCCM_WM_HINT_X_URGENCY) {
            client_urge(client);
        }
    }

    /* Read 'WM_CLIENT_LEADER': ICCCM §5.1 property used, together with
     * the 'WM_HINTS' window group above, to cluster windows belonging
     * to the same application for placement (see 'client_group_leader'
     * and 'place_apply') */
    client->client_leader = XCB_WINDOW_NONE;
    if (client_leader_atom != XCB_ATOM_NONE) {
        xcb_get_property_reply_t *client_leader_reply;

        client_leader_reply = xcb_get_property_reply(connection,
                client_leader_cookie, NULL);
        if (client_leader_reply != NULL) {
            if (client_leader_reply->type == XCB_ATOM_WINDOW &&
                    client_leader_reply->format == 32 &&
                    xcb_get_property_value_length(client_leader_reply) >=
                        (int) sizeof(xcb_window_t)) {
                client->client_leader = *(xcb_window_t *)
                    xcb_get_property_value(client_leader_reply);
            }
            free(client_leader_reply);
        }
    }

    /* Read 'WM_TRANSIENT_FOR': identify dialogs and their parent */
    client->transient_for = XCB_WINDOW_NONE;
    if (xcb_icccm_get_wm_transient_for_reply(connection, transient_cookie,
                &transient, NULL)) {
        client->transient_for = transient;
    }
}


/**
 * @brief Read '_NET_WM_STRUT_PARTIAL', falling back to legacy
 *        '_NET_WM_STRUT', for dock/panel windows
 *
 * @c _NET_WM_STRUT_PARTIAL additionally carries the start/end range
 * each edge's reservation applies to; the legacy, coordinate-less
 * '_NET_WM_STRUT' is only consulted when the partial form is absent.
 *
 * @param ewmh   EWMH connection
 * @param window Window being adopted
 * @param client Client being initialized; its
 *               @c layout.strut_partial fields are set here
 *
 * @note Complexity: @e O(1)
 */
static void s_client_read_struts(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, client_td *client)
{
    xcb_ewmh_get_extents_reply_t strut;
    xcb_ewmh_wm_strut_partial_t partial;

    memset(&strut, 0, sizeof(strut));
    memset(&partial, 0, sizeof(partial));
    if (xcb_ewmh_get_wm_strut_partial_reply(ewmh,
                xcb_ewmh_get_wm_strut_partial(ewmh, window),
                &partial, NULL)) {
        client->layout.strut_partial.sides.left =
            (int32_t) partial.left;
        client->layout.strut_partial.sides.right =
            (int32_t) partial.right;
        client->layout.strut_partial.sides.top =
            (int32_t) partial.top;
        client->layout.strut_partial.sides.bottom =
            (int32_t) partial.bottom;
        /* start: maps {left->left_start_y, right->right_start_y,
         *              top->top_start_x,   bottom->bottom_start_x} */
        client->layout.strut_partial.start.left =
            (int32_t) partial.left_start_y;
        client->layout.strut_partial.start.right =
            (int32_t) partial.right_start_y;
        client->layout.strut_partial.start.top =
            (int32_t) partial.top_start_x;
        client->layout.strut_partial.start.bottom =
            (int32_t) partial.bottom_start_x;
        /* end: maps {left->left_end_y, right->right_end_y,
         *            top->top_end_x,   bottom->bottom_end_x} */
        client->layout.strut_partial.end.left =
            (int32_t) partial.left_end_y;
        client->layout.strut_partial.end.right =
            (int32_t) partial.right_end_y;
        client->layout.strut_partial.end.top =
            (int32_t) partial.top_end_x;
        client->layout.strut_partial.end.bottom =
            (int32_t) partial.bottom_end_x;
    } else if (xcb_ewmh_get_wm_strut_reply(ewmh,
                xcb_ewmh_get_wm_strut(ewmh, window),
                &strut, NULL)) {
        /* Legacy '_NET_WM_STRUT': no start/end coordinates */
        client->layout.strut_partial.sides.left =
            (int32_t) strut.left;
        client->layout.strut_partial.sides.right =
            (int32_t) strut.right;
        client->layout.strut_partial.sides.top =
            (int32_t) strut.top;
        client->layout.strut_partial.sides.bottom =
            (int32_t) strut.bottom;
    }
}


/**
 * @brief Read '_NET_WM_WINDOW_TYPE' to determine client type and
 *        decoration
 *
 * Dock and notification windows are additionally stripped of frame
 * extents, made sticky (dock only), excluded from taskbar/pager, and
 * unfocusable, on top of the type itself; every other recognized type
 * only sets @c properties.type, undecorating menu/splash windows.
 * The first recognized type in @c type_reply wins; an unrecognized
 * type leaves @c properties.type at whatever @c client_init already
 * defaulted it to.
 *
 * @param connection XCB connection, to intern
 *                   @c _NET_WM_WINDOW_TYPE_NOTIFICATION
 * @param ewmh       EWMH connection
 * @param window     Window being adopted
 * @param client     Client being initialized; its type, decoration,
 *                   frame extents, and several property flags are
 *                   set here
 *
 * @note Complexity: @e O(n), where @e n is the number of atoms
 *       '_NET_WM_WINDOW_TYPE' lists
 */
static void s_client_read_window_type(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        client_td *client)
{
    xcb_ewmh_get_atoms_reply_t type_reply;

    memset(&type_reply, 0, sizeof(type_reply));
    if (xcb_ewmh_get_wm_window_type_reply(ewmh,
                xcb_ewmh_get_wm_window_type(ewmh, window),
                &type_reply, NULL)) {
        xcb_atom_t atom_notification = atom_intern(connection,
                "_NET_WM_WINDOW_TYPE_NOTIFICATION", true);

        for (uint32_t ti = 0; ti < type_reply.atoms_len; ++ti) {
            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_DOCK) {
                client->properties.type = CLIENT_TYPE_DOCK;
                client_undecorate(client);
                client_pin(client);
                client_skip_taskbar(client);
                client_skip_pager(client);
                client->layout.frame_extents.left = 0;
                client->layout.frame_extents.right = 0;
                client->layout.frame_extents.top = 0;
                client->layout.frame_extents.bottom = 0;
                client->properties.layer = CLIENT_LAYER_ABOVE;
                client_forbid_focus(client);
                break;
            }

            if (atom_notification != XCB_ATOM_NONE &&
                    type_reply.atoms[ti] == atom_notification) {
                client->properties.type = CLIENT_TYPE_NOTIFICATION;
                client_undecorate(client);
                client->layout.frame_extents.left = 0;
                client->layout.frame_extents.right = 0;
                client->layout.frame_extents.top = 0;
                client->layout.frame_extents.bottom = 0;
                client->properties.layer = CLIENT_LAYER_ABOVE;
                client_forbid_focus(client);
                break;
            }

            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_DIALOG) {
                client->properties.type = CLIENT_TYPE_DIALOG;
                break;
            }

            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR) {
                client->properties.type = CLIENT_TYPE_TOOLBAR;
                break;
            }

            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_MENU) {
                client->properties.type = CLIENT_TYPE_MENU;
                client_undecorate(client);
                break;
            }

            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_SPLASH) {
                client->properties.type = CLIENT_TYPE_SPLASH;
                client_undecorate(client);
                break;
            }

            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_UTILITY) {
                client->properties.type = CLIENT_TYPE_UTILITY;
                break;
            }
        }
        xcb_ewmh_get_atoms_reply_wipe(&type_reply);
    }
}


/**
 * @brief Read '_MOTIF_WM_HINTS' to honor a client's own decoration
 *        request
 *
 * The long-standing de-facto convention several toolkits and
 * applications (e.g., Xpad) still use to explicitly request no window
 * decorations, predating '_NET_WM_WINDOW_TYPE'.
 *
 * Format: 5x CARD32
 *      { flags, functions, decorations, input_mode, status };
 *
 * only 'flags' bit-1 ('MWM_HINTS_DECORATIONS') and 'decorations' are
 * consulted here.
 *
 * An explicit request to turn decorations off overrides whatever the
 * type-based defaults @c s_client_read_window_type already chose; a
 * request to turn them on is honored only if the theme itself
 * decorates windows by default, so this never re-decorates a client
 * type (dock, splash, menu, &c.) that is unconditionally undecorated
 * there.
 *
 * @param connection XCB connection
 * @param window     Window being adopted
 * @param theme      Active theme, to check @c window.is_decorated
 *                   before honoring a request to turn decorations on
 * @param client     Client being initialized; its decoration flag and
 *                   frame extents may be changed here
 *
 * @note Complexity: @e O(1)
 */
static void s_client_read_motif_hints(xcb_connection_t *connection,
        xcb_window_t window, const struct config_theme_s *theme,
        client_td *client)
{
    xcb_atom_t motif_hints_atom;
    xcb_get_property_cookie_t motif_ck;

    motif_hints_atom = atom_intern(connection, "_MOTIF_WM_HINTS", true);
    if (motif_hints_atom != XCB_ATOM_NONE) {
        xcb_get_property_reply_t *motif_r;

        motif_ck = xcb_get_property(connection, 0, window,
                motif_hints_atom, motif_hints_atom, 0, 5);
        motif_r = xcb_get_property_reply(connection, motif_ck, NULL);
        if (motif_r != NULL) {
            if (motif_r->format == 32 &&
                    xcb_get_property_value_length(motif_r) >=
                        (int) (3u * sizeof(uint32_t))) {
                const uint32_t *motif_vals = (const uint32_t *)
                    xcb_get_property_value(motif_r);
                uint32_t motif_flags = motif_vals[0];

                if ((motif_flags & 0x2u) != 0u) {
                    uint32_t motif_decorations = motif_vals[2];

                    /* MWM_HINTS_DECORATIONS set: 'decorations' is
                     * meaningful */
                    if (motif_decorations == 0u) {
                        client_undecorate(client);
                        client->layout.frame_extents =
                            (struct sides_s) {0, 0, 0, 0};
                    } else if (theme != NULL &&
                            theme->window.is_decorated) {
                        client_decorate(client);
                    }
                }
            }
            free(motif_r);
        }
    }
}


/**
 * @brief Read the pre-existing '_NET_WM_STATE' property so that
 *        states an application sets on itself before ever mapping
 *        are honored from the start
 *
 * Without this, such a state would only take effect the first time the
 * application happens to resend it later via a @c _NET_WM_STATE
 * @c ClientMessage (i.e., before the window manager has a chance to
 * intervene; e.g., toggling a "skip taskbar" preference off and back on
 * in xpad's settings): panels and dock windows that set
 * @c _NET_WM_STATE_BELOW (e.g., tint2) get the BELOW layer, and
 * applications that set @c _NET_WM_STATE_SKIP_TASKBAR /
 * @c _NET_WM_STATE_SKIP_PAGER' (e.g., xpad's "hide from taskbar"
 * option, enabled from its own startup) are excluded from the cycle
 * menu and window list immediately rather than only after the user
 * re-toggles the same preference in that application once the window
 * manager is already running.
 *
 * @param connection XCB connection
 * @param ewmh       EWMH connection; a no-op if @c NULL
 * @param window     Window being adopted
 * @param client     Client being initialized; its layer and
 *                   skip-taskbar/skip-pager flags may be set here
 *
 * @note Complexity: @e O(n), where @e n is the number of atoms
 *       '_NET_WM_STATE' lists
 */
static void s_client_read_pre_existing_state(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        client_td *client)
{
    if (ewmh != NULL) {
        xcb_get_property_cookie_t state_ck;
        xcb_atom_t atom_above;
        xcb_atom_t atom_below;
        xcb_atom_t atom_skip_taskbar;
        xcb_atom_t atom_skip_pager;
        xcb_atom_t atom_fullscreen;
        xcb_atom_t atom_max_horz;
        xcb_atom_t atom_max_vert;
        xcb_atom_t atom_demands_attention;

        atom_above = atom_intern(connection, "_NET_WM_STATE_ABOVE", true);
        atom_below = atom_intern(connection, "_NET_WM_STATE_BELOW", true);
        atom_skip_taskbar = atom_intern(connection,
                "_NET_WM_STATE_SKIP_TASKBAR", true);
        atom_skip_pager = atom_intern(connection,
                "_NET_WM_STATE_SKIP_PAGER", true);
        atom_fullscreen = atom_intern(connection,
                "_NET_WM_STATE_FULLSCREEN", true);
        atom_max_horz = atom_intern(connection,
                "_NET_WM_STATE_MAXIMIZED_HORZ", true);
        atom_max_vert = atom_intern(connection,
                "_NET_WM_STATE_MAXIMIZED_VERT", true);
        atom_demands_attention = atom_intern(connection,
                "_NET_WM_STATE_DEMANDS_ATTENTION", true);

        if (atom_above != XCB_ATOM_NONE ||
                atom_below != XCB_ATOM_NONE ||
                atom_skip_taskbar != XCB_ATOM_NONE ||
                atom_skip_pager != XCB_ATOM_NONE ||
                atom_fullscreen != XCB_ATOM_NONE ||
                atom_max_horz != XCB_ATOM_NONE ||
                atom_max_vert != XCB_ATOM_NONE ||
                atom_demands_attention != XCB_ATOM_NONE) {
            xcb_get_property_reply_t *state_r;

            state_ck = xcb_ewmh_get_wm_state(ewmh, window);
            state_r = xcb_get_property_reply(connection, state_ck, NULL);
            if (state_r != NULL) {
                uint32_t natoms = (uint32_t)
                    xcb_get_property_value_length(state_r) /
                    sizeof(xcb_atom_t);
                const xcb_atom_t *atoms = (xcb_atom_t *)
                    xcb_get_property_value(state_r);
                for (uint32_t si = 0; si < natoms; ++si) {
                    if (atoms[si] == atom_above) {
                        client->properties.layer = CLIENT_LAYER_ABOVE;
                    } else if (atoms[si] == atom_below) {
                        client->properties.layer = CLIENT_LAYER_BELOW;
                    } else if (atoms[si] == atom_skip_taskbar) {
                        client_skip_taskbar(client);
                    } else if (atoms[si] == atom_skip_pager) {
                        client_skip_pager(client);
                    } else if (atoms[si] == atom_fullscreen) {
                        client->initial_fullscreen = true;
                    } else if (atoms[si] == atom_max_horz) {
                        client->initial_maximized_horz = true;
                    } else if (atoms[si] == atom_max_vert) {
                        client->initial_maximized_vert = true;
                    } else if (atoms[si] == atom_demands_attention) {
                        client_urge(client);
                    }
                }
                LOGGER_TRACE("window=0x%x pre-existing _NET_WM_STATE:" \
                        " layer=%u, skip_taskbar=%d, skip_pager=%d," \
                        " fullscreen=%d, maximized_horz=%d," \
                        " maximized_vert=%d, urgent=%d",
                        window, (unsigned int) client->properties.layer,
                        (int) ((client->properties.flags &
                                CLIENT_FLAG_SKIP_TASKBAR) != 0u),
                        (int) ((client->properties.flags &
                                CLIENT_FLAG_SKIP_PAGER) != 0u),
                        (int) client->initial_fullscreen,
                        (int) client->initial_maximized_horz,
                        (int) client->initial_maximized_vert,
                        (int) client_is_urgent(client));
                free(state_r);
            } /* ! if (!state_r) */
        } /* ! if (atom_above) */
    } /* ! if (!ewmh) */
}


/**
 * @brief Subscribe to events on the adopted window, apply its border
 *        width, and set its default cursor
 *
 * For dock and notification windows, preserves the application's own
 * event mask (which includes 'ButtonPress'/'ButtonRelease' needed for
 * systray interaction) and ORs in only the window manager's required
 * events; replacing the mask wholesale would strip 'ButtonPress',
 * making systray icons non-interactive after a 'PassiveGrab' replay.
 * Every other client type gets a fresh mask covering focus, geometry,
 * and pointer tracking.
 *
 * The border width is applied before subscribing to
 * 'STRUCTURE_NOTIFY' so the resulting 'ConfigureNotify' is not
 * delivered to the window manager: at this point the window has not
 * yet been placed, so the event would carry the X-server-initial
 * position (typically (0,0)), and 'handler_configure_notify' would
 * overwrite the placement position computed later by 'place_apply',
 * causing an undecorated window to flicker back to the origin on
 * every render cycle.  Dock windows always get zero border width.
 *
 * The explicit plain-pointer cursor set here, once, is what makes the
 * resize cursor set while hovering the frame's own border reliably
 * give way to a plain pointer the instant the pointer crosses into
 * this client's own content: X11 always prefers the nearest explicit
 * cursor over an inherited one, resolved by the server itself on
 * every crossing, with no window-manager-side event handling
 * required.  A purely event-driven reset (motion, or even
 * enter-notify) can be preempted by a client that intercepts pointer
 * motion for its own purposes (e.g., GTK/Qt applications tracking
 * hover for their own UI), which stops those events from ever
 * reaching this window manager at all; this static default has no
 * such dependency.
 *
 * @param connection XCB connection
 * @param window     Window being adopted
 * @param theme      Active theme, for the border width; a @c NULL
 *                   theme (or a dock window) gets a zero-width border
 * @param client     Client being initialized, for its @c properties.type
 *
 * @note Complexity: @e O(1)
 */
static void s_client_events_subscribe(xcb_connection_t *connection,
        xcb_window_t window, struct config_theme_s *theme,
        client_td *client)
{
    uint32_t values[2];
    uint32_t bw[1];

    if (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK ||
            client->properties.type ==
                (uint16_t) CLIENT_TYPE_NOTIFICATION) {
        xcb_get_window_attributes_cookie_t wac =
            xcb_get_window_attributes(connection, window);
        xcb_get_window_attributes_reply_t *const war =
            xcb_get_window_attributes_reply(connection, wac, NULL);
        uint32_t existing_mask = (war != NULL)
            ? (uint32_t) war->your_event_mask : 0u;
        if (war != NULL) {
            free(war);
        }
        values[0] = existing_mask            |
                    XCB_EVENT_MASK_PROPERTY_CHANGE  |
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    } else {
        values[0] = XCB_EVENT_MASK_ENTER_WINDOW     |
                    XCB_EVENT_MASK_LEAVE_WINDOW     |
                    XCB_EVENT_MASK_FOCUS_CHANGE     |
                    XCB_EVENT_MASK_PROPERTY_CHANGE  |
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                    XCB_EVENT_MASK_POINTER_MOTION;
    }

    bw[0] = (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK)
        ? 0u
        : ((theme != NULL) ? theme->window.active.border.width : 0u);
    xcb_configure_window(connection, window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, bw);

    values[1] = mouse_plain_cursor();
    xcb_change_window_attributes(connection, window,
            XCB_CW_EVENT_MASK | XCB_CW_CURSOR, values);
    LOGGER_TRACE("Set cursor (window=0x%x, cursor=0x%x)", window,
            values[1]);
}


/* Initialize a new client, adopting an existing X window under
 * window manager control */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        xcb_window_t window,
        struct config_theme_s *theme,
        const struct config_base_s *config_base,
        const struct config_a11y_s *a11y)
{
    client_td *client;
    xcb_get_window_attributes_cookie_t attr_cookie;
    xcb_get_geometry_cookie_t geom_cookie;
    xcb_get_geometry_reply_t *geom_reply;
    xcb_get_window_attributes_reply_t *attr_reply;
    char wm_class[256];
    char wm_instance[256];
    char net_wm_name[256];
    uint32_t ewmh_pid;
    uint32_t utime;

    LOGGER_TRACE("Attempting to manage existing window %#x", window);

    /* Both requests are sent before either reply is awaited: neither
     * depends on the other's value, so the server can work on both at
     * once.  Geometry's own reply is still collected (and discarded)
     * even on the rare override-redirect rejection path just below,
     * rather than left uncollected, so a window this function ends up
     * never managing never leaves a stray unclaimed reply sitting in
     * this connection's own queue. */
    attr_cookie = xcb_get_window_attributes(connection, window);
    geom_cookie = xcb_get_geometry(connection, window);

    /* Reject override-redirect windows, for they manage themselves */
    attr_reply = xcb_get_window_attributes_reply(connection,
            attr_cookie, NULL);
    if (attr_reply != NULL) {
        bool skip = attr_reply->override_redirect;
        free(attr_reply);
        if (skip) {
            geom_reply = xcb_get_geometry_reply(connection,
                    geom_cookie, NULL);
            if (geom_reply != NULL) {
                free(geom_reply);
            }
            LOGGER_TRACE("Skipping override-redirect window %#x",
                    window);
            return NULL;
        }
    }

    client = calloc(1, sizeof(client_td));
    if (client == NULL) {
        LOGGER_ERROR("Failed to allocate memory for managed client",
                L_NARG);
        geom_reply = xcb_get_geometry_reply(connection, geom_cookie, NULL);
        if (geom_reply != NULL) {
            free(geom_reply);
        }
        return NULL;
    }

    s_client_init_common(client, connection, ewmh, theme, config_base,
            a11y);

    /* Use the X window ID as both window handle and hash/lookup key */
    client->window = window;
    client->id = window;

    /* Collect the geometry request sent at the very top of this
     * function, alongside the window-attributes one above */
    geom_reply = xcb_get_geometry_reply(connection, geom_cookie, NULL);
    if (geom_reply != NULL) {
        client->parent_id = geom_reply->root;
        client->layout.geometry.cur.pos.x = geom_reply->x;
        client->layout.geometry.cur.pos.y = geom_reply->y;
        client->layout.geometry.cur.dim.w = geom_reply->width;
        client->layout.geometry.cur.dim.h = geom_reply->height;
        free(geom_reply);
    } else {
        client->layout.geometry.cur.pos.x = 0;
        client->layout.geometry.cur.pos.y = 0;
        client->layout.geometry.cur.dim.w = WM_CLIENT_DEFAULT_DIM;
        client->layout.geometry.cur.dim.h = WM_CLIENT_DEFAULT_DIM;
    }
    client->layout.geometry.old = client->layout.geometry.cur;

    /* Allocate string buffers */
    if (ci_alloc_strings(client) != 0) {
        LOGGER_ERROR("Failed to allocate string buffers"
                " for managed client", L_NARG);
        free(client);
        return NULL;
    }

    /* Default string values */
    snprintf(client->info.name,
            CONFIG_MAX_LENGTH_NAME - 1, "Window %#x", window);
    snprintf(client->info.visible_name,
            CONFIG_MAX_LENGTH_NAME - 1, "Window %#x", window);

    /* Read '_NET_WM_NAME' (UTF-8) first; fall back to 'WM_NAME' (Latin-1) */
    ci_get_net_wm_name(ewmh, window, net_wm_name, sizeof(net_wm_name));
    if (net_wm_name[0] != '\0') {
        s_client_display_name_set(client, net_wm_name);
    } else {
        char wm_name[256];
        ci_get_wm_name(connection, window, wm_name, sizeof(wm_name));
        s_client_display_name_set(client, wm_name);
    }

    /* Read '_NET_WM_ICON_NAME'/'WM_ICON_NAME' for iconified caption */
    client_props_refresh_icon_name(client);

    /* Read 'WM_CLASS' */
    ci_get_wm_class(connection, window,
            wm_class, sizeof(wm_class),
            wm_instance, sizeof(wm_instance));
    if (wm_class[0] != '\0') {
        safe_strncpy(client->info.class_name[1], wm_class,
                CONFIG_MAX_LENGTH_NAME - 1);
    }
    if (wm_instance[0] != '\0') {
        safe_strncpy(client->info.class_name[0], wm_instance,
                CONFIG_MAX_LENGTH_NAME - 1);
    }

    /* Read 'WM_PROTOCOLS' and set up '_NET_WM_SYNC_REQUEST' support */
    s_client_read_wm_protocols(connection, ewmh, window, client);

    /* Read 'WM_HINTS', 'WM_CLIENT_LEADER', and 'WM_TRANSIENT_FOR' */
    s_client_read_wm_hints_and_leader(connection, window, client);

    /* Read 'WM_NORMAL_HINTS': size constraints and increment grid */
    client_props_refresh_normal_hints(client);

    /* Read '_NET_WM_STRUT_PARTIAL' for dock/panel windows */
    s_client_read_struts(ewmh, window, client);

    /* Read '_NET_WM_WINDOW_TYPE' to determine client type and
     * decoration */
    s_client_read_window_type(connection, ewmh, window, client);

    /* Read '_MOTIF_WM_HINTS': see the sibling function's comment for
     * the full rationale */
    s_client_read_motif_hints(connection, window, theme, client);

    if (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK &&
            client->ewmh != NULL) {
        ccmd_add_states(client, 3,
                "_NET_WM_STATE_STICKY",
                "_NET_WM_STATE_SKIP_TASKBAR",
                "_NET_WM_STATE_SKIP_PAGER");
    }

    /* Read the pre-existing '_NET_WM_STATE' property; see the sibling
     * function's comment for the full explanation */
    s_client_read_pre_existing_state(connection, ewmh, window, client);

    /* Read '_NET_WM_PID': associate X window with its owning process */
    ewmh_pid = 0u;
    if (xcb_ewmh_get_wm_pid_reply(ewmh,
                xcb_ewmh_get_wm_pid(ewmh, window),
                &ewmh_pid, NULL)) {
        client->process.pid = (int) ewmh_pid;
    }

    /* Read '_NET_WM_USER_TIME': used for initial focus policy */
    utime = 0u;
    if (xcb_ewmh_get_wm_user_time_reply(ewmh,
                xcb_ewmh_get_wm_user_time(ewmh, window),
                &utime, NULL)) {
        client->user_time = utime;
    }

    /* Publish initial '_NET_WM_ALLOWED_ACTIONS' */
    ccmd_client_update_allowed_actions(client);

    /* Subscribe to events, apply border width, and set the default
     * cursor; see the sibling function's comment for more information */
    s_client_events_subscribe(connection, window, theme, client);

    /* Ignore return value, as decoration creation is non-fatal here */
    (void) ci_create_decorations(client);

    /* Only grab buttons on client windows that the window manager
     * decorates or that could receive focus.  Dock and notification
     * windows manage their own pointer events; grabbing buttons on them
     * intercepts systray icon clicks and breaks context-menu
     * interaction. */
    if (client->frame == 0 &&
            client->properties.type != (uint16_t) CLIENT_TYPE_DOCK &&
            client->properties.type !=
                (uint16_t) CLIENT_TYPE_NOTIFICATION) {
        ccmd_client_grab_buttons(client);
    }

    /* Initialize '_NET_WM_STATE' to an empty list for newly adopted
     * windows so taskbars and pagers always see a clean state even if
     * the application left a stale property from a previous session. */
    if (client->properties.type == (uint16_t) CLIENT_TYPE_NORMAL ||
            client->properties.type == (uint16_t) CLIENT_TYPE_DIALOG ||
            client->properties.type == (uint16_t) CLIENT_TYPE_TOOLBAR ||
            client->properties.type == (uint16_t) CLIENT_TYPE_UTILITY) {
        if (client->ewmh != NULL) {
            xcb_ewmh_set_wm_state(client->ewmh, client->window, 0, NULL);
        }
    }

    ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL, XCB_NONE);

    /* Mark the client as needing a full geometry configure and repaint
     * on the first render pass so the decoration and content area are
     * correctly sized and positioned from the outset */
    client->is_outdated = true;

    scratchpad_notice_client_created(client);

    LOGGER_TRACE("Now managing window %#x ('%s')",
            window, client->info.name);

    return client;
}
