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

/* Utils includes */
#include <utils/xcb/connection.h>

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
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <input/kbd/event.h>
#include <input/kbd/internal.h>


/* Stage lookup */
/**
 * @brief Look up a stage associated to a root window, with fallback
 *
 * Attempts to find a @c stage_td that corresponds to the given X11
 * @c root window by searching the @c stages list.  If no matching
 * stage is found, but the list is non-empty, this function falls back
 * to returning the first stage in the list.
 *
 * @param stages List of available stages to search in, or @c NULL
 * @param root   X11 root window identifier used as lookup key
 *
 * @return The matching @c stage_td, or the first stage in the
 *         list where none matches, or @c NULL where @p stages is
 *         null or empty
 *
 * @note Meant for the moments when a particular root stage may not
 *       exist yet, giving the caller a sensible default
 */
static stage_td *s_lookup_stage_fallback(list_td *stages,
        xcb_window_t root)
{
    stage_td *stage;

    stage = lookup_stage_for_root(stages, root);
    if (stage == NULL && stages != NULL &&
            !list_is_empty(stages)) {
        stage = (stage_td *) list_data(list_head(stages));
    }

    return stage;
}


/* Handle a key-release event to auto-confirm the cycle menu */
void keyboard_handle_release(xcb_key_symbols_t *keysyms,
        xcb_key_release_event_t *event, list_td *stages,
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
        const stage_td *const stage =
            s_lookup_stage_fallback(stages, event->root);
        if (stage != NULL) {
            cycle_confirm(xcb_connection_get(), stages, config);
        }
        return;
    }
}


/* Translate a key-press event into an action and dispatch it */
void keyboard_handle_press(wm_td *wm, xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event, list_td *stages,
        const config_td *config)
{
    enum wm_keybind_type_e btype;
    xcb_keysym_t keysym;
    uint16_t state;
    uint16_t modmask = 0u;
    stage_td *stage;

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

    stage = s_lookup_stage_fallback(stages, event->root);

    if (ik_intercept_keypress(keysym,
                keyboard_keysym_for_state(keysyms, event->detail,
                        event->state),
                state, stage, stages,
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

    ik_execute_binding(wm, btype, modmask, event->detail, stage,
            stages, config);
}
