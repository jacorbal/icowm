/**
 * @file cmds/ccmd.c
 *
 * @brief Implementation on executions over clients using the XCB
 *        interface while updating EWMH and ICCCM hints
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>     /* va_arg, va_end, va_start */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* size_t, snprintf */
#include <stdlib.h>     /* NULL, free, malloc */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safemem.h>
#include <utils/safestr.h>

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
    xcb_get_property_cookie_t cookie =
        xcb_ewmh_get_active_window(ewmh, (int) screen_id);
    xcb_window_t active_window;

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
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0,
            (uint16_t) safe_strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection,
            cookie, NULL);

    if (reply) {
        xcb_atom_t atom = reply->atom;
        free(reply);
        return atom;
    }

    return XCB_ATOM_NONE;
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

    xcb_ewmh_set_wm_state(client->ewmh, client->id, num_states, states);

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
 * @note Complexity: @e O(n), where @e n is the number of states passed
 *       to the function
 */
static void s_wcmd_rem_window_states(client_td *client,
        uint32_t num_states, ...)
{

}


/* Close the client */
void wcmd_client_close(client_td *client)
{
    /* This destroys the client in client->window, but does not
     * deallocates the memory of the client object.  This action is
     * intended to be called by the desktop, therefore, it's
     * responsibility of the desktop to execute this action, and then
     * invoke 'client_destroy' */
    xcb_destroy_window(client->connection, client->window);
}


/* Restore a client to its normal state */
void wcmd_client_restore(client_td *client)
{
    client_geometry_restore(client);

    xcb_map_window(client->connection, client->window);

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
    xcb_set_input_focus(client->connection, XCB_INPUT_FOCUS_PARENT,
                        client->window, XCB_CURRENT_TIME);
    xcb_map_window(client->connection, client->window);

    xcb_ewmh_request_change_active_window(client->ewmh,
            (int) client->screen_id,
            client->id, 0,
            XCB_CURRENT_TIME,
            s_wcmd_get_active_window(client->ewmh, client->screen_id));
}


/* Unfocus the client */
void wcmd_client_unfocus(client_td *client)
{
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
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {
            (uint32_t) client_data->new_data.geometry.pos.x,
            (uint32_t) client_data->new_data.geometry.pos.y
            });
    client->layout.geometry.cur.pos =
        client_data->new_data.geometry.pos;

    // TODO: hints
}


/* Resize client */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data)
{
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            (uint32_t) client_data->new_data.geometry.dim.w,
            (uint32_t) client_data->new_data.geometry.dim.h
            });

    client->layout.geometry.cur.dim =
        client_data->new_data.geometry.dim;

    // TODO: hints
}


/* Rename the client */
void wcmd_client_rename(client_td *client,
        action_data_client_td *client_data)
{
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
    xcb_ewmh_set_wm_name(client->ewmh, client->id,
            (uint32_t) safe_strlen(client->info.name),
            client->info.name);
}


/* Change the class of the client */
void wcmd_client_reclass(client_td *client,
        action_data_client_td *client_data)
{
    char *wm_class[2];
    char *wm_class_combined;
    size_t wm_class_combined_len;

    wm_class[0] = safe_strdup(client_data->new_data.str.str0);
    wm_class[1] = safe_strdup(client_data->new_data.str.str1);

    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,  /* insert */
            client->id,
            XCB_ATOM_WM_CLASS,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(wm_class[0]),
            wm_class[0]);

    xcb_change_property(client->connection,
            XCB_PROP_MODE_APPEND,   /* append */
            client->id,
            XCB_ATOM_WM_CLASS,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(wm_class[1]),
            wm_class[1]);

    free(wm_class[0]);
    free(wm_class[1]);

    /* The 'WM_CLASS' property (of type 'STRING' without control
     * characters) contains two consecutive null-terminated
     * strings.  These specify the Instance and Class names to be used
     * by both the client and the window manager for looking up
     * resources for the application or as identifying information
     * (ICCCM v 2.0, § 4.1.2.5). */
    wm_class_combined_len =
        safe_strlen(client->info.class_name[0]) +
        safe_strlen(client->info.class_name[1]) + 2;
    wm_class_combined = malloc(wm_class_combined_len);
    if (wm_class_combined) {
        snprintf(wm_class_combined, wm_class_combined_len, "%s%s%s",
                client->info.class_name[0], "\0",
                client->info.class_name[1]);

        xcb_icccm_set_wm_class(client->connection, client->id, 1,
                wm_class_combined);
        free(wm_class_combined);
    } else {
        /* On error, at least get the first string */
        xcb_icccm_set_wm_class(client->connection, client->id, 1,
                client->info.class_name[0]);
    }
}


