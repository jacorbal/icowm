/**
 * @file desktop.c
 *
 * @brief Desktop handling implementation
 */

/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stdint.h>     /* uint32_t, uint64_t */
#include <stdlib.h>     /* NULL, free, malloc, rand */
#include <time.h>       /* time */
#include <unistd.h>     /* getpid */

/* External libraries */
//#include <X11/Xlib.h>

/* ADT */
#include <adt/ohtbl.h>  /* Open-addressed hash table (closed hasing) */

/* Project includes */
#include <logger.h>
#include <window.h>

/* Local includes */
#include <desktop.h>


/*
 * @brief Computes the 32-bit MurmurHash3 hash of a given input key
 *
 * This function implements the MurmurHash3 algorithm, which is a fast,
 * non-cryptographic hash function suitable for general hash-based
 * lookup tasks.  It generates a 32-bit hash value based on the input
 * key and an optional seed value.
 *
 * @param key  Pointer to the data to be hashed
 * @param len  Length of the input data in bytes.
 * @param seed Seed value used to initialize the hash calculation
 *
 * @return Computed 32-bit hash value as an unsigned integer
 *
 * @note Parameter @p len should be the size of the data pointed by the
 *       @p key parameter.
 * @note Parameter @p seed can be used to produce different hash results
 *       for the same input key
 */
static uint32_t _murmurhash3_32(const void *key, int len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t*) key;
    const int nblocks = len / 4;

    uint32_t h = seed;
    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    /* Process groups of 4 bytes */
    for (int i = 0; i < nblocks; ++i) {
        uint32_t k = *(uint32_t *) (data + i * 4);
        k *= c1;
        k = (k << 15) | (k >> (32 - 15));           /* ROTL32 */
        k *= c2;

        h ^= k;
        h = (h << 13) | (h >> (32 - 13));           /* ROTL32 */
        h = h * 5 + 0xe6546b64;
    }

    /* Process the rest */
    const uint8_t *tail = (const uint8_t *) (data + nblocks * 4);
    uint32_t k = 0;
    switch (len & 3) {
        case 3:
            k ^= tail[2] << 16;
            /* fall through */
        case 2:
            k ^= tail[1] << 8;
            /* fall through */
        case 1: k ^= tail[0];
                k *= c1;
                k = (k << 15) | (k >> (32 - 15));   /* ROTL32 */
                k *= c2;
                h ^= k;
    }

    /* Finish the hash */
    h ^= (uint32_t) len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}


/* Define a hash function */
static size_t _h1(const void *data)
{
    uint32_t seed;
    const window_td *window = (const window_td *) data;

    seed = (uint32_t) (time(NULL) ^ getpid() ^ rand());
    return (size_t) _murmurhash3_32(window, sizeof(window_td), seed);
}


/* Define an auxiliary hash function */
static size_t _h2(const void *data)
{
    uint32_t seed;
    const window_td *window = (const window_td *) data;

    seed = (uint32_t) (time(NULL) ^ (getpid() << 16) ^ rand());
    return (size_t) _murmurhash3_32(window, sizeof(window_td), seed);
}


/* Members of the hash table match if they have equal key value */
static bool _match(const void *key1, const void *key2)
{
    const window_td *window1 = (const window_td *) key1;
    const window_td *window2 = (const window_td *) key2;
    return window1->id == window2->id;
}


/* Initialize a new desktop */
desktop_td *desktop_init(unsigned int screen_id,
        unsigned int desktop_id,
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

    desktop->screen_id = screen_id;
    desktop->id = desktop_id;
    desktop->window_active = NULL;
    desktop->config_base = config_base;
    desktop->config_theme = config_theme;

    desktop->background.bg.color =
        config_base->screens[screen_id].desktops[desktop_id].settings.background.color;

    LOGGER_TRACE("Initializing window list structure for" \
            " desktop %u on screen %u", desktop_id, screen_id);
    desktop->windows =
        ohtbl_init(DESKTOP_INITIAL_CAPACITY, _h1, _h2, _match,
                (void(*)(void *)) window_destroy);
    if (desktop->windows == NULL) {
        LOGGER_ERROR("Failed to allocate memory for window hash table" \
                " on desktop %u on screen %u", desktop_id, screen_id);
        free(desktop);
        return NULL;
    }

    desktop->is_outdated = true;

    return desktop;
}


/* Free memory for allocated desktop */
void desktop_destroy(desktop_td *desktop)
{
    LOGGER_DEBUG("Deallocating structure for desktop %u", desktop->id);
    if (desktop != NULL) {
        LOGGER_TRACE("Deallocating windows on desktop %u", desktop->id);
        ohtbl_destroy(desktop->windows);
        LOGGER_TRACE("Destroying desktop %u", desktop->id);
        free(desktop);
    }
}

/* Soft desktop update */
void desktop_update(desktop_td *desktop)
{
    LOGGER_TRACE("Updating desktop %u", desktop->id);


    /* Establish that this desktop is already updated */
    desktop->is_outdated = false;
}


/* Full desktop update */
void desktop_update_full(desktop_td *desktop)
{
    LOGGER_TRACE("Fully updating desktop %u", desktop->id);

    /* Soft update */
    desktop_update(desktop);

    /* Update all windows on the hash table */
    for (size_t i = 0; i < desktop->windows->positions; ++i) {
        /* Check if the position has an element */
        if (desktop->windows->table[i] != NULL &&
            desktop->windows->table[i] != desktop->windows->vacated) {
            /* Get the window and update it */
            window_td *window =
                (window_td *) desktop->windows->table[i];
            window_update(window);
        }
    }
}


/* Clear a desktop by removing all its windows */
void desktop_clear(desktop_td *desktop)
{
    LOGGER_DEBUG("Preparing to clear desktop %u", desktop->id);
    if (desktop != NULL && desktop->windows != NULL) {
        ohtbl_destroy(desktop->windows);
    }
}


/* Add a previously allocated window in the desktop */
int desktop_window_add(desktop_td *desktop, window_td *window)
{
    LOGGER_DEBUG("Preparing to add window %#lx to desktop %u",
            window->id, desktop->id);

    if (desktop == NULL || window == NULL) {
        return -1;
    }

    if (ohtbl_insert(desktop->windows, (void *) window) != 0) {
        LOGGER_ALERT("Failed to allocate memory for window" \
                " %#lx on desktop %u on screen %u",
                window->id, desktop->id, desktop->screen_id);
        return -1;
    }

    LOGGER_TRACE("Added window %#lx to desktop %u",
            window->id, desktop->id);

    return 0;
}

/* Remove a window from the desktop */
int desktop_window_rem(desktop_td *desktop, window_td *window)
{
    LOGGER_DEBUG("Preparing to remove window %#lx from desktop %u",
            window->id, desktop->id);

    if (desktop == NULL || window == NULL) {
        return -1;
    }

    /* Search window in list of windows */
    void *removed_window = NULL;
    if (ohtbl_remove(desktop->windows,
                (void **) &removed_window) == 0) {
        window_td *removed_window_td = (window_td *) removed_window;
        if (removed_window_td != NULL &&
            removed_window_td->id == window->id) {
            window_destroy(removed_window_td);
            LOGGER_TRACE("Removing window %#lx from desktop %u",
                    window->id, desktop->id);
            return 0;
        }
    }

    /* Window not found */
    LOGGER_TRACE("Window %#lx not found on desktop %u",
            window->id, desktop->id);
    return -1;
}
