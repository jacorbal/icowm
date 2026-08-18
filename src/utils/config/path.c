/**
 * @file utils/config/path.c
 *
 * @brief Implementation for path handling functions
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>    /* bool, true, false */
#include <stddef.h>     /* NULL, size_t */
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* getenv */

/* Default initial values */
#include <defs/config.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <utils/config/path.h>


/**
 * @brief One @c xdg_dir_kind_e's own resolution rule set
 */
struct xdg_dir_def_s {
    const char *env_var;       /**< Environment variable to check
                                    first, e.g., @c XDG_CONFIG_HOME */
    const char *xdg_suffix;    /**< Appended to that variable's own
                                    value, e.g., "icowm" */
    const char *home_relative; /**< Full path relative to @c $HOME to
                                    fall back to when the variable above
                                    is unset or empty, already including
                                    this project's own name in whatever
                                    form that kind's own convention uses
                                    (see @a xdg_resolve_dir's comment
                                    in @c path.h); @c NULL for the one
                                    kind with no such fallback tier
                                    at all */
};

/**
 * @brief One resolution rule set per @p xdg_dir_kind_e value, indexed
 *        by it
 *
 *
 *  Every @p xdg_suffix and the name embedded in each @p home_relative
 *  is @c CONFIG_DIR_BASE ("icowm"), the same single name every other
 *  path in the project already resolves against, rather than five
 *  separate copies of the literal that could drift out of sync with it.
 */
static const struct xdg_dir_def_s s_xdg_defs[] = {
    [XDG_DIR_CONFIG]  = { "XDG_CONFIG_HOME", CONFIG_DIR_BASE,
                           "." CONFIG_DIR_BASE },
    [XDG_DIR_DATA]    = { "XDG_DATA_HOME",   CONFIG_DIR_BASE,
                           ".local/share/" CONFIG_DIR_BASE },
    [XDG_DIR_STATE]   = { "XDG_STATE_HOME",  CONFIG_DIR_BASE,
                           ".local/state/" CONFIG_DIR_BASE },
    [XDG_DIR_CACHE]   = { "XDG_CACHE_HOME",  CONFIG_DIR_BASE,
                           ".cache/" CONFIG_DIR_BASE },
    [XDG_DIR_RUNTIME] = { "XDG_RUNTIME_DIR", CONFIG_DIR_BASE, NULL },
};


/* Normalize a given file path by removing unnecessary components */
void path_simplify(char *restrict path)
{
    char *src = path, *dst = path;
    bool is_absolute = (path[0] == '/');
    size_t len;

    while (*src) {
        if (*src == '/') {
            /* Avoid multiple slashes, and a leading one on what should
             * be a purely relative path: the second case only ever
             * arises right after resolving a '..' back to the very
             * start of a relative 'dst' (see below), where the '/' that
             * used to separate the popped component from whatever
             * follows it no longer separates anything and must be
             * dropped too, not kept as a spurious leading slash an
             * otherwise-relative path never had. */
            if ((dst != path && *(dst - 1) == '/') ||
                    (dst == path && !is_absolute)) {
                src++;
                continue;
            }
            *dst++ = *src++;
        } else if (safe_strncmp(src, "./", 2) == 0) {
            src += 2;   /* Jump over "./" */
        } else if (safe_strncmp(src, "../", 3) == 0) {
            /* Resolve this '..' against the previous component
             * already written to 'dst', if there is one to resolve
             * it against: scan back from 'dst' to find where that
             * component starts (right after its own leading slash,
             * or the very start of 'dst' if it has none), so the
             * whole component can be removed, not just the slash
             * before it. Only a genuine, already-resolved component
             * (neither empty nor itself an unresolved '..') can
             * actually be cancelled this way; otherwise '..' cannot
             * be resolved here at all, so it is either kept as a
             * literal component (a relative path with nothing left
             * behind it to cancel against, or a previous component
             * that is itself an unresolved '..') or dropped outright
             * (an absolute path already at its own root, which has
             * nothing above it to name). */
            char *const comp_end = (dst > path && *(dst - 1) == '/')
                ? dst - 1 : dst;
            char *comp_start = comp_end;
            bool have_component;
            bool component_is_dotdot;

            while (comp_start > path && *(comp_start - 1) != '/') {
                comp_start--;
            }
            have_component = (comp_end > comp_start);
            component_is_dotdot = (comp_end - comp_start == 2 &&
                    comp_start[0] == '.' && comp_start[1] == '.');

            if (have_component && !component_is_dotdot) {
                dst = comp_start;
            } else if (!is_absolute) {
                *dst++ = '.';
                *dst++ = '.';
            }
            /* Absolute path already at root: '..' dropped outright,
             * nothing written at all; there is nothing above root
             * to name */

            src += 2;   /* Jump over the two dots; the slash right
                         * after them (if any) is left for the next
                         * pass through this same loop, whose own
                         * slash handling above already deals with
                         * it correctly either way */
            continue;
        } else {
            *dst++ = *src++;
        }
    }

    /* A relative path emptied out entirely by resolving every one of
     * its own components against a '..' (e.g., 'a/../' simplifies to
     * nothing at all, the current directory) is represented as '.'
     * itself, never as an empty string: an empty path and "the current
     * directory" are not interchangeable to whatever this result gets
     * used for next. */
    if (dst == path && !is_absolute) {
        *dst++ = '.';
    }

    /* Trim a single trailing slash this process may have left behind
     * (e.g., simplifying 'a/b/../' down to 'a/'), except when the whole
     * simplified path is the root by itself.  Computed as an integer
     * length rather than compared and dereferenced via pointer
     * arithmetic on 'dst' directly: GCC's static analyzer
     * ('-fanalyzer') cannot always follow the bound this pointer is
     * actually kept within by the loop above (with its many nested
     * branches and 'dst' resets while resolving '..'), and flags
     * a false out-of-bounds read on 'dst - 1' otherwise, despite that
     * loop only ever advancing 'dst' by writing through it (so it can
     * never end up past 'src', let alone past 'path' itself).  Indexing
     * 'path[]' by an integer length here is semantically identical, and
     * easier for it to verify as safe. */
    len = (size_t) (dst - path);

    if (len > 1u && path[len - 1u] == '/') {
        dst = path + (len - 1u);
    }

    *dst = '\0';    /* End string */
}


/* Resolve one of IcoWM's own XDG base directories */
void xdg_resolve_dir(enum xdg_dir_kind_e kind,
        const char *restrict final_fallback, char *restrict out,
        size_t out_size)
{
    const struct xdg_dir_def_s *def = &s_xdg_defs[kind];
    const char *xdg_value = getenv(def->env_var);
    const char *home = getenv("HOME");
    char temp_path[CONFIG_MAX_LENGTH_PATH_BASE];

    if (xdg_value != NULL && xdg_value[0] != '\0') {
        snprintf(temp_path, sizeof(temp_path), "%s/%s",
                xdg_value, def->xdg_suffix);
    } else if (def->home_relative != NULL &&
            home != NULL && home[0] != '\0') {
        snprintf(temp_path, sizeof(temp_path), "%s/%s",
                home, def->home_relative);
    } else {
        snprintf(temp_path, sizeof(temp_path), "%s", final_fallback);
    }

    path_simplify(temp_path);
    safe_strncpy(out, temp_path, out_size);
}
