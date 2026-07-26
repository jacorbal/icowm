/**
 * @file cmds/ccmd.c
 *
 * @brief Implementation on executions over clients using the XCB
 *        interface while updating EWMH and ICCCM hints
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>     /* va_arg, va_end, va_start */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* size_t, snprintf */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <actdata.h>
#include <client.h>

/* Local includes */
#include <cmds/ccmd.h>


/**
 * @brief Retrieve the ID of the currently active window for the
 *        specified screen
 *
 * Utilizes the EWMH (Extended Window Manager Hints) to query the active
 * window from the X server.  It returns the window ID for the active
 * window or a predefined value indicating an error if the request
 * fails.
 *
 * @param ewmh      Pointer to the EWMH structure
 * @param screen_id ID of the screen
 *
 * @return ID of the currently active window, or @c XCB_WINDOW_NONE on
 *         error
 *
 * @note Assumes that the EWMH connection has been previously
 *       initialized and is valid
 * @note Complexity: @e O(1)
 */
static xcb_window_t s_wcmd_get_active_window(xcb_ewmh_connection_t *ewmh,
        uint32_t screen_id)
{
    xcb_window_t active_window;
    xcb_get_property_cookie_t cookie =
        xcb_ewmh_get_active_window(ewmh, (int) screen_id);

    if (!xcb_ewmh_get_active_window_reply(ewmh, cookie,
                &active_window, NULL)) {
        return XCB_WINDOW_NONE;
    }

    return active_window;
}


/**
 * @brief Intern an atom for a given name in the X11 system
 *
 * Takes a connection to the X11 server and a string name, and it
 * requests the interned atom corresponding to that name.  If the atom
 * is successfully created, it returns the atom ID; otherwise, it
 * returns @c XCB_ATOM_NONE.
 *
 * @param connection Pointer to the X11 connection
 * @param name       Name of the atom to intern
 *^
 * @return Interned atom ID, or @c XCB_ATOM_NONE on failure
 *
 * @note Complexity: @e O(n), where @e n is the length of the atom name,
 *       due to the string processing involved in intern request
 *       handling and response retrieval from the X server.  However,
 *       the actual X11 server response time may vary based on server
 *       load and connection quality, which could affect the overall
 *       performance.
 */
static xcb_atom_t s_wcmd_intern_atom(xcb_connection_t *connection,
        const char *name)
{
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(connection, 0, (uint16_t) safe_strlen(name), name);
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(connection, cookie, NULL);

    if (reply) {
        xcb_atom_t atom = reply->atom;
        free(reply);
        return atom;
    }

    return XCB_ATOM_NONE;
}


/**
 * @brief Return the frame window when decorated, otherwise the client
 *        window
 *
 * Returns the decoration frame window when the client is decorated and
 * the frame has already been created.  In any other case, returns the
 * client window itself.
 *
 * @param client Pointer to the client to inspect
 *
 * @return Frame window when available, or the client window otherwise;
 *         @c XCB_WINDOW_NONE if @p client is @c NULL
 *
 * @note Complexity: @e O(1)
 */
static xcb_window_t s_wcmd_target_window(client_td *client)
{
    if (client == NULL) {
        return XCB_WINDOW_NONE;
    }
    if (client_is_decorated(client) && client->frame != 0) {
        return client->frame;
    }
    return client->window;
}


