/**
 * @file desktop/dclient.c
 *
 * @brief Desktop client management and action dispatchers
 *        implementation
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
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL */
#include <string.h>     /* memset */
#include <strings.h>    /* strcasecmp */
#include <sys/types.h>  /* pid_t */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/safe/safestr.h>
#include <utils/spawn.h>

/* Command includes */
#include <cmds/client/layer.h>
#include <cmds/surface.h>
#include <cmds/client/transient.h>

/* Project includes */
#include <client.h>
#include <enact.h>
#include <logger.h>
#include <cctl/sn.h>
#include <wm.h>

/* Menu includes */
#include <menu/dialog/message.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/uistr.h>

/* i18n */
#include <i18n.h>

/* Local includes */
#include <desktop.h>
#include <policy/stacking.h>
#include <utils/xcb/connection.h>

/**
 * @brief Move a client to the front or back of the desktop's window
 *        stack
 *
 * Shared by @a desktop_action_client_send_front and
 * @a desktop_action_client_send_back below, which only differ in
 * which end of
 * the stacking list the client is reinserted at and the wording of
 * their log message.
 *
 * @param desktop  Desktop whose stacking order is changed
 * @param client   Client to move
 * @param to_front @c true to move to the front (top of the stack),
 *                 @c false to move to the back (bottom)
 *
 * @return @c 0 on success, @c -1 on failure
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
static int s_desktop_client_send_to_end(desktop_td *desktop,
        client_td *client, bool to_front)
{
    int status;

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Sending client 0x%08x ('%s') to %s of desktop %u" \
            " ('%s')",
            client->id, client->info.name,
            (to_front) ? "front" : "back", desktop->id, desktop->name);

    status = (to_front)
        ? stacking_raise(desktop, client)
        : stacking_lower(desktop, client);

    if (status == 1) {
        LOGGER_ERROR("Client not found in desktop stacking", L_NARG);
        return -1;
    }
    if (status != 0) {
        LOGGER_ERROR("Failed to move client within stacking", L_NARG);
        return -1;
    }

    ccmd_desktop_enforce_layers(desktop);
    desktop->is_outdated = true;

    return 0;
}


/**
 * @brief Raise every transient descendant of a client along with it
 *
 * Walks @p client's @c transients tree directly (see its doc
 * comment, client.h), depth-first, rather than scanning @p desktop's
 * own entire stacking order comparing raw @c transient_for window
 * IDs.  A chain of dialogs (a dialog's own dialog, and so on) rises
 * together all the same, by following real pointers instead of
 * rediscovering the
 * relationship from scratch on every call.  A client transient for
 * its whole group (ICCCM §4.1.2.6) has no @c transient_parent to
 * appear in that tree; raised alongside @p client too, right after
 * its specific-parent descendants, whenever
 * @a client_group_transient_anchor in @c cmds/client/transient.c
 * currently resolves
 * it to @p client specifically, the same check @c cmds/client/
 * layer.c's @c s_enforce_layer_place_family already makes for
 * stacking.
 *
 * Scoped to @p desktop, the same as before.  A descendant registered
 * under some other desktop (a pinned parent's un-pinned dialog,
 * say, still on whichever desktop it was originally created on; see
 * @a ccmd_client_bring_family's comment, cmds/client/
 * transient.c, for the fuller reasoning) is left untouched here,
 * since @a s_desktop_client_send_to_end itself only ever reorders
 * @p desktop's stacking list.
 *
 * @param desktop Desktop whose stacking order is searched and updated
 * @param client  Client whose transient descendants get raised too
 * @param depth   Current recursion depth; the caller's first
 *                call always passes @c 0
 *
 * @note A null @p client, or exceeding @c WM_TRANSIENT_CHAIN_MAX_DEPTH,
 *       is a silent no-op
 * @note Complexity: @e O(f + n), where @e f is the number of
 *       @p client's transient descendants, at every depth combined,
 *       sharing @p desktop with it, and @e n is the number of clients
 *       on @p desktop (for the group-transient search)
 */
