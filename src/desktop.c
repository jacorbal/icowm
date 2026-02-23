/**
 * @file desktop.c
 *
 * @brief Desktop handling implementation
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* NULL, free, malloc, rand */
#include <time.h>       /* time */
#include <unistd.h>     /* getpid */

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


/* Define a hash function with a random seed */
static size_t s_h1(const void *data)
{
    uint32_t seed;
    const client_td *client = (const client_td *) data;

    seed = (uint32_t) (time(NULL) ^ getpid() ^ rand());
    return (size_t) murmurhash3_32(client, sizeof(client_td), seed);
}


/* Define an auxiliary hash function with a random seed */
static size_t s_h2(const void *data)
{
    uint32_t seed;
    const client_td *client = (const client_td *) data;

    seed = (uint32_t) (time(NULL) ^ (getpid() << 16) ^ rand());
    return (size_t) murmurhash3_32(client, sizeof(client_td), seed);
}


/* Members of the hash table (clients) match if they have equal key
 * value (identifier) */
static bool s_client_match(const void *key1, const void *key2)
{
    const client_td *client1 = (const client_td *) key1;
    const client_td *client2 = (const client_td *) key2;

    return client1->id == client2->id;
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
                " desktop %u on screen %u",
                desktop_id, screen_id);
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

    /* Set desktop name */
    if (config_base->screens[screen_id].desktops[desktop_id].name[0] == '\0') {
        snprintf(desktop->name, DESKTOP_MAX_LENGTH_NAME - 1,
                "Desktop %u", desktop_id);
    } else {
        snprintf(desktop->name, DESKTOP_MAX_LENGTH_NAME - 1,
                config_base->screens[screen_id].desktops[desktop_id].name,
                DESKTOP_MAX_LENGTH_NAME - 1);
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

    /* Initialize circular list for rendering in stacking order */
//    desktop->stacking = cdlist_init(NULL);  /* FIXME: 'NULL' destroy */
    desktop->stacking = cdlist_init((void(*)(void *)) client_destroy);
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

    LOGGER_TRACE("Desktop %u ('%s') on screen %u initialized" \
            " successfully with geometry %ux%u",
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
    LOGGER_DEBUG("Preparing to clear desktop %u ('%s')",
            desktop->id, desktop->name);
    if (desktop != NULL && desktop->clients != NULL) {
        ohtbl_destroy(desktop->clients);
    }
}


/* Add a previously allocated client in the desktop */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    LOGGER_DEBUG("Adding client 0x%08x ('%s') to desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

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
        ohtbl_remove(desktop->clients, (void *) client);
        return -1;
    }

    LOGGER_TRACE("Successfully added client 0x%08x to desktop %u ('%s')",
            client->id, desktop->id, desktop->name);
    desktop->is_outdated = true;  /* Mark for redraw */

    return 0;
}


/* Remove a client from the desktop */
int desktop_action_client_rem(desktop_td *desktop, client_td *client)
{
    cdlist_item_td *node;

    LOGGER_DEBUG("Removing client 0x%08x ('%s') from desktop %u ('%s')",
            client->id, client->info.name, desktop->id, desktop->name);

    if (desktop == NULL || client == NULL) {
        LOGGER_ERROR("Invalid desktop or client pointer", L_NARG);
        return -1;
    }

    /* Remove from hash table */
    if (ohtbl_remove(desktop->clients, (void *) client) != 0) {
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

    LOGGER_TRACE("Successfully removed client 0x%08x from desktop %u",
            client->id, desktop->id);
    desktop->is_outdated = true;  /* Mark for redraw */

    return 0;
}
