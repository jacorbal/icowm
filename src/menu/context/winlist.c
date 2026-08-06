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
#include <adt/cdlist.h>
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
    client_td *client;      /**< Client to focus, or @c NULL */
    uint32_t desktop_id;    /**< Destination desktop index */
} winlist_entry_data_td;


/** Per-entry userdata pool */
static winlist_entry_data_td s_entry_data[WINLIST_MAX_ENTRIES];


/**
 * @brief Switch to a desktop from the window list
 *
 * Callback invoked when a desktop entry in the window list is
 * activated.  Triggers a desktop switch to the desktop stored in
 * @p userdata.
 *
 * @param connection XCB connection (unused)
 * @param userdata   Pointer to a @c winlist_entry_data_td with the
 *                   target surface and desktop ID
 */
static void s_cb_goto_desktop(xcb_connection_t *connection,
        void *userdata)
{
    winlist_entry_data_td *data;
    action_data_surface_td sdata;

    (void) connection;

    data = (winlist_entry_data_td *) userdata;
    if (data == NULL || data->surface == NULL) {
        return;
    }

    sdata.surface = data->surface;
    sdata.action_surface = ACTION_SURFACE_DESKTOP_SWITCH;
    sdata.new_data.uvalue = data->desktop_id;
    scmd_surface_desktop_switch(data->surface, &sdata);
}



/**
 * @brief Switch to the selected desktop and optionally focus its client
 *
 * Callback invoked when a client entry in the window list is activated.
 * Switches to the associated desktop and, if a client is stored in
 * @p userdata, sends focus and raise events to that client.
 *
 * @param connection XCB connection (unused)
 * @param userdata   Pointer to a @c winlist_entry_data_td with the
 *                   target surface, desktop ID, and optional client
 */
static void s_cb_focus_client(xcb_connection_t *connection,
        void *userdata)
{
    winlist_entry_data_td *data;
    action_data_surface_td sdata;
    uint32_t target_did;

    (void) connection;

    data = (winlist_entry_data_td *) userdata;
    if (data == NULL || data->surface == NULL) {
        return;
    }

    target_did = (data->client != NULL)
        ? data->client->desktop_id
        : data->desktop_id;

    sdata.surface = data->surface;
    sdata.action_surface = ACTION_SURFACE_DESKTOP_SWITCH;
    sdata.new_data.uvalue = target_did;
    scmd_surface_desktop_switch(data->surface, &sdata);
    if (data->client == NULL) {
        return;
    }

    /* Restore the window that was selected */
    if (data->client->properties.flags & CLIENT_FLAG_HIDDEN) {
        if (data->client->properties.state ==
                (uint16_t) CLIENT_STATE_ICONIFIED) {
            (void) client_send_event_restore(data->client);
        } else {
            (void) client_send_event(data->client,
                    ACTION_CLIENT_UNHIDE, PRIORITY_NORMAL);
        }
    }
    if (client_is_shaded(data->client)) {
        (void) client_send_event(data->client,
                ACTION_CLIENT_UNSHADE, PRIORITY_NORMAL);
    }

    (void) client_send_event(data->client,
            ACTION_CLIENT_FOCUS, PRIORITY_NORMAL);
    (void) client_send_event(data->client,
            ACTION_CLIENT_RAISE, PRIORITY_NORMAL);
}


/**
 * @brief Format a window-list entry label according to the client state
 *
 * Wraps @p name in curly braces if the client is hidden, in parentheses
 * if iconified, or copies it verbatim otherwise.  The result is written
 * into @p buf.
 *
 * @param client   Client whose state determines the format
 * @param name     Base name string to format
 * @param buf      Destination buffer for the formatted label
 * @param buf_size Size of @p buf in bytes
 */
static void s_format_client_label(const client_td *client,
        const char *name, char *buf, size_t buf_size)
{
    if (client == NULL || name == NULL || buf == NULL ||
            buf_size == 0u) {
        return;
    }

    if (client->properties.state == (uint16_t) CLIENT_STATE_ICONIFIED) {
        (void) snprintf(buf, buf_size, "{%s}", name);
    } else if (client->properties.flags &
            CLIENT_FLAG_HIDDEN) {
        (void) snprintf(buf, buf_size, "(%s)", name);
    } else {
        (void) snprintf(buf, buf_size, "%s", name);
    }
}


