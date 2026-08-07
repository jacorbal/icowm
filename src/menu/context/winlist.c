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

/* Policy includes */
#include <policy/focus.h>

/* Initial definition values */
#include <defs/wm.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/winlist.h>


/**
 * @brief Maximum desktops shown as top-level entries
 *
 * A generous cap on how many per-desktop submenus can exist at once;
 * far above any realistic desktop count.
 */
#define WINLIST_MAX_DESKTOPS (16)

/**
 * @brief Maximum entries (windows and application-group submenus
 *        combined) inside a single desktop's submenu
 */
#define WINLIST_MAX_ENTRIES_PER_DESKTOP (64)

/**
 * @brief Maximum simultaneously open application-group submenus, summed
 *        across every desktop submenu
 *
 * Only applications with two or more windows on the same desktop get
 * one of these; single-window applications are listed directly.
 */
#define WINLIST_MAX_APPGROUPS (32)

/**
 * @brief Maximum windows listed inside a single application-group
 *        submenu
 */
#define WINLIST_MAX_APPGROUP_SIZE (32)

/**
 * @brief Size of the scratch buffer used to collect a desktop's
 *        candidate clients before grouping them by application
 */
#define WINLIST_MAX_COLLECTED (128)

/**
 * @brief Size of the shared pool of per-entry userdata records
 *
 * Sized to cover the worst case at every level: one per desktop (for
 * its "Go there..." entry), one per entry in every desktop submenu, and
 * one per window in every application-group submenu.
 */
#define WINLIST_MAX_ENTRY_DATA \
    (WINLIST_MAX_DESKTOPS + \
     WINLIST_MAX_DESKTOPS * WINLIST_MAX_ENTRIES_PER_DESKTOP + \
     WINLIST_MAX_APPGROUPS * WINLIST_MAX_APPGROUP_SIZE)

/** Singleton root menu state: one @c CTXMENU_SUBMENU entry per desktop */
static ctxmenu_state_td s_root;

/** Entries for the top-level (per-desktop) menu */
static ctxmenu_entry_td s_root_entries[WINLIST_MAX_DESKTOPS + 1];

/** State for each desktop's submenu */
static ctxmenu_state_td s_desktop_state[WINLIST_MAX_DESKTOPS];

/** Entries for each desktop's submenu */
static ctxmenu_entry_td
    s_desktop_entries[WINLIST_MAX_DESKTOPS][WINLIST_MAX_ENTRIES_PER_DESKTOP];

/** State for each application-group submenu, allocated on demand */
static ctxmenu_state_td s_appgroup_state[WINLIST_MAX_APPGROUPS];

/** Entries for each application-group submenu */
static ctxmenu_entry_td
    s_appgroup_entries[WINLIST_MAX_APPGROUPS][WINLIST_MAX_APPGROUP_SIZE];

/** Number of application-group slots claimed during this 'winlist_show' */
static int s_appgroup_used = 0;



/**
 * @brief Userdata structure for a window-list menu entry
 */
typedef struct {
    surface_td *surface;    /**< Surface that owns the desktop */
    client_td *client;      /**< Client to focus, or @c NULL */
    uint32_t desktop_id;    /**< Destination desktop index */
} winlist_entry_data_td;


/** Shared pool of per-entry userdata, handed out sequentially */
static winlist_entry_data_td s_entry_data[WINLIST_MAX_ENTRY_DATA];

/** Number of @e s_entry_data slots claimed during this @a winlist_show */
static int s_entry_data_used = 0;


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
 * @brief Switch to the selected desktop, focus, and raise its client
 *
 * Callback invoked when a client entry in the window list is activated.
 * Switches to the associated desktop and, if a client is stored in
 * @p userdata, makes it the active client on that desktop (not just
 * giving it real keyboard focus): sets @c client_active_id, unfocuses
 * whichever client was active there before, and raises it, all via
 * @c focus_apply; the same path every other "focus this client" action
 * in the window manager goes through, instead of only sending a raw
 * focus event that would leave the window receiving keystrokes without
 * ever becoming the window manager's own notion of the active window
 * (e.g., its titlebar not highlighting as active).
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
    desktop_td *target_desktop;

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

    target_desktop = surface_desktop_get(data->surface, target_did);
    focus_apply(wm_get_surfaces(), data->surface, target_desktop,
            data->client, true, NULL);
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
 * @return Pointer to a zeroed @c winlist_entry_data_td slot, or @c NULL
 *         if the pool is exhausted
 */
