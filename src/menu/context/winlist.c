/**
 * @file menu/context/winlist.c
 *
 * @brief Window list menu implementation
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdlib.h>     /* calloc, free */
#include <string.h>     /* memset, strncpy, snprintf */
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/ohtbl.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <priority.h>
#include <surface.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/winlist.h>


/**
 * @brief Maximum total entries in the window list menu
 *
 * One label per desktop + one entry per client.  Cap to avoid
 * over-allocation when many clients are open.
 */
#define WINLIST_MAX_ENTRIES (256)


/** Singleton menu state */
static ctxmenu_state_td s_root;

/** Dynamically allocated entry array */
static ctxmenu_entry_td *s_entries = NULL;

/** Number of entries in @a s_entries */
static int s_entry_count = 0;

/** Per-entry client back-pointers for the activation callback */
static client_td *s_client_refs[WINLIST_MAX_ENTRIES];


/**
 * @brief Callback: focus and raise the selected client
 *
 * @param connection XCB connection (unused; dispatch via event queue)
 * @param userdata   Pointer to @c client_td
 */
static void s_cb_focus_client(xcb_connection_t *connection,
        void *userdata)
{
    client_td *client;

    (void) connection;

    client = (client_td *) userdata;
    if (client == NULL) {
        return;
    }

    (void) client_send_event(client, ACTION_CLIENT_FOCUS, PRIORITY_NORMAL);
    (void) client_send_event(client, ACTION_CLIENT_RAISE, PRIORITY_NORMAL);
}


/* Open the window list menu */
void winlist_show(xcb_connection_t *connection,
        surface_td *surface, int16_t x, int16_t y,
        const config_td *config)
{
    uint32_t did;
    desktop_td *desktop;
    int n;
    client_td *client;
    const char *cname;

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    winlist_close();

    s_entries = (ctxmenu_entry_td *) calloc(
            (size_t) WINLIST_MAX_ENTRIES, sizeof(ctxmenu_entry_td));
    if (s_entries == NULL) {
        return;
    }

    n = 0;
    memset(s_client_refs, 0, sizeof(s_client_refs));

    for (did = 0; did < surface->desktop_count &&
            n < WINLIST_MAX_ENTRIES - 1; ++did) {
        desktop = surface_desktop_get(surface, did);
        if (desktop == NULL || desktop->clients == NULL) {
            continue;
        }
        if (desktop->clients->size == 0) {
            continue;
        }

        /* Desktop label */
        s_entries[n].type = CTXMENU_LABEL;
        if (desktop->name[0] != '\0') {
            (void) snprintf(s_entries[n].label,
                    sizeof(s_entries[n].label),
                    "[%u] -- %s", did, desktop->name);
        } else {
            (void) snprintf(s_entries[n].label,
                    sizeof(s_entries[n].label),
                    "[%u]", did);
        }
        ++n;

        /* One entry per client on this desktop */
        ohtbl_foreach(desktop->clients, client) {
            if (n >= WINLIST_MAX_ENTRIES - 1) {
                break;
            }
            s_entries[n].type = CTXMENU_COMMAND;
            cname = (client->info.name != NULL &&
                    client->info.name[0] != '\0')
                ? client->info.name : "(unnamed)";
            strncpy(s_entries[n].label, cname,
                    sizeof(s_entries[n].label) - 1u);
            s_entries[n].on_activate = s_cb_focus_client;
            s_client_refs[n] = client;
            s_entries[n].userdata = client;
            ++n;
        }
    }

    if (n == 0) {
        /* Nothing to show; add a placeholder */
        s_entries[n].type = CTXMENU_LABEL;
        strncpy(s_entries[n].label, "(no windows)",
                sizeof(s_entries[n].label) - 1u);
        ++n;
    }

    s_entry_count = n;

    memset(&s_root, 0, sizeof(s_root));
    s_root.window = XCB_WINDOW_NONE;
    s_root.entries = s_entries;
    s_root.entry_count = n;

    ctxmenu_show(connection, surface, &s_root, x, y, config);
}


/* Close the window list menu */
void winlist_close(void)
{
    ctxmenu_close(&s_root);

    if (s_entries != NULL) {
        free(s_entries);
        s_entries = NULL;
    }
    s_entry_count = 0;
}


/* Repaint the window list menu */
void winlist_repaint(xcb_window_t win)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state != NULL) {
        ctxmenu_repaint(state);
    }
}


/* Handle a button-press event inside the window list menu */
bool winlist_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int x, int y,
        const config_td *config)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state == NULL) {
        return false;
    }
    return ctxmenu_handle_click(connection, surface, state,
            x, y, config);
}


/* Query whether the window list menu is currently open */
bool winlist_is_open(void)
{
    return ctxmenu_is_open(&s_root);
}


/* Return the window list menu XCB window */
xcb_window_t winlist_window(void)
{
    return s_root.window;
}


/* Check whether 'win' belongs to the window list menu */
bool winlist_owns_window(xcb_window_t win)
{
    return (s_root.window != XCB_WINDOW_NONE) && (s_root.window == win);
}