/**
 * @brief Get the dimensions of the client's current screen
 *
 * Retrieves the width and height in pixels of the screen associated
 * with the client. Either output parameter may be @c NULL, but not
 * both.  The function walks the XCB screen iterator until it reaches
 * the client's screen index.
 *
 * @param client Pointer to the client whose screen is queried
 * @param out_w  Destination for the screen width in pixels, or @c NULL
 * @param out_h  Destination for the screen height in pixels, or @c NULL
 *
 * @return @c true on success, @c false on failure
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
static bool s_wcmd_get_screen_dimensions(client_td *client,
        uint16_t *out_w, uint16_t *out_h)
{
    xcb_screen_iterator_t iter;

    if (client == NULL || (out_w == NULL && out_h == NULL)) {
        return false;
    }

    iter = xcb_setup_roots_iterator(xcb_get_setup(client->connection));
    for (uint32_t i = 0; i < client->screen_id && iter.rem > 0; ++i) {
        xcb_screen_next(&iter);
    }
    if (iter.rem == 0 || iter.data == NULL) {
        return false;
    }

    if (out_w != NULL) {
        *out_w = iter.data->width_in_pixels;
    }
    if (out_h != NULL) {
        *out_h = iter.data->height_in_pixels;
    }
    return true;
}


/**
 * @brief Add multiple EWMH states to a client
 *
 * Accepts a variable number of string arguments representing the states
 * to be added to the specified window.
 *
 * @param client     Pointer to client where states are to be added
 * @param num_states Number of states to add
 * @param ...        Variable number of string arguments representing
 *                   the states
 *
 * @note It is the caller's responsibility to ensure that the @p client
 *       is properly initialized and has enough space allocated to store
 *       the states
 * @note Complexity: @e O(n), where @e n is the number of states passed
 *       to the function
 */
static void s_wcmd_add_window_states(client_td *client,
        uint32_t num_states, ...)
{
    xcb_atom_t *states;
    va_list args;

    if (client == NULL || num_states == 0) {
        return;
    }

    states = malloc(num_states * sizeof(xcb_atom_t));
    if (states == NULL) {
        return;
    }

    va_start(args, num_states);
    for (uint32_t i = 0; i < num_states; ++i) {
        const char *state_name = va_arg(args, const char *);
        states[i] = s_wcmd_intern_atom(client->connection, state_name);
        if (states[i] == XCB_ATOM_NONE) {
            free(states);
            va_end(args);
            return;
        }
    }
    va_end(args);

    xcb_ewmh_set_wm_state(client->ewmh, client->window, num_states, states);

    free(states);
}


/**
 * @brief Remove multiple EWMH states to a client
 *
 * Accepts a variable number of string arguments representing the states
 * to be removed to the specified window.
 *
 * @param client     Pointer to client where states are to be removed
 * @param num_states Number of states to add
 * @param ...        Variable number of string arguments representing
 *                   the states
 *
 * @note It is the caller's responsibility to ensure that the @p client
 *       is properly initialized and has enough space allocated to store
 *       the states
 * @note EWMH Compliance: properly removes window states per
 *       @c _NET_WM_STATE property
 * @note Complexity: @e O(n), where @e n is the number of states passed
 *       to the function
 */
static void s_wcmd_rem_window_states(client_td *client,
        uint32_t num_states, ...)
{
    xcb_atom_t *remove_states;
    xcb_atom_t *new_states;
    xcb_get_property_cookie_t cookie;
    xcb_ewmh_get_atoms_reply_t current_states_reply;
    uint32_t new_count;
    va_list args;
    bool should_remove;
    uint8_t success;

    if (client == NULL || num_states == 0) {
        return;
    }

    /* Allocate array for states to remove */
    remove_states = malloc(num_states * sizeof(xcb_atom_t));
    if (remove_states == NULL) {
        return;
    }

    /* Get all atoms for states to remove */
    va_start(args, num_states);
    for (uint32_t i = 0; i < num_states; ++i) {
        const char *state_name = va_arg(args, const char *);
        remove_states[i] =
            s_wcmd_intern_atom(client->connection, state_name);
        if (remove_states[i] == XCB_ATOM_NONE) {
            free(remove_states);
            va_end(args);
            return;
        }
    }
    va_end(args);

    /* Get current window states using proper EWMH API */
    cookie = xcb_ewmh_get_wm_state(client->ewmh, client->window);
    success = xcb_ewmh_get_wm_state_reply(client->ewmh, cookie,
            &current_states_reply, NULL);

    if (!success || current_states_reply.atoms_len == 0) {
        /* Failed to get current states, or no states present */
        free(remove_states);
        if (success) {
            free(current_states_reply.atoms);
        }
        return;
    }

    /* Allocate new array for filtered states */
    new_states =
        malloc(current_states_reply.atoms_len * sizeof(xcb_atom_t));
    if (new_states == NULL) {
        free(remove_states);
        free(current_states_reply.atoms);
        return;
    }

    /* Filter: copy all states that are not in remove_states */
    new_count = 0;
    for (uint32_t i = 0; i < current_states_reply.atoms_len; ++i) {
        should_remove = false;

        /* Check if current atom matches any state to remove */
        for (uint32_t j = 0; j < num_states; ++j) {
            if (current_states_reply.atoms[i] == remove_states[j]) {
                should_remove = true;
                break;
            }
        }

        /* Add to new list if not marked for removal */
        if (!should_remove) {
            new_states[new_count++] = current_states_reply.atoms[i];
        }
    }

    /* Set the filtered states back to the window */
    if (new_count > 0) {
        xcb_ewmh_set_wm_state(client->ewmh, client->window, new_count,
                new_states);
    } else {
        /* No states remain, set empty state list */
        xcb_ewmh_set_wm_state(client->ewmh, client->window, 0, NULL);
    }

    /* Cleanup allocated memory */
    free(remove_states);
    free(new_states);
    free(current_states_reply.atoms);
}


