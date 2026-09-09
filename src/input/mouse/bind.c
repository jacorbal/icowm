/**
 * @file input/mouse/bind.c
 *
 * @brief Mouse binding parsing, grab, and binding table
 *
 * Implements modifier-alias resolution, button and modifier token
 * parsing, the binding table, and passive button grab installation on
 * all root windows.  Provides the accessor functions used by the event
 * handler module.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* strtok_r */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* strtol, NULL, size_t */
#include <string.h>     /* memcpy */
#include <strings.h>    /* strcasecmp */

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/input.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/safe/safestr.h>
#include <utils/xcb/connection.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>

/* Local includes */
#include <input/modifier.h>
#include <input/mouse/bind.h>


/* Module state */
/** Registered mouse bindings */
static wm_mousebinding_td s_mousebindings[WM_MAX_MOUSEBINDINGS];

/** Number of active mouse bindings */
static int s_mousebindings_count = 0;


/**
 * @brief Parse a button token such as @c button1 to button index
 *
 * @param tok Token string
 *
 * @return Button index (1 to 5), or 0 on failure
 *
 * @note Complexity: @e O(1)
 */
static xcb_button_index_t s_parse_button_token(const char *tok)
{
    long n;
    char *end = NULL;

    if (tok == NULL || strncasecmp(tok, "button", 6) != 0) {
        return 0;
    }

    n = strtol(tok + 6, &end, 10);
    if (end == NULL || *end != '\0' || tok[6] == '\0' ||
            n < 1 || n > 5) {
        return 0;
    }

    return (xcb_button_index_t) n;
}


/**
 * @brief Parse a mouse binding string such as @c mod1+button1
 *
 * Splits on '+', treating all tokens except the last as modifiers and
 * the last token as a button name.
 *
 * @param config     Configuration (for modifier alias resolution)
 * @param binding Binding string from configuration
 * @param modmask Receives the combined modifier mask
 * @param button  Receives the parsed button index
 *
 * @return @c true when the binding could be parsed
 *
 * @note Complexity: @e O(n), where @e n is the length of @p binding
 */
static bool s_parse_mouse_binding(const config_td *config,
        const char *binding,
        uint16_t *modmask,
        xcb_button_index_t *button)
{
    char buf[128];
    char *tok;
    char *save;
    const char *prev_tok = NULL;
    size_t len;

    if (binding == NULL || binding[0] == '\0') {
        return false;
    }

    len = safe_strlen(binding);
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    memcpy(buf, binding, len);
    buf[len] = '\0';

    *modmask = 0;
    *button = 0;

    tok = strtok_r(buf, "+", &save);
    while (tok != NULL) {
        if (prev_tok != NULL) {
            uint16_t mod = im_parse_modifier_token(config, prev_tok);
            if (mod != 0) {
                *modmask |= mod;
            }
        }
        prev_tok = tok;
        tok = strtok_r(NULL, "+", &save);
    }

    if (prev_tok != NULL) {
        *button = s_parse_button_token(prev_tok);
    }

    return *button != 0;
}