/**
 * @brief Append one menu entry for a client in a given desktop section
 *
 * Fills the next available slot in @c s_entries with a command entry
 * for @p client, associating it with @p did so that activating it
 * switches to @p did and then focuses the client.
 *
 * @param client      Client to add
 * @param did         Desktop section this entry belongs to
 * @param surface     Surface that owns the desktop
 * @param entry_count Current number of entries; updated on return
 */
static void s_append_client_entry(client_td *client, uint32_t did,
        surface_td *surface, int *entry_count)
{
    const char *cname;
    char name_buf[WM_CTXMENU_LABEL_MAX_LEN];
    int n;

    if (client == NULL || surface == NULL || entry_count == NULL) {
        return;
    }

    n = *entry_count;
    if (n >= WINLIST_MAX_ENTRIES - 1) {
        return;
    }

    cname = (client->info.name != NULL && client->info.name[0] != '\0')
        ? client->info.name : "(unnamed)";
    s_format_client_label(client, cname, name_buf, sizeof(name_buf));
    s_entries[n].type = CTXMENU_COMMAND;
    safe_strncpy(s_entries[n].label, name_buf,
            sizeof(s_entries[n].label) - 1u);
    s_entry_data[n].surface = surface;
    s_entry_data[n].client = client;
    s_entry_data[n].desktop_id = did;
    s_entries[n].on_activate = s_cb_focus_client;
    s_entries[n].userdata = &s_entry_data[n];
    *entry_count = n + 1;
}


/**
 * @brief Append sticky clients from every desktop to a desktop section
 *
 * Sticky (pinned) clients are physically stored in the current desktop
 * after each desktop switch.  To show them under every desktop section
 * in the window list, this function scans all desktops on @p surface
 * and appends any client that is sticky, regardless of which desktop's
 * hash table currently holds it.  Avoids duplicating a sticky client
 * that is already present in the section because it happens to be in
 * that desktop's hash table.
 *
 * @param surface     Surface that owns all desktops
 * @param did         Desktop section to populate with sticky entries
 * @param entry_count Current number of entries; updated on return
 */
