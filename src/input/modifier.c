/**
 * @file input/modifier.c
 *
 * @brief Modifier-token resolution shared by the keyboard and mouse
 *        binding loaders
 *
 * @c modc/mods/modl and @c mod1-mod5 alias resolution, and the
 * mapping from a resolved modifier name to its XCB modifier mask, do
 * not depend on whether the binding being parsed is a key or a mouse
 * button, so this one implementation replaces what used to be an
 * identical pair of functions copied into @c input/kbd/bind.c and
 * @c input/mouse/bind.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>
#include <strings.h>    /* strcasecmp */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>

/* Local includes */
#include <input/modifier.h>


/* Resolve configured modifier aliases such as 'modc' or 'mods' */
const char *im_resolve_modifier_token(const config_td *config,
        const char *token)
{
    if (token == NULL || config == NULL) {
        return token;
    }

    if (strcasecmp(token, "modc") == 0) { return config->bindings.modc; }
    if (strcasecmp(token, "mods") == 0) { return config->bindings.mods; }
    if (strcasecmp(token, "modl") == 0) { return config->bindings.modl; }
    if (strcasecmp(token, "mod1") == 0) { return config->bindings.mod1; }
    if (strcasecmp(token, "mod2") == 0) { return config->bindings.mod2; }
    if (strcasecmp(token, "mod3") == 0) { return config->bindings.mod3; }
    if (strcasecmp(token, "mod4") == 0) { return config->bindings.mod4; }
    if (strcasecmp(token, "mod5") == 0) { return config->bindings.mod5; }

    return token;
}


/* Map a single modifier token to an XCB modifier mask */
uint16_t im_parse_modifier_token(const config_td *config,
        const char *token)
{
    const char *resolved = im_resolve_modifier_token(config, token);

    if (resolved == NULL || resolved[0] == '\0') {
        return 0;
    }

    if (strcasecmp(resolved, "mod1") == 0 ||
            strcasecmp(resolved, "alt") == 0) {
        return XCB_MOD_MASK_1;
    }
    if (strcasecmp(resolved, "mod2") == 0 ||
            strcasecmp(resolved, "num_lock") == 0 ||
            strcasecmp(resolved, "num-lock") == 0) {
        return XCB_MOD_MASK_2;
    }
    if (strcasecmp(resolved, "mod3") == 0) {
        return XCB_MOD_MASK_3;
    }
    if (strcasecmp(resolved, "mod4") == 0 ||
            strcasecmp(resolved, "super") == 0 ||
            strcasecmp(resolved, "win") == 0) {
        return XCB_MOD_MASK_4;
    }
    if (strcasecmp(resolved, "mod5") == 0 ||
            strcasecmp(resolved, "hyper") == 0) {
        return XCB_MOD_MASK_5;
    }
    if (strcasecmp(resolved, "ctrl") == 0 ||
            strcasecmp(resolved, "control") == 0) {
        return XCB_MOD_MASK_CONTROL;
    }
    if (strcasecmp(resolved, "shift") == 0) {
        return XCB_MOD_MASK_SHIFT;
    }
    if (strcasecmp(resolved, "lock") == 0 ||
            strcasecmp(resolved, "caps_lock") == 0 ||
            strcasecmp(resolved, "caps-lock") == 0) {
        return XCB_MOD_MASK_LOCK;
    }

    return 0;
}
