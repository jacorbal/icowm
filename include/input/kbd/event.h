/**
 * @file input/kbd/event.h
 *
 * @brief Key-press and key-release event dispatch
 *
 * Declares the two event handler functions that translate raw keyboard
 * events into window manager actions, including cycle menu navigation,
 * client operations, desktop switching, program launches, and emergency
 * exit.
 *
 * @defgroup input_kbd Keyboard input
 * @ingroup input
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_KBD_EVENT_H
#define INPUT_KBD_EVENT_H


/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */

/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Dispatch a key-press event to the appropriate window manager
 *        action
 *
 * Translates the keycode in @p event to a keysym, checks for cycle menu
 * navigation, the emergency exit shortcut, and configured key bindings.
 * Dispatches client operations, desktop switches, program launches,
 * move/resize key steps, and cycle menu open/navigate.
 *
 * @param wm       Window manager instance (for actions that need more
 *                 than the surface list and configuration alone, such
 *                 as re-applying the placement policy)
 * @param keysyms  Allocated XCB key-symbols table
 * @param event    Incoming key-press event
 * @param surfaces All managed surfaces (for lookup and focus)
 * @param cfg      Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of loaded
 *       bindings
 */
void keyboard_handle_press(wm_td *wm, xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event,
        list_td *surfaces,
        const config_td *cfg);

/**
 * @brief Dispatch a key-release event to the appropriate window manager
 *        action
 *
 * Checks whether the released key is the modifier used to open the
 * cycle menu; if so, confirms the current selection automatically.
 *
 * @param keysyms  Allocated XCB key-symbols table
 * @param event    Incoming key-release event
 * @param surfaces All managed surfaces (used to pass to cycle_confirm)
 * @param cfg      Active configuration
 *
 * @note Complexity: @e O(1)
 */
void keyboard_handle_release(xcb_key_symbols_t *keysyms,
        xcb_key_release_event_t *event,
        list_td *surfaces,
        const config_td *cfg);


#endif  /* ! INPUT_KBD_EVENT_H */