static void s_add_sticky_clients(surface_td *surface, uint32_t did,
        int *entry_count)
{
    cdlist_item_td *dnode;
    cdlist_item_td *dinitial;
    desktop_td *desktop;
    client_td *client;

    if (surface == NULL || surface->desktops == NULL ||
            entry_count == NULL) {
        return;
    }

    dnode = cdlist_head(surface->desktops);
    if (dnode == NULL) {
        return;
    }

    dinitial = dnode;
    do {
        desktop = (desktop_td *) cdlist_data(dnode);
        if (desktop != NULL && desktop->clients != NULL) {
            ohtbl_foreach(desktop->clients, client) {
                if (client == NULL) {
                    continue;
                }
                if (!client_is_sticky(client)) {
                    continue;
                }
                /* Skip panels, docks, and other windows that have asked
                 * not to appear in taskbar/window lists */
                if (client->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) {
                    continue;
                }
                /* Only add if this desktop's hash table does not
                 * already hold it for this section (it will be listed
                 * by 's_add_desktop_clients' when did matches the
                 * desktop the sticky client is physically stored in). */
                if (desktop == surface_desktop_get(surface, did)) {
                    continue;
                }
                s_append_client_entry(client, did, surface, entry_count);
                if (*entry_count >= WINLIST_MAX_ENTRIES - 1) {
                    return;
                }
            }
        }
        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


/**
 * @brief Append all clients that belong to a desktop as menu entries
 *
 * Iterates over the client hash table of the desktop identified by
 * @p did and appends one @c CTXMENU_COMMAND entry per visible client.
 * Non-sticky clients are included only when their @c desktop_id matches
 * @p did.  Sticky clients found in this desktop's hash table are always
 * included; sticky clients stored in other desktops are added by
 * @c s_add_sticky_clients.  Stops early when @c WINLIST_MAX_ENTRIES is
 * reached.
 *
 * @param surface     Surface that owns the desktop
 * @param did         Desktop ID whose clients are to be listed
 * @param entry_count Current number of entries; updated on return to
 *                    reflect the entries appended
 */
static void s_add_desktop_clients(surface_td *surface, uint32_t did,
        int *entry_count)
{
    desktop_td *desktop;
    client_td *client;

    if (surface == NULL || entry_count == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, did);
    if (desktop == NULL || desktop->clients == NULL) {
        return;
    }

    ohtbl_foreach(desktop->clients, client) {
         if (*entry_count >= WINLIST_MAX_ENTRIES - 1) {
            break;
        }

        /* Skip panels, docks, and other windows that have asked not to
         * appear in taskbar/window lists */
        if (client->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) {
            continue;
        }

        /* Include every client physically stored in this desktop's hash
         * table whose 'desktop_id' matches.  Sticky clients that happen
         * to be stored here (because this is the current desktop) are
         * also included; sticky clients stored in other desktops are
         * added separately by 's_add_sticky_clients'. */
        if (!client_is_sticky(client) && client->desktop_id != did) {
            continue;
        }

        s_append_client_entry(client, did, surface, entry_count);
    }

    /* Add sticky clients that are currently stored in other desktops */
    s_add_sticky_clients(surface, did, entry_count);
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
    const char *label_fmt;
    char label_buf[WM_CTXMENU_LABEL_MAX_LEN];

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
    memset(s_entry_data, 0, sizeof(s_entry_data));

    for (did = 0;
            (did < surface->desktop_count) &&
                (n < WINLIST_MAX_ENTRIES - 1);
            ++did) {
        bool is_cur;
        int before_clients;

        desktop = surface_desktop_get(surface, did);
        if (desktop == NULL) {
            continue;
        }

        is_cur = did == cur_did;
        label_fmt = (desktop->name[0] != '\0')
            ? "%s[%u] -- %s%s"
            : "%s[%u]%s";

        if (desktop->name[0] != '\0') {
                (void) snprintf(label_buf, sizeof(label_buf), label_fmt,
                    MENU_CONTEXT_CTXMENU_LABEL_PREFIX,
                    did, desktop->name,
                    MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);
        } else {
            (void) snprintf(label_buf, sizeof(label_buf), label_fmt,
                    MENU_CONTEXT_CTXMENU_LABEL_PREFIX,
                    did,
                    MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);
        }
        s_entries[n].type = CTXMENU_LABEL;
        safe_strncpy(s_entries[n].label, label_buf,
                sizeof(s_entries[n].label) - 1u);
        ++n;

        before_clients = n;
        s_add_desktop_clients(surface, did, &n);
        if (n != before_clients) {
            continue;
        }

        if (n >= WINLIST_MAX_ENTRIES - 1) {
            break;
        }

        s_entries[n].type = CTXMENU_COMMAND;
        safe_strncpy(s_entries[n].label, "Go there...",
                sizeof(s_entries[n].label) - 1u);
        s_entries[n].is_disabled = is_cur;
        if (!is_cur) {
            s_entry_data[n].surface = surface;
            s_entry_data[n].client = NULL;
            s_entry_data[n].desktop_id = did;
            s_entries[n].on_activate = s_cb_goto_desktop;
            s_entries[n].userdata = &s_entry_data[n];
        }
        ++n;
    }

    if (n == 0) {
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
        surface_td *surface, xcb_window_t win, int root_x, int root_y,
        const config_td *config)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state == NULL) {
        return false;
    }

    return ctxmenu_handle_click(connection, surface, state,
            root_x, root_y, config);
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


/* Handle a key-press event while the window list menu is open */
bool winlist_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config)
{
    return ctxmenu_handle_keypress(connection, surface, &s_root,
            keysym, config);
}


/* Handle a pointer-motion event over the window list menu */
void winlist_handle_motion(xcb_window_t win, int x, int y)
{
    /* Window list uses a single state; win check is implicit */
    (void) win;
    ctxmenu_handle_motion(&s_root, x, y);
}