/* Parse mouse bindings from configuration and grab buttons */
void mouse_load(list_td *surfaces, const config_td *config)
{
    static const uint16_t lockmods[] = {
        0,
        XCB_MOD_MASK_LOCK,
        XCB_MOD_MASK_2,
        XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2
    };

    xcb_connection_t *connection = NULL;
    struct {
        const char *binding;
        enum wm_mousebind_type_e type;
    } defs[8];

    if (config == NULL) {
        return;
    }

    defs[0].binding = config->bindings.mouse.window.move;
    defs[0].type = MOUSEBIND_MOVE;
    defs[1].binding = config->bindings.mouse.window.resize;
    defs[1].type = MOUSEBIND_RESIZE;
    defs[2].binding = config->bindings.mouse.window.lower;
    defs[2].type = MOUSEBIND_LOWER;
    defs[3].binding = config->bindings.mouse.cycle.desktop.north;
    defs[3].type = MOUSEBIND_DESKTOP_NORTH;
    defs[4].binding = config->bindings.mouse.cycle.desktop.south;
    defs[4].type = MOUSEBIND_DESKTOP_SOUTH;
    defs[5].binding = config->bindings.mouse.cycle.desktop.east;
    defs[5].type = MOUSEBIND_DESKTOP_EAST;
    defs[6].binding = config->bindings.mouse.cycle.desktop.west;
    defs[6].type = MOUSEBIND_DESKTOP_WEST;
    defs[7].binding = NULL;
    defs[7].type = MOUSEBIND_NONE;

    connection = xcb_connection_get();

    s_mousebindings_count = 0;

    /* Release every button grab this window manager previously made on
     * each root window before re-grabbing below, for the same reason
     * 'keyboard_load' does: otherwise a binding's old button/modifier
     * combination stays grabbed and active alongside its new one after
     * a configuration reload actually changes it. */
    if (surfaces != NULL) {
        for (list_item_td *node = list_head(surfaces);
                node != NULL; node = list_next(node)) {
            surface_td *surface = (surface_td *) list_data(node);

            if (surface == NULL || surface->screen == NULL) {
                continue;
            }

            xcb_ungrab_button(xcb_connection_get(),
                    XCB_BUTTON_INDEX_ANY, surface->screen->root,
                    XCB_MOD_MASK_ANY);
        }
    }

    for (int i = 0; defs[i].binding != NULL; ++i) {
        xcb_button_index_t button;
        uint16_t modmask;

        if (!s_parse_mouse_binding(config, defs[i].binding,
                &modmask, &button)) {
            LOGGER_WARNING("Ignoring unparseable mouse binding '%s'",
                    defs[i].binding);
            continue;
        }

        if (s_mousebindings_count < WM_MAX_MOUSEBINDINGS) {
            s_mousebindings[s_mousebindings_count].button = button;
            s_mousebindings[s_mousebindings_count].modmask = modmask;
            s_mousebindings[s_mousebindings_count].type = defs[i].type;
            s_mousebindings_count++;
        }

        if (surfaces == NULL) {
            continue;
        }

        /* Scroll-wheel bindings ('DESKTOP_NORTH'/'_SOUTH'/'_EAST'/
         * '_WEST') must not be grabbed passively on the root window.
         * A root passive grab uses async pointer mode, which means
         * the pointer is never frozen; 'AllowEvents'/'ReplayPointer'
         * becomes a no-op and the event cannot be forwarded to the
         * application under the pointer.  These bindings are still
         * registered in the binding table so that scroll events
         * arriving on the root via the root's event-mask
         * subscription ('XSelectInput') are still dispatched to the
         * desktop-cycle handler.  Events over managed client windows
         * are caught by the per-frame sync grab (ANY button, ANY
         * modifier) and reach this handler via that path, where
         * 'ReplayPointer' correctly thaws the pointer and
         * re-delivers the event to the application. */
        if (defs[i].type == MOUSEBIND_DESKTOP_NORTH ||
                defs[i].type == MOUSEBIND_DESKTOP_SOUTH ||
                defs[i].type == MOUSEBIND_DESKTOP_EAST ||
                defs[i].type == MOUSEBIND_DESKTOP_WEST) {
            continue;
        }

        for (list_item_td *node = list_head(surfaces);
                node != NULL; node = list_next(node)) {
            surface_td *surface = (surface_td *) list_data(node);
            if (surface == NULL || surface->screen == NULL) {
                continue;
            }

            for (size_t k = 0;
                    k < sizeof(lockmods) / sizeof(lockmods[0]);
                    ++k) {
                xcb_void_cookie_t ck;
                xcb_generic_error_t *err;
                ck = xcb_grab_button_checked(connection,
                        0,
                        surface->screen->root,
                        XCB_EVENT_MASK_BUTTON_PRESS   |
                        XCB_EVENT_MASK_BUTTON_RELEASE |
                        XCB_EVENT_MASK_POINTER_MOTION,
                        XCB_GRAB_MODE_ASYNC,
                        XCB_GRAB_MODE_ASYNC,
                        XCB_NONE,
                        XCB_NONE,
                        (uint8_t) button,
                        (uint16_t) (modmask | lockmods[k]));
                err = xcb_request_check(connection, ck);
                if (err != NULL) {
                    LOGGER_WARNING(
                            "xcb_grab_button failed for" \
                            " button=%u modmask=0x%x error=%d",
                            (unsigned) button,
                            (unsigned) (modmask | lockmods[k]),
                            err->error_code);
                    free(err);
                }
            }
        }
    }

    LOGGER_DEBUG("Grabbed %d mouse binding(s)", s_mousebindings_count);
}


/* Return the number of loaded mouse bindings */
int mousebind_count(void)
{
    return s_mousebindings_count;
}


/* Access a binding entry by index */
enum wm_mousebind_type_e mousebind_at(int idx,
        xcb_button_index_t *button_out, uint16_t *modmask_out)
{
    if (idx < 0 || idx >= s_mousebindings_count) {
        if (button_out != NULL) {
            *button_out = 0;
        }
        if (modmask_out != NULL) {
            *modmask_out = 0;
        }

        return MOUSEBIND_NONE;
    }

    if (button_out != NULL) {
        *button_out = s_mousebindings[idx].button;
    }
    if (modmask_out != NULL) {
        *modmask_out = s_mousebindings[idx].modmask;
    }

    return s_mousebindings[idx].type;
}