/* Change the role of the client */
void wcmd_client_rerole(client_td *client,
        action_data_client_td *client_data)
{
    xcb_intern_atom_cookie_t role_atom_cookie;
    xcb_intern_atom_reply_t *role_atom_reply;

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
}


/* Maximize client horizontally */
void wcmd_client_maximize_horz(client_td *client)
{
    xcb_get_geometry_reply_t *attrs =
        xcb_get_geometry_reply(client->connection,
                xcb_get_geometry(client->connection,
                    client->window), NULL);
    if (!attrs) {
        return;
    }

    client_geometry_save(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH,
            (const uint32_t[]) {
            0,
            0,  /* Y */
            attrs->width,   /* Width */
            client->layout.geometry.cur.dim.h   /* Current height */
            });
    safe_free((void *) attrs);

    client->properties.state = CLIENT_STATE_MAXIMIZED_HORZ;

    s_wcmd_rem_window_states(client, 2,
            "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_VERT");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_MAXIMIZED_HORZ");
}


/* Maximize client vertically */
void wcmd_client_maximize_vert(client_td *client)
{
    xcb_get_geometry_reply_t *attrs =
        xcb_get_geometry_reply(client->connection,
                xcb_get_geometry(client->connection,
                    client->window), NULL);
    if (!attrs) {
        return;
    }

    // TODO: Deactivate CLIENT_STATE_MAXIMIZED first?
    client_geometry_save(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            0,  /* X */
            0,  /* Y */
            client->layout.geometry.cur.dim.w, /* Current width */
            attrs->height   /* Height */
            });
    safe_free((void *) attrs);

    client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;

    s_wcmd_rem_window_states(client, 2,
            "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_HORZ");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_MAXIMIZED_VERT");
}


/* Maximize client entirely */
void wcmd_client_maximize(client_td *client)
{
    xcb_get_geometry_reply_t *attrs =
        xcb_get_geometry_reply(client->connection,
                xcb_get_geometry(client->connection,
                    client->window), NULL);
    if (!attrs) {
        return;
    }

    client_geometry_save(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            0,  /* X */
            0,  /* Y */
            attrs->width,   /* Width */
            attrs->height   /* Height */
            });

    client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    s_wcmd_add_window_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");

    safe_free((void *) attrs);
}


