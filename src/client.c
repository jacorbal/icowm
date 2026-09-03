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
#include <string.h>     /* memcpy, memset, snprintf, strncmp */
#include <unistd.h>     /* gethostname */

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
#include <cmds/client/ewmh.h>
#include <cmds/client/flags.h>
#include <cmds/client/grab.h>
#include <cmds/client/move.h>
#include <cmds/client/transient.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/client.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <logger.h>
#include <render/wmicon.h>
#include <policy/focus.h>
#include <policy/stacking.h>
#include <menu/cycle.h>
#include <scratchpad.h>
#include <wm.h>

/* Input includes */
#include <input/mouse/cursor.h>

/* Local includes */
#include <client/internal.h>
#include <client/props.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/**
 * @brief Timestamp of the most recent genuine user input seen
 *
 * Kept for the focus-granting path, which ICCCM requires to carry a
 * real timestamp and forbids from carrying @c CurrentTime, but which
 * is reached from places that hold no event of their own.  A fallback
 * after a window closed, a desktop switch, an activation request.
 *
 * File scope, and updated by the event loop from every real key or
 * button press, which is how Openbox keeps its @c event_curtime
 * for the same purpose.
 */
static uint32_t s_last_user_time = 0u;


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
 * @param config        Shared base/theme/a11y configuration
 */
