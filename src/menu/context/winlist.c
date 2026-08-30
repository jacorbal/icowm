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
#include <stdint.h>
#include <stdlib.h>     /* calloc, free */
#include <string.h>     /* memset */
#include <stdio.h>      /* snprintf */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Default initial values */
#include <defs/uistr.h>
#include <i18n.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <memguard.h>
#include <surface.h>
#include <wm.h>

/* Command includes */
#include <cmds/surface.h>

/* Policy includes */
#include <policy/focus.h>

/* Initial definition values */
#include <defs/ctxmenu.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/tree.h>
#include <menu/context/winlist.h>
#include <menu/draw.h>

/* Render includes */
#include <render/text.h>


/** Singleton root menu state, one @c CTXMENU_SUBMENU per desktop */
static ctxmenu_state_td s_root;

/** Entries for the top-level (per-desktop) menu: one
 *  @c CTXMENU_SUBMENU slot per desktop, plus room for the
 *  "(no windows)" fallback and for the trailing "Add new desktop" and
 *  "Remove last desktop" pair with the separator ahead of them (see
 *  the tail append in @a winlist_show) */
static ctxmenu_entry_td s_root_entries[WINLIST_MAX_DESKTOPS + 4];

/** State for each desktop's submenu */
static ctxmenu_state_td s_desktop_state[WINLIST_MAX_DESKTOPS];

/** Entries for each desktop's submenu, allocated afresh on every
 *  @a winlist_show and sized to the real @c surface->desktop_count
 *  clamped to @c WINLIST_MAX_DESKTOPS, then released by
 *  @a winlist_close.  Freeing and allocating anew on each open is why
 *  no @c realloc is wanted to follow a changing desktop count; the
 *  comment at the allocation gives the reasoning.  One @c free on the
 *  array releases the whole of it, unlike @c s_entries in
 *  @c menu/context/rootmenu.c, which needs a loop: no entry built
 *  here sets @c command or @c class_name, only @c on_activate and
 *  @c userdata, so no individual entry holds anything to release */
static ctxmenu_entry_td (*s_desktop_entries)[WINLIST_MAX_ENTRIES_PER_DESKTOP]
    = NULL;

/** State for each application-group submenu, allocated afresh and
 *  sized to what @a s_count_appgroups_needed answered for this exact
 *  @a winlist_show call, then released by @a winlist_close; the
 *  reasoning against a @c realloc is the one given for
 *  @c s_desktop_entries above */
static ctxmenu_state_td *s_appgroup_state = NULL;

/** Entries for each application-group submenu, sized, allocated and
 *  released alongside @c s_appgroup_state above and for the same
 *  reasons */
static ctxmenu_entry_td (*s_appgroup_entries)[WINLIST_MAX_APPGROUP_SIZE]
    = NULL;

/** Application-group slots claimed during this @a winlist_show */
static int s_appgroup_used = 0;

/** How many application-group slots @c s_appgroup_state and
 *  @c s_appgroup_entries were allocated for by this exact
 *  @a winlist_show call, and therefore how many of them may be
 *  written; zero while neither is allocated.  Checked against in
 *  preference to @c WINLIST_MAX_APPGROUPS, which is the ceiling those
 *  two are ever sized up to rather than the size either one holds:
 *  the counting pass and the building pass agree, and a build that
 *  outran its count once wrote past the end of both instead of
 *  falling back to listing the group's windows one at a time, which
 *  is what the cap is there to do */
static int s_appgroup_capacity = 0;



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

/** @c s_entry_data slots claimed during this @a winlist_show */
static int s_entry_data_used = 0;


/**
 * @brief Request a desktop switch on the given surface
 *
 * Shared by @c s_cb_goto_desktop and @c s_cb_focus_client below, both
 * of which switch @p surface to a target desktop by ID before doing
 * anything else specific to their entry type.
 *
 * @param surface    Surface to switch
 * @param desktop_id Target desktop ID
 *
 * @note Complexity: @e O(1)
 */
static void s_switch_to_desktop(surface_td *surface,
        uint32_t desktop_id)
{
    enact_surface_desktop_switch(surface, desktop_id);
}


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

    (void) connection;

    data = (winlist_entry_data_td *) userdata;
    if (data == NULL || data->surface == NULL) {
        return;
    }

    s_switch_to_desktop(data->surface, data->desktop_id);
}


/**
 * @brief Add a new, empty desktop to the surface
 *
 * Callback invoked from the window list's trailing "Add new
 * desktop" entry.  Not about any particular desktop, unlike
 * @a s_cb_goto_desktop just above: only @p data->surface is read,
 * @p data->desktop_id is left unused.
 *
 * @param connection XCB connection (unused)
 * @param userdata   Pointer to a @c winlist_entry_data_td with the
 *                   target surface
 */
