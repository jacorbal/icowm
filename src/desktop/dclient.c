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
#include <stdlib.h>     /* NULL */
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

/* Command includes */
#include <cmds/layer.h>

/* Project includes */
#include <client.h>
#include <logger.h>
#include <sn.h>
#include <wm.h>

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
static int s_desktop_set_clients_enabled(desktop_td *desktop,
        bool enabled)
{
    void *elem;

    if (desktop == NULL || desktop->clients == NULL) {
        return 1;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *client = (client_td *) elem;
        if (enabled) {
            client_unset_disable(client);
            client_set_focusable(client);
        } else {
            client_set_disable(client);
            client_unset_focusable(client);
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
 * @note Supported layouts include: "floating", "stacking", "tiling",
 *       and "monocle".
 * @note Complexity: @e O(k), where @e k is the number of supported
 *       layouts
 */
static bool s_desktop_layout_supported(const char *layout)
{
    static const char *layouts[] = {
        "stacking",
        "floating",
        "tiling",
        "monocle"
    };

    for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
        if (strcasecmp(layout, layouts[i]) == 0) {
            return true;
        }
    }

    return false;
}


/* Send a client to another desktop */
int desktop_action_send_client(desktop_td *desktop, client_td *client,
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
        cdlist_item_td *initial = node;
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

    return 0;
}


/* Send a client to the front of the desktop's window stack */
int desktop_action_client_send_front(desktop_td *desktop,
        client_td *client)
{
    cdlist_item_td *node;

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Sending client 0x%08x ('%s') to front" \
            " of desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    /* Find the client in the stacking list */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        cdlist_item_td *initial = node;
        do {
            if (cdlist_data(node) == (void *) client) {
                /* Found it, move to tail (front/top of stack) */
                if (cdlist_rem_next(desktop->stacking,
                            cdlist_prev(node), NULL) != 0) {
                    LOGGER_ERROR("Failed to remove client from stacking",
                            L_NARG);
                    return -1;
                }

                if (cdlist_ins_next(desktop->stacking,
                            cdlist_tail(desktop->stacking),
                            (void *) client) != 0) {
                    LOGGER_ERROR("Failed to insert client to stacking",
                            L_NARG);
                    return -1;
                }

                wcmd_desktop_enforce_layers(desktop);
                desktop->is_outdated = true;
                return 0;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    LOGGER_ERROR("Client not found in desktop stacking", L_NARG);
    return -1;
}


/* Send a client to the back of the desktop's window stack */
int desktop_action_client_send_back(desktop_td *desktop,
        client_td *client)
{
    cdlist_item_td *node;

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Sending client 0x%08x ('%s') to back" \
            " of desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    /* Find the client in the stacking list */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        cdlist_item_td *initial = node;
        do {
            if (cdlist_data(node) == (void *) client) {
                /* Found it, move to head (back/bottom of stack) */
                if (cdlist_rem_next(desktop->stacking,
                            cdlist_prev(node), NULL) != 0) {
                    LOGGER_ERROR("Failed to remove client from stacking",
                            L_NARG);
                    return -1;
                }

                if (cdlist_ins_next(desktop->stacking,
                            NULL/* cdlist_head(desktop->stacking) */,
                            (void *) client) != 0) {
                    LOGGER_ERROR("Failed to insert client to stacking",
                            L_NARG);
                    return -1;
                }

                wcmd_desktop_enforce_layers(desktop);
                desktop->is_outdated = true;
                return 0;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    LOGGER_ERROR("Client not found in desktop stacking", L_NARG);
    return -1;
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
        client_send_event_iconify((client_td *) elem);
    }

    return 0;
}


/* Cycle through active clients on the desktop */
int desktop_action_cycle_clients_active(desktop_td *desktop)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    cdlist_item_td *active_node = NULL;

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Cycling through active clients on desktop %u ('%s')",
            desktop->id, desktop->name);

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return 0;
    }

    /* Find the node holding the currently active client */
    initial = node;
    active_node = NULL;
    do {
        client_td *c = (client_td *) cdlist_data(node);
        if (c != NULL && c->id == desktop->client_active_id) {
            active_node = node;
            break;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    /* Start searching from the node after the active one */
    node = (active_node != NULL)
        ? cdlist_next(active_node)
        : cdlist_head(desktop->stacking);
    if (node == NULL) {
        node = cdlist_head(desktop->stacking);
    }

    /* Find next non-iconified client */
    initial = node;
    do {
        client_td *c = (client_td *) cdlist_data(node);
        if (c != NULL && !client_is_iconified(c)) {
            client_send_event_focus(c);
            return 0;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return 0;
}


/* Cycle through active clients in reverse order on the desktop */
int desktop_action_cycle_clients_prev(desktop_td *desktop)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    cdlist_item_td *active_node = NULL;

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Cycling to previous active client on desktop %u ('%s')",
            desktop->id, desktop->name);

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return 0;
    }

    /* Find the node holding the currently active client */
    initial = node;
    active_node = NULL;
    do {
        client_td *c = (client_td *) cdlist_data(node);
        if (c != NULL && c->id == desktop->client_active_id) {
            active_node = node;
            break;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    /* Start searching from the node before the active one.
     * Since the list is circular, cdlist_prev(head) == tail. */
    node = (active_node != NULL)
        ? cdlist_prev(active_node)
        : cdlist_tail(desktop->stacking);
    if (node == NULL) {
        node = cdlist_tail(desktop->stacking);
    }

    /* Find previous non-iconified client */
    initial = node;
    do {
        client_td *c = (client_td *) cdlist_data(node);
        if (c != NULL && !client_is_iconified(c)) {
            client_send_event_focus(c);
            return 0;
        }
        node = cdlist_prev(node);
    } while (node != NULL && node != initial);

    return 0;
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
        cdlist_item_td *initial = node;
        do {
            client_td *client = (client_td *) cdlist_data(node);
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
            client_send_event_restore(client);
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

    if (s_desktop_set_clients_enabled(desktop, false) != 0) {
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

    if (s_desktop_set_clients_enabled(desktop, true) != 0) {
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

    if (!s_desktop_layout_supported(layout)) {
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
        const char *executable_path, const char *class_name)
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
     * broadcasts its own completion once its main window is ready. */
    have_startup_id = (desktop->connection != NULL) &&
        sn_begin(desktop->connection, wm_get_surfaces(),
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
        /* 'execvp' failed: report 'errno' to parent */
        child_errno = errno;
        (void) write(err_pipe[1], &child_errno, sizeof(child_errno));

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
            sn_cancel(desktop->connection, wm_get_surfaces(), startup_id);
        }
        return -2;
    }

    LOGGER_DEBUG("Process for '%s' running with PID %d",
            executable_path, (int) pid);

    return 0;
}