/* Close the client */
void wcmd_client_close(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* This destroys the client in client->window, but does not
     * deallocates the memory of the client object.  This action is
     * intended to be called by the desktop, therefore, it's
     * responsibility of the desktop to execute this action, and then
     * invoke 'client_destroy' */
    xcb_destroy_window(client->connection, client->window);
}


/* Forcibly kill the client's X connection */
void wcmd_client_kill(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* Unlike 'wcmd_client_close' (a request to destroy a single window
     * resource), 'xcb_kill_client' terminates the owning client's
     * ENTIRE connection to the X server.  Meant as a last resort for
     * unresponsive clients that ignore a normal close request. */
    xcb_kill_client(client->connection, client->window);
}


/* Restore a client to its normal state */
void wcmd_client_restore(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = s_wcmd_target_window(client);
    client_geometry_restore(client);

    xcb_map_window(client->connection, client->window);
    if (client->icon_window != 0 && client->is_icon_mapped) {
        xcb_unmap_window(client->connection, client->icon_window);
        client->is_icon_mapped = false;
    }
    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }
    xcb_map_window(client->connection, target);
    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_toggle_hidden(client);
    client->properties.state = CLIENT_STATE_NORMAL;

    client_geometry_save(client);

    s_wcmd_rem_window_states(client, 3,
            "_NET_WM_STATE_HIDDEN",
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
}


/* Focus a client */
void wcmd_client_focus(client_td *client)
{
    if (client == NULL) {
        return;
    }

    xcb_set_input_focus(client->connection, XCB_INPUT_FOCUS_PARENT,
                        client->window, XCB_CURRENT_TIME);
    xcb_map_window(client->connection, client->window);

    if (client->ewmh != NULL) {
        xcb_ewmh_request_change_active_window(client->ewmh,
                (int) client->screen_id,
                client->window, 0,
                XCB_CURRENT_TIME,
                s_wcmd_get_active_window(client->ewmh,
                    client->screen_id));
    }
}


/* Unfocus the client */
void wcmd_client_unfocus(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_unfocus(client);

    xcb_ewmh_request_change_active_window(client->ewmh,
            (int) client->screen_id,
            XCB_NONE, 0,
            XCB_CURRENT_TIME, 0);
}


/* Move client */
void wcmd_client_move(client_td *client,
        action_data_client_td *client_data)
{
    xcb_window_t target;

    if (client == NULL || client_data == NULL) {
        return;
    }

    target = s_wcmd_target_window(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {
                (uint32_t) client_data->new_data.geometry.pos.x,
                (uint32_t) client_data->new_data.geometry.pos.y
            });
    client->layout.geometry.cur.pos =
        client_data->new_data.geometry.pos;

    // TODO: Update 'WM_NORMAL_HINTS' with window gravity hints
}


