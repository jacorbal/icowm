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

#define _POSIX_C_SOURCE 200112L /* execvp, fork, pipe */


/* System includes */
#include <errno.h>      /* errno */
#include <fcntl.h>      /* fcntl, F_SETFD, FD_CLOEXEC */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL, setenv */
#include <string.h>     /* strerror */
#include <strings.h>    /* strcasecmp */
#include <sys/types.h>  /* pid_t */
#include <unistd.h>     /* execvp, _exit, fork, close, pipe, read */
#include <wordexp.h>    /* wordexp, wordfree */

/* XCB includes */
#include <xcb/xcb.h>    /* xcb_get_file_descriptor */

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>
#include <adt/queue.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Command includes */
#include <cmds/client/layer.h>

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


/**
 * @brief Enable or disable all clients on a desktop
 *
 * Iterates over the desktop's client collection and updates each
 * client's enabled and focusable state according to @p enabled.
 *
 * @param desktop Target desktop containing the clients
 * @param enabled If @c true, clients are enabled and focusable;
 *                otherwise, they are disabled and not focusable
 *
 * @return Status code
 * @retval  0 Success
 * @retval  1 Invalid input (@p desktop or its client list is null)
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
static int s_desktop_clients_enabled_set(desktop_td *desktop,
        bool enabled)
{
    void *elem;

    if (desktop == NULL || desktop->clients == NULL) {
        return 1;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *const client = (client_td *) elem;
        if (enabled) {
            client_enable(client);
            client_allow_focus(client);
        } else {
            client_disable(client);
            client_forbid_focus(client);
        }
    }

    return 0;
}


/**
 * @brief Check whether a layout name is supported
 *
 * Compares the provided layout string against a fixed set of known
 * layouts using a case-insensitive comparison.
 *
 * @param layout Layout name to validate
 *
 * @return Validation result
 * @retval true  Layout is supported
 * @retval false Layout is not supported
 *
 * @note Complexity: @e O(k), where @e k is the number of supported
 *       layouts
 */
static bool s_desktop_layout_is_supported(const char *layout)
{
    /* A tiling and a monocle layout were both considered at one
     * point, alongside this one, but neither was ultimately
     * implemented; left here, commented out, rather than removed
     * outright, in case either is picked back up later. */
    static const char *layouts[] = {
        "stacking",
//        "tiling",
//        "monocle",
    };

    for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
        if (strcasecmp(layout, layouts[i]) == 0) {
            return true;
        }
    }

    return false;
}


