/**
 * @file input/kbd/event.c
 *
 * @brief Key-press and key-release event dispatch
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <signal.h>     /* SIGTERM */
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Menu includes */
#include <menu/cycle.h>

/* Default initial values */
#include <defs/kbd.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <input/kbd/event.h>
#include <input/kbd/internal.h>


/* Surface lookup */
/**
 * @brief Look up a surface associated to a root window, with fallback
 *
 * Attempts to find a @c surface_td that corresponds to the given X11
 * @c root window by searching the @c surfaces list.  If no matching
 * surface is found, but the list is non-empty, this function falls back
 * to returning the first surface in the list.
 *
 * @param surfaces List of available surfaces to search in, or @c NULL
 * @param root     X11 root window identifier used as lookup key
 *
 * @return Pointer to the matching @c surface_td, or the first surface
 *         in the list if no match is found; returns @c NULL if
 *         @p surfaces is null or empty.
 *
 * @note Intended for use when a specific root surface may not exist
 *       yet, providing a reasonable default for callers.
 */
static surface_td *s_lookup_surface_fallback(list_td *surfaces,
        xcb_window_t root)
{
    surface_td *surface;

    surface = lookup_surface_for_root(surfaces, root);
    if (surface == NULL && surfaces != NULL &&
            !list_is_empty(surfaces)) {
        surface = (surface_td *) list_data(list_head(surfaces));
    }

    return surface;
}


/* Handle a key-release event to auto-confirm the cycle menu */
void keyboard_handle_release(xcb_key_symbols_t *keysyms,
        xcb_key_release_event_t *event, list_td *surfaces,
        const config_td *config)
{
    xcb_keysym_t keysym;

    if (keysyms == NULL || event == NULL) {
        return;
    }

    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);

    /* Auto-confirm cycle menu when its modifier is released */
    if (cycle_is_open() && cycle_modifier() != 0 &&
            keyboard_is_modifier_for_mask(keysym, cycle_modifier())) {
        surface_td *const surface = s_lookup_surface_fallback(surfaces,
                event->root);
        if (surface != NULL) {
            cycle_confirm(surface->connection, surfaces, config);
        }
        return;
    }
}


/* Translate a key-press event into an action and dispatch it */
void keyboard_handle_press(wm_td *wm, xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event, list_td *surfaces,
        const config_td *config)
{
    enum wm_keybind_type_e btype;
    xcb_keysym_t keysym;
    uint16_t state;
    uint16_t modmask = 0u;
    surface_td *surface;

    if (keysyms == NULL || event == NULL || config == NULL) {
        LOGGER_ERROR("Received null pointer in key press handler",
                L_NARG);
        return;
    }

    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);
    state = (uint16_t) ((unsigned int) event->state &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    LOGGER_TRACE("Key press event (keysym=0x%x, state=0x%x)",
            keysym, state);

    surface = s_lookup_surface_fallback(surfaces, event->root);

    if (ik_intercept_keypress(keysym, state, surface, surfaces,
                config)) {
        return;
    }

    /* Emergency exit 'Ctrl+Mod1+Backspace'.  Checked against the raw
     * modifier state rather than the stripped one, and ahead of the
     * binding table, so that it still works when the configuration
     * itself is what left the window manager unusable. */
    if (config->base.shutdown.enable_emergency_shortcut &&
            keysym == KS_BACKSPACE &&
            (event->state & XCB_MOD_MASK_CONTROL) &&
            (event->state & XCB_MOD_MASK_1)) {
        LOGGER_NOTICE("Emergency exit key combination detected", L_NARG);
        wm_emergency_exit_enable();
        raise(SIGTERM);
        return;
    }

    btype = ik_resolve_binding(keysym, state, &modmask);
    if (btype == KEYBIND_NONE) {
        return;
    }

    ik_execute_binding(wm, btype, modmask, event->detail, surface,
            surfaces, config);
}
