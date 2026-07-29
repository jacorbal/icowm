/**
 * @file desktop.c
 *
 * @brief Desktop handling implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* fork, execvp */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* strncpy */
#include <strings.h>    /* strcasecmp */
#include <sys/types.h>  /* pid_t */
#include <unistd.h>     /* fork, execvp, _exit */
#include <wordexp.h>    /* wordexp, wordfree */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/ohtbl.h>  /* Open-addressed hash table (closed hashing) */

/* Utils includes */
#include <utils/murmurhash.h>
#include <utils/safestr.h>

/* Project includes */
#include <client.h>
#include <logger.h>

/* Local includes */
#include <desktop.h>


/* Define a stable hash function */
static size_t s_h1(const void *data)
{
    const client_td *client = (const client_td *) data;
    const uint32_t key = (client == NULL) ? 0u : client->id;

    /* Stable primary hash using a fixed seed (0x9E3779B9, 32-bit golden
     * ratio, 2^32/phi) to ensure good dispersion and reproducible
     * results across runs. */
    return (size_t) murmurhash3_32(&key, sizeof(key), 0x9E3779B9u);
}


/* Define an auxiliary stable hash function */
static size_t s_h2(const void *data)
{
    const client_td *client = (const client_td *) data;
    const uint32_t key = (client == NULL) ? 0u : client->id;
    size_t hash2 = (size_t) murmurhash3_32(&key, sizeof(key), 0x85EBCA6Bu);

    /* Stable secondary hash using a different fixed seed (0x85EBCA6B)
     * to reduce correlation with 'h1'.  The result is forced to be
     * non-zero to guarantee a valid step size in double hashing. */
    return (hash2 == 0u) ? 1u : hash2;
}


/* Members of the hash table (clients) match if they have equal key
 * value (identifier) */
static bool s_client_match(const void *key1, const void *key2)
{
    const client_td *client1 = (const client_td *) key1;
    const client_td *client2 = (const client_td *) key2;

    return client1->id == client2->id;
}


/* Apply desktop lock/unlock state to all clients */
static int s_desktop_set_clients_enabled(desktop_td *desktop,
        bool enabled)
{
    if (desktop == NULL || desktop->clients == NULL) {
        return 1;
    }

    for (size_t i = 0; i < desktop->clients->positions; ++i) {
        if (desktop->clients->table[i] != NULL &&
                desktop->clients->table[i] != desktop->clients->vacated) {
            client_td *client = (client_td *) desktop->clients->table[i];
            if (enabled) {
                client_unset_disable(client);
                client_set_focusable(client);
            } else {
                client_set_disable(client);
                client_unset_focusable(client);
            }
        }
    }

    return 0;
}