/* Center client on screen */
void wcmd_client_center(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    int32_t x;
    int32_t y;
    xcb_window_t target;

    if (client == NULL ||
            !s_wcmd_get_screen_dimensions(client, &sw, &sh)) {
        return;
    }

    target = s_wcmd_target_window(client);
    x = ((int32_t) sw - (int32_t) client->layout.geometry.cur.dim.w) / 2;
    y = ((int32_t) sh - (int32_t) client->layout.geometry.cur.dim.h) / 2;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) x, (uint32_t) y});
    client->layout.geometry.cur.pos.x = x;
    client->layout.geometry.cur.pos.y = y;
}


/* Resize client */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data)
{
    xcb_window_t target;

    if (client == NULL || client_data == NULL) {
        return;
    }

    target = s_wcmd_target_window(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) client_data->new_data.geometry.dim.w,
                (uint32_t) client_data->new_data.geometry.dim.h
            });

    client->layout.geometry.cur.dim =
        client_data->new_data.geometry.dim;

    // TODO: Update 'WM_NORMAL_HINTS' with window gravity hints
}


/* Rename the client */
void wcmd_client_rename(client_td *client,
        action_data_client_td *client_data)
{
    if (client == NULL || client_data == NULL) {
        return;
    }

    free(client->info.name);
    client->info.name = safe_strdup(client_data->new_data.str.str0);

    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,/*bits*/
            (uint32_t) safe_strlen(client->info.name),
            client->info.name);

    /* '_NET_WM_NAME' */
    xcb_ewmh_set_wm_name(client->ewmh, client->window,
            (uint32_t) safe_strlen(client->info.name),
            client->info.name);
}


/* Change the class of the client */
void wcmd_client_reclass(client_td *client,
        action_data_client_td *client_data)
{
    char *wm_class_combined;
    size_t wm_class_combined_len;
    size_t len0;
    size_t len1;

    if (client == NULL || client_data == NULL) {
        return;
    }

    free(client->info.class_name[0]);
    free(client->info.class_name[1]);
    client->info.class_name[0] =
        safe_strdup(client_data->new_data.str.str0);
    client->info.class_name[1] =
        safe_strdup(client_data->new_data.str.str1);

    /* The 'WM_CLASS' property (of type 'STRING' without control
     * characters) contains two consecutive null-terminated
     * strings.  These specify the Instance and Class names to be used
     * by both the client and the window manager for looking up
     * resources for the application or as identifying information
     * (ICCCM v 2.0, § 4.1.2.5).  A single buffer is built manually
     * (instead of relying on '%s' with an embedded '\0' argument, which
     * prints as an empty string and therefore never inserts the
     * separator) so that both null terminators actually land in the
     * buffer passed to 'xcb_icccm_set_wm_class' */
    len0 = safe_strlen(client->info.class_name[0]);
    len1 = safe_strlen(client->info.class_name[1]);
    wm_class_combined_len = (len0 + 1) + (len1 + 1);
    wm_class_combined = malloc(wm_class_combined_len);
    if (wm_class_combined) {
        memcpy(wm_class_combined, client->info.class_name[0], len0);
        wm_class_combined[len0] = '\0';
        memcpy(wm_class_combined + len0 + 1,
                client->info.class_name[1], len1);
        wm_class_combined[len0 + 1 + len1] = '\0';

        xcb_icccm_set_wm_class(client->connection, client->window,
                (uint32_t) wm_class_combined_len, wm_class_combined);
        free(wm_class_combined);
    } else {
        /* On error, at least get the first string */
        xcb_icccm_set_wm_class(client->connection, client->window,
                (uint32_t) (len0 + 1), client->info.class_name[0]);
    }
}


