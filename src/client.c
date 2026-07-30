/**
 * @file client.c
 *
 * @brief Client (window) management implementation
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
#include <string.h>     /* memcpy, strlen */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>
#include <utils/safeflg.h>

/* Default initial values */
#include <defs/wm.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <config.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <priority.h>


/**
 * @brief Retrieve the @c WM_NAME property of a window
 *
 * Attempts to fetch the @c WM_NAME atom from the specified window to
 * retrieve the client's human-readable name from the X server.
 *
 * @param connection Pointer to the XCB connection
 * @param window     Window ID to query
 * @param buffer     Destination buffer for the name
 * @param buffer_sz  Size of the destination buffer
 *
 * @return Length of name retrieved on success, 0 otherwise
 *
 * @note Complexity: @e O(1)
 */
static size_t s_client_get_wm_name(xcb_connection_t *connection,
        xcb_window_t window, char *buffer, size_t buffer_sz)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    size_t name_len = 0;

    if (buffer == NULL || buffer_sz == 0) {
        return 0;
    }

    cookie = xcb_get_property(connection, 0, window,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(connection, cookie, NULL);

    if (reply != NULL && reply->value_len > 0) {
        name_len = (reply->value_len < buffer_sz - 1) ?
            reply->value_len : buffer_sz - 1;
        memcpy(buffer, xcb_get_property_value(reply), name_len);
        buffer[name_len] = '\0';
        free(reply);
    } else {
        buffer[0] = '\0';
        if (reply != NULL) {
            free(reply);
        }
    }

    return name_len;
}


/**
 * @brief Retrieve the @c _NET_WM_NAME property of a window (UTF-8)
 *
 * Attempts to fetch the @c _NET_WM_NAME EWMH atom from the specified
 * window.  Falls back silently (returns 0) when the property is absent.
 *
 * @param ewmh      Pointer to the EWMH connection
 * @param window    Window ID to query
 * @param buffer    Destination buffer for the name
 * @param buffer_sz Size of the destination buffer
 *
 * @return Length of name retrieved on success, 0 otherwise
 *
 * @note Complexity: @e O(1)
 */
static size_t s_client_get_net_wm_name(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, char *buffer, size_t buffer_sz)
{
    xcb_ewmh_get_utf8_strings_reply_t reply;
    size_t len = 0;

    if (ewmh == NULL || buffer == NULL || buffer_sz == 0) {
        return 0;
    }

    memset(&reply, 0, sizeof(reply));
    if (xcb_ewmh_get_wm_name_reply(ewmh,
                xcb_ewmh_get_wm_name(ewmh, window),
                &reply, NULL) && reply.strings_len > 0) {
        len = (reply.strings_len < buffer_sz - 1u)
            ? reply.strings_len : buffer_sz - 1u;
        memcpy(buffer, reply.strings, len);
        buffer[len] = '\0';
        xcb_ewmh_get_utf8_strings_reply_wipe(&reply);
    } else {
        buffer[0] = '\0';
    }

    return len;
}


/**
 * @brief Retrieve the @c WM_CLASS property of a window
 *
 * Fetches the @c WM_CLASS atom from the window.  The @c WM_CLASS
 * property consists of two null-terminated strings: instance name and
 * class name, both separated by a null terminator.
 *
 * @param connection Pointer to the XCB connection
 * @param window     Window ID to query
 * @param class_buf  Destination buffer for class name
 * @param class_sz   Size of class buffer
 * @param inst_buf   Destination buffer for instance name (may be null)
 * @param inst_sz    Size of instance buffer (ignored when @p inst_buf
 *                   is null)
 *
 * @return 0 on success, -1 otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the property
 *       value
 */
