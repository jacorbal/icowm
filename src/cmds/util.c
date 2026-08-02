/**
 * @file cmds/util.c
 *
 * @brief Internal utility implementation shared across the @c cmds
          subsystem
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
#include <stdlib.h>     /* NULL, free, malloc */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Utils includes */
#include <utils/safestr.h>

/* Project includes */
#include <client.h>

/* Local includes */
#include <cmds/util.h>


/* Retrieve the ID of the currently active window for a screen */
xcb_window_t wcmd_active_win(xcb_ewmh_connection_t *ewmh,
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


/* Intern an atom name in the X11 system */
xcb_atom_t wcmd_intern_atom(xcb_connection_t *connection,
        const char *name)
{
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(connection, 0,
                (uint16_t) safe_strlen(name), name);
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(connection, cookie, NULL);

    if (reply) {
        xcb_atom_t atom = reply->atom;
        free(reply);
        return atom;
    }

    return XCB_ATOM_NONE;
}


/* Return the frame window when decorated, otherwise the client window */
xcb_window_t wcmd_target_win(client_td *client)
{
    if (client == NULL) {
        return XCB_WINDOW_NONE;
    }
    if (client_is_decorated(client) && client->frame != 0) {
        return client->frame;
    }
    return client->window;
}


/* Passively grab all mouse buttons on an undecorated client */
void wcmd_client_grab_buttons(client_td *client)
{
    if (client == NULL || client->connection == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_grab_button(client->connection,
            0,
            client->window,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
            XCB_GRAB_MODE_SYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            XCB_NONE,
            XCB_BUTTON_INDEX_ANY,
            XCB_MOD_MASK_ANY);
    xcb_flush(client->connection);
}


/* Remove passive button grabs from an undecorated client */
void wcmd_client_ungrab_buttons(client_td *client)
{
    if (client == NULL || client->connection == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_ungrab_button(client->connection,
            (uint8_t) XCB_BUTTON_INDEX_ANY,
            client->window,
            (uint16_t) XCB_MOD_MASK_ANY);
    xcb_flush(client->connection);
}


/* Write the ICCCM 'WM_STATE' property for a client */
void wcmd_set_wm_state(client_td *client,
        uint32_t state, xcb_window_t icon_window)
{
    xcb_atom_t wm_state;
    uint32_t values[2];

    if (client == NULL || client->connection == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    wm_state = wcmd_intern_atom(client->connection, "WM_STATE");
    if (wm_state == XCB_ATOM_NONE) {
        return;
    }

    values[0] = state;
    values[1] = icon_window;
    xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
            client->window, wm_state, wm_state, 32, 2, values);
    xcb_flush(client->connection);
}


/*  Remove the ICCCM 'WM_STATE' property from a client */
void wcmd_clear_wm_state(client_td *client)
{
    xcb_atom_t wm_state;

    if (client == NULL || client->connection == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    wm_state = wcmd_intern_atom(client->connection, "WM_STATE");
    if (wm_state == XCB_ATOM_NONE) {
        return;
    }

    xcb_delete_property(client->connection, client->window, wm_state);
    xcb_flush(client->connection);
}


/* Retrieve the pixel dimensions of the client's current screen */
bool wcmd_screen_dim(client_td *client, uint16_t *out_w, uint16_t *out_h)
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


/* Add multiple EWMH window states to a client */
void wcmd_add_states(client_td *client, uint32_t num_states, ...)
{
    xcb_atom_t *add_atoms;
    xcb_atom_t *merged;
    xcb_atom_t *cur_atoms;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    uint32_t cur_len;
    uint32_t merged_count;
    bool already_set;
    va_list args;

    if (client == NULL || client->ewmh == NULL || num_states == 0) {
        return;
    }

    add_atoms = malloc(num_states * sizeof(xcb_atom_t));
    if (add_atoms == NULL) {
        return;
    }

    va_start(args, num_states);
    for (uint32_t i = 0; i < num_states; ++i) {
        const char *state_name = va_arg(args, const char *);
        add_atoms[i] = wcmd_intern_atom(client->connection, state_name);
        if (add_atoms[i] == XCB_ATOM_NONE) {
            free(add_atoms);
            va_end(args);
            return;
        }
    }
    va_end(args);

    /* Read existing '_NET_WM_STATE' to merge rather than replace */
    cookie = xcb_ewmh_get_wm_state(client->ewmh, client->window);
    reply = xcb_get_property_reply(client->ewmh->connection,
            cookie, NULL);

    cur_len = 0;
    cur_atoms = NULL;
    if (reply != NULL && xcb_get_property_value_length(reply) > 0) {
        cur_len = (uint32_t) xcb_get_property_value_length(reply) /
            sizeof(xcb_atom_t);
        cur_atoms = (xcb_atom_t *) xcb_get_property_value(reply);
    }

    merged = malloc((cur_len + num_states) * sizeof(xcb_atom_t));
    if (merged == NULL) {
        free(add_atoms);
        if (reply != NULL) {
            free(reply);
        }
        return;
    }

    for (uint32_t i = 0; i < cur_len; ++i) {
        merged[i] = cur_atoms[i];
    }
    merged_count = cur_len;


    for (uint32_t i = 0; i < num_states; ++i) {
        already_set = false;
        for (uint32_t j = 0; j < cur_len; ++j) {
            if (cur_atoms[j] == add_atoms[i]) {
                already_set = true;
                break;
            }
        }
        if (!already_set) {
            merged[merged_count++] = add_atoms[i];
        }
    }

    xcb_ewmh_set_wm_state(client->ewmh, client->window,
            merged_count, merged);
    xcb_flush(client->connection);

    free(merged);
    free(add_atoms);
    if (reply != NULL) {
        free(reply);
    }

}


/* Remove multiple EWMH window states from a client */
void wcmd_rem_states(client_td *client, uint32_t num_states, ...)
{
    xcb_atom_t *remove_states;
    xcb_atom_t *new_states;
    xcb_atom_t *current_atoms;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    uint32_t current_len;
    uint32_t new_count;
    va_list args;
    bool should_remove;

    if (client == NULL || client->ewmh == NULL || num_states == 0) {
        return;
    }

    remove_states = malloc(num_states * sizeof(xcb_atom_t));
    if (remove_states == NULL) {
        return;
    }

    va_start(args, num_states);
    for (uint32_t i = 0; i < num_states; ++i) {
        const char *state_name = va_arg(args, const char *);
        remove_states[i] = wcmd_intern_atom(client->connection,
                state_name);
        if (remove_states[i] == XCB_ATOM_NONE) {
            free(remove_states);
            va_end(args);
            return;
        }
    }
    va_end(args);

    /* Use 'xcb_get_property_reply' directly so we hold the full reply
     * object and can free it with a single 'free'(reply) call.  The
     * 'xcb_ewmh' atoms pointer would point into the middle of the same
     * allocation and must never be passed to 'free' individually. */
    cookie = xcb_ewmh_get_wm_state(client->ewmh, client->window);
    reply = xcb_get_property_reply(client->ewmh->connection,
            cookie, NULL);

    if (reply == NULL ||
            xcb_get_property_value_length(reply) == 0) {
        free(remove_states);
        if (reply != NULL) {
            free(reply);
        }
        return;
    }

    current_len = (uint32_t) xcb_get_property_value_length(reply) /
        sizeof(xcb_atom_t);
    current_atoms = (xcb_atom_t *) xcb_get_property_value(reply);
    new_states = malloc(current_len * sizeof(xcb_atom_t));
    if (new_states == NULL) {
        free(remove_states);
        free(reply);
        return;
    }

    new_count = 0;
    for (uint32_t i = 0; i < current_len; ++i) {
        should_remove = false;

        for (uint32_t j = 0; j < num_states; ++j) {
            if (current_atoms[i] == remove_states[j]) {
                should_remove = true;
                break;
            }
        }

        if (!should_remove) {
            new_states[new_count++] = current_atoms[i];
        }
    }

    if (new_count > 0) {
        xcb_ewmh_set_wm_state(client->ewmh, client->window, new_count,
                new_states);
    } else {
        xcb_ewmh_set_wm_state(client->ewmh, client->window, 0, NULL);
    }
    xcb_flush(client->connection);

    free(remove_states);
    free(new_states);
    free(reply);
}


/* Publish '_NET_FRAME_EXTENTS' on the client window */
void wcmd_publish_frame_extents(client_td *client,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom)
{
    uint32_t extents[4];

    if (client == NULL || client->ewmh == NULL) {
        return;
    }

    extents[0] = left;
    extents[1] = right;
    extents[2] = top;
    extents[3] = bottom;
    xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
            client->window, client->ewmh->_NET_FRAME_EXTENTS,
            XCB_ATOM_CARDINAL, 32, 4, extents);
}
