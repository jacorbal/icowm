/**
 * @file desktop.c
 *
 * @brief Desktop handling implementation
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>     /* uint32_t */
#include <stdlib.h>     /* NULL, free, malloc, rand */
#include <time.h>       /* time */
#include <unistd.h>     /* getpid */

/* XCB includes */
#include <xcb/xcb.h>

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
desktop_td *desktop_init(uint32_t screen_id, uint32_t desktop_id,
        struct config_base_s *config_base,
        struct config_theme_s *config_theme)
{
    desktop_td *desktop;

    LOGGER_DEBUG("Initializing desktop %u on screen %u",
            desktop_id, screen_id);
    desktop = malloc(sizeof(desktop_td));
    if (desktop == NULL) {
        LOGGER_ERROR("Failed to allocate memory for" \
                "desktop %u on screen %u", desktop_id, screen_id);
        return NULL;
    }

    /* Stablish the basics */
    desktop->screen_id = screen_id;
    desktop->id = desktop_id;
    desktop->client_active_id = 0;

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
    /* TODO: use image instead of color, and 'image_path' */
    desktop->background.is_image = false;
    desktop->background.bg.color =
        config_base->screens[screen_id].desktops[desktop_id].settings.background.color;

    LOGGER_TRACE("Initializing client list structure for" \
            " desktop %u ('%s') on screen %u",
            desktop_id, desktop->name, screen_id);
    desktop->clients =
        ohtbl_init(DESKTOP_INITIAL_CAPACITY, 0,
                s_h1, s_h2, s_client_match,
                (void(*)(void *)) client_destroy);
    if (desktop->clients == NULL) {
        LOGGER_ERROR("Failed to allocate memory for client hash table" \
                " on desktop %u ('%s') on screen %u",
                desktop_id, desktop->name, screen_id);
        free(desktop);
        return NULL;
    }

    /* Needs too be updated */
    desktop->is_outdated = true;

    return desktop;
}


/* Free memory for allocated desktop */
void desktop_destroy(desktop_td *desktop)
{
    LOGGER_DEBUG("Deallocating structure for desktop %u ('%s')",
            desktop->id, desktop->name);
    if (desktop == NULL) {
        return;
    }

    LOGGER_TRACE("Deallocating clients on desktop %u ('%s')",
            desktop->id, desktop->name);
    ohtbl_destroy(desktop->clients);
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
    LOGGER_DEBUG("Preparing to add client %#x ('%s') to" \
            " desktop %u ('%s')",
            client->id, client->names.name, desktop->id, desktop->name);

    if (desktop == NULL || client == NULL) {
        return -1;
    }

    /* Add client */
    if (ohtbl_insert(desktop->clients, (void *) client) != 0) {
        LOGGER_ALERT("Failed to allocate memory for client" \
                " %#x on desktop %u ('%s') on screen %u",
                client->id,
                desktop->id, desktop->name, desktop->screen_id);
        return -1;
    }

    LOGGER_TRACE("Added client %#x ('%s') to desktop %u ('%s')",
            client->id, client->names.name, desktop->id, desktop->name);
    return 0;
}


/* Remove a client from the desktop */
int desktop_action_client_rem(desktop_td *desktop, client_td *client)
{
    void *removed_client = NULL;

    LOGGER_DEBUG("Preparing to remove client %#x ('%s') from" \
            " desktop %u ('%s')",
            client->id, client->names.name, desktop->id, desktop->name);

    if (desktop == NULL || client == NULL) {
        return -1;
    }

    /* Search client in list of clients */
    if (ohtbl_remove(desktop->clients,
                (void **) &removed_client) == 0) {
        client_td *removed_client_td = (client_td *) removed_client;
        if (removed_client_td != NULL &&
            removed_client_td->id == client->id) {
            /* Remove client */
            client_destroy(removed_client_td);
            LOGGER_TRACE("Removing client %#x ('%s') from" \
                    " desktop %u ('%s')",
                    client->id, client->names.name,
                    desktop->id, desktop->name);
            return 0;
        }
    }

    /* Window not found */
    LOGGER_TRACE("Window %#x ('%s') not found on desktop %u ('%s')",
            client->id, client->names.name, desktop->id, desktop->name);
    return -1;
}