static void s_cb_add_desktop(xcb_connection_t *connection,
        void *userdata)
{
    winlist_entry_data_td *data;

    (void) connection;

    data = (winlist_entry_data_td *) userdata;
    if (data == NULL || data->surface == NULL) {
        return;
    }

    enact_surface_desktop_add(data->surface);
}


/**
 * @brief Remove the surface's last desktop
 *
 * Callback invoked from the window list's trailing "Remove last
 * desktop" entry.  A no-op, silently, when only one desktop remains;
 * see @a surface_action_desktop_remove (surface.h) for the exact
 * refusal conditions, and this same entry's @c is_disabled below
 * (@a winlist_show) for how that state reaches the person before
 * they even try.
 *
 * @param connection XCB connection (unused)
 * @param userdata   Pointer to a @c winlist_entry_data_td with the
 *                   target surface
 */
static void s_cb_remove_desktop(xcb_connection_t *connection,
        void *userdata)
{
    winlist_entry_data_td *data;

    (void) connection;

    data = (winlist_entry_data_td *) userdata;
    if (data == NULL || data->surface == NULL) {
        return;
    }

    enact_surface_desktop_remove(data->surface);
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
 * ever becoming the window manager's notion of the active window
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
    uint32_t target_did;
    desktop_td *target_desktop;

    (void) connection;

    data = (winlist_entry_data_td *) userdata;
    if (data == NULL || data->surface == NULL) {
        return;
    }

    /* Always the desktop this entry was actually listed under (see
     * 's_client_entry_append''s comment for 'did'), never
     * 'data->client->desktop_id'.  For a plain client the two agree
     * anyway, since it can only ever be listed under its desktop,
     * but for a sticky one they routinely do not, a sticky client's
     * 'desktop_id' being nominal at best (see the "sticky clients live
     * wherever the desktop switch last put them" comment in
     * 's_build_desktop_entries') and does not track which of the (six
     * shown as its submenu here) desktops this particular entry
     * actually came from.  Using it instead of 'data->desktop_id' meant
     * activating a sticky client's entry under a desktop other than the
     * current one silently did nothing: since a sticky client already
     * stays visible wherever the desktop switch last left it,
     * 'target_did' would resolve to that same already-current desktop
     * regardless of which desktop's submenu the entry was actually
     * picked from. */
    target_did = data->desktop_id;

    s_switch_to_desktop(data->surface, target_did);
    if (data->client == NULL) {
        return;
    }

    /* Restore the window that was selected */
    if (data->client->properties.flags & CLIENT_FLAG_HIDDEN) {
        if (client_is_iconified(data->client)) {
            enact_client_restore(data->client);
        } else {
            enact_client_unhide(data->client);
        }
    }
    if (client_is_shaded(data->client)) {
        enact_client_unshade(data->client);
    }

    target_desktop = surface_desktop_get(data->surface, target_did);
    focus_apply(wm_get_surfaces(), data->surface, target_desktop,
            data->client, true, NULL);
}


/**
 * @brief Format a window-list entry label according to the client state
 *
 * Wraps @p name in parentheses if the client is iconified, in angle
 * brackets if hidden (but not iconified), or copies it verbatim
 * otherwise.  The result is written into @p buf.
 *
 * @c CLIENT_FLAG_HIDDEN is set for both an iconified and a genuinely
 * hidden client (see @c client_hide, called from both paths),
 * so the more specific iconified state has to be checked first; the
 * hidden flag is only checked once iconified has already been ruled
 * out.
 *
 * @param client   Client whose state determines the format
 * @param name     Base name string to format
 * @param buf      Destination buffer for the formatted label
 * @param buf_size Size of @p buf in bytes
 */
