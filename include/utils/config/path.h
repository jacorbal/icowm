/**
 * @file utils/config/path.h
 *
 * @brief Declarations of path handling functions
 *
 * @ingroup utils_config
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_CONFIG_PATH
#define UTILS_CONFIG_PATH


/* System includes */
#include <stddef.h>     /* size_t */


/**
 * @brief One user-specific base directory defined by the XDG Base
 *        Directory Specification
 *
 * Covers only the five "_HOME"-style variables, each of which
 * resolves to a single directory to write into
 * (@c XDG_CONFIG_HOME, @c XDG_DATA_HOME, @c XDG_STATE_HOME,
 * @c XDG_CACHE_HOME, @c XDG_RUNTIME_DIR).  Deliberately excludes the
 * "_DIRS"-style variables (@c XDG_CONFIG_DIRS, @c XDG_DATA_DIRS):
 * those are colon-separated search lists for locating an existing
 * file across several system-wide directories, a different shape of
 * problem ("find a file among these") than this enum's own ("resolve
 * the one directory of mine to write into"), so they do not belong
 * as another case of the same function.
 */
enum xdg_dir_kind_e {
    XDG_DIR_CONFIG,   /**< XDG_CONFIG_HOME; user-specific config */
    XDG_DIR_DATA,     /**< XDG_DATA_HOME; user-specific data files */
    XDG_DIR_STATE,    /**< XDG_STATE_HOME; state meant to persist
                            across restarts (history, recent files) */
    XDG_DIR_CACHE,    /**< XDG_CACHE_HOME; regenerable, non-essential
                            data */
    XDG_DIR_RUNTIME   /**< XDG_RUNTIME_DIR; ephemeral runtime objects
                            (sockets, named pipes); see this file's
                            own @c xdg_resolve_dir doc comment for why
                            this one kind has no @c $HOME fallback
                            tier the other four do */
};


/**
 * @brief Normalize a given file path by removing unnecessary components
 *
 * Processes the input path and simplifies it by:
 *   - removing consecutive slashes;
 *   - ignoring the current directory indicators ('./');
 *   - resolving the parent directory indicators ('../'), popping
 *     the previous real component when there is one to cancel
 *     against, keeping '..' itself literally when there is not (a
 *     relative path with nothing behind it, or a previous component
 *     that is itself an unresolved '..'), and dropping it outright
 *     for an absolute path already at its own root.
 *
 * @param path Pointer to the input path string to be normalized
 *
 * @note The function modifies the path in place
 * @note A relative path that resolves down to nothing at all (e.g.,
 *       @c "a/../") becomes @c "." (the current directory), never
 *       an empty string
 * @note Only matches a @c '..' component immediately followed by a
 *       @c '/' (i.e., followed by more path, not at the very end of
 *       @p path with nothing after it): @c "a/.." is left
 *       unresolved, unlike @c "a/../" or @c "a/../b"
 * @note Complexity: @e O(n), where @e n is the length of the input path
 */
void path_simplify(char *restrict path);

/**
 * @brief Resolve one of IcoWM's own XDG base directories
 *
 * Tries, in order:
 *   -# The environment variable the XDG specification defines for
 *      @p kind (e.g., @c XDG_CONFIG_HOME for @c XDG_DIR_CONFIG), if
 *      set and non-empty: @c "${that}/icowm".
 *   -# For every kind except @c XDG_DIR_RUNTIME, @c $HOME, if set
 *      and non-empty, joined with that kind's own established
 *      relative path.  @c XDG_DIR_CONFIG's own is the flat, dotted
 *      @c "${HOME}/.icowm", the pre-XDG convention this project
 *      already shipped and must keep resolving unchanged; the
 *      other three follow the spec's own documented default
 *      location instead, e.g., @c "${HOME}/.local/share/icowm" for
 *      @c XDG_DIR_DATA, since neither has any pre-existing
 *      behavior of its own to preserve.  @c XDG_DIR_RUNTIME skips
 *      this tier entirely: an ordinary @c $HOME directory offers
 *      none of the guarantees (mode @c 0700, cleared on logout)
 *      the specification requires of this one, so falling back to
 *      one would be actively wrong rather than merely imprecise.
 *   -# @p final_fallback, verbatim, whatever the caller passes;
 *      this project's own callers use @c "./icowm" for
 *      @c XDG_DIR_CONFIG (matching this project's existing
 *      behavior) and a @c /tmp location carrying the numeric user
 *      ID for @c XDG_DIR_RUNTIME, since the specification leaves
 *      this last resort entirely up to the application.
 *
 * Only ever resolves and writes the path string; never creates the
 * directory on disk (callers that need it to exist, e.g., before
 * binding a socket inside it, create it themselves, with whatever
 * permissions that particular use needs).
 *
 * @param kind           Which base directory to resolve
 * @param final_fallback Path to use verbatim if neither the
 *                       environment variable nor (when applicable)
 *                       @c $HOME resolved to anything
 * @param out            Destination buffer for the resolved,
 *                       simplified path
 * @param out_size       Size of @p out in bytes
 *
 * @note Complexity: @e O(n), where @e n is the length of the
 *       resolved path
 */
void xdg_resolve_dir(enum xdg_dir_kind_e kind,
        const char *final_fallback, char *out, size_t out_size);


#endif  /* UTILS_CONFIG_PATH */