static int s_client_get_wm_class(xcb_connection_t *connection,
        xcb_window_t window, char *class_buf, size_t class_sz,
        char *inst_buf, size_t inst_sz)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    char *value;
    size_t value_len;
    size_t inst_len;
    size_t copy_len;

    if (class_buf == NULL || class_sz == 0) {
        return -1;
    }

    cookie = xcb_get_property(connection, 0, window,
            XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(connection, cookie, NULL);

    class_buf[0] = '\0';
    if (inst_buf != NULL) {
        inst_buf[0] = '\0';
    }

    if (reply != NULL && reply->value_len > 0) {
        value = (char *) xcb_get_property_value(reply);
        value_len = reply->value_len;

        /* 'WM_CLASS' format: "instance\0class\0"
         * Find the first null terminator to separate the two strings */
        inst_len = 0;
        for (size_t i = 0; i < value_len; ++i) {
            if (value[i] == '\0') {
                inst_len = i;
                break;
            }
        }

        /* Copy instance name to the destination buffer if provided and
         * if there is data to copy */
        if (inst_buf != NULL && inst_len > 0) {
            copy_len = (inst_len < inst_sz - 1) ?
                inst_len : inst_sz - 1;
            memcpy(inst_buf, value, copy_len);
            inst_buf[copy_len] = '\0';
        }

        /* Copy class name to the destination buffer.
         * Class starts after instance_name + 1 (skip null terminator) */
        if (inst_len + 1 < value_len) {
            size_t class_len = value_len - inst_len - 1;
            copy_len = (class_len < class_sz - 1) ?
                class_len : class_sz - 1;
            memcpy(class_buf, value + inst_len + 1, copy_len);
            class_buf[copy_len] = '\0';
        }

        free(reply);
        return 0;
    }

    if (reply != NULL) {
        free(reply);
    }

    return -1;
}


/**
 * @brief Apply decoration defaults from the loaded theme
 *
 * Initializes the client's decoration-related fields using the
 * currently selected theme.  This includes the titlebar height, frame
 * extents, and the decorated state of the client.
 *
 * @param client Pointer to the client to update
 * @param theme  Pointer to the theme providing decoration settings
 *
 * @note Complexity: @e O(1)
 */
static void s_client_set_decoration_defaults(client_td *client,
        struct config_theme_s *theme)
{
    uint16_t border_width = 0;

    if (client == NULL) {
        return;
    }

    client->title_height = 22;
    if (theme != NULL) {
        border_width = (uint16_t) theme->window.general.border_width;
    }

    if (theme != NULL && theme->window.general.is_decorated) {
        client_set_decoration(client);
        client->layout.frame_extents.left = border_width;
        client->layout.frame_extents.right = border_width;
        client->layout.frame_extents.top =
            (uint16_t) (border_width + client->title_height);
        client->layout.frame_extents.bottom = border_width;
    } else {
        client_unset_decoration(client);
        client->layout.frame_extents = (struct sides_s) {0, 0, 0, 0};
    }
}


/**
 * @brief Reconfigure a decorated client's child windows to match extents
 *
 * Applies the current cached frame extents to the reparented client
 * window and titlebar so the themed border area, titlebar, and client
 * content stay aligned after geometry changes.
 *
 * @param client Pointer to the decorated client to synchronize
 *
 * @note Complexity: @e O(1)
 */
static void s_client_sync_decoration_layout(client_td *client)
{
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
    uint16_t title_h;
    uint16_t inner_w;
    uint16_t inner_h;
    uint16_t title_y;

    if (client == NULL || client->frame == 0 || !client_is_decorated(client)) {
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    top = (uint16_t) client->layout.frame_extents.top;
    bottom = (uint16_t) client->layout.frame_extents.bottom;
    title_h = client->title_height;
    title_y = (top > title_h) ? (uint16_t) (top - title_h) : 0u;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w - left - right)
        : WM_MIN_WINDOW_DIMENSION;
    inner_h = (client->layout.geometry.cur.dim.h > top + bottom)
        ? (uint16_t) (client->layout.geometry.cur.dim.h - top - bottom)
        : WM_MIN_WINDOW_DIMENSION;

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                left, top, inner_w, inner_h
            });

    if (client->titlebar != 0) {
        xcb_configure_window(client->connection, client->titlebar,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    left, title_y, inner_w, title_h
                });
    }
}