/* Change the role of the client */
void wcmd_client_rerole(client_td *client,
        action_data_client_td *client_data)
{
    xcb_intern_atom_cookie_t role_atom_cookie;
    xcb_intern_atom_reply_t *role_atom_reply;

    if (client == NULL || client_data == NULL) {
        return;
    }

    free(client->info.role_name);
    client->info.role_name =
        safe_strdup(client_data->new_data.str.str0);

    role_atom_cookie = xcb_intern_atom(client->connection, 0,
            (uint16_t) safe_strlen("WM_WINDOW_ROLE"), "WM_WINDOW_ROLE");
    role_atom_reply = xcb_intern_atom_reply(client->connection,
            role_atom_cookie, NULL);

    if (!role_atom_reply) {
        return;
    }

    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            role_atom_reply->atom,
            XCB_ATOM_STRING,
            8,/*bits*/
            (uint32_t) safe_strlen(client->info.role_name),
            client->info.role_name);

    free(role_atom_reply);
}


/* Maximize client horizontally */
void wcmd_client_maximize_horz(client_td *client)
{
    uint16_t sw;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    if (!s_wcmd_get_screen_dimensions(client, &sw, NULL)) {
        return;
    }

    target = s_wcmd_target_window(client);

    // TODO: Deactivate CLIENT_STATE_MAXIMIZED first?
    client_geometry_save(client);

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_WIDTH,
            (const uint32_t[]) {
                0,                                              /* X */
                (uint32_t) client->layout.geometry.cur.pos.y,   /* Keep Y */
                (uint32_t) sw                                   /* Width */
            });

    client->layout.geometry.cur.pos.x = 0;
    client->layout.geometry.cur.dim.w = sw;

    client->properties.state = CLIENT_STATE_MAXIMIZED_HORZ;

    s_wcmd_rem_window_states(client, 2,
            "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_VERT");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_MAXIMIZED_HORZ");
}


/* Maximize client vertically */
void wcmd_client_maximize_vert(client_td *client)
{
    uint16_t sh;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    if (!s_wcmd_get_screen_dimensions(client, NULL, &sh)) {
        return;
    }

    target = s_wcmd_target_window(client);

    // TODO: Deactivate CLIENT_STATE_MAXIMIZED first?
    client_geometry_save(client);

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,   /* Keep X */
                0,                                              /* Y */
                (uint32_t) sh                                   /* Height */
            });

    client->layout.geometry.cur.pos.y = 0;
    client->layout.geometry.cur.dim.h = sh;

    client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;

    s_wcmd_rem_window_states(client, 2,
            "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_HORZ");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_MAXIMIZED_VERT");
}


/* Maximize client entirely */
void wcmd_client_maximize(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    if (!s_wcmd_get_screen_dimensions(client, &sw, &sh)) {
        return;
    }

    target = s_wcmd_target_window(client);
    client_geometry_save(client);

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                0,                  /* X */
                0,                  /* Y */
                (uint32_t) sw,      /* Width */
                (uint32_t) sh       /* Height */
            });
    client->layout.geometry.cur.pos.x = 0;
    client->layout.geometry.cur.pos.y = 0;
    client->layout.geometry.cur.dim.w = sw;
    client->layout.geometry.cur.dim.h = sh;

    client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    s_wcmd_add_window_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
}


/* Iconify client (and minimize it) */
void wcmd_client_iconify(client_td *client)
{
    xcb_window_t target;
    uint32_t mask;
    uint32_t values[3];

    if (client == NULL) {
        return;
    }

    target = s_wcmd_target_window(client);
    client_geometry_save(client);

    if (client->icon_window == 0) {
        uint16_t icon_h;

        icon_h = (uint16_t) (WM_ICON_SQUARE_SIZE +
                ((client->theme->icon.is_captioned)
                 ? WM_ICON_CAPTION_HEIGHT
                 : 0u));

        client->icon_window = xcb_generate_id(client->connection);
        mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
        values[0] = client->theme->icon.background_color;
        values[1] = client->theme->icon.border_color;
        values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS;
        xcb_create_window(client->connection,
                XCB_COPY_FROM_PARENT,
                client->icon_window,
                client->parent_id,
                8, 8,
                (uint16_t) WM_ICON_SQUARE_SIZE, icon_h,
                (uint16_t) client->theme->icon.border_width,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                XCB_COPY_FROM_PARENT,
                mask, values);
    }

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }
    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }
    xcb_map_window(client->connection, client->icon_window);
    client->is_icon_mapped = true;

    client_set_hidden(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    /* Iconify per EWMH: window hidden with '_NET_WM_STATE_HIDDEN'.
     * Icon display handled by pager/desktop */

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_HIDDEN");

    xcb_flush(client->connection);
}