static winlist_entry_data_td *s_alloc_entry_data(void)
{
    winlist_entry_data_td *slot;

    if (s_entry_data_used >= WINLIST_MAX_ENTRY_DATA) {
        return NULL;
    }

    slot = &s_entry_data[s_entry_data_used];
    s_entry_data_used++;
    memset(slot, 0, sizeof(*slot));

    return slot;
}


/**
 * @brief Append one command entry for a client into an entries array
 *
 * @param client      Client to add
 * @param did         Desktop this entry switches to when activated
 * @param surface     Surface that owns the desktop
 * @param out_entries Destination entries array
 * @param out_cap     Capacity of @p out_entries
 * @param out_count   Current entry count in @p out_entries; advanced by
 *                    one on success
 */
static void s_append_client_entry(client_td *client, uint32_t did,
        surface_td *surface, ctxmenu_entry_td *out_entries,
        int out_cap, int *out_count)
{
    const char *cname;
    char name_buf[WM_CTXMENU_LABEL_MAX_LEN];
    winlist_entry_data_td *data;
    int n;

    if (client == NULL || surface == NULL || out_entries == NULL ||
            out_count == NULL) {
        return;
    }

    n = *out_count;
    if (n >= out_cap) {
        return;
    }

    data = s_alloc_entry_data();
    if (data == NULL) {
        return;
    }

    cname = (client->info.name != NULL && client->info.name[0] != '\0')
        ? client->info.name : "(unnamed)";
    s_format_client_label(client, cname, name_buf, sizeof(name_buf));
    out_entries[n].type = CTXMENU_COMMAND;
    safe_strncpy(out_entries[n].label, name_buf,
            sizeof(out_entries[n].label) - 1u);
    data->surface = surface;
    data->client = client;
    data->desktop_id = did;
    out_entries[n].on_activate = s_cb_focus_client;
    out_entries[n].userdata = data;
    *out_count = n + 1;
}


/**
 * @brief Best-effort display name for an application group
 *
 * Prefers the group leader's own @c WM_CLASS class name (more stable
 * across an application's windows than each window's own title); falls
 * back to the first member's window title when unavailable.
 *
 * @param members  Array of the group's client pointers
 * @param member_n Number of entries in @p members
 * @param buf      Destination buffer
 * @param buf_size Size of @p buf in bytes
 */
static void s_appgroup_label(client_td * const *members, int member_n,
        char *buf, size_t buf_size)
{
    const char *name;

    if (members == NULL || member_n <= 0 || buf == NULL ||
            buf_size == 0u) {
        return;
    }

    name = NULL;
    for (int i = 0; i < member_n && name == NULL; ++i) {
        if (members[i] != NULL && members[i]->info.class_name[1] != NULL &&
                members[i]->info.class_name[1][0] != '\0') {
            name = members[i]->info.class_name[1];
        }
    }
    if (name == NULL) {
        name = (members[0] != NULL && members[0]->info.name != NULL &&
                members[0]->info.name[0] != '\0')
            ? members[0]->info.name : "(unnamed)";
    }

    (void) snprintf(buf, buf_size, "%s (%d)", name, member_n);
}


/**
 * @brief Build one desktop's submenu entries
 *
 * Collects every client that belongs to @p did (physically stored
 * there, plus sticky clients stored elsewhere), then groups them by
 * @c client_group_leader: an application with two or more windows on
 * this desktop collapses into a single "ProgName (N)" submenu instead
 * of @e N separate rows, so a desktop with many windows from a handful
 * of applications (e.g., several Xpad notes) stays short enough to fit
 * on screen without needing to scroll.
 *
 * @param surface     Surface that owns the desktop
 * @param did         Desktop ID whose clients are to be listed
 * @param out_entries Destination entries array for this desktop
 * @param out_count   Entry count in @p out_entries; advanced as entries
 *                    are appended
 *
 * @note Applications with only one window here are listed directly
 */
