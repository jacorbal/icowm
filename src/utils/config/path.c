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
                                     first, e.g. "XDG_CONFIG_HOME" */
    const char *xdg_suffix;    /**< Appended to that variable's own
                                     value, e.g. "icowm" */
    const char *home_relative; /**< Full path relative to "$HOME" to
                                     fall back to when the variable
                                     above is unset or empty, already
                                     including this project's own
                                     name in whatever form that
                                     kind's own convention uses (see
                                     'xdg_resolve_dir''s own doc
                                     comment in path.h); NULL for
                                     the one kind with no such
                                     fallback tier at all */
};

/** One resolution rule set per 'xdg_dir_kind_e' value, indexed by it.
 *  Every "xdg_suffix" and the name embedded in each "home_relative"
 *  is 'CONFIG_DIR_BASE' ("icowm"), the same single name every other
 *  path in the project already resolves against, rather than five
 *  separate copies of the literal that could drift out of sync with
 *  it. */
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
    char *last_slash = NULL;

    while (*src) {
        if (*src == '/') {
            /* Avoid multiple slashes */
            if (dst != path && *(dst - 1) == '/') {
                src++;
                continue;
            }
            *dst++ = *src++;
            last_slash = dst - 1;   /* Save last slash position */
        } else if (safe_strncmp(src, "./", 2) == 0) {
            src += 2;   /* Jump over "./" */
        } else if (safe_strncmp(src, "../", 3) == 0 && last_slash) {
            dst = last_slash;       /* Go back to last slash */
            src += 3;   /* Jump over "../" */
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';    /* End string */
}


/* Resolve one of IcoWM's own XDG base directories */
void xdg_resolve_dir(enum xdg_dir_kind_e kind,
        const char *final_fallback, char *out, size_t out_size)
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
