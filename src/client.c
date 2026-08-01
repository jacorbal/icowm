/**
 * @file client.c
 *
 * @brief Client structure management
 *
 * Owns allocation, initialisation, adoption (manage), update and
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
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy, memset, snprintf */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>
#include <utils/safeflg.h>

/* Type includes */
#include <types/pair.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/util.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/wm.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <config.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <priority.h>

/* Local includes */
#include <client/internal.h>


/* Initialize a new client with the specified parameters */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        xcb_window_t parent_id,
        uint32_t w, uint32_t h, int32_t x, int32_t y,
        struct config_theme_s *theme,
        const struct config_base_s *config_base)
{
    client_td *client;
    xcb_void_cookie_t create_cookie;
    xcb_generic_error_t *err;
    uint32_t mask;
    uint32_t values[3];
    char wm_name[256];
    char wm_class[256];
    char wm_instance[256];

    LOGGER_TRACE("Initializing client at (%d, %d)" \
            " with size %ux%u", x, y, w, h);

    /* Allocate memory for the client structure and verify the
     * allocation was successful before proceeding */
    client = malloc(sizeof(client_td));
    if (client == NULL) {
        LOGGER_ERROR("Failed to allocate memory for client", L_NARG);
        return NULL;
    }

    /* Initialize basic client properties and connections */
    client->connection = connection;
    client->ewmh = ewmh;
    client->parent_id = parent_id;
    client->user_time = 0;
    client->theme = theme;
    client->config_base = config_base;
    client->process.pid = -1;
    client->process.command = NULL;

    client->frame = 0;
    client->titlebar = 0;
    client->icon_window = 0;
    client->is_icon_mapped = false;
    client->was_decorated_fullscreen = false;
    client->ignore_unmap = 0;
    client->ignore_focus_unmap = 0;
    client->icon_x = -1;
    client->icon_y = -1;

    /* Set the current geometry, and the "old" as the current one.
     * This allows for saved state when resizing or maximizing */
    client->layout.geometry.cur =
        (struct geometry_s) {.pos = {.x = x, .y = y},
                             .dim = {.w = w, .h = h}};
    client->layout.geometry.old = client->layout.geometry.cur;

    /* Initialize strut and frame extents to zero.
     * These are used for panel and decoration management */
    client->layout.strut_partial.sides =
        (struct sides_s) {0, 0, 0, 0};
    client->layout.strut_partial.start =
        (struct sides_s) {0, 0, 0, 0};
    client->layout.strut_partial.end =
        (struct sides_s) {0, 0, 0, 0};
    client->layout.frame_extents =
        (struct sides_s) {0, 0, 0, 0};
    client->layout.gravity = CLIENT_GRAVITY_NORTH_WEST;

    /* Initialize client properties with sensible defaults */
    client->properties.flags =
        CLIENT_FLAG_HIDDEN | CLIENT_FLAG_FOCUSABLE |
        CLIENT_FLAG_RESIZABLE;
    client->properties.type = CLIENT_TYPE_NORMAL;
    client->properties.state = CLIENT_STATE_NORMAL;
    client->properties.layer = CLIENT_LAYER_NORMAL;
    client->properties.operation = CLIENT_OPERATION_IDLE;
    client->properties.focusing = CLIENT_FOCUSING_UNFOCUSED;
    ci_set_decoration_defaults(client, theme);

    /* Allocate string buffers for client information */
    if (ci_alloc_strings(client) != 0) {
        LOGGER_ERROR("Failed to allocate string buffers for client",
                L_NARG);
        free(client);
        return NULL;
    }

    /* Initialize default name strings */
    snprintf(client->info.name,
            CONFIG_MAX_LENGTH_NAME - 1, "Client %p", (void *) client);
    snprintf(client->info.visible_name,
            CONFIG_MAX_LENGTH_NAME - 1, "Client %p", (void *) client);

    /* Create the XCB window that represents this client */
    client->window = xcb_generate_id(connection);

    /* Set window attributes using values from the theme configuration.
     * Background pixel, border pixel, and event mask are theme-aware */
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
           XCB_CW_EVENT_MASK;
    values[0] = theme->window.inactive.background_color;
    values[1] = theme->window.inactive.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE         |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_PROPERTY_CHANGE  |
                XCB_EVENT_MASK_ENTER_WINDOW     |
                XCB_EVENT_MASK_LEAVE_WINDOW     |
                XCB_EVENT_MASK_FOCUS_CHANGE     |
                XCB_EVENT_MASK_BUTTON_PRESS;

    /* Create the window with checked cookie to detect immediate errors */
    create_cookie = xcb_create_window_checked(
            connection,
            XCB_COPY_FROM_PARENT,
            client->window,
            parent_id,
            (int16_t) x, (int16_t) y,
            (uint16_t) w, (uint16_t) h,
            (uint16_t) theme->window.general.border_width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    err = xcb_request_check(connection, create_cookie);
    if (err != NULL) {
        LOGGER_ERROR("Failed to create X client window", L_NARG);
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        safe_free((void **) &client->icon_info.icon_name);
        safe_free((void **) &client->icon_info.visible_icon_name);
        free(err);
        free(client);
        return NULL;
    }

    /* Generate unique client identifier based on the address of the
     * allocated client structure */
    client->id = (xcb_window_t) (uintptr_t) client;

    /* Attempt to retrieve 'WM_NAME' property from the X server to
     * populate the client's name field instead of using a default */
    ci_get_wm_name(connection, parent_id, wm_name, sizeof(wm_name));
    if (wm_name[0] != '\0') {
        safe_strncpy(client->info.name, wm_name, CONFIG_MAX_LENGTH_NAME - 1);
        safe_strncpy(client->info.visible_name, wm_name,
                CONFIG_MAX_LENGTH_NAME - 1);
    }

    /* Attempt to retrieve 'WM_CLASS' property from the X server to
     * populate the client's class and instance names */
    ci_get_wm_class(connection, parent_id,
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

    /* Map the window to make it visible on the screen and flush the
     * output buffer to ensure the request is sent */
    xcb_map_window(connection, client->window);
    xcb_flush(connection);

    LOGGER_TRACE("Created X window %#x for client %p" \
            " with name '%s'",
            client->window, (void *) client, client->info.name);

    return client;
}


/* Destroy the specified client and free associated resources */
void client_destroy(client_td *client)
{
    if (client == NULL) {
        return;
    }

    LOGGER_DEBUG("Destroying client %p (window %#x, name '%s')",
            (void *) client, client->window, client->info.name);

    /* Destroy the XCB window representation and flush the output buffer
     * to ensure the request is processed */
    if (client->connection != NULL && client->window != 0) {
        xcb_destroy_window(client->connection, client->window);
        xcb_flush(client->connection);
    }

    /* Destroy decorations if any */
    if (client->connection != NULL && client->titlebar != 0) {
        xcb_destroy_window(client->connection, client->titlebar);
    }
    if (client->connection != NULL && client->icon_window != 0) {
        xcb_destroy_window(client->connection, client->icon_window);
    }
    if (client->connection != NULL && client->frame != 0) {
        xcb_destroy_window(client->connection, client->frame);
    }

    /* Free all allocated string buffers */
    safe_free((void **) &client->info.name);
    safe_free((void **) &client->info.visible_name);
    safe_free((void **) &client->info.role_name);
    safe_free((void **) &client->info.class_name[0]);
    safe_free((void **) &client->info.class_name[1]);
    safe_free((void **) &client->icon_info.icon_name);
    safe_free((void **) &client->icon_info.visible_icon_name);
    safe_free((void **) &client->icon_info.icons);
    safe_free((void **) &client->process.command);

    /* Free the client structure itself */
    free(client);
}


/* Adopt an existing X window under window manager control */
client_td *client_manage(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        xcb_window_t window,
        struct config_theme_s *theme,
        const struct config_base_s *config_base)
{
    client_td *client;
    xcb_get_geometry_cookie_t geom_cookie;
    xcb_get_geometry_reply_t *geom_reply;
    xcb_get_window_attributes_cookie_t attr_cookie;
    xcb_get_window_attributes_reply_t *attr_reply;
    uint32_t values[1];
    char wm_name[256];
    char wm_class[256];
    char wm_instance[256];
    char net_wm_name[256];
    xcb_window_t transient = XCB_WINDOW_NONE;
    xcb_size_hints_t hints;
    xcb_ewmh_get_extents_reply_t strut;
    xcb_ewmh_wm_strut_partial_t partial;
    xcb_ewmh_get_atoms_reply_t type_reply;
    xcb_intern_atom_reply_t *ia;
    xcb_atom_t wm_delete_atom = XCB_ATOM_NONE;
    xcb_atom_t wm_take_focus_atom = XCB_ATOM_NONE;
    xcb_atom_t net_wm_ping_atom = XCB_ATOM_NONE;
    xcb_icccm_get_wm_protocols_reply_t proto;
    xcb_icccm_wm_hints_t wm_hints;
    uint32_t bw[1];
    uint32_t ewmh_pid;
    uint32_t utime;

    LOGGER_TRACE("Attempting to manage existing window %#x", window);

    /* Reject override-redirect windows, for they manage themselves */
    attr_cookie = xcb_get_window_attributes(connection, window);
    attr_reply = xcb_get_window_attributes_reply(connection,
            attr_cookie, NULL);
    if (attr_reply != NULL) {
        bool skip = attr_reply->override_redirect;
        free(attr_reply);
        if (skip) {
            LOGGER_TRACE("Skipping override-redirect window %#x",
                    window);
            return NULL;
        }
    }

    client = malloc(sizeof(client_td));
    if (client == NULL) {
        LOGGER_ERROR("Failed to allocate memory for managed client",
                L_NARG);
        return NULL;
    }

    /* Zero-initialized to prevent uninitialized reads */
    memset(client, 0, sizeof(client_td));

    /* 'ICCCM WM_HINTS': input defaults to 'true' when the hint is
     * absent */
    client->wm_input_hint = true;

    /* Basic connections */
    client->connection = connection;
    client->ewmh = ewmh;
    client->theme = theme;
    client->config_base = config_base;
    client->frame = 0;
    client->titlebar = 0;
    client->icon_window = 0;
    client->is_icon_mapped = false;
    client->ignore_focus_unmap = 0;
    client->was_decorated_fullscreen = false;
    client->ignore_unmap = 0;
    client->icon_x = -1;
    client->icon_y = -1;
    client->process.pid = -1;

    /* Use the X window ID as both window handle and hash/lookup key */
    client->window = window;
    client->id = window;

    /* Query existing geometry */
    geom_cookie = xcb_get_geometry(connection, window);
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
    client->layout.gravity = CLIENT_GRAVITY_NORTH_WEST;

    /* Default properties: visible, focusable, resizable */
    client->properties.flags = CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_RESIZABLE;
    client->properties.type = CLIENT_TYPE_NORMAL;
    client->properties.state = CLIENT_STATE_NORMAL;
    client->properties.layer = CLIENT_LAYER_NORMAL;
    client->properties.operation = CLIENT_OPERATION_IDLE;
    client->properties.focusing = CLIENT_FOCUSING_UNFOCUSED;
    ci_set_decoration_defaults(client, theme);

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
        safe_strncpy(client->info.name, net_wm_name,
                CONFIG_MAX_LENGTH_NAME - 1);
        safe_strncpy(client->info.visible_name, net_wm_name,
                CONFIG_MAX_LENGTH_NAME - 1);
    } else {
        ci_get_wm_name(connection, window, wm_name, sizeof(wm_name));
        if (wm_name[0] != '\0') {
            safe_strncpy(client->info.name, wm_name,
                    CONFIG_MAX_LENGTH_NAME - 1);
            safe_strncpy(client->info.visible_name, wm_name,
                    CONFIG_MAX_LENGTH_NAME - 1);
        }
    }

    /* Read WM_CLASS */
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

    /* Read 'WM_PROTOCOLS': cache 'WM_DELETE_WINDOW', 'WM_TAKE_FOCUS',
     * and '_NET_WM_PING' support */
    ia = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, 1, 16, "WM_DELETE_WINDOW"),
            NULL);
    if (ia != NULL) {
        wm_delete_atom = ia->atom;
        free(ia);
    }

    ia = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, 1, 14, "WM_TAKE_FOCUS"),
            NULL);
    if (ia != NULL) {
        wm_take_focus_atom = ia->atom;
        free(ia);
    }

    ia = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, 1, 12, "_NET_WM_PING"),
            NULL);
    if (ia != NULL) {
        net_wm_ping_atom = ia->atom;
        free(ia);
    }

    client->wm_delete_atom = wm_delete_atom;
    client->has_wm_delete_window = false;
    client->wm_take_focus_atom = wm_take_focus_atom;
    client->has_wm_take_focus = false;
    client->has_net_wm_ping = false;
    memset(&proto, 0, sizeof(proto));
    if (xcb_icccm_get_wm_protocols_reply(connection,
                xcb_icccm_get_wm_protocols(connection, window,
                    ewmh->WM_PROTOCOLS),
                &proto, NULL)) {
        for (uint32_t pi = 0; pi < proto.atoms_len; ++pi) {
            if (proto.atoms[pi] == wm_delete_atom) {
                client->has_wm_delete_window = true;
                break;
            } else if (proto.atoms[pi] == wm_take_focus_atom) {
                client->has_wm_take_focus = true;
            } else if (proto.atoms[pi] == net_wm_ping_atom) {
                client->has_net_wm_ping = true;
            }
        }
        xcb_icccm_get_wm_protocols_reply_wipe(&proto);
    }

    /* Read 'WM_HINTS': input model and window group */
    memset(&wm_hints, 0, sizeof(wm_hints));
    if (xcb_icccm_get_wm_hints_reply(connection,
                xcb_icccm_get_wm_hints(connection, window),
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
            client_set_urgent(client);
        }
    }

    /* Read 'WM_TRANSIENT_FOR': identify dialogs and their parent */
    client->transient_for = XCB_WINDOW_NONE;
    if (xcb_icccm_get_wm_transient_for_reply(connection,
                xcb_icccm_get_wm_transient_for(connection, window),
                &transient, NULL)) {
        client->transient_for = transient;
    }

    /* Read 'WM_NORMAL_HINTS': size constraints and increment grid */
    memset(&hints, 0, sizeof(hints));
    memset(&client->size_hints, 0, sizeof(client->size_hints));
    if (xcb_icccm_get_wm_normal_hints_reply(connection,
                xcb_icccm_get_wm_normal_hints(connection, window),
                &hints, NULL)) {
        client->size_hints.valid = true;

        if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
            client->size_hints.min_w = (int32_t) hints.min_width;
            client->size_hints.min_h = (int32_t) hints.min_height;
        }

        if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
            client->size_hints.max_w = (int32_t) hints.max_width;
            client->size_hints.max_h = (int32_t) hints.max_height;
        }

        if (hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
            client->size_hints.base_w = (int32_t) hints.base_width;
            client->size_hints.base_h = (int32_t) hints.base_height;
        }

        if (hints.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
            client->size_hints.inc_w = (int32_t) hints.width_inc;
            client->size_hints.inc_h = (int32_t) hints.height_inc;
        }

        if (hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY) {
            client->layout.gravity =
                (uint16_t) hints.win_gravity;
        }

        if ((hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) &&
                (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) &&
                hints.min_width > 0 && hints.min_height > 0 &&
                hints.min_width == hints.max_width &&
                hints.min_height == hints.max_height) {
            client_unset_resizable(client);
        }
    }

    /* Read '_NET_WM_STRUT_PARTIAL' for dock/panel windows */
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

    /* Read '_NET_WM_WINDOW_TYPE' to determine client type and
     * decoration */
    memset(&type_reply, 0, sizeof(type_reply));
    if (xcb_ewmh_get_wm_window_type_reply(ewmh,
                xcb_ewmh_get_wm_window_type(ewmh, window),
                &type_reply, NULL)) {
        xcb_atom_t atom_notification = XCB_ATOM_NONE;
        xcb_intern_atom_reply_t *notif_ia;

        notif_ia = xcb_intern_atom_reply(connection,
                xcb_intern_atom(connection, 1,
                    sizeof("_NET_WM_WINDOW_TYPE_NOTIFICATION") - 1u,
                    "_NET_WM_WINDOW_TYPE_NOTIFICATION"), NULL);
        if (notif_ia != NULL) {
            atom_notification = notif_ia->atom;
            free(notif_ia);
        }

        for (uint32_t ti = 0; ti < type_reply.atoms_len; ++ti) {
            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_DOCK) {
                client->properties.type = CLIENT_TYPE_DOCK;
                client_unset_decoration(client);
                client_set_sticky(client);
                client_set_skip_taskbar(client);
                client_set_skip_pager(client);
                client->layout.frame_extents.left = 0;
                client->layout.frame_extents.right = 0;
                client->layout.frame_extents.top = 0;
                client->layout.frame_extents.bottom = 0;
                client->properties.layer = CLIENT_LAYER_ABOVE;
                client_unset_focusable(client);
                break;
            }

            if (atom_notification != XCB_ATOM_NONE &&
                    type_reply.atoms[ti] == atom_notification) {
                client->properties.type = CLIENT_TYPE_NOTIFICATION;
                client_unset_decoration(client);
                client->layout.frame_extents.left = 0;
                client->layout.frame_extents.right = 0;
                client->layout.frame_extents.top = 0;
                client->layout.frame_extents.bottom = 0;
                client->properties.layer = CLIENT_LAYER_ABOVE;
                client_unset_focusable(client);
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
                client_unset_decoration(client);
                break;
            }

            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_SPLASH) {
                client->properties.type = CLIENT_TYPE_SPLASH;
                client_unset_decoration(client);
                break;
            }

            if (type_reply.atoms[ti] == ewmh->_NET_WM_WINDOW_TYPE_UTILITY) {
                client->properties.type = CLIENT_TYPE_UTILITY;
                break;
            }
        }
        xcb_ewmh_get_atoms_reply_wipe(&type_reply);
    }

    if (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK &&
            client->ewmh != NULL) {
        wcmd_add_states(client, 3,
                "_NET_WM_STATE_STICKY",
                "_NET_WM_STATE_SKIP_TASKBAR",
                "_NET_WM_STATE_SKIP_PAGER");
    }

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
    wcmd_client_update_allowed_actions(client);

    /* Subscribe to events on the adopted window.
     * For dock and notification windows, preserve the application's
     * event mask (which includes 'ButtonPress'/'ButtonRelease' needed
     * for systray interaction) and OR in only the WM's required events.
     * Replacing the mask wholesale would strip 'ButtonPress', making
     * systray icons non-interactive after a 'PassiveGrab replay. */
    if (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK ||
            client->properties.type ==
                (uint16_t) CLIENT_TYPE_NOTIFICATION) {
        xcb_get_window_attributes_cookie_t wac =
            xcb_get_window_attributes(connection, window);
        xcb_get_window_attributes_reply_t *war =
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
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    }

    /* Subscribe to events on the adopted window */
    values[0] = XCB_EVENT_MASK_ENTER_WINDOW     |
                XCB_EVENT_MASK_LEAVE_WINDOW     |
                XCB_EVENT_MASK_FOCUS_CHANGE     |
                XCB_EVENT_MASK_PROPERTY_CHANGE  |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(connection, window,
            XCB_CW_EVENT_MASK, values);

    /* Apply border width from theme; dock windows always get 0 */
    bw[0] = (client->properties.type == (uint16_t) CLIENT_TYPE_DOCK)
        ? 0u
        : ((theme != NULL) ? theme->window.general.border_width : 0u);
    xcb_configure_window(connection, window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, bw);

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
        wcmd_client_grab_buttons(client);
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

    wcmd_set_wm_state(client, WCMD_WM_STATE_NORMAL, XCB_NONE);

    LOGGER_TRACE("Now managing window %#x ('%s')",
            window, client->info.name);

    return client;
}


/* Update the content of the specified client */
void client_update(client_td *client)
{
    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Updated client %p (window %#x)",
            (void *) client, client->window);
}