static void s_build_desktop_entries(surface_td *surface, uint32_t did,
        ctxmenu_entry_td *out_entries, int *out_count)
{
    client_td *collected[WINLIST_MAX_COLLECTED];
    bool placed[WINLIST_MAX_COLLECTED];
    int collected_n;
    desktop_td *desktop;
    desktop_td *home_desktop;
    client_td *client;
    cdlist_item_td *dnode;
    cdlist_item_td *dinitial;
    int group_idx;
    int group_n;
    char label_buf[WM_CTXMENU_LABEL_MAX_LEN];
    int n;

    if (surface == NULL || out_entries == NULL || out_count == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, did);
    if (desktop == NULL) {
        return;
    }

    /* Collect: every client physically stored in this desktop's own
     * table (sticky ones stored here because it is the current desktop
     * included), plus sticky clients physically stored in other
     * desktops (sticky clients live wherever the desktop switch last
     * put them, not necessarily their nominal 'desktop_id') */
    collected_n = 0;
    if (desktop->clients != NULL) {
        ohtbl_foreach(desktop->clients, client) {
            if (client == NULL ||
                    (client->properties.flags &
                        CLIENT_FLAG_SKIP_TASKBAR) ||
                    (!client_is_sticky(client) &&
                        client->desktop_id != did)) {
                continue;
            }
            if (collected_n < WINLIST_MAX_COLLECTED) {
                collected[collected_n] = client;
                placed[collected_n] = false;
                collected_n++;
            }
        }
    }

    if (surface->desktops != NULL) {
        dnode = cdlist_head(surface->desktops);
        if (dnode != NULL) {
            dinitial = dnode;
            do {
                home_desktop = (desktop_td *) cdlist_data(dnode);
                if (home_desktop != NULL && home_desktop != desktop &&
                        home_desktop->clients != NULL) {
                    ohtbl_foreach(home_desktop->clients, client) {
                        if (client == NULL || !client_is_sticky(client) ||
                                (client->properties.flags &
                                    CLIENT_FLAG_SKIP_TASKBAR)) {
                            continue;
                        }
                        if (collected_n < WINLIST_MAX_COLLECTED) {
                            collected[collected_n] = client;
                            placed[collected_n] = false;
                            collected_n++;
                        }
                    } /* ! ohtbl_foreach */
                }
                dnode = cdlist_next(dnode);
            } while (dnode != NULL && dnode != dinitial);
        }
    }

    /* Group by application (shared 'WM_CLIENT_LEADER'/'WM_HINTS' group
     * leader); ungrouped clients (no leader) are always listed alone */
    for (int i = 0; i < collected_n; ++i) {
        xcb_window_t leader;
        client_td *members[WINLIST_MAX_APPGROUP_SIZE];
        int member_n;

        if (placed[i]) {
            continue;
        }

        leader = client_group_leader(collected[i]);
        if (leader == XCB_WINDOW_NONE) {
            s_append_client_entry(collected[i], did, surface,
                    out_entries, WINLIST_MAX_ENTRIES_PER_DESKTOP,
                    out_count);
            placed[i] = true;
            continue;
        }

        member_n = 0;
        for (int j = i; j < collected_n; ++j) {
            if (!placed[j] &&
                    client_group_leader(collected[j]) == leader) {
                if (member_n < WINLIST_MAX_APPGROUP_SIZE) {
                    members[member_n] = collected[j];
                    member_n++;
                }
                placed[j] = true;
            }
        }

        if (member_n <= 1) {
            if (member_n == 1) {
                s_append_client_entry(members[0], did, surface,
                        out_entries, WINLIST_MAX_ENTRIES_PER_DESKTOP,
                        out_count);
            }
            continue;
        }

        if (*out_count >= WINLIST_MAX_ENTRIES_PER_DESKTOP ||
                s_appgroup_used >= WINLIST_MAX_APPGROUPS) {
            /* Out of submenu slots: fall back to listing this group's
             * windows directly rather than dropping them silently */
            for (int k = 0; k < member_n; ++k) {
                s_append_client_entry(members[k], did, surface,
                        out_entries, WINLIST_MAX_ENTRIES_PER_DESKTOP,
                        out_count);
            }
            continue;
        }

        group_idx = s_appgroup_used;
        group_n = 0;
        n = *out_count;

        s_appgroup_used++;
        for (int k = 0; k < member_n; ++k) {
            s_append_client_entry(members[k], did, surface,
                    s_appgroup_entries[group_idx],
                    WINLIST_MAX_APPGROUP_SIZE, &group_n);
        }

        memset(&s_appgroup_state[group_idx], 0,
                sizeof(s_appgroup_state[group_idx]));
        s_appgroup_state[group_idx].window = XCB_WINDOW_NONE;
        s_appgroup_state[group_idx].entries =
            s_appgroup_entries[group_idx];
        s_appgroup_state[group_idx].entry_count = group_n;

        s_appgroup_label(members, member_n, label_buf,
                sizeof(label_buf));

        out_entries[n].type = CTXMENU_SUBMENU;
        safe_strncpy(out_entries[n].label, label_buf,
                sizeof(out_entries[n].label) - 1u);
        out_entries[n].items = s_appgroup_entries[group_idx];
        out_entries[n].item_count = group_n;
        out_entries[n].userdata = &s_appgroup_state[group_idx];
        *out_count = n + 1;
    }
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
    int desktop_count;
    const char *label_fmt;
    char label_buf[WM_CTXMENU_LABEL_MAX_LEN];

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    winlist_close();

    s_entry_data_used = 0;
    s_appgroup_used = 0;
    memset(s_root_entries, 0, sizeof(s_root_entries));
    memset(s_desktop_entries, 0, sizeof(s_desktop_entries));

    n = 0;
    cur_did = surface->desktop_cur;
    desktop_count = (surface->desktop_count < (uint32_t) WINLIST_MAX_DESKTOPS)
        ? (int) surface->desktop_count : WINLIST_MAX_DESKTOPS;

    for (did = 0; (int) did < desktop_count; ++did) {
        winlist_entry_data_td *data;
        int desktop_n;
        bool is_cur;

        desktop = surface_desktop_get(surface, did);
        if (desktop == NULL) {
            continue;
        }

        desktop_n = 0;
        s_build_desktop_entries(surface, did, s_desktop_entries[did],
                &desktop_n);

        is_cur = did == cur_did;

        /* Always add a "Go there..." entry at the top of the desktop's
         * own submenu, same as before, so picking the desktop itself
         * (with no particular window) still works */
        if (desktop_n < WINLIST_MAX_ENTRIES_PER_DESKTOP) {
            data = s_alloc_entry_data();
            if (data != NULL) {
                /* Shift existing entries down by one to make room at
                 * the front; desktop_n is always small enough for this
                 * to be cheap */
                for (int i = desktop_n; i > 0; --i) {
                    s_desktop_entries[did][i] =
                        s_desktop_entries[did][i - 1];
                }
                s_desktop_entries[did][0].type = CTXMENU_COMMAND;
                safe_strncpy(s_desktop_entries[did][0].label,
                        "Go there...",
                        sizeof(s_desktop_entries[did][0].label) - 1u);
                s_desktop_entries[did][0].is_disabled = is_cur;
                s_desktop_entries[did][0].on_activate = NULL;
                s_desktop_entries[did][0].userdata = NULL;
                if (!is_cur) {
                    data->surface = surface;
                    data->client = NULL;
                    data->desktop_id = did;
                    s_desktop_entries[did][0].on_activate =
                        s_cb_goto_desktop;
                    s_desktop_entries[did][0].userdata = data;
                }
                desktop_n++;
            }
        }

        if (desktop_n == 0) {
            continue;
        }

        memset(&s_desktop_state[did], 0, sizeof(s_desktop_state[did]));
        s_desktop_state[did].window = XCB_WINDOW_NONE;
        s_desktop_state[did].entries = s_desktop_entries[did];
        s_desktop_state[did].entry_count = desktop_n;

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

        s_root_entries[n].type = CTXMENU_SUBMENU;
        safe_strncpy(s_root_entries[n].label, label_buf,
                sizeof(s_root_entries[n].label) - 1u);
        s_root_entries[n].items = s_desktop_entries[did];
        s_root_entries[n].item_count = desktop_n;
        s_root_entries[n].userdata = &s_desktop_state[did];
        ++n;
    }

    if (n == 0) {
        s_root_entries[n].type = CTXMENU_LABEL;
        safe_strncpy(s_root_entries[n].label, "(no windows)",
                sizeof(s_root_entries[n].label) - 1u);
        ++n;
    }

    memset(&s_root, 0, sizeof(s_root));
    s_root.window = XCB_WINDOW_NONE;
    s_root.entries = s_root_entries;
    s_root.entry_count = n;

    ctxmenu_show(connection, surface, &s_root, x, y, config);
}


/* Close the window list menu */
void winlist_close(void)
{
    ctxmenu_close(&s_root);
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
    x -= state->origin_x;
    y -= state->origin_y;

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
    return ctxmenu_find_state_for_window(&s_root, win) != NULL;
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
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state != NULL) {
        ctxmenu_handle_motion(state, x, y);
    }
}
