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
    xcb_atom_t *states;
    va_list args;

    if (client == NULL || client->ewmh == NULL || num_states == 0) {
        return;
    }

    states = malloc(num_states * sizeof(xcb_atom_t));
    if (states == NULL) {
        return;
    }

    va_start(args, num_states);
    for (uint32_t i = 0; i < num_states; ++i) {
        const char *state_name = va_arg(args, const char *);
        states[i] = wcmd_intern_atom(client->connection, state_name);
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


/* Remove multiple EWMH window states from a client */
void wcmd_rem_states(client_td *client, uint32_t num_states, ...)
{
    xcb_atom_t *remove_states;
    xcb_atom_t *new_states;
    xcb_get_property_cookie_t cookie;
    xcb_ewmh_get_atoms_reply_t current_states_reply;
    uint32_t new_count;
    va_list args;
    bool should_remove;
    uint8_t success;

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
        remove_states[i] = wcmd_intern_atom(client->connection, state_name);
        if (remove_states[i] == XCB_ATOM_NONE) {
            free(remove_states);
            va_end(args);
            return;
        }
    }
    va_end(args);

    cookie = xcb_ewmh_get_wm_state(client->ewmh, client->window);
    success = xcb_ewmh_get_wm_state_reply(client->ewmh, cookie,
            &current_states_reply, NULL);

    if (!success || current_states_reply.atoms_len == 0) {
        free(remove_states);
        if (success) {
            free(current_states_reply.atoms);
        }
        return;
    }

    new_states =
        malloc(current_states_reply.atoms_len * sizeof(xcb_atom_t));
    if (new_states == NULL) {
        free(remove_states);
        free(current_states_reply.atoms);
        return;
    }

    new_count = 0;
    for (uint32_t i = 0; i < current_states_reply.atoms_len; ++i) {
        should_remove = false;

        for (uint32_t j = 0; j < num_states; ++j) {
            if (current_states_reply.atoms[i] == remove_states[j]) {
                should_remove = true;
                break;
            }
        }

        if (!should_remove) {
            new_states[new_count++] = current_states_reply.atoms[i];
        }
    }

    if (new_count > 0) {
        xcb_ewmh_set_wm_state(client->ewmh, client->window, new_count,
                new_states);
    } else {
        xcb_ewmh_set_wm_state(client->ewmh, client->window, 0, NULL);
    }

    free(remove_states);
    free(new_states);
    free(current_states_reply.atoms);
}