/* Hide the client (minimize, but not iconify) */
void wcmd_client_hide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = s_wcmd_target_window(client);
    client_geometry_save(client);

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }
    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    client_set_hidden(client);

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Show (unhide) the client */
void wcmd_client_unhide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = s_wcmd_target_window(client);
    client_geometry_save(client);

    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }
    xcb_map_window(client->connection, target);
    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unset_hidden(client);

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Shade client (roll-up), if decorated */
void wcmd_client_shade(client_td *client)
{
    if (client == NULL || !client_is_decorated(client)) {
        return;
    }

    client_geometry_save(client);
    client_set_shade(client);

    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_SHADED");
    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_HIDDEN");

    xcb_flush(client->connection);
}


/* Unshade client (roll-down), if decorated */
void wcmd_client_unshade(client_td *client)
{
    if (client == NULL || !client_is_decorated(client)) {
        return;
    }

    client_geometry_restore(client);
    client_unset_shade(client);
    client_unset_hidden(client);

    s_wcmd_rem_window_states(client, 2,
            "_NET_WM_STATE_SHADED",
            "_NET_WM_STATE_HIDDEN");

    xcb_flush(client->connection);
}


/* Toggle shading */
void wcmd_client_toggle_shade(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    } else {
        wcmd_client_shade(client);
    }
}


/* Set client sticky mode */
void wcmd_client_sticky(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_set_sticky(client);
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_STICKY");
}

/* Remove client sticky mode */
void wcmd_client_unsticky(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_unset_sticky(client);
    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_STICKY");
}


/* Toggle stickiness */
void wcmd_client_toggle_sticky(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client_is_sticky(client)) {
        wcmd_client_unsticky(client);
    } else {
        wcmd_client_sticky(client);
    }
}


/* Set full screen mode */
void wcmd_client_fullscreen(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    if (!s_wcmd_get_screen_dimensions(client, &sw, &sh)) {
        return;
    }

    target = s_wcmd_target_window(client);
    client_geometry_save(client);

    /* Configure window to fill entire screen */
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                0,                  /* X: top-left corner */
                0,                  /* Y: top-left corner */
                (uint32_t) sw,      /* Width: full screen width */
                (uint32_t) sh       /* Height: full screen height */
            });

    client->layout.geometry.cur.pos.x = 0;
    client->layout.geometry.cur.pos.y = 0;
    client->layout.geometry.cur.dim.w = sw;
    client->layout.geometry.cur.dim.h = sh;

    client->properties.state = CLIENT_STATE_FULLSCREEN;

    /* Hide client decorations if decorated (EWMH recommendation) */
    if (client_is_decorated(client)) {
        /* Mark that decorations should be hidden.  This would typically
         * be handled by the theme/decoration system */
    }

    /* Update EWMH states */
    s_wcmd_rem_window_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");

    xcb_flush(client->connection);
}


/* Remove full screen mode */
void wcmd_client_unfullscreen(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = s_wcmd_target_window(client);
    client_geometry_restore(client);

    /* Reconfigure window to restored position and dimensions */
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,
                (uint32_t) client->layout.geometry.cur.pos.y,
                (uint32_t) client->layout.geometry.cur.dim.w,
                (uint32_t) client->layout.geometry.cur.dim.h
            });

    /* Update internal client state */
    client->properties.state = CLIENT_STATE_NORMAL;

    /* Restore client decorations if previously decorated */
    if (client_is_decorated(client)) {
        /* Mark that decorations should be shown again */
    }

    /* Update EWMH states */
    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");

    xcb_flush(client->connection);
}