/**
 * @brief Create frame and titlebar windows for a decorated client
 *
 * Creates the outer frame window and the titlebar window associated
 * with a client, then reparents the client window into that frame.
 * The frame geometry is computed from the current client geometry and
 * the configured frame extents.
 *
 * @param client Pointer to client for which decorations are created
 *
 * @return 0 on success or if decoration creation is skipped, or
 *         otherwise
 *
 * @note If the client is not decorated, has no theme, or has no valid
 *       parent window, the function returns without creating decoration
 *       windows
 * @note On success, the client's stored geometry is updated to match
 *       the newly created frame dimensions and position
 * @note Complexity: @e O(1)
 */
static int s_client_create_decorations(client_td *client)
{
    uint32_t mask;
    uint32_t values[3];
    uint16_t frame_w;
    uint16_t frame_h;
    int16_t frame_x;
    int16_t frame_y;
    int32_t frame_x32;
    int32_t frame_y32;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
    uint16_t inner_w;
    uint16_t title_h;
    uint16_t title_y;

    if (client == NULL || !client_is_decorated(client) ||
            client->theme == NULL || client->parent_id == 0) {
        return 0;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    top = (uint16_t) client->layout.frame_extents.top;
    bottom = (uint16_t) client->layout.frame_extents.bottom;
    inner_w = (uint16_t) client->layout.geometry.cur.dim.w;
    title_h = client->title_height;
    title_y = (top > title_h) ? (uint16_t) (top - title_h) : 0u;

    frame_x32 = client->layout.geometry.cur.pos.x - (int32_t) left;
    frame_y32 = client->layout.geometry.cur.pos.y - (int32_t) top;
    if (frame_x32 < INT16_MIN) {
        frame_x = INT16_MIN;
    } else if (frame_x32 > INT16_MAX) {
        frame_x = INT16_MAX;
    } else {
        frame_x = (int16_t) frame_x32;
    }
    if (frame_y32 < INT16_MIN) {
        frame_y = INT16_MIN;
    } else if (frame_y32 > INT16_MAX) {
        frame_y = INT16_MAX;
    } else {
        frame_y = (int16_t) frame_y32;
    }
    frame_w =
        (uint16_t) (client->layout.geometry.cur.dim.w + left + right);
    frame_h =
        (uint16_t) (client->layout.geometry.cur.dim.h + top + bottom);

    client->frame = xcb_generate_id(client->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = client->theme->window.inactive.border_color;
    values[1] = client->theme->window.inactive.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_create_window(client->connection,
            XCB_COPY_FROM_PARENT,
            client->frame,
            client->parent_id,
            frame_x, frame_y,
            frame_w, frame_h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    client->titlebar = xcb_generate_id(client->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = client->theme->window.inactive.background_color;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS;
    xcb_create_window(client->connection,
            XCB_COPY_FROM_PARENT,
            client->titlebar,
            client->frame,
            (int16_t) left, (int16_t) title_y,
            inner_w,
            title_h,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_reparent_window(client->connection,
            client->window,
            client->frame,
            (int16_t) left, (int16_t) top);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, (const uint32_t[]) {0});

    /* Passive grab: any button, any modifier, SYNC pointer mode.  With
     * 'owner_events=0' ALL button presses on the frame or any of its
     * children (titlebar, client window) are delivered to the window
     * manager through this grab rather than via SelectInput.  SYNC mode
     * freezes pointer events until the window manager calls
     * 'xcb_allow_events', which lets the manager focus the window
     * before deciding whether to replay the click to the application or
     * consume it silently.  'XCB_MOD_MASK_ANY' already covers all
     * lock-modifier combinations, so no lock-modifier loop is
     * required. */
    /* NOTE: root-level 'MOD1+button' grabs are more specific (specific
     * modifier beats 'XCB_MOD_MASK_ANY') and therefore still take
     * priority for move/resize interactions. */
    xcb_grab_button(client->connection,
            0,                                  /* owner_events */
            client->frame,
            XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE,
            XCB_GRAB_MODE_SYNC,                 /* freeze until allow_events */
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            XCB_NONE,
            XCB_BUTTON_INDEX_ANY,
            XCB_MOD_MASK_ANY);

    client->layout.geometry.cur.pos.x = frame_x;
    client->layout.geometry.cur.pos.y = frame_y;
    client->layout.geometry.cur.dim.w = frame_w;
    client->layout.geometry.cur.dim.h = frame_h;
    client->layout.geometry.old = client->layout.geometry.cur;
    s_client_sync_decoration_layout(client);

    return 0;
}


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

    /* Initialize client properties with sensible defaults */
    client->properties.flags =
        CLIENT_FLAG_HIDDEN | CLIENT_FLAG_FOCUSABLE |
        CLIENT_FLAG_RESIZABLE;
    client->properties.type = CLIENT_TYPE_NORMAL;
    client->properties.state = CLIENT_STATE_NORMAL;
    client->properties.layer = CLIENT_LAYER_NORMAL;
    client->properties.operation = CLIENT_OPERATION_IDLE;
    client->properties.focusing = CLIENT_FOCUSING_UNFOCUSED;
    client->properties.gravity = CLIENT_GRAVITY_NORTH_WEST;
    s_client_set_decoration_defaults(client, theme);

    /* Allocate buffers for client information strings.
     * Each buffer is separately allocated for independent management */
    client->info.name = malloc(256);
    if (client->info.name == NULL) {
        LOGGER_ERROR("Failed to allocate 'name' buffer", L_NARG);
        free(client);
        return NULL;
    }

    client->info.visible_name = malloc(256);
    if (client->info.visible_name == NULL) {
        LOGGER_ERROR("Failed to allocate 'visible_name' buffer", L_NARG);
        free(client->info.name);
        free(client);
        return NULL;
    }

    client->info.role_name = malloc(256);
    if (client->info.role_name == NULL) {
        LOGGER_ERROR("Failed to allocate 'role_name' buffer", L_NARG);
        free(client->info.visible_name);
        free(client->info.name);
        free(client);
        return NULL;
    }

    /* Allocate buffers for window class information */
    client->info.class_name[0] = malloc(256);
    client->info.class_name[1] = malloc(256);
    if (client->info.class_name[0] == NULL ||
            client->info.class_name[1] == NULL) {
        LOGGER_ERROR("Failed to allocate 'class_name' buffers", L_NARG);
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        free(client);
        return NULL;
    }

    /* Allocate buffers for icon information */
    client->icon_info.icon_name = malloc(256);
    client->icon_info.visible_icon_name = malloc(256);
    if (client->icon_info.icon_name == NULL ||
            client->icon_info.visible_icon_name == NULL) {
        LOGGER_ERROR("Failed to allocate icon buffers", L_NARG);
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        safe_free((void **) &client->icon_info.icon_name);
        safe_free((void **) &client->icon_info.visible_icon_name);
        free(client);
        return NULL;
    }

    client->icon_info.icons = NULL;

    /* Initialize all strings with default empty values */
    snprintf(client->info.name, 255, "Client %p", (void *) client);
    snprintf(client->info.visible_name, 255, "Client %p", (void *) client);
    client->info.role_name[0] = '\0';
    client->info.class_name[0][0] = '\0';
    client->info.class_name[1][0] = '\0';
    client->icon_info.icon_name[0] = '\0';
    client->icon_info.visible_icon_name[0] = '\0';

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
    s_client_get_wm_name(connection, parent_id, wm_name,
            sizeof(wm_name));
    if (wm_name[0] != '\0') {
        safe_strncpy(client->info.name, wm_name, 255);
        safe_strncpy(client->info.visible_name, wm_name, 255);
    }

    /* Attempt to retrieve 'WM_CLASS' property from the X server to
     * populate the client's class and instance names */
    s_client_get_wm_class(connection, parent_id,
            wm_class, sizeof(wm_class),
            wm_instance, sizeof(wm_instance));
    if (wm_class[0] != '\0') {
        safe_strncpy(client->info.class_name[1], wm_class, 255);
    }
    if (wm_instance[0] != '\0') {
        safe_strncpy(client->info.class_name[0], wm_instance, 255);
    }

    /*
     * Map the window to make it visible on the screen
     * and flush the output buffer to ensure the request is sent
     */
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
    xcb_intern_atom_reply_t *ia;
    xcb_atom_t wm_delete_atom = XCB_ATOM_NONE;
    xcb_icccm_get_wm_protocols_reply_t proto;

    LOGGER_TRACE("Attempting to manage existing window %#x", window);

    /* Reject override-redirect windows — they manage themselves */
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

    /* Zero-initialise to prevent uninitialised reads */
    memset(client, 0, sizeof(client_td));

    /* Basic connections */
    client->connection = connection;
    client->ewmh = ewmh;
    client->theme = theme;
    client->config_base = config_base;
    client->frame = 0;
    client->titlebar = 0;
    client->icon_window = 0;
    client->is_icon_mapped = false;
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
        client->layout.geometry.cur.dim.w = 100;
        client->layout.geometry.cur.dim.h = 100;
    }
    client->layout.geometry.old = client->layout.geometry.cur;

    /* Default properties: visible, focusable, resizable */
    client->properties.flags = CLIENT_FLAG_FOCUSABLE | CLIENT_FLAG_RESIZABLE;
    client->properties.type = CLIENT_TYPE_NORMAL;
    client->properties.state = CLIENT_STATE_NORMAL;
    client->properties.layer = CLIENT_LAYER_NORMAL;
    client->properties.operation = CLIENT_OPERATION_IDLE;
    client->properties.focusing = CLIENT_FOCUSING_UNFOCUSED;
    client->properties.gravity = CLIENT_GRAVITY_NORTH_WEST;
    s_client_set_decoration_defaults(client, theme);

    /* Allocate string buffers */
    client->info.name = malloc(256);
    client->info.visible_name = malloc(256);
    client->info.role_name = malloc(256);
    client->info.class_name[0] = malloc(256);
    client->info.class_name[1] = malloc(256);
    client->icon_info.icon_name = malloc(256);
    client->icon_info.visible_icon_name = malloc(256);

    if (client->info.name == NULL ||
            client->info.visible_name == NULL ||
            client->info.role_name == NULL ||
            client->info.class_name[0] == NULL ||
            client->info.class_name[1] == NULL ||
            client->icon_info.icon_name == NULL ||
            client->icon_info.visible_icon_name == NULL) {
        LOGGER_ERROR("Failed to allocate string buffers" \
                " for managed client", L_NARG);
        safe_free((void **) &client->info.name);
        safe_free((void **) &client->info.visible_name);
        safe_free((void **) &client->info.role_name);
        safe_free((void **) &client->info.class_name[0]);
        safe_free((void **) &client->info.class_name[1]);
        safe_free((void **) &client->icon_info.icon_name);
        safe_free((void **) &client->icon_info.visible_icon_name);
        free(client);
        return NULL;
    }

    client->icon_info.icons = NULL;

    /* Default string values */
    snprintf(client->info.name, 255, "Window %#x", window);
    snprintf(client->info.visible_name, 255, "Window %#x", window);
    client->info.role_name[0] = '\0';
    client->info.class_name[0][0] = '\0';
    client->info.class_name[1][0] = '\0';
    client->icon_info.icon_name[0] = '\0';
    client->icon_info.visible_icon_name[0] = '\0';

    /* Read '_NET_WM_NAME' (UTF-8) first; fall back to 'WM_NAME' (Latin-1) */
    s_client_get_net_wm_name(ewmh, window,
            net_wm_name, sizeof(net_wm_name));
    if (net_wm_name[0] != '\0') {
        safe_strncpy(client->info.name, net_wm_name, 255);
        safe_strncpy(client->info.visible_name, net_wm_name, 255);
    } else {
        s_client_get_wm_name(connection, window,
                wm_name, sizeof(wm_name));
        if (wm_name[0] != '\0') {
            safe_strncpy(client->info.name, wm_name, 255);
            safe_strncpy(client->info.visible_name, wm_name, 255);
        }
    }

    /* Read WM_CLASS */
    s_client_get_wm_class(connection, window,
            wm_class, sizeof(wm_class),
            wm_instance, sizeof(wm_instance));
    if (wm_class[0] != '\0') {
        safe_strncpy(client->info.class_name[1], wm_class, 255);
    }
    if (wm_instance[0] != '\0') {
        safe_strncpy(client->info.class_name[0], wm_instance, 255);
    }

    /* Read 'WM_PROTOCOLS': cache 'WM_DELETE_WINDOW' support */
    ia = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, 1, 16, "WM_DELETE_WINDOW"),
            NULL);
    if (ia != NULL) {
        wm_delete_atom = ia->atom;
        free(ia);
    }

    client->has_wm_delete_window = false;
    memset(&proto, 0, sizeof(proto));
    if (xcb_icccm_get_wm_protocols_reply(connection,
                xcb_icccm_get_wm_protocols(connection, window,
                    ewmh->WM_PROTOCOLS),
                &proto, NULL)) {
        for (uint32_t pi = 0; pi < proto.atoms_len; ++pi) {
            if (proto.atoms[pi] == wm_delete_atom) {
                client->has_wm_delete_window = true;
                break;
            }
        }
        xcb_icccm_get_wm_protocols_reply_wipe(&proto);
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
            client->properties.gravity =
                (uint16_t) hints.win_gravity;
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
        /* start: maps {left→left_start_y, right→right_start_y,
         *              top→top_start_x,   bottom→bottom_start_x} */
        client->layout.strut_partial.start.left =
            (int32_t) partial.left_start_y;
        client->layout.strut_partial.start.right =
            (int32_t) partial.right_start_y;
        client->layout.strut_partial.start.top =
            (int32_t) partial.top_start_x;
        client->layout.strut_partial.start.bottom =
            (int32_t) partial.bottom_start_x;
        /* end: maps {left→left_end_y, right→right_end_y,
         *            top→top_end_x,   bottom→bottom_end_x} */
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

    /* Subscribe to events on the adopted window */
    values[0] = XCB_EVENT_MASK_ENTER_WINDOW     |
                XCB_EVENT_MASK_LEAVE_WINDOW     |
                XCB_EVENT_MASK_FOCUS_CHANGE     |
                XCB_EVENT_MASK_PROPERTY_CHANGE  |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(connection, window,
            XCB_CW_EVENT_MASK, values);

    /* Apply border width from theme */
    if (theme != NULL) {
        uint32_t bw[1] = { theme->window.general.border_width };
        xcb_configure_window(connection, window,
                XCB_CONFIG_WINDOW_BORDER_WIDTH, bw);
    }

    /* Ignore return value, as decoration creation is non-fatal here */
    (void) s_client_create_decorations(client);

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


/* Generic event sender for a client */
int client_send_event(client_td *client,
        enum action_client_e action_client, enum priority_e priority)
{
    event_td *event;
    action_td action;

    if (client == NULL) {
        LOGGER_ERROR("Received null client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Sending event to client %p: action %d, priority %d",
            (void *) client, action_client, priority);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = action_client;

    event = event_init((void *) client, NULL, action, priority);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event for client", L_NARG);
        return -1;
    }

    return eventq_add(event);
}


/* Send an event to rename a specified client */
int client_send_event_rename(client_td *client, const char *new_name)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || new_name == NULL) {
        LOGGER_ERROR("Received null pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Renaming client %p to '%s'",
            (void *) client, new_name);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RENAME;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.str.str0 = safe_strdup(new_name);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/* Send an event to change the class of a specified client */
int client_send_event_reclass(client_td *client, const char *new_class)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || new_class == NULL) {
        LOGGER_ERROR("Received null pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Reclassifying client %p to '%s'",
            (void *) client, new_class);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RECLASS;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.str.str0 = safe_strdup(new_class);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/* Send event to move the specified client to given coordinates */
int client_send_event_move(client_td *client,
        int32_t new_x, int32_t new_y)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;
    uint32_t values[2];

    if (client == NULL) {
        LOGGER_ERROR("Received null client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Moving client %p to (%d, %d)",
            (void *) client, new_x, new_y);

    /* Update client's internal geometry */
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;

    /* Configure the XCB window immediately */
    /* NOTE: For decorated clients the outermost positioned window is
     * the frame; moving the inner client window (which is reparented
     * INSIDE the frame) would place it at screen-relative coordinates
     * relative to the frame, making the content appear shifted. */
    values[0] = (uint32_t) new_x;
    values[1] = (uint32_t) new_y;

    xcb_configure_window(client->connection,
            (client->frame != 0 && client_is_decorated(client))
                ? client->frame : client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(client->connection);

    /* Create and queue the event for the action handler */
    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_MOVE;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.geometry.pos.x = new_x;
    data->new_data.geometry.pos.y = new_y;

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/* Send an event to resize the specified client */
int client_send_event_resize(client_td *client,
        uint32_t new_w, uint32_t new_h)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;
    uint32_t values[2];

    if (client == NULL) {
        LOGGER_ERROR("Received null client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Resizing client %p to %ux%u",
            (void *) client, new_w, new_h);

    /* Update client's internal geometry */
    client->layout.geometry.cur.dim.w = new_w;
    client->layout.geometry.cur.dim.h = new_h;

    /* Configure the XCB window immediately */
    /* NOTE: For decorated clients the frame must be resized; resizing
     * only the inner window would leave the decoration at the wrong
     * size */
    values[0] = new_w;
    values[1] = new_h;

    xcb_configure_window(client->connection,
            (client->frame != 0 && client_is_decorated(client))
                ? client->frame : client->window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            values);
    s_client_sync_decoration_layout(client);
    xcb_flush(client->connection);

    /* Create and queue the event for the action handler */
    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_RESIZE;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.geometry.dim.w = new_w;
    data->new_data.geometry.dim.h = new_h;

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}


/* Send an event to change the icon of a specified client */
int client_send_event_set_icon(client_td *client, const char *icon_name)
{
    event_td *event;
    action_td action;
    action_data_client_td *data;

    if (client == NULL || icon_name == NULL) {
        LOGGER_ERROR("Received null pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Setting icon for client %p to '%s'",
            (void *) client, icon_name);

    action.type = ACTION_TYPE_CLIENT;
    action.object.client = ACTION_CLIENT_SET_ICON;

    data = action_data_client_init(client, action.object.client);
    if (data == NULL) {
        LOGGER_ERROR("Failed to create action data", L_NARG);
        return -1;
    }

    data->new_data.str.str0 = safe_strdup(icon_name);

    event = event_init((void *) client, (void *) data,
            action, CLIENT_PRIORITY_DEFAULT);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create event", L_NARG);
        action_data_client_destroy(data);
        return -1;
    }

    return eventq_add(event);
}
