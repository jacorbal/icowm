/**
 * @file utils/xcb/atom.c
 *
 * @brief X atom lookup by name, and a couple of small property
 *        writes shared across more than one call site
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
#include <stddef.h>     /* size_t */
#include <stdlib.h>     /* NULL, free */
#include <string.h>     /* memcpy, strcmp */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <utils/xcb/atom.h>


/**
 * @brief Longest atom name @a atom_intern's own cache stores in full
 *
 * The longest name any call site in this project actually passes today
 * (@c _NET_WM_STATE_DEMANDS_ATTENTION) is 32 bytes including the
 * terminator; this leaves comfortable headroom for a longer one added
 * later without needing to revisit this constant.  A name longer than
 * this is simply never cached (see @a s_atom_cache_find), which only
 * costs that one call site the speedup, not correctness.
 */
#define ATOM_CACHE_NAME_MAX_LENGTH (64)

/**
 * @brief How many distinct (name, only_if_exists) pairs @a atom_intern's
 *        own cache holds at once
 *
 * A window manager only ever interns a fixed, small set of well-known
 * EWMH/ICCCM atom names over its own lifetime, entirely independent of
 * how many client windows or desktops it manages; this project's own
 * call sites currently name around thirty distinct ones between them.
 * Sized well above that so ordinary use never fills the cache, since a
 * full cache does not overflow (see @a s_atom_cache_find), it just
 * stops caching further distinct names.
 */
#define ATOM_CACHE_CAPACITY (64)

/**
 * @brief One cached @a atom_intern result
 */
struct s_atom_cache_entry_s {
    char name[ATOM_CACHE_NAME_MAX_LENGTH];  /**< Interned atom's name */
    bool only_if_exists;   /**< @a atom_intern's own request flavor */
    xcb_atom_t atom;        /**< The resolved atom */
};

/* Every atom looked up so far, in first-seen order; only entries
 * [0, s_atom_cache_count) are valid */
static struct s_atom_cache_entry_s s_atom_cache[ATOM_CACHE_CAPACITY];

/* How many entries of 's_atom_cache' are currently in use */
static size_t s_atom_cache_count = 0u;


/**
 * @brief Look up a previously interned atom in @a atom_intern's own
 *        cache
 *
 * A linear scan, not a hash table: the cache holds only a few dozen
 * entries at most (see @c ATOM_CACHE_CAPACITY's own doc comment), so
 * scanning every one of them is already effectively free next to the
 * X server round trip it exists to avoid.
 *
 * @param name           Atom name to look up
 * @param only_if_exists Must match the flavor the entry was originally
 *                        cached under; see @a atom_intern's own doc
 *                        comment for why the two are never conflated
 *
 * @return The cached atom, or @c XCB_ATOM_NONE if not yet cached
 *
 * @note Complexity: @e O(n), where @e n is the number of atoms cached
 *       so far, bounded by @c ATOM_CACHE_CAPACITY
 */
static xcb_atom_t s_atom_cache_find(const char *name, bool only_if_exists)
{
    for (size_t i = 0u; i < s_atom_cache_count; ++i) {
        if (s_atom_cache[i].only_if_exists == only_if_exists &&
                strcmp(s_atom_cache[i].name, name) == 0) {
            return s_atom_cache[i].atom;
        }
    }

    return XCB_ATOM_NONE;
}


/**
 * @brief Add a freshly resolved atom to @a atom_intern's own cache
 *
 * A no-op, rather than an error, if @p name does not fit in
 * @c ATOM_CACHE_NAME_MAX_LENGTH or the cache is already at
 * @c ATOM_CACHE_CAPACITY: either way, the caller already has its own
 * correct @p atom to use right now, and every future call for this
 * same name simply pays for another round trip instead of a cached
 * hit, exactly as @a atom_intern behaved everywhere before this cache
 * existed.
 *
 * @param name           Atom name just resolved
 * @param only_if_exists The flavor @p atom was resolved under
 * @param atom           The resolved atom; never @c XCB_ATOM_NONE, see
 *                        @a atom_intern's own caller of this function
 *
 * @note Complexity: @e O(1)
 */
static void s_atom_cache_add(const char *name, bool only_if_exists,
        xcb_atom_t atom)
{
    struct s_atom_cache_entry_s *slot;

    if (s_atom_cache_count >= ATOM_CACHE_CAPACITY ||
            safe_strlen(name) >= ATOM_CACHE_NAME_MAX_LENGTH) {
        return;
    }

    slot = &s_atom_cache[s_atom_cache_count];
    safe_strncpy(slot->name, name, sizeof(slot->name));
    slot->only_if_exists = only_if_exists;
    slot->atom = atom;
    ++s_atom_cache_count;
}


/* Intern an X atom by its string name */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom;

    if (connection == NULL || name == NULL) {
        return XCB_ATOM_NONE;
    }

    atom = s_atom_cache_find(name, only_if_exists);
    if (atom != XCB_ATOM_NONE) {
        return atom;
    }

    atom = XCB_ATOM_NONE;
    reply = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, (uint8_t) only_if_exists,
                (uint16_t) safe_strlen(name), name),
            NULL);
    if (reply != NULL) {
        atom = reply->atom;
        free(reply);
    }

    /* A 'not found' result (only reachable with only_if_exists true)
     * is deliberately never cached: unlike every name this cache
     * actually targets, which are well-known EWMH/ICCCM atoms already
     * in use from very early in any X session, a name that does not
     * exist yet could still be interned by some other client later,
     * and caching 'still absent' here would keep returning it forever
     * regardless. */
    if (atom != XCB_ATOM_NONE) {
        s_atom_cache_add(name, only_if_exists, atom);
    }

    return atom;
}


/* Resolve an X atom to its string name */
bool atom_name(xcb_connection_t *connection, xcb_atom_t atom,
        char *out_name, size_t out_name_size)
{
    xcb_get_atom_name_reply_t *reply;
    int name_len;
    size_t copy_len;

    if (connection == NULL || out_name == NULL || out_name_size == 0u) {
        return false;
    }
    out_name[0] = '\0';

    if (atom == XCB_ATOM_NONE) {
        return false;
    }

    reply = xcb_get_atom_name_reply(connection,
            xcb_get_atom_name(connection, atom), NULL);
    if (reply == NULL) {
        return false;
    }

    name_len = xcb_get_atom_name_name_length(reply);
    if (name_len < 0) {
        name_len = 0;
    }
    copy_len = ((size_t) name_len < out_name_size - 1u)
        ? (size_t) name_len : out_name_size - 1u;
    memcpy(out_name, xcb_get_atom_name_name(reply), copy_len);
    out_name[copy_len] = '\0';
    free(reply);

    return true;
}


/* Publish '_NET_WM_WINDOW_OPACITY' on a window */
void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw)
{
    xcb_atom_t opacity_atom;

    if (connection == NULL || window == XCB_WINDOW_NONE) {
        return;
    }

    opacity_atom = atom_intern(connection, "_NET_WM_WINDOW_OPACITY",
            false);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window,
            opacity_atom, XCB_ATOM_CARDINAL, 32, 1, &raw);
}