static void s_client_label_format(const client_td *client,
        const char *restrict name, char *restrict buf, size_t buf_size)
{
    if (client == NULL || name == NULL || buf == NULL ||
            buf_size == 0u) {
        return;
    }

    if (client_is_iconified(client)) {
        (void) snprintf(buf, buf_size, "(%s)", name);
    } else if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
        (void) snprintf(buf, buf_size, "<%s>", name);
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
static void s_client_entry_append(client_td *client, uint32_t did,
        surface_td *surface, ctxmenu_entry_td *out_entries,
        int out_cap, int *out_count)
{
    const char *cname;
    char name_buf[WM_CTXMENU_LABEL_MAX_LENGTH];
    winlist_entry_data_td *data;
    int n;

    if (client == NULL || surface == NULL || out_count == NULL) {
        return;
    }

    n = *out_count;
    if (n >= out_cap) {
        return;
    }

    /* 'out_entries == NULL' means a counting-only pass (see
     * 's_count_appgroups_needed'): '*out_count' still advances, so
     * the same per-desktop cap this function enforces just above
     * stays identical between that pass and a real one, but every
     * bit of work an entry that will never actually be shown has no
     * use for is skipped, including claiming one of the limited slots
     * of 's_entry_data' for it. */
    if (out_entries == NULL) {
        *out_count = n + 1;
        return;
    }

    data = s_alloc_entry_data();
    if (data == NULL) {
        return;
    }

    cname = (client->info.name != NULL && client->info.name[0] != '\0')
        ? client->info.name : "(unnamed)";
    s_client_label_format(client, cname, name_buf, sizeof(name_buf));
    menu_draw_truncate(name_buf, (uint16_t) WINLIST_LABEL_MAX_WIDTH);
    out_entries[n].type = CTXMENU_COMMAND;
    safe_strncpy(out_entries[n].label, name_buf,
            sizeof(out_entries[n].label) - 1u);
    data->surface = surface;
    data->client = client;
    data->desktop_id = did;
    out_entries[n].on_activate = s_cb_focus_client;
    out_entries[n].userdata = data;
    out_entries[n].icon_window = client->window;
    out_entries[n].icon_cache = &client->icon_pixmap_cache;
    *out_count = n + 1;
}


/**
 * @brief Best-effort display name for an application group
 *
 * Prefers the group leader's @c WM_CLASS class name (more stable
 * across an application's windows than each window's title); falls
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
        if (members[i] != NULL &&
                members[i]->info.class_name[1] != NULL &&
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
 * @brief Append a client to the collected-for-this-desktop array, if
 *        there is room
 *
 * Shared by @c s_build_desktop_entries' two collection passes
 * below (this desktop's clients, and sticky clients physically
 * stored on other desktops), which otherwise repeated the same
 * bounds-checked append.
 *
 * @param client     Client to append
 * @param collected  Destination array
 * @param placed     Parallel "already grouped" array; the new slot is
 *                   initialized to @c false
 * @param collected_n Current count in @p collected; incremented on
 *                    success
 *
 * @note Complexity: @e O(1)
 */
static void s_client_collect(client_td *client,
        client_td **collected, bool *placed, int *collected_n)
{
    if (*collected_n < WINLIST_MAX_COLLECTED) {
        collected[*collected_n] = client;
        placed[*collected_n] = false;
        (*collected_n)++;
    }
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
 * A counting-only pass, used by @a s_count_appgroups_needed to size
 * @c s_appgroup_entries before any of it actually exists yet, runs
 * this exact same collection and grouping logic (so the two can
 * never drift out of step on what actually counts as a group) with
 * @p out_entries itself @c NULL: every real side effect (writing an
 * entry, claiming an @c s_entry_data slot, or touching the real,
 * shared @c s_appgroup_used / @c s_appgroup_entries /
 * @c s_appgroup_state) is skipped in that mode, in favor of only
 * advancing @p out_count (so the same per-desktop cap still applies
 * identically either way) and, once found, @p out_appgroup_count.
 *
 * @param surface           Surface that owns the desktop
 * @param did               Desktop ID whose clients are to be listed
 * @param out_entries       Destination entries array for this
 *                          desktop, or @c NULL for a counting-only
 *                          pass
 * @param out_count         Entry count in @p out_entries; advanced
 *                          as entries are appended (or would be, in
 *                          a counting-only pass)
 * @param out_appgroup_count @c NULL for a real pass, using the real
 *                          @c s_appgroup_* globals as before; non-
 *                          @c NULL for a counting-only pass, which
 *                          increments @p *out_appgroup_count once
 *                          per group found instead of touching them
 *
 * @note Applications with only one window here are listed directly
 */
static void s_build_desktop_entries(surface_td *surface, uint32_t did,
        ctxmenu_entry_td *out_entries, int *out_count,
        int *out_appgroup_count)
{
    client_td *collected[WINLIST_MAX_COLLECTED];
    bool placed[WINLIST_MAX_COLLECTED];
    int collected_n;
    desktop_td *desktop;
    client_td *client;
    cdlist_item_td *dnode;
    int group_n;
    char label_buf[WM_CTXMENU_LABEL_MAX_LENGTH];
    int n;

    /* 'out_entries' is deliberately NOT rejected here: a counting-only
     * pass passes it null on purpose, and this whole function is
     * written to skip every real side effect in that case (see this
     * function's comment above, and 's_client_entry_append', which
     * handles the null itself).  Rejecting it made every counting
     * pass return before counting anything, so the appgroup total
     * came back zero however many groups there really were, and both
     * appgroup arrays were then allocated one single row long; the
     * second group a real pass went on to build wrote past the end of
     * both. */
    if (surface == NULL || out_count == NULL) {
        return;
    }

    desktop = surface_desktop_get(surface, did);
    if (desktop == NULL) {
        return;
    }

    /* Collect: every client physically stored in this desktop's
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
                    (!client_is_pinned(client) &&
                        client->desktop_id != did)) {
                continue;
            }
            s_client_collect(client, collected, placed, &collected_n);
        }
    }

    if (surface->desktops != NULL) {
        dnode = cdlist_head(surface->desktops);
        if (dnode != NULL) {
            const cdlist_item_td *dinitial;
            dinitial = dnode;
            do {
                desktop_td *const home_desktop=
                    (desktop_td *) cdlist_data(dnode);
                if (home_desktop != NULL && home_desktop != desktop &&
                        home_desktop->clients != NULL) {
                    ohtbl_foreach(home_desktop->clients, client) {
                        if (client == NULL || !client_is_pinned(client) ||
                                (client->properties.flags &
                                    CLIENT_FLAG_SKIP_TASKBAR)) {
                            continue;
                        }
                        s_client_collect(client, collected, placed,
                                &collected_n);
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
        int group_idx;
        int member_n;

        if (placed[i]) {
            continue;
        }

        leader = client_group_leader(collected[i]);
        if (leader == XCB_WINDOW_NONE) {
            s_client_entry_append(collected[i], did, surface,
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
                s_client_entry_append(members[0], did, surface,
                        out_entries, WINLIST_MAX_ENTRIES_PER_DESKTOP,
                        out_count);
            }
            continue;
        }

        if (*out_count >= WINLIST_MAX_ENTRIES_PER_DESKTOP ||
                (out_appgroup_count != NULL
                    ? *out_appgroup_count >= WINLIST_MAX_APPGROUPS
                    : s_appgroup_used >= s_appgroup_capacity)) {
            /* Out of submenu slots: fall back to listing this group's
             * windows directly rather than dropping them silently */
            for (int k = 0; k < member_n; ++k) {
                s_client_entry_append(members[k], did, surface,
                        out_entries, WINLIST_MAX_ENTRIES_PER_DESKTOP,
                        out_count);
            }
            continue;
        }

        if (out_appgroup_count != NULL) {
            /* Counting-only pass: this group would consume one
             * 's_appgroup_used' slot in a real pass, so count it as
             * such; advance 'out_count' by exactly one too, the same
             * as the single submenu-marker entry a real pass would
             * append to the parent desktop's list for it, and
             * move on without touching any of the real, shared
             * appgroup state below, which does not exist yet at this
             * point. */
            (*out_appgroup_count)++;
            (*out_count)++;
            continue;
        }

        group_idx = s_appgroup_used;
        group_n = 0;
        n = *out_count;

        s_appgroup_used++;
        for (int k = 0; k < member_n; ++k) {
            s_client_entry_append(members[k], did, surface,
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
        out_entries[n].icon_window = members[0]->window;
        out_entries[n].icon_cache = &members[0]->icon_pixmap_cache;
        *out_count = n + 1;
    }
}


/**
 * @brief Count how many application-group submenus this same
 *        @c winlist_show call will actually go on to create, across
 *        every desktop
 *
 * Runs @a s_build_desktop_entries once per desktop in its
 * counting-only mode (see that function's comment),
 * accumulating the total so @c s_appgroup_entries and
 * @c s_appgroup_state can be sized to it before either actually
 * exists, rather than to the fixed worst case @c WINLIST_MAX_APPGROUPS
 * would otherwise always need to cover regardless of how many groups
 * this desktop count and these clients actually produce.
 *
 * @param surface       Surface whose desktops to count groups across
 * @param desktop_count Already-clamped desktop count, the same one
 *                      the real build pass right after this call
 *                      goes on to iterate itself
 *
 * @return Total application-group count, already clamped to
 *         @c WINLIST_MAX_APPGROUPS
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       clients across every desktop counted
 */
static int s_count_appgroups_needed(surface_td *surface, int desktop_count)
{
    int total;
    int dummy_count;

    total = 0;
    if (desktop_count <= 1) {
        dummy_count = 0;
        s_build_desktop_entries(surface, 0u, NULL, &dummy_count, &total);
    } else {
        for (uint32_t did = 0; (int) did < desktop_count; ++did) {
            dummy_count = 0;
            s_build_desktop_entries(surface, did, NULL, &dummy_count,
                    &total);
        }
    }

    return total;
}


/**
 * @brief What @a s_desktop_submenu_visit is building
 */
struct s_submenu_ctx_s {
    surface_td *surface;    /**< Surface whose desktops are listed */
    int desktop_count;      /**< How many to list at most */
    uint32_t cur_did;       /**< Desktop showing right now */
    int entry_count;        /**< Root entries written so far */
    uint32_t index;         /**< Desktop index reached */
};


/**
 * @brief Build one desktop's submenu of the windows it holds
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_submenu_ctx_s being built
 *
 * @note Stops building once the menu holds every desktop asked for,
 *       the whole walk still running: a visitor has no way to end one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_desktop_submenu_visit(desktop_td *desktop, void *data)
{
    struct s_submenu_ctx_s *const ctx = data;
    char label_buf[WM_CTXMENU_LABEL_MAX_LENGTH];
    char desk_label[WM_DESKTOP_MAX_LENGTH_NAME + 64];
    winlist_entry_data_td *entry_data;
    uint32_t did;
    int desktop_n;
    bool is_cur;
    int n;

    if (ctx == NULL || (int) ctx->index >= ctx->desktop_count) {
        return;
    }

    did = ctx->index;
    n = ctx->entry_count;
    ctx->index++;

    desktop_n = 0;
    s_build_desktop_entries(ctx->surface, did, s_desktop_entries[did],
            &desktop_n, NULL);

    is_cur = did == ctx->cur_did;

    /* Always add a "Go there..." entry at the top of the desktop's
     * submenu, same as before, so picking the desktop itself
     * (with no particular window) still works.  Followed by a
     * separator before the real client entries below it, but
     * only when this desktop actually has any: with none,
     * "Go there..." would otherwise be followed by a bare
     * separator leading nowhere. */
    if (desktop_n < WINLIST_MAX_ENTRIES_PER_DESKTOP) {
        /* Compared against the room left rather than
         * against the count plus one: a signed sum tested
         * against a constant lets the optimizer assume the
         * sum never overflows */
        int shift_count = (desktop_n > 0 &&
                desktop_n < WINLIST_MAX_ENTRIES_PER_DESKTOP - 1)
            ? 2 : 1;

        entry_data = s_alloc_entry_data();
        if (entry_data != NULL) {
            /* Shift existing entries down to make room at the
             * front for "Go there..." (and the separator, if
             * one fits); desktop_n is always small enough for
             * this to be cheap */
            for (int i = desktop_n + shift_count - 1;
                    i >= shift_count; --i) {
                s_desktop_entries[did][i] =
                    s_desktop_entries[did][i - shift_count];
            }
            s_desktop_entries[did][0].type = CTXMENU_COMMAND;
            safe_strncpy(s_desktop_entries[did][0].label,
                    _(STR_WINLIST_GO_THERE),
                    sizeof(s_desktop_entries[did][0].label) - 1u);
            s_desktop_entries[did][0].is_disabled = is_cur;
            s_desktop_entries[did][0].on_activate = NULL;
            s_desktop_entries[did][0].userdata = NULL;
            /* Never inherited from whichever real client entry
             * used to occupy this same slot before the shift
             * above: without this, "Go there..." would show
             * that client's icon. */
            s_desktop_entries[did][0].icon_window = XCB_WINDOW_NONE;
            s_desktop_entries[did][0].icon_cache = NULL;
            if (!is_cur) {
                entry_data->surface = ctx->surface;
                entry_data->client = NULL;
                entry_data->desktop_id = did;
                s_desktop_entries[did][0].on_activate =
                    s_cb_goto_desktop;
                s_desktop_entries[did][0].userdata = entry_data;
            }

            if (shift_count == 2) {
                s_desktop_entries[did][1].type = CTXMENU_SEPARATOR;
                s_desktop_entries[did][1].icon_window =
                    XCB_WINDOW_NONE;
                s_desktop_entries[did][1].icon_cache = NULL;
            }

            desktop_n += shift_count;
        }
    }

    if (desktop_n == 0) {
        return;
    }

    memset(&s_desktop_state[did], 0, sizeof(s_desktop_state[did]));
    s_desktop_state[did].window = XCB_WINDOW_NONE;
    s_desktop_state[did].entries = s_desktop_entries[did];
    s_desktop_state[did].entry_count = desktop_n;

    surface_desktop_label(ctx->surface, did, desktop->name, false,
            true, desk_label, sizeof(desk_label));
    (void) snprintf(label_buf, sizeof(label_buf), "%s%s%s",
            MENU_CONTEXT_CTXMENU_LABEL_PREFIX, desk_label,
            MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);

    s_root_entries[n].type = CTXMENU_SUBMENU;
    safe_strncpy(s_root_entries[n].label, label_buf,
            sizeof(s_root_entries[n].label) - 1u);
    s_root_entries[n].items = s_desktop_entries[did];
    s_root_entries[n].item_count = desktop_n;
    s_root_entries[n].userdata = &s_desktop_state[did];
    ctx->entry_count++;
}


/**
 * @brief Build one root entry per desktop, each a submenu of its own
 *
 * Only for a surface with more than one desktop; a single one is
 * flattened by the caller instead, since there would be nothing to
 * choose between.
 *
 * @param surface       Surface whose desktops are listed
 * @param desktop_count How many desktops to list at most
 * @param n_out         Entry count, advanced by what is written
 *
 * @note Complexity: @e O(d * c), where @e d is the number of
 *       desktops and @e c the clients on each
 */
static void s_winlist_build_desktop_submenus(surface_td *surface,
        int desktop_count, int *n_out)
{
    const uint32_t cur_did = surface->desktop_cur;
    int n = *n_out;

    struct s_submenu_ctx_s submenu_ctx;

    submenu_ctx.surface = surface;
    submenu_ctx.desktop_count = desktop_count;
    submenu_ctx.cur_did = cur_did;
    submenu_ctx.entry_count = n;
    submenu_ctx.index = 0u;
    surface_desktops_walk(surface, s_desktop_submenu_visit,
            &submenu_ctx);
    n = submenu_ctx.entry_count;

    *n_out = n;
}


/**
 * @brief Append the add and remove desktop actions
 *
 * A surface-wide pair, not tied to any one desktop's window list, so
 * it belongs after every entry above rather than duplicated into
 * each submenu.  Skipped outright when the buffer is already full,
 * rather than overflowing it, and under restricted-memory mode,
 * which stays locked to exactly one desktop for the life of the
 * process, so neither action could ever succeed there.
 *
 * @param surface       Surface the actions operate on
 * @param root_target   Entry buffer the caller is filling
 * @param desktop_count How many desktops the surface has
 * @param n_out         Entry count, advanced by what is written
 *
 * @note Complexity: @e O(1)
 */
static void s_winlist_append_desktop_actions(surface_td *surface,
        ctxmenu_entry_td *root_target, int desktop_count, int *n_out)
{
    winlist_entry_data_td *add_data;
    winlist_entry_data_td *remove_data;
    int n = *n_out;

    /* "Add new desktop" / "Remove last desktop", always appended at
     * the very end after a separator, in both display modes: a
     * surface-wide action, not tied to any particular desktop's
     * window list, so it belongs outside the per-desktop submenu
     * layer above rather than duplicated into every one of them.
     * Guarded against whichever buffer 'root_target' actually points
     * to being completely full already (an extreme number of windows
     * on the one desktop that exists, in the flattened single-desktop
     * case): skipped outright rather than overflowing it, the same
     * defensive reasoning the "Go there" prepend above already
     * follows for the exact same kind of buffer.  Also skipped
     * outright, omitted rather than merely disabled, under
     * restricted-memory mode, which is deliberately locked to
     * exactly one desktop always (see
     * 'surface_action_desktop_add''s comment): a person
     * running that mode has no use for either action ever
     * succeeding, unlike an ordinary session's "only one desktop
     * remains for now" case just below, where adding a second one
     * back remains a real possibility worth surfacing. */
    if (memguard_max_clients() == 0u &&
            n <= ((desktop_count <= 1)
                ? WINLIST_MAX_ENTRIES_PER_DESKTOP
                : (int) (sizeof(s_root_entries) /
                    sizeof(s_root_entries[0]))) - 3) {
        root_target[n].type = CTXMENU_SEPARATOR;
        root_target[n].icon_window = XCB_WINDOW_NONE;
        root_target[n].icon_cache = NULL;
        ++n;

        add_data = s_alloc_entry_data();
        root_target[n].type = CTXMENU_COMMAND;
        safe_strncpy(root_target[n].label, _(STR_WINLIST_DESKTOP_ADD),
                sizeof(root_target[n].label) - 1u);
        root_target[n].is_disabled =
            (surface->desktop_count >= (uint32_t) CONFIG_MAX_DESKTOPS);
        root_target[n].on_activate = s_cb_add_desktop;
        root_target[n].icon_window = XCB_WINDOW_NONE;
        root_target[n].icon_cache = NULL;
        if (add_data != NULL) {
            add_data->surface = surface;
            add_data->client = NULL;
            add_data->desktop_id = 0u;
        }
        root_target[n].userdata = add_data;
        ++n;

        /* Disabled, not omitted, when only one desktop remains:
         * unlike the desktop-count-driven omission of the whole
         * per-desktop submenu layer elsewhere in this function, a
         * person opening this menu specifically to manage desktops
         * still benefits from seeing this entry exists, just not
         * currently available, the same way "Send to desktop"'s
         * "All desktops (pin)" entry (wincmenu.c) stays visible
         * rather than disappearing. */
        remove_data = s_alloc_entry_data();
        root_target[n].type = CTXMENU_COMMAND;
        safe_strncpy(root_target[n].label, _(STR_WINLIST_DESKTOP_REMOVE),
                sizeof(root_target[n].label) - 1u);
        root_target[n].is_disabled = (surface->desktop_count <= 1u);
        root_target[n].on_activate = s_cb_remove_desktop;
        root_target[n].icon_window = XCB_WINDOW_NONE;
        root_target[n].icon_cache = NULL;
        if (remove_data != NULL) {
            remove_data->surface = surface;
            remove_data->client = NULL;
            remove_data->desktop_id = 0u;
        }
        root_target[n].userdata = remove_data;
        ++n;
    }

    *n_out = n;
}


/* Open the window list menu */
void winlist_show(xcb_connection_t *connection,
        surface_td *surface, struct position_s pos,
        const config_td *config)
{
    int n;
    int desktop_count;
    int needed_appgroups;
    ctxmenu_entry_td *root_target;

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    winlist_close();

    /* Every per-client label built below gets truncated against
     * 'WINLIST_LABEL_MAX_WIDTH' (see 's_client_entry_append'), which
     * measures in whatever font the text renderer currently has
     * active; initialized here, once, up front, so every one of
     * those measurements is against the actual menu font rather than
     * whatever an unrelated earlier caller happened to leave active. */
    (void) text_renderer_use_font(connection,
            config->theme.menu.unselected.font);

    s_entry_data_used = 0;
    s_appgroup_used = 0;
    memset(s_root_entries, 0, sizeof(s_root_entries));

    desktop_count =
        (surface->desktop_count < (uint32_t) WINLIST_MAX_DESKTOPS)
        ? (int) surface->desktop_count
        : WINLIST_MAX_DESKTOPS;

    /* Sized to 'desktop_count' itself, computed fresh just above from
     * 'surface->desktop_count' as it stands at this exact moment,
     * rather than to 'WINLIST_MAX_DESKTOPS' (the most desktops a
     * surface could ever have, not how many this one actually does
     * right now): a session with only two or three desktops in use
     * no longer pays for the full, far larger worst case every
     * single time this menu opens.  No 'realloc' of any kind is
     * needed to track 'desktop_count' changing over the window
     * manager's lifetime, since this whole array is already
     * rebuilt from a blank slate on every single 'winlist_show' call
     * regardless (matching every entry it holds, which are always
     * rebuilt fresh too, never carried over from the last time this
     * menu happened to be open); freeing whatever was allocated last
     * time and allocating fresh again this time, already the shape
     * 'winlist_close' followed even before this was dynamic, keeps
     * naturally matching whatever 'desktop_count' happens to be each
     * time with no extra bookkeeping between one open and the next.
     * At least one row always, matching a session's standing
     * invariant of never actually reaching zero desktops, but kept
     * as an explicit floor here regardless, defensively, the same
     * way 'desktop_count' itself already gets clamped above rather
     * than trusted outright. */
    s_desktop_entries = calloc((size_t) ((desktop_count > 0)
                ? desktop_count : 1),
            sizeof(*s_desktop_entries));
    if (s_desktop_entries == NULL) {
        return;
    }

    /* Sized to how many application-group submenus this exact call
     * will actually go on to create (see 's_count_appgroups_needed'
     * itself for why that count is trustworthy), rather than to
     * 'WINLIST_MAX_APPGROUPS' (the most this menu could ever create
     * across every desktop, an extreme worst case realistically
     * never approached).  At least one row always, the same
     * defensive floor 's_desktop_entries' just above already applies
     * for the same reason, even though a session with genuinely no
     * multi-window application anywhere would only ever index row
     * zero regardless. */
    needed_appgroups = s_count_appgroups_needed(surface, desktop_count);
    s_appgroup_entries = calloc((size_t) ((needed_appgroups > 0)
                ? needed_appgroups : 1),
            sizeof(*s_appgroup_entries));
    s_appgroup_state = (ctxmenu_state_td *) calloc(
            (size_t) ((needed_appgroups > 0) ? needed_appgroups : 1),
            sizeof(*s_appgroup_state));
    if (s_appgroup_entries == NULL || s_appgroup_state == NULL) {
        winlist_close();
        return;
    }
    s_appgroup_capacity = (needed_appgroups > 0) ? needed_appgroups : 1;

    n = 0;

    /* A single desktop has nothing to choose between, so the usual
     * per-desktop submenu layer below would only ever wrap around one
     * disabled "Go there..." entry (always disabled, since the one
     * desktop that exists is always already the current one) sitting
     * above the window list itself.  Skipped entirely for that case:
     * 's_desktop_entries[0]' is filled directly below instead of
     * going through the per-desktop submenu wrapping the loop in the
     * 'else' branch does, and 's_root' is pointed at it directly near
     * the end of this function instead of at 's_root_entries'
     * (correctly sized for one entry per desktop, not for a whole
     * desktop's window list, so it must never be the target
     * 's_build_desktop_entries' writes into).
     * The same general behavior (keyed off the real desktop count,
     * not specific to restricted-memory mode) already used elsewhere
     * for "Send to desktop" and desktop-cycling key bindings; see
     * 'wincmenu.c' and 'input/kbd/bind.c'. */
    if (desktop_count <= 1) {
        const desktop_td *desktop = surface_desktop_get(surface, 0u);
        if (desktop != NULL) {
            s_build_desktop_entries(surface, 0u, s_desktop_entries[0],
                    &n, NULL);
        }
    } else {
        s_winlist_build_desktop_submenus(surface,
                desktop_count, &n);
    }

    /* Whichever of the two buffers above 'n' actually counts entries
     * in: 's_desktop_entries[0]' directly for the flattened, single-
     * desktop case, 's_root_entries' (one entry per desktop, each a
     * submenu) otherwise.  Both are used below instead of just
     * 's_root_entries', so the "no windows" fallback and the final
     * root assignment land on the right one either way. */
    root_target =
        (desktop_count <= 1) ? s_desktop_entries[0] : s_root_entries;

    if (n == 0) {
        root_target[n].type = CTXMENU_LABEL;
        safe_strncpy(root_target[n].label, _(STR_WINLIST_NO_WINDOWS),
                sizeof(root_target[n].label) - 1u);
        ++n;
    }

    s_winlist_append_desktop_actions(surface, root_target,
            desktop_count, &n);

    memset(&s_root, 0, sizeof(s_root));
    s_root.window = XCB_WINDOW_NONE;
    s_root.entries = root_target;
    s_root.entry_count = n;

    ctxmenu_show(connection, surface, &s_root, pos, config);
}


/* Close the window list menu */
void winlist_close(void)
{
    ctxmenu_close(&s_root);

    /* See 's_desktop_entries' comment (its declaration, above)
     * for why a single 'free' here, with no per-row loop of its own,
     * is always enough; the same reasoning applies to
     * 's_appgroup_entries' and 's_appgroup_state' right below it. */
    if (s_desktop_entries != NULL) {
        free(s_desktop_entries);
        s_desktop_entries = NULL;
    }
    if (s_appgroup_entries != NULL) {
        free(s_appgroup_entries);
        s_appgroup_entries = NULL;
    }
    if (s_appgroup_state != NULL) {
        free(s_appgroup_state);
        s_appgroup_state = NULL;
    }
    s_appgroup_capacity = 0;
}


/* Repaint the window list menu */
void winlist_repaint(xcb_window_t win)
{
    ctxmenu_tree_redraw_window(&s_root, win);
}


/* Handle a button-press event inside the window list menu */
bool winlist_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int y,
        const config_td *config)
{
    return ctxmenu_tree_handle_click_window(connection, surface, &s_root,
            win, y, config);
}


/* Query whether the window list menu is currently open */
bool winlist_is_open(void)
{
    return ctxmenu_is_open(&s_root);
}


/* Check whether 'win' belongs to the window list menu */
bool winlist_owns_window(xcb_window_t win)
{
    return ctxmenu_tree_state_find_for_window(&s_root, win) != NULL;
}


/* Handle a key-press event while the window list menu is open */
bool winlist_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config)
{
    return ctxmenu_tree_handle_keypress_deepest(connection, surface,
            &s_root, keysym, config);
}


/* Handle a pointer-motion event over the window list menu */
void winlist_handle_motion(xcb_window_t win, int x, int y)
{
    ctxmenu_tree_handle_motion_window(&s_root, win, x, y);
}