/* Toggle full screen mode */
void wcmd_client_toggle_fullscreen(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client->properties.state == CLIENT_STATE_FULLSCREEN) {
        wcmd_client_unfullscreen(client);
    } else {
        wcmd_client_fullscreen(client);
    }
}


/* Raise the client to the top */
void wcmd_client_raise(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    /* Get the desktop containing this client */
//    if (client->desktop_id == 0xFFFFFFFF) {
//        /* Client is sticky (on all desktops): handle separately */
//        return;
//    }

    /* Raise the window in XCB using StackMode */
    target = s_wcmd_target_window(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    /* Update the desktop's internal stack if needed.
     * This would be done through 'desktop_action_client_send_front()' */
    xcb_flush(client->connection);
}


/* Lower the client to the bottom */
void wcmd_client_lower(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_BELOW };
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    /* Get the desktop containing this client */
//    if (client->desktop_id == 0xFFFFFFFF) {
//        /* Client is sticky (on all desktops): handle separately */
//        return;
//    }

    /* Lower the window in XCB using StackMode */
    target = s_wcmd_target_window(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    /* Update the desktop's internal stack if needed.
     * This would be done through 'desktop_action_client_send_back()' */
    xcb_flush(client->connection);
}


/* Put the client in the above layer */
void wcmd_client_layer_above(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_ABOVE };

    if (client == NULL) {
        return;
    }

    client->properties.layer = CLIENT_LAYER_ABOVE;
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_BELOW");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_ABOVE");

    xcb_flush(client->connection);
}


/* Put the client in the normal layer */
void wcmd_client_layer_normal(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_OPPOSITE };

    if (client == NULL) {
        return;
    }

    /* Update internal client layer state */
    client->properties.layer = CLIENT_LAYER_NORMAL;

    /* Raise to normal position using stacking order
     * This is between the BELOW and ABOVE layers */
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    /* Remove special layer states */
    s_wcmd_rem_window_states(client, 2,
            "_NET_WM_STATE_ABOVE",
            "_NET_WM_STATE_BELOW");

    xcb_flush(client->connection);
}


/* Put the client in the below layer */
void wcmd_client_layer_below(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_BELOW };

    if (client == NULL) {
        return;
    }

    /* Update internal client layer state */
    client->properties.layer = CLIENT_LAYER_BELOW;

    /* Lower the window to the bottom */
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    /* Update EWMH states */
    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_ABOVE");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_BELOW");

    xcb_flush(client->connection);
}