/* Iconify client (and minimize it) */
void wcmd_client_iconify(client_td *client)
{
    client_geometry_save(client);

    //unmap === iconify?  iconify => unmap?
    xcb_unmap_window(client->connection, client->window);

    client_set_hidden(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    // TODO: Logic to actually iconify!

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Hide the client (minimize, but not iconify) */
void wcmd_client_hide(client_td *client)
{
    client_geometry_save(client);

    xcb_unmap_window(client->connection, client->window);

    client_set_hidden(client);

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Show (unhide) the client */
void wcmd_client_unhide(client_td *client)
{
    client_geometry_save(client);

    xcb_map_window(client->connection, client->window);

    client_unset_hidden(client);

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Shade client (roll-up), if decorated */
void wcmd_client_shade(client_td *client)
{
    /* Only allow rolling-up if there's client decoration */
    if (!client_is_decorated(client)) {
        return;
    }

    // TODO: Logic to shade the client

    // TODO: This could involve reconfiguring client dimensions and
    // toggling state

    client_set_shade(client);

    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_SHADED");
    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_HIDDEN");
}

/* Unshade client (roll-down), if decorated */
void wcmd_client_unshade(client_td *client)
{
    /* Only allow rolling-down if there's client decoration */
    if (!client_is_decorated(client)) {
        return;
    }

    // TODO: Logic to unshade the client

    // TODO: This could involve reconfiguring client dimensions and
    // toggling state

    client_unset_shade(client);
    client_unset_hidden(client);

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_SHADED");
    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Toggle shading */
void wcmd_client_toggle_shade(client_td *client)
{
    if (client_is_shaded(client)) {
        client_unset_shade(client);
        s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_SHADED");
    } else {
        client_set_shade(client);
        s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_SHADED");
    }
}


/* Set client sticky mode */
void wcmd_client_sticky(client_td *client)
{
    client_set_sticky(client);
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_STICKY");
}

/* Remove client sticky mode */
void wcmd_client_unsticky(client_td *client)
{
    client_unset_sticky(client);
    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_STICKY");
}


/* Toggle stickiness */
void wcmd_client_toggle_sticky(client_td *client)
{
    if (client_is_sticky(client)) {
        client_unset_sticky(client);
        s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_STICKY");
    } else {
        client_set_sticky(client);
        s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_STICKY");
    }
}


/* Set full screen mode */
void wcmd_client_fullscreen(client_td *client)
{
    xcb_get_geometry_reply_t *attrs =
        xcb_get_geometry_reply(client->connection,
                xcb_get_geometry(client->connection,
                    client->window), NULL);
    if (!attrs) {
        return;
    }

    // TODO: Don't get the desktop dimensions and set the client size

    client_geometry_save(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            0, /* X */
            0, /* Y */
            attrs->width,  /* Width */
            attrs->height  /* Height */
            });

    client->properties.state = CLIENT_STATE_FULLSCREEN;

    // TODO: if decorated, hid the decoration whilst in this state

    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    safe_free((void *) attrs);
}


/* Remove full screen mode */
void wcmd_client_unfullscreen(client_td *client)
{
    client_geometry_restore(client);

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
            (uint32_t) client->layout.geometry.cur.dim.w,
            (uint32_t) client->layout.geometry.cur.dim.h
            });

    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {
            (uint32_t) client->layout.geometry.cur.pos.x,
            (uint32_t) client->layout.geometry.cur.pos.y
            });

    client->properties.state = CLIENT_STATE_NORMAL;

    // TODO: if decorated, restore the decoration

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
}


/* Toggle full screen mode */
void wcmd_client_toggle_fullscreen(client_td *client)
{
    if (client->properties.state == CLIENT_STATE_FULLSCREEN) {
        wcmd_client_unfullscreen(client);
        s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    } else {
        wcmd_client_fullscreen(client);
        s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    }
}



/* Raise the client to the top */
void wcmd_client_raise(client_td *client)
{
//    s_wcmd_raise_window(client->connection, client->window);

    // TODO: update the stack?
}


/* Lower the client to the bottom */
void wcmd_client_lower(client_td *client)
{
//    s_wcmd_lower_window(client->connection, client->window);
    // TODO: update the stack?
}


/* Put the client in the above layer */
void wcmd_client_layer_above(client_td *client)
{
//    s_wcmd_raise_window(client->connection, client->window);
    client->properties.layer = CLIENT_LAYER_ABOVE;

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_BELOW");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_ABOVE");
}


/* Put the client in the normal layer */
void wcmd_client_layer_normal(client_td *client)
{
//    s_wcmd_raise_window(client->connection, client->window);
    // TODO: update the stack?

    client->properties.layer = CLIENT_LAYER_NORMAL;

    s_wcmd_rem_window_states(client, 2,
            "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_BELOW");
}



/* Put the client in the below layer */
void wcmd_client_layer_below(client_td *client)
{
//    s_wcmd_lower_window(client->connection, client->window);
    // TODO: update the stack?

    client->properties.layer = CLIENT_LAYER_BELOW;

    s_wcmd_rem_window_states(client, 1, "_NET_WM_STATE_ABOVE");
    s_wcmd_add_window_states(client, 1, "_NET_WM_STATE_BELOW");
}

/* Set client urgency */
void wcmd_client_set_urgent(client_td *client)
{
    client_set_urgent(client);
//    ewmh_add_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Clear client urgency */
void wcmd_client_clear_urgent(client_td *client)
{
    client_unset_urgent(client);
//    ewmh_rem_net_wm_state(client->connection, client->window,
//    "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Set icon for the client */
void wcmd_client_set_icon(client_td *client,
        action_data_client_td *client_data)
{
    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_ICON_NAME,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(client_data->new_data.str.str0),
            client_data->new_data.str.str0);

    // Update the client properties.  Call any additional functions that
    // might handle update graphics.
    // ewmh_alter_net_wm_icon_name(client->connection, client->window,
    // client_data->new_data.icon_name);
}