static void s_client_common_init(client_td *client,
        const config_td *config)
{
    if (client == NULL) {
        return;
    }

    client->config = config;
    client->process.pid = -1;
    client->hints_icccm.hints.accepts_input = true;
    client->icon_pos.x = -1;
    client->icon_pos.y = -1;
    client->layout.gravity =
        (uint16_t) ((config != NULL)
                ? config->base.windows.gravity
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
    client->last_border_color = UINT32_MAX;
    ci_set_decoration_defaults(client);
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


/**
 * @brief Every request @a client_init issues in one go, awaiting reply
 *
 * XCB splits a request from its reply.  The request function returns a
 * cookie without blocking, and only the reply function waits.  Issuing
 * every independent request first and collecting the replies
 * afterwards costs one round trip to the server rather than one per
 * property, which is the whole reason this project uses XCB rather
 * than Xlib.
 *
 * Three requests are deliberately absent.  @c GetWindowAttributes is
 * issued and awaited before any of these, since an override-redirect
 * window is discarded on its answer and issuing a dozen requests for a
 * window about to be thrown away would make every menu and tooltip
 * more expensive rather than less.  The plain @c _NET_WM_STRUT is
 * asked for only when @c _NET_WM_STRUT_PARTIAL has no answer, so
 * batching it would ask every time.  And @c _NET_WM_USER_TIME is asked
 * of whichever window @c _NET_WM_USER_TIME_WINDOW names, which is not
 * known until that reply arrives.
 *
 * A dependency between what two readers do with their replies does not
 * stop their requests going out together: @c _MOTIF_WM_HINTS is read
 * after the window type because it overrides the defaults that type
 * chose, and that orders the processing alone.
 */
struct s_client_cookies_init_s {
    xcb_get_geometry_cookie_t geometry;      /**< @c GetGeometry */
    xcb_get_property_cookie_t wm_protocols;  /**< @c WM_PROTOCOLS */
    xcb_get_property_cookie_t wm_hints;      /**< @c WM_HINTS */
    xcb_get_property_cookie_t client_leader; /**< @c WM_CLIENT_LEADER */
    xcb_get_property_cookie_t transient_for; /**< @c WM_TRANSIENT_FOR */
    /** @c _NET_WM_STRUT_PARTIAL */
    xcb_get_property_cookie_t strut_partial;
    /** @c _NET_WM_WINDOW_TYPE */
    xcb_get_property_cookie_t window_type;
    xcb_get_property_cookie_t motif_hints;   /**< @c _MOTIF_WM_HINTS */
    xcb_get_property_cookie_t wm_state;      /**< @c _NET_WM_STATE */
    xcb_get_property_cookie_t wm_pid;        /**< @c _NET_WM_PID */
    /** @c WM_CLIENT_MACHINE */
    xcb_get_property_cookie_t client_machine;
    /** @c _NET_WM_USER_TIME_WINDOW */
    xcb_get_property_cookie_t user_time_window;
};


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
 * @param ck         Requests already issued by @a client_init; this
 *                   reader awaits its rather than making one
 * @param client     Client being initialized; its protocol-support
 *                   flags, 'sync_counter', and 'sync_alarm' fields
 *                   are set here
 *
 * @note Complexity: @e O(n), where @e n is the number of protocols
 *       'WM_PROTOCOLS' advertises
 */
static void s_client_read_wm_protocols(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        client_td *client,
        const struct s_client_cookies_init_s *ck)
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

    client->hints_icccm.protocols.delete_atom = wm_delete_atom;
    client->hints_icccm.protocols.has_delete = false;
    client->hints_icccm.protocols.take_focus_atom = wm_take_focus_atom;
    client->hints_icccm.protocols.has_take_focus = false;
    client->hints_ewmh.ping.is_supported = false;
    client->hints_ewmh.sync.is_supported = false;

    memset(&proto, 0, sizeof(proto));
    if (xcb_icccm_get_wm_protocols_reply(connection, ck->wm_protocols,
                &proto, NULL)) {
        for (uint32_t pi = 0; pi < proto.atoms_len; ++pi) {
            if (proto.atoms[pi] == wm_delete_atom) {
                client->hints_icccm.protocols.has_delete = true;
            } else if (proto.atoms[pi] == wm_take_focus_atom) {
                client->hints_icccm.protocols.has_take_focus = true;
            } else if (proto.atoms[pi] == net_wm_ping_atom) {
                client->hints_ewmh.ping.is_supported = true;
            } else if (proto.atoms[pi] == ewmh->_NET_WM_SYNC_REQUEST) {
                client->hints_ewmh.sync.is_supported = true;
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
     * or server at worst leaves 'hints_ewmh.sync.is_supported'
     * effectively unusable, not a crash. */
    client->hints_ewmh.sync.counter = 0u;
    client->hints_ewmh.sync.alarm = 0u;
    if (client->hints_ewmh.sync.is_supported && wm_sync_is_available()) {
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
                client->hints_ewmh.sync.counter = *(uint32_t *)
                    xcb_get_property_value(counter_reply);
            }
            free(counter_reply);
        }

        if (client->hints_ewmh.sync.counter != 0u) {
            uint32_t alarm_values[7];

            client->hints_ewmh.sync.alarm = xcb_generate_id(connection);
            /* The value list follows the ascending 'CA_*' bit order:
             * 'COUNTER', 'VALUE_TYPE', 'VALUE', 'TEST_TYPE', 'DELTA'.
             * 'VALUE' and 'DELTA' are each a 64-bit 'INT64', a high
             * word then a low word, not a single 'CARD32'.  A list
             * shorter than what the request's mask calls for is
             * rejected by the server, and the alarm XID then never
             * exists server-side, so it can never fire.  Every resize
             * would silently fall back to applying once every
             * 'WM_SYNC_MAX_WAIT_TICKS' attempts instead of being
             * acknowledged promptly. */
            /* COUNTER */
            alarm_values[0] = client->hints_ewmh.sync.counter;
            /* VALUE_TYPE */
            alarm_values[1] = (uint32_t) XCB_SYNC_VALUETYPE_RELATIVE;
            /* VALUE, high word then low word */
            alarm_values[2] = 0u;
            alarm_values[3] = 0u;
            /* TEST_TYPE */
            alarm_values[4] =
                (uint32_t) XCB_SYNC_TESTTYPE_POSITIVE_TRANSITION;
            /* DELTA, high word then low word */
            alarm_values[5] = 0u;
            alarm_values[6] = 1u;
            xcb_sync_create_alarm(connection,
                    (xcb_sync_alarm_t) client->hints_ewmh.sync.alarm,
                    (uint32_t) (XCB_SYNC_CA_COUNTER |
                            XCB_SYNC_CA_VALUE_TYPE |
                            XCB_SYNC_CA_VALUE |
                            XCB_SYNC_CA_TEST_TYPE |
                            XCB_SYNC_CA_DELTA),
                    alarm_values);
            LOGGER_DEBUG("Enabled '_NET_WM_SYNC_REQUEST' for" \
                    " window=0x%x (counter=0x%x, alarm=0x%x)", window,
                    client->hints_ewmh.sync.counter,
                    client->hints_ewmh.sync.alarm);
        } else {
            /* Client advertised the protocol but never actually set
             * its counter property; treat it as unsupported rather
             * than sending requests nobody will ever answer */
            LOGGER_DEBUG("window=0x%x advertised" \
                    " '_NET_WM_SYNC_REQUEST' but never set its" \
                    " counter property; treating it as unsupported",
                    window);
            client->hints_ewmh.sync.is_supported = false;
        }
    }
}


/**
 * @brief Read 'WM_HINTS', 'WM_CLIENT_LEADER', and 'WM_TRANSIENT_FOR'
 *
 * @c WM_HINTS supplies the input model, initial iconic state, window
 * group, and urgency; @c WM_CLIENT_LEADER (ICCCM §5.1) and
 * @c WM_HINTS' own window group together let @c client_group_leader
 * and @c place_window_apply cluster windows belonging to the same
 * application; @c WM_TRANSIENT_FOR identifies dialogs and their
 * parent.
 *
 * @param connection XCB connection
 * @param ck         Requests already issued by @a client_init; this
 *                   reader awaits its rather than making one
 * @param client     Client being initialized; every field these three
 *                   properties feed is set here
 *
 * @note Complexity: @e O(1)
 */
static void s_client_read_wm_hints_and_leader(xcb_connection_t *connection,
        client_td *client, const struct s_client_cookies_init_s *ck)
{
    xcb_atom_t client_leader_atom;
    xcb_icccm_wm_hints_t wm_hints;
    xcb_window_t transient = XCB_WINDOW_NONE;

    /* Read 'WM_HINTS': input model and window group */
    memset(&wm_hints, 0, sizeof(wm_hints));
    if (xcb_icccm_get_wm_hints_reply(connection,
                ck->wm_hints,
                &wm_hints, NULL)) {
        if (wm_hints.flags & XCB_ICCCM_WM_HINT_INPUT) {
            client->hints_icccm.hints.accepts_input =
                (wm_hints.input != 0);
        }
        if (wm_hints.flags & XCB_ICCCM_WM_HINT_STATE &&
                wm_hints.initial_state == XCB_ICCCM_WM_STATE_ICONIC) {
            client->hints_icccm.hints.is_initial_iconic = true;
        }
        if (wm_hints.flags & XCB_ICCCM_WM_HINT_WINDOW_GROUP) {
            client->hints_icccm.hints.group_leader = wm_hints.window_group;
        }
        if (wm_hints.flags & XCB_ICCCM_WM_HINT_X_URGENCY) {
            client_urge(client);
        }
    }

    /* Read 'WM_CLIENT_LEADER': ICCCM §5.1 property used, together with
     * the 'WM_HINTS' window group above, to cluster windows belonging
     * to the same application for placement (see 'client_group_leader'
     * and 'place_window_apply') */
    client->hints_icccm.hints.client_leader = XCB_WINDOW_NONE;
    client_leader_atom = atom_intern(connection, "WM_CLIENT_LEADER", true);
    if (client_leader_atom != XCB_ATOM_NONE) {
        xcb_get_property_reply_t *client_leader_reply;

        client_leader_reply = xcb_get_property_reply(connection,
                ck->client_leader, NULL);
        if (client_leader_reply != NULL) {
            if (client_leader_reply->type == XCB_ATOM_WINDOW &&
                    client_leader_reply->format == 32 &&
                    xcb_get_property_value_length(client_leader_reply) >=
                        (int) sizeof(xcb_window_t)) {
                client->hints_icccm.hints.client_leader = *(xcb_window_t *)
                    xcb_get_property_value(client_leader_reply);
            }
            free(client_leader_reply);
        }
    }

    /* Read 'WM_TRANSIENT_FOR': identify dialogs and their parent.
     * ICCCM §4.1.2.6: a value of the root window itself means the
     * client is transient for its whole application group, not one
     * specific window; 'client->parent_id' already holds that root
     * (set from 'xcb_get_geometry''s reply, above, before this
     * client is ever reparented) so no separate lookup is needed
     * here to tell the two cases apart. */
    client->transient_for = XCB_WINDOW_NONE;
    client->is_transient_for_group = false;
    if (xcb_icccm_get_wm_transient_for_reply(connection,
                ck->transient_for,
                &transient, NULL)) {
        client->transient_for = transient;
        client->is_transient_for_group =
            (transient != XCB_WINDOW_NONE &&
                    transient == client->parent_id);
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
 * @param ck     Requests already issued by @a client_init; this reader
 *               awaits its rather than making one
 * @param client Client being initialized; its
 *               @c layout.strut_partial fields are set here
 *
 * @note Complexity: @e O(1)
 */
static void s_client_read_struts(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, client_td *client,
        const struct s_client_cookies_init_s *ck)
{
    xcb_ewmh_get_extents_reply_t strut;
    xcb_ewmh_wm_strut_partial_t partial;

    memset(&strut, 0, sizeof(strut));
    memset(&partial, 0, sizeof(partial));
    if (xcb_ewmh_get_wm_strut_partial_reply(ewmh,
                ck->strut_partial,
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
 * @param ck         Requests already issued by @a client_init; this
 *                   reader awaits its rather than making one
 * @param client     Client being initialized; its type, decoration,
 *                   frame extents, and several property flags are
 *                   set here
 *
 * @note Complexity: @e O(n), where @e n is the number of atoms
 *       '_NET_WM_WINDOW_TYPE' lists
 */
static void s_client_read_window_type(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, client_td *client,
        const struct s_client_cookies_init_s *ck)
{
    xcb_ewmh_get_atoms_reply_t type_reply;

    memset(&type_reply, 0, sizeof(type_reply));
    if (xcb_ewmh_get_wm_window_type_reply(ewmh,
                ck->window_type,
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
                /* A start-up screen is not somewhere the user works:
                 * it is not worth a taskbar entry it will outlive by
                 * seconds, and it must not take the keyboard away from
                 * whatever they were typing into while the application
                 * behind it loads.  Neither is spelled out in EWMH,
                 * which says only what the type means, but both are
                 * what every desktop does with one. */
                client->properties.flags |=
                    (uint16_t) CLIENT_FLAG_SKIP_TASKBAR;
                client->properties.flags &=
                    (uint16_t) ~(uint16_t) CLIENT_FLAG_FOCUSABLE;
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
 * @brief Read '_MOTIF_WM_HINTS' to honor a client's decoration
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
 * @param ck         Requests already issued by @a client_init; this
 *                   reader awaits its rather than making one
 * @param client     Client being initialized; its decoration flag,
 *                   frame extents, and own @c config (checked for
 *                   @c window.is_decorated before honoring a request
 *                   to turn decorations on) may be used or changed
 *                   here
 *
 * @note Complexity: @e O(1)
 */
static void s_client_read_motif_hints(xcb_connection_t *connection,
        client_td *client, const struct s_client_cookies_init_s *ck)
{
    xcb_atom_t motif_hints_atom;

    motif_hints_atom = atom_intern(connection, "_MOTIF_WM_HINTS", true);
    if (motif_hints_atom != XCB_ATOM_NONE) {
        xcb_get_property_reply_t *motif_r;

        motif_r = xcb_get_property_reply(connection, ck->motif_hints,
                NULL);
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
                    } else if (client->config != NULL &&
                            client->config->theme.window.is_decorated) {
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
 * option, enabled from its startup) are excluded from the cycle
 * menu and window list immediately rather than only after the user
 * re-toggles the same preference in that application once the window
 * manager is already running.
 *
 * @param connection XCB connection
 * @param window     Window being adopted
 * @param ck         Requests already issued by @a client_init; this
 *                   reader awaits its rather than making one
 * @param client     Client being initialized; its layer and
 *                   skip-taskbar/skip-pager flags may be set here
 *
 * @note Complexity: @e O(n), where @e n is the number of atoms
 *       '_NET_WM_STATE' lists
 */
static void s_client_read_pre_existing_state(xcb_connection_t *connection,
        xcb_window_t window, client_td *client,
        const struct s_client_cookies_init_s *ck)
{
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

        state_r = xcb_get_property_reply(connection, ck->wm_state,
                NULL);
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
                    client->hints_ewmh.initial_state.is_fullscreen =
                        true;
                } else if (atoms[si] == atom_max_horz) {
                    client->hints_ewmh.initial_state
                        .is_maximized_horz = true;
                } else if (atoms[si] == atom_max_vert) {
                    client->hints_ewmh.initial_state
                        .is_maximized_vert = true;
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
                    (int) client->hints_ewmh.initial_state
                        .is_fullscreen,
                    (int) client->hints_ewmh.initial_state
                        .is_maximized_horz,
                    (int) client->hints_ewmh.initial_state
                        .is_maximized_vert,
                    (int) client_is_urgent(client));
            free(state_r);
        } /* ! if (!state_r) */
    } /* ! if (atom_above) */
}


/**
 * @brief Subscribe to events on the adopted window, apply its border
 *        width, and set its default cursor
 *
 * For dock and notification windows, preserves the application's
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
 * overwrite the placement position computed later by
 * 'place_window_apply', causing an undecorated window to flicker back
 * to the origin on every render cycle.  Dock windows always get zero
 * border width.
 *
 * The explicit plain-pointer cursor set here, once, is what makes the
 * resize cursor set while hovering the frame's border reliably
 * give way to a plain pointer the instant the pointer crosses into
 * this client's content: X11 always prefers the nearest explicit
 * cursor over an inherited one, resolved by the server itself on
 * every crossing, with no window-manager-side event handling
 * required.  A purely event-driven reset (motion, or even
 * enter-notify) can be preempted by a client that intercepts pointer
 * motion for its purposes (e.g., GTK/Qt applications tracking
 * hover for their UI), which stops those events from ever
 * reaching this window manager at all; this static default has no
 * such dependency.
 *
 * @param connection XCB connection
 * @param window     Window being adopted
 * @param client     Client being initialized; its @c config
 *                   (checked for the active theme's border width; a
 *                   @c NULL config or a dock window gets a zero-width
 *                   border) and @c properties.type are read here
 *
 * @note Complexity: @e O(1)
 */
static void s_client_events_subscribe(xcb_connection_t *connection,
        xcb_window_t window, client_td *client)
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
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                    XCB_EVENT_MASK_COLOR_MAP_CHANGE;
    } else {
        values[0] = XCB_EVENT_MASK_ENTER_WINDOW     |
                    XCB_EVENT_MASK_LEAVE_WINDOW     |
                    XCB_EVENT_MASK_FOCUS_CHANGE     |
                    XCB_EVENT_MASK_PROPERTY_CHANGE  |
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                    XCB_EVENT_MASK_COLOR_MAP_CHANGE |
                    XCB_EVENT_MASK_POINTER_MOTION;
    }

    bw[0] = (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK)
        ? 0u
        : ((client->config != NULL)
                ? client->config->theme.window.active.border.width : 0u);
    xcb_configure_window(connection, window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, bw);

    values[1] = mouse_plain_cursor();
    xcb_change_window_attributes(connection, window,
            XCB_CW_EVENT_MASK | XCB_CW_CURSOR, values);
    LOGGER_TRACE("Set cursor (window=0x%x, cursor=0x%x)", window,
            values[1]);
}


/* Destroy the specified client and free associated resources */
void client_destroy(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* Out of both orders before anything else.  One left in the focus
     * order would be handed real input focus by the next fallback that
     * walked far enough to reach it, and one left in the stacking
     * order would be restacked as a dangling pointer */
    focus_order_remove(client);
    (void) stacking_remove(client);

    /* Removes 'client' from its parent's 'transients' list (true
     * O(1), see 'transient_node''s comment, client.h) and
     * orphans every one of its own children, before anything below
     * frees so much as a single field: every other function walking
     * the transient tree (top-parent walks, focus redirection, family
     * cascades) follows real 'client_td*' pointers now, so a client
     * freed while still linked in would leave those pointers dangling
     * for whoever encounters it next. */
    client_unlink_transient(client);

    cycle_notice_client_destroyed(client);
    scratchpad_notice_client_destroyed(client);

    LOGGER_DEBUG("Destroying client %p (window %#x, name '%s')",
            (void *) client, client->window, client->info.name);

    /* Destroy the XCB window representation and flush the output buffer
     * to ensure the request is processed */
    if (xcb_connection_get() != NULL && client->window != 0) {
        xcb_window_destroy(client->window);
    }

    /* Release the '_NET_WM_SYNC_REQUEST' alarm, if any.  It is
     * a server-side resource owned by the window manager's
     * connection (unlike the counter it watches, which belongs to the
     * client and is not ours to destroy), so it is not freed
     * automatically when the client window above is destroyed */
    if (xcb_connection_get() != NULL &&
            client->hints_ewmh.sync.alarm != 0u) {
        xcb_sync_destroy_alarm(xcb_connection_get(),
                (xcb_sync_alarm_t) client->hints_ewmh.sync.alarm);
    }

    /* Destroy decorations if any */
    if (xcb_connection_get() != NULL && client->titlebar != 0) {
        xcb_window_destroy(client->titlebar);
    }
    if (xcb_connection_get() != NULL && client->icon_window != 0) {
        xcb_window_destroy(client->icon_window);
    }
    /* Frees the cached '_NET_WM_ICON' Picture built by 'wmicon_draw'
     * (see render/wmicon.h), if any; a no-op if nothing was ever
     * cached, e.g., a client that never had 'theme.icon.show-pixmaps'
     * draw anything for it in the first place */
    wmicon_invalidate(xcb_connection_get(), &client->icon_pixmap_cache);
    if (xcb_connection_get() != NULL && client->frame != 0) {
        xcb_window_destroy(client->frame);
    }

    /* Free all allocated string buffers */
    s_client_heap_fields_release(client);

    /* Free the client structure itself */
    free(client);
}


/* Refresh a client's user-time from a genuine input event that
 * just reached it */
/* Record the timestamp of a genuine user input event */
void client_note_user_time(uint32_t time)
{
    if (client_user_time_is_newer(time, s_last_user_time)) {
        s_last_user_time = time;
    }
}


/* Most recent genuine user input timestamp seen */
uint32_t client_last_user_time(void)
{
    return s_last_user_time;
}


void client_update_user_time(client_td *client, uint32_t time)
{
    if (client == NULL) {
        return;
    }

    if (client_user_time_is_newer(time, client->user_time)) {
        client->user_time = time;
    }
}


/* Keep a cached visible name and its matching EWMH property in sync
 * with whether the caller's just-rendered text was truncated */
void client_sync_visible_name(client_td *client, char *cached,
        const char *full_name, const char *rendered,
        xcb_void_cookie_t (*set_fn)(xcb_ewmh_connection_t *,
            xcb_window_t, uint32_t, const char *),
        xcb_atom_t atom)
{
    xcb_ewmh_connection_t *const ewmh = xcb_ewmh_connection_get();

    if (client == NULL || ewmh == NULL ||
            cached == NULL || full_name == NULL || rendered == NULL ||
            set_fn == NULL) {
        return;
    }

    if (safe_strcmp(rendered, full_name) != 0) {
        /* Actually truncated right now */
        if (safe_strcmp(cached, rendered) == 0) {
            return;
        }
        safe_strncpy(cached, rendered, CONFIG_MAX_LENGTH_NAME);
        set_fn(ewmh, client->window,
                (uint32_t) safe_strlen(rendered), rendered);
    } else {
        /* No longer (or never) truncated: the property should not be
         * advertised at all, rather than set to a redundant copy of
         * 'full_name' */
        if (safe_strcmp(cached, full_name) == 0) {
            return;
        }
        safe_strncpy(cached, full_name, CONFIG_MAX_LENGTH_NAME);
        xcb_delete_property(xcb_connection_get(), client->window, atom);
    }
}


/* Subscribe 'ColormapChangeMask' on every window in a client's
 * 'WM_COLORMAP_WINDOWS' list */
void client_subscribe_colormap_windows(
        xcb_connection_t *connection, const client_td *client)
{
    uint32_t values[1];

    values[0] = XCB_EVENT_MASK_COLOR_MAP_CHANGE;
    for (uint32_t i = 0u; i < client->colormap_windows.count; ++i) {
        xcb_change_window_attributes(connection,
                client->colormap_windows.windows[i],
                XCB_CW_EVENT_MASK, values);
    }
}


/**
 * @brief This host's own hostname, as set in @c WM_CLIENT_MACHINE by
 *        a well-behaved local client
 *
 * Read once and cached for the life of the process: a host's own
 * name does not change while it is running, so every later client
 * adopted reuses this same answer instead of each paying for its own
 * @c gethostname(2) call.
 *
 * @return This host's hostname, or an empty string if @c gethostname
 *         itself failed, in which case no client will ever be found
 *         to match it
 *
 * @note Complexity: @e O(1) amortized; the underlying system call
 *       runs at most once
 */
static const char *s_local_hostname(void)
{
    static char name[HOST_NAME_MAX + 1] = "";
    static bool is_read = false;

    if (!is_read) {
        if (gethostname(name, sizeof(name)) != 0) {
            name[0] = '\0';
        }
        /* POSIX leaves the string unterminated if it was truncated
         * to fit; the buffer is one byte larger than advertised
         * precisely to give this an always-safe place to land */
        name[HOST_NAME_MAX] = '\0';
        is_read = true;
    }
    return name;
}


/* Initialize a new client, adopting an existing X window under
 * window manager control */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        xcb_window_t window,
        const config_td *config)
{
    client_td *client;
    xcb_get_geometry_reply_t *geom_reply;
    xcb_get_window_attributes_reply_t *attr_reply;
    char wm_class[256];
    char wm_instance[256];
    char net_wm_name[256];
    uint32_t ewmh_pid;
    xcb_icccm_get_text_property_reply_t machine_prop;
    uint32_t utime;
    xcb_window_t user_time_window;
    struct s_client_cookies_init_s ck;
    xcb_atom_t client_leader_atom;
    xcb_atom_t motif_hints_atom;
    uint32_t user_time_window_raw;

    LOGGER_TRACE("Attempting to manage existing window %#x", window);

    /* Reject override-redirect windows, for they manage themselves */
    attr_reply = xcb_get_window_attributes_reply(connection,
            xcb_get_window_attributes(connection, window), NULL);
    if (attr_reply != NULL) {
        bool skip = attr_reply->override_redirect;
        free(attr_reply);
        if (skip) {
            LOGGER_TRACE("Skipping override-redirect window %#x",
                    window);
            return NULL;
        }
    }

    client = calloc(1, sizeof(client_td));
    if (client == NULL) {
        LOGGER_ERROR("Failed to allocate memory for managed client",
                L_NARG);
        return NULL;
    }

    s_client_common_init(client, config);

    /* Use the X window ID as both window handle and hash/lookup key */
    client->window = window;
    client->id = window;

    /* Query existing geometry */
    /* Every independent request goes out here, before a single reply
     * is awaited, so that the whole set costs one round trip to the
     * server rather than one apiece.  See
     * 'struct s_client_cookies_init_s' for what is deliberately left
     * out of the batch and why.
     *
     * The two atoms are interned first because a request needs them,
     * and 'atom_intern' answers from its cache after the first
     * window, so they cost no round trip of their own here. */
    client_leader_atom = atom_intern(connection, "WM_CLIENT_LEADER",
            true);
    motif_hints_atom = atom_intern(connection, "_MOTIF_WM_HINTS", true);

    ck.geometry = xcb_get_geometry(connection, window);
    ck.wm_protocols = xcb_icccm_get_wm_protocols(connection, window,
            ewmh->WM_PROTOCOLS);
    ck.wm_hints = xcb_icccm_get_wm_hints(connection, window);
    ck.client_leader = xcb_get_property(connection, 0, window,
            client_leader_atom, XCB_ATOM_WINDOW, 0, 1);
    ck.transient_for = xcb_icccm_get_wm_transient_for(connection,
            window);
    ck.strut_partial = xcb_ewmh_get_wm_strut_partial(ewmh, window);
    ck.window_type = xcb_ewmh_get_wm_window_type(ewmh, window);
    ck.motif_hints = xcb_get_property(connection, 0, window,
            motif_hints_atom, motif_hints_atom, 0, 5);
    ck.wm_state = xcb_ewmh_get_wm_state(ewmh, window);
    ck.wm_pid = xcb_ewmh_get_wm_pid(ewmh, window);
    ck.client_machine = xcb_icccm_get_wm_client_machine(connection,
            window);
    ck.user_time_window = xcb_ewmh_get_wm_user_time_window(ewmh,
            window);

    geom_reply = xcb_get_geometry_reply(connection, ck.geometry, NULL);
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
    (void) snprintf(client->info.name,
            CONFIG_MAX_LENGTH_NAME - 1, "Window %#x", window);
    (void) snprintf(client->info.visible_name,
            CONFIG_MAX_LENGTH_NAME - 1, "Window %#x", window);

    /* Read '_NET_WM_NAME', which is UTF-8, and fall back to
     * 'WM_NAME', which is Latin-1 */
    client_props_get_net_wm_name(ewmh, window, net_wm_name,
            sizeof(net_wm_name));
    if (net_wm_name[0] != '\0') {
        s_client_display_name_set(client, net_wm_name);
    } else {
        char wm_name[256];
        client_props_get_wm_name(connection, window, wm_name,
                sizeof(wm_name));
        s_client_display_name_set(client, wm_name);
    }

    /* Read '_NET_WM_ICON_NAME'/'WM_ICON_NAME' for iconified caption */
    client_props_refresh_icon_name(client);

    /* Read 'WM_CLASS' */
    client_props_get_wm_class(connection, window,
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
    s_client_read_wm_protocols(connection, ewmh, window, client,
            &ck);

    /* Read 'WM_HINTS', 'WM_CLIENT_LEADER', and 'WM_TRANSIENT_FOR' */
    s_client_read_wm_hints_and_leader(connection, client, &ck);

    /* Read 'WM_NORMAL_HINTS': size constraints and increment grid */
    client_props_refresh_normal_hints(client);

    /* ICCCM §4.1.8: read 'WM_COLORMAP_WINDOWS', then subscribe to
     * 'ColormapNotify' on whichever subwindows it lists, so a later
     * change to any of their own colormap attribute is caught even
     * between refreshes of the list itself */
    client_props_refresh_colormap_windows(client);
    client_subscribe_colormap_windows(connection, client);

    /* Read '_NET_WM_STRUT_PARTIAL' for dock/panel windows */
    s_client_read_struts(ewmh, window, client, &ck);

    /* Read '_NET_WM_WINDOW_TYPE' to determine client type and
     * decoration */
    s_client_read_window_type(connection, ewmh, client, &ck);

    /* Read '_MOTIF_WM_HINTS': see the sibling function's comment for
     * the full rationale */
    s_client_read_motif_hints(connection, client, &ck);

    if (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK &&
            ewmh != NULL) {
        client_pin(client);
        client_skip_taskbar(client);
        client_skip_pager(client);
        ccmd_client_sync_states(client);
    }

    /* Read the pre-existing '_NET_WM_STATE' property; see the sibling
     * function's comment for the full explanation */
    s_client_read_pre_existing_state(connection, window, client, &ck);

    /* Read '_NET_WM_PID': associate X window with its owning process */
    ewmh_pid = 0u;
    if (xcb_ewmh_get_wm_pid_reply(ewmh, ck.wm_pid,
                &ewmh_pid, NULL)) {
        client->process.pid = (int) ewmh_pid;
    }

    /* Read 'WM_CLIENT_MACHINE': a bare '_NET_WM_PID' names a process
     * table this window manager shares only when the two agree on
     * which host that table belongs to.  A client silent on the
     * matter, same as one naming some other host, leaves
     * 'pid_is_local' at its 'false' default, since a PID is never
     * safe to act on without that confirmation. */
    if (xcb_icccm_get_wm_client_machine_reply(connection,
                ck.client_machine, &machine_prop, NULL)) {
        const char *local_name = s_local_hostname();
        size_t local_len = strlen(local_name);
        client->process.pid_is_local = (local_len > 0u) &&
            (machine_prop.name_len == local_len) &&
            (strncmp(machine_prop.name, local_name, local_len) == 0);
        xcb_icccm_get_text_property_reply_wipe(&machine_prop);
    }

    /* Read '_NET_WM_USER_TIME': used for initial focus policy.
     * Checked on '_NET_WM_USER_TIME_WINDOW' first.  Some toolkits
     * (GTK among them) set the frequently-changing
     * '_NET_WM_USER_TIME' on a dedicated, often-unmapped window
     * instead of the client's toplevel, specifically so that
     * every tool interested in any of the toplevel's other
     * properties is not woken up on every keypress (EWMH §5.16); a
     * client relying on that indirection would otherwise never have
     * its genuine value seen here at all, always reading as the
     * default 0 instead. */
    utime = 0u;
    user_time_window = window;
    user_time_window_raw = 0u;
    if (xcb_ewmh_get_wm_user_time_window_reply(ewmh,
                ck.user_time_window,
                &user_time_window_raw, NULL) &&
            user_time_window_raw != 0u) {
        user_time_window = (xcb_window_t) user_time_window_raw;
    }
    if (xcb_ewmh_get_wm_user_time_reply(ewmh,
                xcb_ewmh_get_wm_user_time(ewmh, user_time_window),
                &utime, NULL)) {
        client->user_time = utime;
    }

    /* Publish initial '_NET_WM_ALLOWED_ACTIONS' */
    ccmd_client_update_allowed_actions(client);

    /* Subscribe to events, apply border width, and set the default
     * cursor.  The sibling function's comment explains it in full. */
    s_client_events_subscribe(connection, window, client);

    /* Ignore return value, as decoration creation is non-fatal here */
    (void) ci_create_decorations(client);

    /* Only grab buttons on client windows that the window manager
     * decorates or that could receive focus.  Dock and notification
     * windows manage their pointer events; grabbing buttons on them
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
        if (ewmh != NULL) {
            xcb_ewmh_set_wm_state(ewmh,
                    client->window, 0, NULL);
        }
    }

    ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL, XCB_NONE);

    /* Mark the client as needing a full geometry configure and repaint
     * on the first render pass so the decoration and content area are
     * correctly sized and positioned from the outset */
    client->is_outdated = true;

    /* Into the focus order, at its far end.  It exists, so a fallback
     * must be able to reach it, but nobody has worked in it yet */
    focus_order_add(client);

    scratchpad_notice_client_created(client);

    LOGGER_TRACE("Now managing window %#x ('%s')",
            window, client->info.name);

    return client;
}


/* Send this client's border color and opacity for the focus state */
void client_border_color_apply(client_td *client, bool is_focused)
{
    uint32_t color;
    uint8_t opacity_percent;

    if (client == NULL || xcb_connection_get() == NULL ||
            client->config == NULL || client_is_fullscreen(client) ||
            (client_is_decorated(client) && client->frame != 0)) {
        return;
    }

    if (client->border_override.is_set) {
        color = client->border_override.color;
    } else if (is_focused) {
        color = client->config->theme.window.active.border.color;
    } else {
        color = client->config->theme.window.inactive.border.color;
    }

    if (is_focused) {
        opacity_percent = (client->opacity_override.is_set_active)
            ? client->opacity_override.active
            : client->config->theme.window.active.opacity;
    } else {
        opacity_percent = (client->opacity_override.is_set_inactive)
            ? client->opacity_override.inactive
            : client->config->theme.window.inactive.opacity;
    }

    /* Sent only when it would actually change, the render pass
     * reaching every client on the desktop on every turn */
    if (color == client->last_border_color) {
        return;
    }
    client->last_border_color = color;

    xcb_change_window_attributes(xcb_connection_get(), client->window,
            XCB_CW_BORDER_PIXEL, &color);
    atom_set_window_opacity(xcb_connection_get(), client->window,
            config_theme_opacity_to_raw(opacity_percent));
}