/* Set client urgency */
void wcmd_client_set_urgent(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_set_urgent(client);
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Clear client urgency */
void wcmd_client_clear_urgent(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_unset_urgent(client);
    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Set icon for the client */
void wcmd_client_set_icon(client_td *client,
        action_data_client_td *client_data)
{
    if (client == NULL || client_data == NULL) {
        return;
    }

    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_ICON_NAME,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(client_data->new_data.str.str0),
            client_data->new_data.str.str0);

    /* UPDT: '_NET_WM_ICON_NAME EWMH' property for compliance
     * NOTE: '_NET_WM_ICON' pixmap data is typically set by the client
     *       itself, not by the window manager */
    xcb_ewmh_set_wm_icon_name(client->ewmh, client->window,
            (uint32_t) safe_strlen(client_data->new_data.str.str0),
            client_data->new_data.str.str0);
}


/* Toggle window decoration on or off */
void wcmd_client_toggle_decoration(client_td *client)
{
    int32_t bw;
    int32_t th;

    if (client == NULL || client->frame == 0) {
        return;
    }

    bw = (int32_t) client->theme->window.general.border_width;
    th = (int32_t) client->title_height;

    if (client_is_decorated(client)) {  /* Remove decoration */
         /* Compute inner client geometry from current frame geometry */
        int32_t inner_x = client->layout.geometry.cur.pos.x +
                          client->layout.frame_extents.left;
        int32_t inner_y = client->layout.geometry.cur.pos.y +
                          client->layout.frame_extents.top;
        int32_t inner_w = (int32_t) client->layout.geometry.cur.dim.w -
                          client->layout.frame_extents.left -
                          client->layout.frame_extents.right;
        int32_t inner_h = (int32_t) client->layout.geometry.cur.dim.h -
                          client->layout.frame_extents.top -
                          client->layout.frame_extents.bottom;

        if (inner_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            inner_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }
        if (inner_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            inner_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }

        /* Reposition frame to cover only the client content area */
        xcb_configure_window(client->connection, client->frame,
                XCB_CONFIG_WINDOW_X     |
                XCB_CONFIG_WINDOW_Y     |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    (uint32_t) inner_x, (uint32_t) inner_y,
                    (uint32_t) inner_w, (uint32_t) inner_h
                });

        /* Place client window at (0, 0) within the now-borderless frame */
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_X     |
                XCB_CONFIG_WINDOW_Y     |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    0u, 0u,
                    (uint32_t) inner_w, (uint32_t) inner_h
                });

        if (client->titlebar != 0) {
            xcb_unmap_window(client->connection, client->titlebar);
        }

        client->layout.geometry.cur.pos.x = inner_x;
        client->layout.geometry.cur.pos.y = inner_y;
        client->layout.geometry.cur.dim.w = (uint16_t) inner_w;
        client->layout.geometry.cur.dim.h = (uint16_t) inner_h;
        client->layout.frame_extents.left   = 0;
        client->layout.frame_extents.right  = 0;
        client->layout.frame_extents.top    = 0;
        client->layout.frame_extents.bottom = 0;
        client_unset_decoration(client);
    } else {                            /* Restore decoration */
        /* The frame currently wraps the bare client content; expand it
         * to include the titlebar above and borders on all sides. */
        int32_t frame_x = client->layout.geometry.cur.pos.x - bw;
        int32_t frame_y = client->layout.geometry.cur.pos.y - (bw + th);
        int32_t frame_w = (int32_t) client->layout.geometry.cur.dim.w +
                          2 * bw;
        int32_t frame_h = (int32_t) client->layout.geometry.cur.dim.h +
                          2 * bw + th;

        if (frame_w < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            frame_w = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }
        if (frame_h < (int32_t) WM_MIN_WINDOW_DIMENSION) {
            frame_h = (int32_t) WM_MIN_WINDOW_DIMENSION;
        }

        /* Expand frame to include borders and titlebar */
        xcb_configure_window(client->connection, client->frame,
                XCB_CONFIG_WINDOW_X     |
                XCB_CONFIG_WINDOW_Y     |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    (uint32_t) frame_x, (uint32_t) frame_y,
                    (uint32_t) frame_w, (uint32_t) frame_h
                });

        /* Reposition client window inside frame at (bw, bw+th) */
        xcb_configure_window(client->connection, client->window,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                (const uint32_t[]) {
                    (uint32_t) bw, (uint32_t) (bw + th)
                });

        /* Configure and map titlebar */
        if (client->titlebar != 0) {
            xcb_configure_window(client->connection, client->titlebar,
                    XCB_CONFIG_WINDOW_X     |
                    XCB_CONFIG_WINDOW_Y     |
                    XCB_CONFIG_WINDOW_WIDTH |
                    XCB_CONFIG_WINDOW_HEIGHT,
                    (const uint32_t[]) {
                        0u, 0u,
                        (uint32_t) frame_w, (uint32_t) (bw + th)
                    });
            xcb_map_window(client->connection, client->titlebar);
        }

        client->layout.geometry.cur.pos.x = frame_x;
        client->layout.geometry.cur.pos.y = frame_y;
        client->layout.geometry.cur.dim.w = (uint16_t) frame_w;
        client->layout.geometry.cur.dim.h = (uint16_t) frame_h;
        client->layout.frame_extents.left   = bw;
        client->layout.frame_extents.right  = bw;
        client->layout.frame_extents.top    = bw + th;
        client->layout.frame_extents.bottom = bw;
        client_set_decoration(client);
    }

    xcb_flush(client->connection);
}