static void s_desktop_transients_raise(desktop_td *desktop,
        client_td *client, uint32_t depth)
{
    cdlist_item_td *node;

    if (client == NULL || depth >= WM_TRANSIENT_CHAIN_MAX_DEPTH) {
        return;
    }

    if (client->transients != NULL) {
        const cdlist_item_td *initial;

        node = cdlist_head(client->transients);
        initial = node;
        if (node != NULL) {
            do {
                client_td *const child = (client_td *) cdlist_data(node);

                if (child != NULL && child->desktop_id == desktop->id) {
                    (void) s_desktop_client_send_to_end(desktop, child,
                            true);
                    s_desktop_transients_raise(desktop, child, depth + 1);
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }
    }

    if (client->desktop_id == desktop->id && desktop->clients != NULL) {
        void *elem;

        ohtbl_foreach(desktop->clients, elem) {
            client_td *const gchild = (client_td *) elem;

            if (gchild != NULL && gchild != client &&
                    gchild->is_transient_for_group &&
                    gchild->desktop_id == desktop->id &&
                    client_group_transient_anchor(gchild) == client) {
                (void) s_desktop_client_send_to_end(desktop, gchild,
                        true);
                s_desktop_transients_raise(desktop, gchild, depth + 1);
            }
        }
    }
}


/**
 * @brief Announce a fresh attention request the user cannot see
 *
 * Raised on a genuinely new request only: a desktop that was already
 * urgent stays quiet unless the request has moved to a different
 * viewport page, which @p is_urgent alone cannot tell apart from the
 * one already known.
 *
 * What the notice names is what the user would still have to do to
 * reach the window.  A different desktop is named; a page other than
 * the one that desktop is panned to is named; both are named when
 * both differ, since switching desktops alone would land on the page
 * that desktop was left on and the window would still be off screen.
 * Neither differing means the window is on screen already, with its
 * own titlebar blink (@c policy/urgency.c), and a dialog would only
 * repeat what is in front of the user.
 *
 * @param desktop      Desktop just recomputed
 * @param surface      Surface owning it, or @c NULL
 * @param was_urgent   Whether it held an urgent client before
 * @param had_page     Whether @p was_page holds a page at all
 * @param was_page     Page the previous request sat on
 * @param is_urgent    Whether it holds one now
 * @param has_page     Whether @p page holds a page at all
 * @param page         Page the current request sits on
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static void s_notify_urgency(const desktop_td *desktop,
        surface_td *surface, bool was_urgent, bool had_page,
        struct position_s was_page, bool is_urgent, bool has_page,
        struct position_s page)
{
    const config_td *config = wm_get_config();
    const list_td *surfaces;
    uint32_t surface_count;
    uint32_t shown_col;
    uint32_t shown_row;
    /* The name, plus every fixed part that can be appended to it,
     * each sized for a 'uint32_t' spelled out in full: the base
     * message, the ' {column, row}' page suffix and the
     * ' (on surface n)' one.  Sized rather than trimmed because a
     * truncation here would cut a coordinate in half and leave the
     * notice naming a page that does not exist. */
    char text[WM_DESKTOP_MAX_LENGTH_NAME + 96];
    size_t used;
    bool other_desktop;
    bool other_page;
    bool is_fresh;

    if (!is_urgent || surface == NULL || config == NULL ||
            !config->base.urgency.notify_activity ||
            menu_message_dialog_is_open()) {
        return;
    }

    is_fresh = !was_urgent || (has_page && (!had_page ||
                page.x != was_page.x || page.y != was_page.y));
    if (!is_fresh) {
        return;
    }

    other_desktop = (desktop->id != surface->desktop_cur);
    other_page = has_page &&
        scmd_surface_viewport_desktop_page(surface, desktop,
                &shown_col, &shown_row) &&
        (page.x != (int32_t) shown_col || page.y != (int32_t) shown_row);

    if (!other_desktop && !other_page) {
        return;
    }

    /* With only the page to report, the desktop names nothing: it is
     * the one already on screen */
    if (!other_desktop) {
        (void) snprintf(text, sizeof(text), _(STR_PAGE_ACTIVITY_FMT),
                (unsigned int) page.x, (unsigned int) page.y);
        menu_message_dialog_show(xcb_connection_get(), surface, config,
                text, MENU_MSG_LEVEL_INFO);
        return;
    }

    (void) snprintf(text, sizeof(text),
            _(STR_DESKTOP_ACTIVITY_UNNAMED_FMT),
            (unsigned int) desktop->id);

    if (desktop->name[0] != '\0') {
        used = safe_strlen(text);
        if (used < sizeof(text)) {
            (void) snprintf(text + used, sizeof(text) - used,
                    _(STR_DESKTOP_ACTIVITY_NAME_SUFFIX_FMT),
                    desktop->name);
        }
    }

    if (other_page) {
        used = safe_strlen(text);
        if (used < sizeof(text)) {
            (void) snprintf(text + used, sizeof(text) - used,
                    _(STR_PAGE_SUFFIX_FMT),
                    (unsigned int) page.x, (unsigned int) page.y);
        }
    }

    surfaces = wm_get_surfaces();
    surface_count = (surfaces != NULL)
        ? (uint32_t) list_size(surfaces) : 0u;
    if (surface_count > 1u) {
        used = safe_strlen(text);
        if (used < sizeof(text)) {
            (void) snprintf(text + used, sizeof(text) - used,
                    _(STR_DESKTOP_ACTIVITY_SURFACE_SUFFIX_FMT),
                    (unsigned int) surface->id);
        }
    }

    menu_message_dialog_show(xcb_connection_get(), surface, config,
            text, MENU_MSG_LEVEL_INFO);
}


/* Add a previously allocated client in the desktop */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    void *removed_client;
    client_td existing_key;
    client_td *existing;
    int insert_rc;

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Adding client 0x%08x ('%s') to desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    /* If this key is already occupied, find out whether the window
     * it belongs to still genuinely exists before ever attempting the
     * real insert below.  A window whose 'MapRequest' sat queued
     * long enough that it was already gone by the time this window
     * manager got to it (see the 'ghost window' reasoning throughout
     * this project's history) can leave exactly this kind of
     * entry behind: nothing destroyed it from within this window
     * manager, since nothing here ever considered it alive to begin
     * with, so nothing here ever cleaned it up either, and it goes on
     * occupying this exact key forever, blocking every later window
     * whose ID happens to be reused for it.  A single, ordinary
     * blocking XCB call (the exact same one 'client_init' itself
     * already relies on elsewhere, no timeout wrapped around it) is
     * enough to tell a window that no longer exists (a NULL reply,
     * most plausibly 'BadWindow') from one that still does; only in
     * the former case is the stale entry actually removed and this
     * insert retried, never on a guess. */
    memset(&existing_key, 0, sizeof(existing_key));
    existing_key.id = client->id;
    existing = &existing_key;
    if (ohtbl_lookup(desktop->clients, (void **) &existing) == 0 &&
            existing != NULL) {
        bool existing_is_stale = false;

        if (xcb_connection_get() != NULL && existing->window != 0) {
            xcb_get_window_attributes_reply_t *attr_reply =
                xcb_get_window_attributes_reply(xcb_connection_get(),
                        xcb_get_window_attributes(xcb_connection_get(),
                                existing->window),
                        NULL);

            if (attr_reply == NULL) {
                existing_is_stale = true;
            } else {
                free(attr_reply);
            }
        }

        LOGGER_WARNING("Client 0x%08x: hash table already holds an" \
                " entry for this key (existing: window=0x%x" \
                " frame=0x%x titlebar=0x%x icon_window=0x%x" \
                " name='%s' pid=%d); its own window %s", client->id,
                existing->window, existing->frame, existing->titlebar,
                existing->icon_window, existing->info.name,
                (int) existing->process.pid,
                existing_is_stale
                    ? "no longer exists; removing the stale entry"
                    : "still exists; leaving it in place");

        if (existing_is_stale) {
            desktop_action_client_rem(desktop, existing);
            existing->window = 0;
            client_destroy(existing);
        }
    }

    /* Add to hash table for quick lookup */
    insert_rc = ohtbl_insert(desktop->clients, (void *) client);
    if (insert_rc != 0) {
        LOGGER_ERROR("Failed to add client to hash table", L_NARG);
        return -1;
    }

    /* Add to stacking list for rendering order */
    if (stacking_add(desktop, client) != 0) {
        LOGGER_ERROR("Failed to add client to stacking list", L_NARG);
        /* Remove from hash table on failure */
        removed_client = (void *) client;
        ohtbl_remove(desktop->clients, &removed_client);
        return -1;
    }

    LOGGER_TRACE("Added client 0x%08x to desktop %u ('%s')",
            client->id, desktop->id, desktop->name);
    desktop->is_outdated = true;  /* Mark for redraw */
    desktop_action_recompute_urgent(desktop);

    return 0;
}