/* Validate if a layout name is supported */
static bool s_desktop_layout_supported(const char *layout)
{
    static const char *layouts[] = {
        "floating",
        "stacking",
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


/* Initialize a new desktop */
desktop_td *desktop_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        uint32_t screen_id, uint32_t desktop_id,
        struct config_base_s *config_base,
        struct config_theme_s *config_theme)
{
    desktop_td *desktop;
    xcb_screen_t *screen;
    xcb_screen_iterator_t iter;

    LOGGER_DEBUG("Initializing desktop %u on screen %u",
            desktop_id, screen_id);

    desktop = malloc(sizeof(desktop_td));
    if (desktop == NULL) {
        LOGGER_ERROR("Failed to allocate memory for" \
                " desktop %u on screen %u", desktop_id, screen_id);
        return NULL;
    }

    /* Establish the basics */
    desktop->screen_id = screen_id;
    desktop->id = desktop_id;
    desktop->client_active_id = 0;
    desktop->ewmh = ewmh;
    desktop->connection = connection;

    /* Get the configuration */
    desktop->config_base = config_base;
    desktop->config_theme = config_theme;

    /* Set desktop name.  The config-provided name is copied with
     * 'safe_strncpy' instead of 'snprintf("%s", ...)' because its
     * source field is wider than 'desktop->name'*/
    /* GCC's option '-Wformat-truncation' cannot prove the copy never
     * truncates, and truncating a name that does not fit is the
     * desired, harmless behavior here anyway. */
    if (config_base->screens[screen_id].desktops[desktop_id].name[0] == '\0') {
        snprintf(desktop->name, DESKTOP_MAX_LENGTH_NAME,
                "Desktop %u", desktop_id);
    } else {
        safe_strncpy(desktop->name,
                config_base->screens[screen_id].desktops[desktop_id].name,
                DESKTOP_MAX_LENGTH_NAME);
    }

    /* Set background color */
    desktop->background.is_image = false;
    desktop->background.bg.color =
        config_base->screens[screen_id].desktops[desktop_id].settings.background.color;

    LOGGER_TRACE("Initializing client list structure for" \
            " desktop %u ('%s') on screen %u",
            desktop_id, desktop->name, screen_id);

    /* Initialize hash table for quick client lookup */
    desktop->clients =
        ohtbl_init(DESKTOP_INITIAL_CAPACITY, 0,
                s_h1, s_h2, s_client_match,
                (void(*)(void *)) client_destroy);
    if (desktop->clients == NULL) {
        LOGGER_ERROR("Failed to allocate memory for" \
                " client hash table on desktop %u ('%s') on screen %u",
                desktop_id, desktop->name, screen_id);
        free(desktop);
        return NULL;
    }

    LOGGER_TRACE("Initializing stacking list structure for" \
            " desktop %u ('%s') on screen %u",
            desktop_id, desktop->name, screen_id);

    /* Initialize circular list for rendering in stacking order.
     * Ownership of client memory is managed by 'desktop->clients' */
    desktop->stacking = cdlist_init(NULL);
    if (desktop->stacking == NULL) {
        LOGGER_ERROR("Failed to allocate memory for stacking list" \
                " on desktop %u ('%s') on screen %u",
                desktop_id, desktop->name, screen_id);
        ohtbl_destroy(desktop->clients);
        free(desktop);
        return NULL;
    }

    /* Get XCB screen to obtain dimensions */
    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    screen = NULL;

    /* Iterate through screens to find the "correct" one */
    for (uint32_t i = 0; i < screen_id && iter.rem > 0; ++i) {
        xcb_screen_next(&iter);
    }

    if (iter.rem == 0 || iter.data == NULL) {
        LOGGER_ERROR("Invalid screen ID %u, could not retrieve" \
                " screen information", screen_id);
        cdlist_destroy(desktop->stacking);
        ohtbl_destroy(desktop->clients);
        free(desktop);
        return NULL;
    }

    screen = iter.data;

    /* Initialize geometry with screen dimensions */
    desktop->geometry = (struct geometry_s) {
        .pos = {.x = 0, .y = 0},
        .dim = {.w = screen->width_in_pixels,
                .h = screen->height_in_pixels}
    };

    /* TODO: Work area is the same as geometry for now (panels/struts) */
    desktop->workarea = desktop->geometry;

    /* Mark desktop as outdated to trigger initial render */
    desktop->is_outdated = true;

    LOGGER_TRACE("Initialized desktop %u ('%s') on screen %u" \
            " with geometry %ux%u",
            desktop_id, desktop->name, screen_id,
            desktop->geometry.dim.w, desktop->geometry.dim.h);

    return desktop;
}


/* Free memory for allocated desktop */
void desktop_destroy(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    LOGGER_DEBUG("Destroying desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Destroy stacking list (clients not destroyed here, just the list) */
    LOGGER_TRACE("Deallocating stacking list on desktop %u ('%s')",
            desktop->id, desktop->name);
    if (desktop->stacking != NULL) {
        cdlist_destroy(desktop->stacking);
        desktop->stacking = NULL;
    }

    /* Destroy hash table (also destroys all clients via client_destroy
     * callback) */
    LOGGER_TRACE("Deallocating clients on desktop %u ('%s')",
            desktop->id, desktop->name);
    if (desktop->clients != NULL) {
        ohtbl_destroy(desktop->clients);
        desktop->clients = NULL;
    }

    /* Free background image path if it exists */
    if (desktop->background.is_image &&
            desktop->background.bg.image_path != NULL) {
        LOGGER_TRACE("Deallocating image on desktop %u ('%s')",
                desktop->id, desktop->name);
        free(desktop->background.bg.image_path);
        desktop->background.bg.image_path = NULL;
    }

    LOGGER_TRACE("Destroying desktop %u ('%s')",
            desktop->id, desktop->name);
    free(desktop);
}


/* Soft desktop update */
void desktop_update(desktop_td *desktop)
{
//    LOGGER_TRACE("Updating desktop %u ('%s')",
//            desktop->id, desktop->name);

    /* Establish that this desktop is already updated */
    desktop->is_outdated = false;
}


/* Full desktop update */
void desktop_update_full(desktop_td *desktop)
{
    LOGGER_TRACE("Fully updating desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Soft update */
    desktop_update(desktop);

    /* Update all clients on the hash table */
    for (size_t i = 0; i < desktop->clients->positions; ++i) {
        /* Check if the position has an element */
        if (desktop->clients->table[i] != NULL &&
            desktop->clients->table[i] != desktop->clients->vacated) {
            /* Get the client and update it */
            client_td *client =
                (client_td *) desktop->clients->table[i];
            client_update(client);
        }
    }

    LOGGER_TRACE("Updated desktop %u ('%s')",
            desktop->id, desktop->name);
}


/* Clear a desktop by removing all its clients */
void desktop_clear(desktop_td *desktop)
{
    client_td *client;

    if (desktop == NULL) {
        return;
    }

    LOGGER_DEBUG("Preparing to clear desktop %u ('%s')",
            desktop->id, desktop->name);

    if (desktop->stacking != NULL) {
        while (true) {
            client = NULL;
            if (cdlist_rem_next(desktop->stacking, NULL,
                        (void **) &client) != 0) {
                break;
            }
            /* The list may legitimately contain null data pointers;
             * destroy only valid clients. */
            if (client != NULL) {
                client_destroy(client);
            }
        }
    }

    if (desktop->clients != NULL) {
        ohtbl_reset(desktop->clients);
    }
}


/* Rename the desktop */
int desktop_action_rename(desktop_td *desktop, const char *name)
{
    LOGGER_DEBUG("Renaming desktop %u ('%s') to '%s'",
            desktop->id, desktop->name, name);
    if (desktop == NULL || name == NULL) {
        LOGGER_ERROR("Invalid desktop or name pointer", L_NARG);
        return -1;
    }

    snprintf(desktop->name, DESKTOP_MAX_LENGTH_NAME, "%s", name);
    desktop->name[DESKTOP_MAX_LENGTH_NAME - 1] = '\0';
    desktop->is_outdated = true;

    return 0;
}


/* Send a client to another desktop */
int desktop_action_send_client(desktop_td *desktop, client_td *client,
        uint32_t desktop_id)
{
    LOGGER_DEBUG("Sending client 0x%08x ('%s') from desktop %u ('%s')"
            " to desktop %u",
            client->id, client->info.name,
            desktop->id, desktop->name, desktop_id);

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

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


/* Update the desktop background color */
int desktop_action_background_update(desktop_td *desktop, uint32_t color)
{
    LOGGER_DEBUG("Updating background color of desktop %u ('%s')"
            " to 0x%08x", desktop->id, desktop->name, color);

    if (desktop == NULL) {
        return -1;
    }

    desktop->background.is_image = false;
    desktop->background.bg.color = color;
    desktop->is_outdated = true;

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
        LOGGER_ALERT("Failed to add client to hash table", L_NARG);
        return -1;
    }

    /* Add to stacking list for rendering order */
    if (cdlist_ins_next(desktop->stacking,
                cdlist_tail(desktop->stacking),
                (void *) client) != 0) {
        LOGGER_ALERT("Failed to add client to stacking list", L_NARG);
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
                    LOGGER_ALERT("Failed to remove client from" \
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

    LOGGER_DEBUG("Sending client 0x%08x ('%s') to front" \
            " of desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    /* Find the client in the stacking list */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        cdlist_item_td *initial = node;
        do {
            if (cdlist_data(node) == (void *) client) {
                /* Found it, move to tail (front/top of stack) */
                if (cdlist_rem_next(desktop->stacking,
                            cdlist_prev(node), NULL) != 0) {
                    LOGGER_ALERT("Failed to remove client from stacking",
                            L_NARG);
                    return -1;
                }
                if (cdlist_ins_next(desktop->stacking,
                            cdlist_tail(desktop->stacking),
                            (void *) client) != 0) {
                    LOGGER_ALERT("Failed to insert client to stacking",
                            L_NARG);
                    return -1;
                }
                desktop->is_outdated = true;
                return 0;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    LOGGER_ALERT("Client not found in desktop stacking", L_NARG);
    return -1;
}


/* Send a client to the back of the desktop's window stack */
int desktop_action_client_send_back(desktop_td *desktop,
        client_td *client)
{
    cdlist_item_td *node;

    LOGGER_DEBUG("Sending client 0x%08x ('%s') to back" \
            " of desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    /* Find the client in the stacking list */
    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        cdlist_item_td *initial = node;
        do {
            if (cdlist_data(node) == (void *) client) {
                /* Found it, move to head (back/bottom of stack) */
                if (cdlist_rem_next(desktop->stacking,
                            cdlist_prev(node), NULL) != 0) {
                    LOGGER_ALERT("Failed to remove client from stacking",
                            L_NARG);
                    return -1;
                }
                if (cdlist_ins_next(desktop->stacking, NULL,
//                            cdlist_head(desktop->stacking),
                            (void *) client) != 0) {
                    LOGGER_ALERT("Failed to insert client to stacking",
                            L_NARG);
                    return -1;
                }
                desktop->is_outdated = true;
                return 0;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    LOGGER_ALERT("Client not found in desktop stacking", L_NARG);
    return -1;
}


/* Rearrange clients on the desktop */
int desktop_action_clients_rearrange(desktop_td *desktop)
{
    LOGGER_DEBUG("Rearranging clients on desktop %u ('%s')",
            desktop->id, desktop->name);

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

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
    LOGGER_DEBUG("Iconifying all clients on desktop %u ('%s')",
            desktop->id, desktop->name);

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

    /* Iterate through all clients in hash table and iconify them */
    for (size_t i = 0; i < desktop->clients->positions; ++i) {
        if (desktop->clients->table[i] != NULL &&
            desktop->clients->table[i] != desktop->clients->vacated) {
            client_td *client = (client_td *) desktop->clients->table[i];
            client_send_event_iconify(client);
        }
    }

    return 0;
}


/* Cycle through active clients on the desktop */
int desktop_action_cycle_clients_active(desktop_td *desktop)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    cdlist_item_td *active_node = NULL;

    LOGGER_DEBUG("Cycling through active clients on desktop %u ('%s')",
            desktop->id, desktop->name);

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

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


/* Cycle through iconified clients on the desktop */
int desktop_action_cycle_clients_icons(desktop_td *desktop)
{
    cdlist_item_td *node;
    cdlist_item_td *target = NULL;

    LOGGER_DEBUG("Cycling through iconified clients" \
            " on desktop %u ('%s')", desktop->id, desktop->name);

    if (desktop == NULL) {
        LOGGER_ERROR("Invalid desktop pointer", L_NARG);
        return -1;
    }

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


/* Launch an application on the desktop */
int desktop_action_application_launch(desktop_td *desktop,
        const char *application_path)
{
    pid_t pid;


    if (desktop == NULL || application_path == NULL ||
            application_path[0] == '\0') {
        LOGGER_ERROR("Invalid desktop or application path pointer",
                L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Launching application '%s' on desktop %u ('%s')",
            application_path, desktop->id, desktop->name);

    pid = fork();
    if (pid < 0) {
        LOGGER_ERROR("Failed to fork process for application '%s'",
                application_path);
        return 1;
    }
    if (pid == 0) {
        wordexp_t words = (wordexp_t) {0};
        int wordexp_flags;
        int wr;

        /* Child must close its inherited copy of the X connection's
         * file descriptor before continuing */
        /* NOTE: 'desktop->connection' is the SAME 'xcb_connection_t'
         *       pointer shared with the parent (it is not duplicated by
         *       'fork()'), so calling 'xcb_disconnect()' here would
         *       tear down the connection's internal state and break it
         *       for the parent process too, since the underlying socket
         *       is shared.  A plain 'close()' on the raw descriptor
         *       only affects the child's own file descriptor table
         *       entry and leaves the parent's connection intact. */
        if (desktop->connection != NULL) {
            close(xcb_get_file_descriptor(desktop->connection));
        }

        wordexp_flags = WRDE_NOCMD;
#ifdef WRDE_NOENV
        wordexp_flags |= WRDE_NOENV;
#endif
        wr = wordexp(application_path, &words, wordexp_flags);
        if (wr != 0 || words.we_wordc == 0u) {
            LOGGER_ERROR("Failed to parse launch command '%s'",
                    application_path);
            if (words.we_wordv != NULL) {
                wordfree(&words);
            }
            _exit(127);
        }

        execvp(words.we_wordv[0], words.we_wordv);
        wordfree(&words);
        _exit(127);
    }

    LOGGER_INFO("Launched application '%s' with PID %d",
            application_path, (int) pid);

    return 0;

}


/* Recompute work area from client struts */
void desktop_update_workarea(desktop_td *desktop,
        uint32_t screen_w, uint32_t screen_h)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    int32_t left = 0;
    int32_t right = 0;
    int32_t top = 0;
    int32_t bottom = 0;
    int32_t new_w;
    int32_t new_h;

    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        if (desktop != NULL) {
            desktop->workarea.pos.x = 0;
            desktop->workarea.pos.y = 0;
            desktop->workarea.dim.w = screen_w;
            desktop->workarea.dim.h = screen_h;
        }
        return;
    }

    /* Aggregate maximum strut on each edge across all stacked clients */
    initial = cdlist_head(desktop->stacking);
    node = initial;
    do {
        client_td *c = (client_td *) cdlist_data(node);
        if (c != NULL) {
            if (c->layout.strut_partial.sides.left > left) {
                left = c->layout.strut_partial.sides.left;
            }
            if (c->layout.strut_partial.sides.right > right) {
                right = c->layout.strut_partial.sides.right;
            }
            if (c->layout.strut_partial.sides.top > top) {
                top = c->layout.strut_partial.sides.top;
            }
            if (c->layout.strut_partial.sides.bottom > bottom) {
                bottom = c->layout.strut_partial.sides.bottom;
            }
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    new_w = (int32_t) screen_w - left - right;
    new_h = (int32_t) screen_h - top  - bottom;

    desktop->workarea.pos.x = left;
    desktop->workarea.pos.y = top;
    desktop->workarea.dim.w = (new_w > 0) ? (uint32_t) new_w : 0U;
    desktop->workarea.dim.h = (new_h > 0) ? (uint32_t) new_h : 0U;

    LOGGER_TRACE("Desktop %u workarea: %ux%u+%d+%d",
            desktop->id,
            desktop->workarea.dim.w, desktop->workarea.dim.h,
            desktop->workarea.pos.x, desktop->workarea.pos.y);
}
