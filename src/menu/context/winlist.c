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
#include <string.h>     /* memset */
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <action.h>
#include <actdata.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <priority.h>
#include <surface.h>
#include <wm.h>

/* Command includes */
#include <cmds/scmd.h>

/* Initial definition values */
#include <defs/wm.h>

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

/**
 * @brief Label prefix/suffix added to non-clickable desktop labels
 */
#define WINLIST_LABEL_PREFIX "--- "
#define WINLIST_LABEL_SUFFIX " ---"

/** Singleton menu state */
static ctxmenu_state_td s_root;

/** Dynamically allocated entry array */
static ctxmenu_entry_td *s_entries = NULL;

/** Number of entries in @a s_entries */
static int s_entry_count = 0;


/**
 * @brief Userdata structure for the "Go there..." desktop-switch entry
 */
typedef struct {
    surface_td *surface;    /**< Surface that owns the desktop */
    uint32_t desktop_id;    /**< Destination desktop index */
} winlist_goto_data_td;


/** Per-entry goto userdata pool */
static winlist_goto_data_td s_goto_data[WINLIST_MAX_ENTRIES];


/**
 * @brief Callback: switch to a desktop from the window list
 *
 * @param connection XCB connection
 * @param userdata   Pointer to 'winlist_goto_data_td'
 */
static void s_cb_goto_desktop(xcb_connection_t *connection,
        void *userdata)
{
    winlist_goto_data_td *d;
    action_data_surface_td sdata;

    (void) connection;

    if (userdata == NULL) {
        return;
    }

    d = (winlist_goto_data_td *) userdata;
    if (d->surface == NULL) {
        return;
    }

    sdata.surface = d->surface;
    sdata.action_surface = ACTION_SURFACE_DESKTOP_SWITCH;
    sdata.new_data.uvalue = d->desktop_id;
    scmd_surface_desktop_switch(d->surface, &sdata);
}


/**
 * @brief Callback: switch to the desktop that holds a client, then
 *        focus and raise it
 *
 * @param connection XCB connection (unused; dispatch via event queue)
 * @param userdata   Pointer to @c client_td
 */
static void s_cb_focus_client(xcb_connection_t *connection,
        void *userdata)
{
    client_td *client;
    action_data_surface_td sdata;
    surface_td *surface;

    (void) connection;

    client = (client_td *) userdata;
    if (client == NULL) {
        return;
    }

    /* Switch to the desktop that contains this client first */
    surface = wm_get_surface_by_id(client->screen_id);
    if (surface != NULL) {
        sdata.surface = surface;
        sdata.action_surface = ACTION_SURFACE_DESKTOP_SWITCH;
        sdata.new_data.uvalue = client->desktop_id;
        scmd_surface_desktop_switch(surface, &sdata);
    }

    /* Then focus and raise */
    (void) client_send_event(client,
            ACTION_CLIENT_FOCUS, PRIORITY_NORMAL);
    (void) client_send_event(client,
            ACTION_CLIENT_RAISE, PRIORITY_NORMAL);
}


/* Open the window list menu */
void winlist_show(xcb_connection_t *connection,
        surface_td *surface, int16_t x, int16_t y,
        const config_td *config)
{
    uint32_t did;
    uint32_t cur_did;
    desktop_td *desktop;
    int n;
    client_td *client;
    const char *cname;
    char label_buf[WM_CTXMENU_LABEL_MAX_LEN];
    char name_buf[WM_CTXMENU_LABEL_MAX_LEN];

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
    cur_did = surface->desktop_cur;
    memset(s_goto_data, 0, sizeof(s_goto_data));

    for (did = 0;
            (did < surface->desktop_count) &&
                (n < WINLIST_MAX_ENTRIES - 1);
            ++did) {
        bool is_cur;
        bool has_clients;

        desktop = surface_desktop_get(surface, did);
        if (desktop == NULL) {
            continue;
        }

        is_cur = (did == cur_did);
        has_clients = (desktop->clients != NULL &&
                desktop->clients->size > 0);

        /* Desktop label with "--- ... ---" decoration */
        if (desktop->name[0] != '\0') {
            (void) snprintf(label_buf, sizeof(label_buf),
                    "%s[%u] -- %s%s",
                    WINLIST_LABEL_PREFIX, did, desktop->name,
                    WINLIST_LABEL_SUFFIX);
        } else {
            s_entries[n].type = CTXMENU_LABEL;
            safe_strncpy(s_entries[n].label, label_buf,
                    sizeof(s_entries[n].label) - 1u);
        }
        ++n;

        if (!has_clients) {
            /* Empty desktop: show "Go there..." entry.
             * Disabled when it is the currently active desktop. */
            if (n >= WINLIST_MAX_ENTRIES - 1) {
                break;
            }

            s_entries[n].type = CTXMENU_COMMAND;
            safe_strncpy(s_entries[n].label, "Go there...",
                    sizeof(s_entries[n].label) - 1u);
            s_entries[n].is_disabled = is_cur;
            if (!is_cur) {
                s_goto_data[n].surface = surface;
                s_goto_data[n].desktop_id = did;
                s_entries[n].on_activate = s_cb_goto_desktop;
                s_entries[n].userdata = &s_goto_data[n];
            }
            ++n;
        } else {
            /* One entry per client on this desktop */
            ohtbl_foreach(desktop->clients, client) {
                if (n >= WINLIST_MAX_ENTRIES - 1) {
                    break;
                }

                cname = (client->info.name != NULL &&
                        client->info.name[0] != '\0')
                    ? client->info.name : "(unnamed)";

                /* Format based on state */
                if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
                    (void) snprintf(name_buf, sizeof(name_buf),
                            "(%s)", cname);
                } else if (client->properties.state ==
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                    (void) snprintf(name_buf, sizeof(name_buf),
                            "[%s]", cname);
                } else {
                    (void) snprintf(name_buf, sizeof(name_buf),
                            "%s", cname);
                }

                s_entries[n].type = CTXMENU_COMMAND;
                safe_strncpy(s_entries[n].label, name_buf,
                        sizeof(s_entries[n].label) - 1u);
                s_entries[n].on_activate = s_cb_focus_client;
                s_entries[n].userdata = client;
                ++n;
            }
        }
    }

    if (n == 0) {
        /* Nothing to show; add a placeholder */
        s_entries[n].type = CTXMENU_LABEL;
        safe_strncpy(s_entries[n].label, "(no windows)",
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