/* Remove a client from the desktop */
int desktop_action_client_rem(desktop_td *desktop, client_td *client)
{
    void *removed_client;

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Removing client 0x%08x ('%s') from desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    /* Remove from hash table */
    removed_client = (void *) client;
    if (ohtbl_remove(desktop->clients, &removed_client) != 0) {
        LOGGER_ALERT("Failed to remove client from hash table", L_NARG);
        return -1;
    }

    /* Deliberately left in the stacking order.  That order spans every
     * managed client whichever desktop shows it, and this function
     * runs for a desktop change as much as for a client going away.  A
     * pinned window passes through here on every switch, and dropping
     * it would lose the height that holding one order exists to keep.
     * 'client_destroy' is what forgets a client there, once it is
     * gone for good. */

    LOGGER_TRACE("Removed client 0x%08x from desktop %u",
            client->id, desktop->id);
    desktop->is_outdated = true;  /* Mark for redraw */
    desktop_action_recompute_urgent(desktop);

    return 0;
}


/* Move a client from one desktop to another */
int desktop_action_client_move(desktop_td *from, desktop_td *to,
        client_td *client)
{
    if (to == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    if (from == to) {
        return 0;
    }

    if (from != NULL) {
        (void) desktop_action_client_rem(from, client);
    }

    if (desktop_action_client_add(to, client) != 0) {
        /* Put it back where it came from: a client that belongs to
         * no desktop's table is reachable through nothing, yet
         * stays mapped on screen, which is worse than a move that
         * simply did not happen */
        LOGGER_ERROR("Failed to move client 0x%08x to desktop %u;" \
                " leaving it on desktop %u",
                client->id, to->id,
                (from != NULL) ? from->id : client->desktop_id);
        if (from != NULL) {
            (void) desktop_action_client_add(from, client);
        }
        return 1;
    }

    client->desktop_id = to->id;

    return 0;
}


/* Recompute whether any client on the desktop currently has its
 * urgency hint set */
void desktop_action_recompute_urgent(desktop_td *desktop)
{
    void *elem;
    surface_td *surface;
    const client_td *urgent = NULL;
    struct position_s page = { 0, 0 };
    bool was_urgent;
    bool had_page;
    bool has_page = false;
    struct position_s was_page;
    bool found = false;

    if (desktop == NULL || desktop->clients == NULL) {
        return;
    }

    was_urgent = desktop->is_urgent;
    had_page = desktop->has_urgent_page;
    was_page = desktop->urgent_page;
    surface = wm_get_desktop_surface(desktop);

    ohtbl_foreach(desktop->clients, elem) {
        client_td *c = (client_td *) elem;

        if (c != NULL && client_is_urgent(c)) {
            found = true;
            urgent = c;
            break;
        }
    }

    /* The page the request came from, not merely that one came: with
     * several pages, urgency moving from one to another never clears
     * 'is_urgent', so that flag alone would swallow the second
     * request entirely.  Only the first urgent client found is
     * located, the same one the loop above already settled on. */
    if (found && urgent != NULL) {
        uint32_t col;
        uint32_t row;

        has_page = scmd_surface_viewport_client_page(surface, desktop,
                urgent, &col, &row);
        page.x = (int32_t) col;
        page.y = (int32_t) row;
    }

    desktop->is_urgent = found;
    desktop->has_urgent_page = has_page;
    desktop->urgent_page = page;

    s_notify_urgency(desktop, surface, was_urgent, had_page, was_page,
            found, has_page, page);
}


/* Send a client to the front of the desktop's window stack, along
 * with every transient descendant it has (a dialog stays above the
 * window it belongs to) */
int desktop_action_client_send_front(desktop_td *desktop,
        client_td *client)
{
    int status;

    status = s_desktop_client_send_to_end(desktop, client, true);
    if (status == 0) {
        s_desktop_transients_raise(desktop, client, 0);
    }

    return status;
}


/* Send a client to the back of the desktop's window stack */
int desktop_action_client_send_back(desktop_td *desktop,
        client_td *client)
{
    return s_desktop_client_send_to_end(desktop, client, false);
}


/* Iconify all clients on the desktop */
int desktop_action_clients_iconify_all(desktop_td *desktop)
{
    void *elem;

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Iconifying all clients on desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Iterate through all clients in hash table and iconify them */
    ohtbl_foreach(desktop->clients, elem) {
        enact_client_iconify((client_td *) elem);
    }

    return 0;
}


/* Restore every iconified client on the desktop */
int desktop_action_clients_deiconify_all(desktop_td *desktop)
{
    void *elem;

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Restoring all iconified clients on desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Iterate through all clients in hash table and restore only the
     * ones currently iconified, leaving every other client (normal,
     * maximized, fullscreen) untouched */
    ohtbl_foreach(desktop->clients, elem) {
        client_td *const client = (client_td *) elem;
        if (client != NULL && client_is_iconified(client)) {
            enact_client_restore(client);
        }
    }

    return 0;
}


/* Launch a process on the desktop */
int desktop_action_process_launch(desktop_td *desktop,
        const char *executable_path)
{
    return desktop_action_process_launch_with_class(desktop,
            executable_path, NULL, NULL);
}


/* Launch a process on the desktop with 'WM_CLASS' override */
int desktop_action_process_launch_with_class(desktop_td *desktop,
        const char *restrict executable_path,
        const char *restrict class_name,
        pid_t *restrict out_pid)
{
    spawn_opts_td opts = { NULL, NULL };
    pid_t pid = 0;
    int spawn_result;
    char startup_id[128];
    bool have_startup_id;

    if (desktop == NULL || executable_path == NULL ||
            executable_path[0] == '\0') {
        LOGGER_ERROR("Invalid desktop or executable path pointer",
                L_NARG);
        return -1;
    }

    LOGGER_TRACE("Launching process for '%s' on desktop %u ('%s')",
            executable_path, desktop->id, desktop->name);

    /* Begin startup notification before forking, so the child can be
     * handed the resulting ID as 'DESKTOP_STARTUP_ID' below; a
     * startup-notification-aware application reads that variable and
     * broadcasts its completion once its main window is ready.
     * Skipped entirely when 'startup_notification.is_enabled' is
     * false: 'have_startup_id' then stays false too, so the option
     * handed to 'spawn_command' below simply stays null. */
    have_startup_id = (xcb_connection_get() != NULL) &&
        desktop->config->base.startup_notification.is_enabled &&
        cctl_sn_begin(xcb_connection_get(), wm_get_surfaces(),
                executable_path, desktop->id, startup_id,
                sizeof(startup_id));

    opts.startup_id = (have_startup_id) ? startup_id : NULL;
    opts.class_name = class_name;

    spawn_result = spawn_command(executable_path, &opts, &pid);
    if (spawn_result != 0) {
        if (have_startup_id) {
            cctl_sn_cancel(xcb_connection_get(), wm_get_surfaces(),
                    startup_id);
        }
        return spawn_result;
    }

    if (have_startup_id) {
        cctl_sn_associate_pid(startup_id, pid);
    }

    if (out_pid != NULL) {
        *out_pid = pid;
    }

    return 0;
}