/**
 * @brief Focus the next (or previous) non-iconified client in the
 *        stacking order after (or before) the currently active one
 *
 * Shared by @c desktop_action_cycle_clients_active and @c desktop_
 * action_cycle_clients_prev below, which only differ in direction:
 * both first locate the node holding @p desktop's own @c client_
 * active_id, then walk from there, wrapping around the circular
 * stacking list, until a non-iconified client is found to focus.
 * Assumes @p desktop is already known non-@c NULL; the caller's own
 * guard and log message stay at each call site since their wording
 * differs by direction.
 *
 * @param desktop Desktop to cycle clients on
 * @param forward @c true to search forward (@c cdlist_next, wrapping
 *                from the list head), @c false to search backward
 *                (@c cdlist_prev, wrapping from the list tail)
 *
 * @return @c 0 whether or not a client was found to focus (an empty
 *         or all-iconified stacking list is not an error)
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static int s_desktop_cycle_clients(desktop_td *desktop, bool forward)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;
    cdlist_item_td *active_node = NULL;

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return 0;
    }

    /* Find the node holding the currently active client */
    initial = node;
    do {
        const client_td *c = (client_td *) cdlist_data(node);
        if (c != NULL && c->id == desktop->client_active_id) {
            active_node = node;
            break;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    /* Start searching from the node after (or before) the active one.
     * Since the list is circular, 'cdlist_prev(head) == tail'. */
    if (forward) {
        node = (active_node != NULL)
            ? cdlist_next(active_node)
            : cdlist_head(desktop->stacking);
        if (node == NULL) {
            node = cdlist_head(desktop->stacking);
        }
    } else {
        node = (active_node != NULL)
            ? cdlist_prev(active_node)
            : cdlist_tail(desktop->stacking);
        if (node == NULL) {
            node = cdlist_tail(desktop->stacking);
        }
    }

    /* Find the next (or previous) non-iconified, non-hidden client:
     * 'client_is_iconified' alone would still let cycling land on one
     * hidden via 'KEYBIND_CLIENT_HIDE' (a distinct state from
     * iconified; see 'client_is_hidden', client.h), silently focusing
     * a window nothing on screen shows as selected. */
    initial = node;
    do {
        client_td *c = (client_td *) cdlist_data(node);
        if (c != NULL && !client_is_iconified(c) && !client_is_hidden(c)) {
            enact_client_focus(c);
            return 0;
        }
        node = (forward) ? cdlist_next(node) : cdlist_prev(node);
    } while (node != NULL && node != initial);

    return 0;
}


/**
 * @brief Move a client to the front or back of the desktop's window
 *        stack
 *
 * Shared by @c desktop_action_client_send_front and @c desktop_
 * action_client_send_back below, which only differ in which end of
 * the stacking list the client is reinserted at and the wording of
 * their own log message.
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
    cdlist_item_td *node;

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Sending client 0x%08x ('%s') to %s of desktop %u" \
            " ('%s')",
            client->id, client->info.name,
            (to_front) ? "front" : "back", desktop->id, desktop->name);

    /* Find the client in the stacking list */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        cdlist_item_td *const initial = node;
        do {
            if (cdlist_data(node) == (void *) client) {
                /* Found it, move to tail (front/top) or head
                 * (back/bottom) of the stack */
                if (cdlist_rem_next(desktop->stacking,
                            cdlist_prev(node), NULL) != 0) {
                    LOGGER_ERROR("Failed to remove client from stacking",
                            L_NARG);
                    return -1;
                }

                if (cdlist_ins_next(desktop->stacking,
                            (to_front)
                                ? cdlist_tail(desktop->stacking)
                                : NULL /* cdlist_head(desktop->stacking) */,
                            (void *) client) != 0) {
                    LOGGER_ERROR("Failed to insert client to stacking",
                            L_NARG);
                    return -1;
                }

                ccmd_desktop_enforce_layers(desktop);
                desktop->is_outdated = true;
                return 0;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    LOGGER_ERROR("Client not found in desktop stacking", L_NARG);
    return -1;
}


/* Send a client to another desktop */
int desktop_action_client_send(desktop_td *desktop, client_td *client,
        uint32_t desktop_id)
{
    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Sending client 0x%08x ('%s') from desktop %u ('%s')"
            " to desktop %u",
            client->id, client->info.name,
            desktop->id, desktop->name, desktop_id);

    /* Remove client from this desktop */
    if (desktop_action_client_rem(desktop, client) != 0) {
        LOGGER_ERROR("Failed to remove client from source desktop",
                L_NARG);
        return 1;
    }

    /* Update client's recorded desktop */
    client->desktop_id = desktop_id;
    return 0;
}


/* Add a previously allocated client in the desktop */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    void *removed_client;

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    LOGGER_TRACE("Adding client 0x%08x ('%s') to desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    /* Add to hash table for quick lookup */
    if (ohtbl_insert(desktop->clients, (void *) client) != 0) {
        LOGGER_ERROR("Failed to add client to hash table", L_NARG);
        return -1;
    }

    /* Add to stacking list for rendering order */
    if (cdlist_ins_next(desktop->stacking,
                cdlist_tail(desktop->stacking),
                (void *) client) != 0) {
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
    cdlist_item_td *node;
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

    /* Remove from stacking list (iterate and find the matching client) */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        const cdlist_item_td *initial = node;
        do {
            if (cdlist_data(node) == (void *) client) {
                /* Found it, remove it */
                if (cdlist_rem_next(desktop->stacking,
                            cdlist_prev(node), NULL) != 0) {
                    LOGGER_ERROR("Failed to remove client from" \
                            " stacking list", L_NARG);
                    return -1;
                }
                break;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    LOGGER_TRACE("Removed client 0x%08x from desktop %u",
            client->id, desktop->id);
    desktop->is_outdated = true;  /* Mark for redraw */
    desktop_action_recompute_urgent(desktop);

    return 0;
}


/* Find the client on a desktop matching a given client ID */
client_td *desktop_find_client_by_id(const desktop_td *desktop,
        uint32_t id)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (desktop == NULL || desktop->stacking == NULL) {
        return NULL;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return NULL;
    }

    initial = node;
    do {
        client_td *const c = (client_td *) cdlist_data(node);
        if (c != NULL && c->id == id) {
            return c;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/* Recompute whether any client on the desktop currently has its own
 * urgency hint set */
void desktop_action_recompute_urgent(desktop_td *desktop)
{
    void *elem;
    bool was_urgent;
    bool found = false;

    if (desktop == NULL || desktop->clients == NULL) {
        return;
    }

    was_urgent = desktop->is_urgent;

    ohtbl_foreach(desktop->clients, elem) {
        const client_td *c = (client_td *) elem;

        if (c != NULL && client_is_urgent(c)) {
            found = true;
            break;
        }
    }

    desktop->is_urgent = found;

    /* Only on the actual false-to-true transition, and only when this
     * is not the desktop currently visible on its own surface: that
     * case already gets its own titlebar blink (policy/urgency.c),
     * so a dialog here would only duplicate what is already on
     * screen. */
    if (!was_urgent && found) {
        surface_td *const surface = wm_get_desktop_surface(desktop);
        const config_td *config = wm_get_config();

        if (surface != NULL && desktop->id != surface->desktop_cur &&
                config != NULL && config->desktops.notify_activity &&
                !menu_message_dialog_is_open()) {
            const list_td *surfaces = wm_get_surfaces();
            uint32_t surface_count = (surfaces != NULL)
                ? (uint32_t) list_size(surfaces) : 0u;
            char text[WM_DESKTOP_MAX_LENGTH_NAME + 48];
            size_t used;

            snprintf(text, sizeof(text),
                    _(STR_DESKTOP_ACTIVITY_UNNAMED_FMT),
                    (unsigned int) desktop->id);

            if (desktop->name[0] != '\0') {
                used = safe_strlen(text);
                if (used < sizeof(text)) {
                    snprintf(text + used, sizeof(text) - used,
                            _(STR_DESKTOP_ACTIVITY_NAME_SUFFIX_FMT),
                            desktop->name);
                }
            }

            if (surface_count > 1u) {
                used = safe_strlen(text);
                if (used < sizeof(text)) {
                    snprintf(text + used, sizeof(text) - used,
                            _(STR_DESKTOP_ACTIVITY_SURFACE_SUFFIX_FMT),
                            (unsigned int) surface->id);
                }
            }

            menu_message_dialog_show(desktop->connection, surface,
                    config, text, MENU_MSG_LEVEL_INFO);
        }
    }
}


/**
 * @brief Raise every transient descendant of a client along with it
 *
 * Breadth-first search through @p desktop's own stacking order: any
 * client whose own @c transient_for names @p client's window is
 * raised right after it, then the search continues from each of
 * those in turn, so a chain of dialogs (a dialog's own dialog, and
 * so on) rises together rather than only the direct child.
 *
 * @param desktop Desktop whose stacking order is searched and updated
 * @param client Client whose transient descendants get raised too
 *
 * @note Bounded by @p desktop's own total client count, so a
 *       @c transient_for cycle (a misbehaving client announcing
 *       itself, directly or indirectly, transient for its own
 *       descendant) can never loop indefinitely; the bound alone is
 *       enough to guarantee termination, so no separate visited set
 *       is needed on top of it
 * @note Complexity: @e O(n ^ 2), where @e n is the number of clients
 *       on @p desktop
 */
static void s_desktop_transients_raise(desktop_td *desktop,
        const client_td *client)
{
    queue_td *pending;
    size_t max_iterations;
    size_t processed;

    pending = queue_init(NULL);
    if (pending == NULL) {
        return;
    }

    max_iterations = cdlist_size(desktop->stacking);
    processed = 0;
    (void) queue_enqueue(pending, client);

    while (!queue_is_empty(pending) && processed < max_iterations) {
        void *data;
        const client_td *parent;
        cdlist_item_td *node;
        const cdlist_item_td *initial;

        (void) queue_dequeue(pending, &data);
        parent = (client_td *) data;
        ++processed;

        node = cdlist_head(desktop->stacking);
        if (node == NULL) {
            continue;
        }
        initial = node;
        do {
            client_td *const candidate = (client_td *) cdlist_data(node);
            if (candidate != NULL &&
                    candidate->transient_for == parent->window) {
                (void) s_desktop_client_send_to_end(desktop, candidate,
                        true);
                (void) queue_enqueue(pending, candidate);
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    queue_destroy(pending);
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
        s_desktop_transients_raise(desktop, client);
    }

    return status;
}


/* Send a client to the back of the desktop's window stack */
int desktop_action_client_send_back(desktop_td *desktop,
        client_td *client)
{
    return s_desktop_client_send_to_end(desktop, client, false);
}


/* Rearrange clients on the desktop */
int desktop_action_clients_rearrange(desktop_td *desktop)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Rearranging clients on desktop %u ('%s')",
            desktop->id, desktop->name);

    if (desktop->clients == NULL) {
        return 1;
    }

    /* Mark desktop as needing redraw */
    desktop->is_outdated = true;

    return 0;
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


/* Cycle through active clients on the desktop */
int desktop_action_cycle_clients_active(desktop_td *desktop)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Cycling through active clients on desktop %u ('%s')",
            desktop->id, desktop->name);

    return s_desktop_cycle_clients(desktop, true);
}


/* Cycle through active clients in reverse order on the desktop */
int desktop_action_cycle_clients_prev(desktop_td *desktop)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Cycling to previous active client on desktop %u ('%s')",
            desktop->id, desktop->name);

    return s_desktop_cycle_clients(desktop, false);
}


/* Cycle through iconified clients on the desktop */
int desktop_action_cycle_clients_icons(desktop_td *desktop)
{
    cdlist_item_td *node;
    cdlist_item_td *target = NULL;

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Cycling through iconified clients" \
            " on desktop %u ('%s')", desktop->id, desktop->name);

    /* Find first iconified client */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        const cdlist_item_td *initial = node;
        do {
            const client_td *client = (client_td *) cdlist_data(node);
            if (client != NULL && client_is_iconified(client)) {
                target = node;
                break;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    /* Restore the target client */
    if (target != NULL) {
        client_td *client = (client_td *) cdlist_data(target);
        if (client != NULL) {
            enact_client_restore(client);
        }
    }

    return 0;
}


/* Lock the desktop */
int desktop_action_lock(desktop_td *desktop)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Locking desktop %u ('%s')",
            desktop->id, desktop->name);

    if (s_desktop_clients_enabled_set(desktop, false) != 0) {
        return 1;
    }
    desktop->client_active_id = 0;
    desktop->focus_dirty = true;
    desktop->is_outdated = true;

    return 0;
}


/* Unlock the desktop */
int desktop_action_unlock(desktop_td *desktop)
{
    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Unlocking desktop %u ('%s')",
            desktop->id, desktop->name);

    if (s_desktop_clients_enabled_set(desktop, true) != 0) {
        return 1;
    }
    desktop->is_outdated = true;

    return 0;
}


/* Set the layout of the desktop */
int desktop_action_set_layout(desktop_td *desktop, const char *layout)
{
    if (desktop == NULL || layout == NULL) {
        LOGGER_ERROR("Invalid desktop or layout pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Setting layout '%s' on desktop %u ('%s')",
            layout, desktop->id, desktop->name);

    if (layout[0] == '\0') {
        LOGGER_WARNING("Cannot set empty desktop layout", L_NARG);
        return 1;
    }

    if (!s_desktop_layout_is_supported(layout)) {
        LOGGER_WARNING("Unsupported desktop layout '%s'", layout);
        return 1;
    }

    /* This currently validates and records a coherent layout choice.
     * Concrete tiling/placement behavior is applied by the
     * render/update pipeline and future layout strategy handlers. */
    desktop->is_outdated = true;

    return 0;
}


/* Launch a process on the desktop */
int desktop_action_process_launch(desktop_td *desktop,
        const char *executable_path)
{
    return desktop_action_process_launch_with_class(desktop,
            executable_path, NULL);
}


/* Launch a process on the desktop with 'WM_CLASS' override */
int desktop_action_process_launch_with_class(desktop_td *desktop,
        const char *restrict executable_path,
        const char *restrict class_name)
{
    pid_t pid;
    int err_pipe[2];
    int exec_errno;
    ssize_t nread;
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
     * broadcasts its own completion once its main window is ready.
     * Skipped entirely when 'startup_notification.is_enabled' is
     * false: 'have_startup_id' then stays false too, so the rest of
     * this function's own logic (skipping 'DESKTOP_STARTUP_ID' below)
     * needs no separate check of its own. */
    have_startup_id = (desktop->connection != NULL) &&
        desktop->config_base->startup_notification.is_enabled &&
        cctl_sn_begin(desktop->connection, wm_get_surfaces(),
                executable_path, startup_id, sizeof(startup_id));

    /* Create a close-on-exec pipe so the parent can detect 'execvp'
     * failures.  If 'exec' succeeds the write end is closed by the
     * kernel ('FD_CLOEXEC') and the parent reads 0 bytes.  If 'exec'
     * fails the child writes 'errno' and exits. */
    if (pipe(err_pipe) != 0) {
        LOGGER_ERROR("Failed to create error pipe for '%s'",
                executable_path);
        return 1;
    }
    (void) fcntl(err_pipe[1], F_SETFD, FD_CLOEXEC);

    pid = fork();
    if (pid < 0) {
        LOGGER_ERROR("Failed to fork process for executable '%s'",
                executable_path);
        close(err_pipe[0]);
        close(err_pipe[1]);
        return 1;
    }
    if (pid == 0) {
        wordexp_t words = (wordexp_t) {0};
        int wordexp_flags;
        int wr;
        int child_errno;
        ssize_t write_result;

        /* Child: close the read end; write end is close-on-exec */
        close(err_pipe[0]);

        /* Child must close its inherited copy of the X connection's
         * file descriptor before continuing */
        if (desktop->connection != NULL) {
            close(xcb_get_file_descriptor(desktop->connection));
        }

        /* Environment variables have to be set here, in the child,
         * before 'execvp' replaces its image: 'setenv' only ever
         * affects the calling process's own environment, so calling
         * it in the parent after 'fork' (as this used to do for
         * 'RESOURCE_NAME'/'RESOURCE_CLASS') has no effect at all on
         * the child, which already has its own independent copy of
         * the environment from the moment 'fork' returns. */
        if (have_startup_id) {
            (void) setenv("DESKTOP_STARTUP_ID", startup_id, 1);
        }
        if (class_name != NULL && class_name[0] != '\0') {
            (void) setenv("RESOURCE_NAME", class_name, 1);
            (void) setenv("RESOURCE_CLASS", class_name, 1);
        }

        wordexp_flags = WRDE_NOCMD;
#ifdef WRDE_NOENV
        wordexp_flags |= WRDE_NOENV;
#endif
        wr = wordexp(executable_path, &words, wordexp_flags);
        if (wr != 0 || words.we_wordc == 0u) {
            if (words.we_wordv != NULL) {
                wordfree(&words);
            }
            _exit(127);
        }

        execvp(words.we_wordv[0], words.we_wordv);
        /* 'execvp' failed: report 'errno' to parent.  The write
         * result itself is deliberately unchecked: the child is
         * already about to '_exit' either way, with nothing left it
         * could do differently if this particular write failed too,
         * so there is no meaningful recovery to attempt; captured in
         * a real variable rather than cast to 'void' directly on the
         * call, since GCC's own 'warn_unused_result' on 'write' does
         * not treat a bare '(void)' cast as acknowledging it. */
        child_errno = errno;
        write_result = write(err_pipe[1], &child_errno,
                sizeof(child_errno));
        (void) write_result;

        wordfree(&words);
        _exit(127);
    }

    /* Parent: close write end and read exec result */
    close(err_pipe[1]);
    exec_errno = 0;
    nread = read(err_pipe[0], &exec_errno, sizeof(exec_errno));
    close(err_pipe[0]);

    if (nread > 0) {
        /* 'execvp' failed in the child */
        LOGGER_WARNING("Failed to launch '%s': %s",
                executable_path, strerror(exec_errno));
        if (have_startup_id) {
            cctl_sn_cancel(desktop->connection, wm_get_surfaces(), startup_id);
        }
        return -2;
    }

    LOGGER_DEBUG("Process for '%s' running with PID %d",
            executable_path, (int) pid);

    return 0;
}
