/**
 * @file input/kbd/bind.h
 *
 * @brief Keyboard binding and event translation API
 *
 * Declares the key-binding action type, the resolved-binding record,
 * and the public functions for loading key grabs from configuration,
 * translating raw key-press events into action identifiers, and querying
 * the active binding table.
 *
 * This module has no knowledge of the window manager singleton; every
 * function receives explicit parameters for the XCB connection,
 * surface list, and configuration pointer.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_KBD_BIND_H
#define INPUT_KBD_BIND_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>

/* Local includes */
#include <input/kbd/keycodes.h>


/**
 * @brief Action types for keyboard bindings
 *
 * Each constant identifies a window-manager action that may be bound to
 * a key combination in the configuration file.
 */
enum wm_keybind_type_e {
    KEYBIND_NONE,

    /* Desktop cycling */
    KEYBIND_DESKTOP_NEXT,               /**< Switch to next desktop */
    KEYBIND_DESKTOP_PREV,               /**< Switch to previous desktop */

    /* Window operations */
    KEYBIND_CLIENT_ICONIFY,             /**< Iconify focused client */
    KEYBIND_CLIENT_HIDE,                /**< Hide (minimize) focused client */
    KEYBIND_CLIENT_CLOSE,               /**< Close focused client */
    KEYBIND_CLIENT_KILL,                /**< Forcibly kill focused client */
    KEYBIND_CLIENT_MAXIMIZE,            /**< Maximize focused client */
    KEYBIND_CLIENT_CENTER,              /**< Center focused client */
    KEYBIND_CLIENT_SHADE,               /**< Toggle focused client shade */
    KEYBIND_CLIENT_FULLSCREEN,          /**< Toggle foc. client fullscreen */
    KEYBIND_CLIENT_PIN,                 /**< Toggle focused client sticky */
    KEYBIND_CLIENT_INFO,                /**< Show focused client info */
    KEYBIND_CLIENT_TOGGLE_DECORATION,   /**< Toggle decoration on client */
    KEYBIND_CLIENT_CYCLE_LAYER,         /**< Cycle layer: normal/above/below */
    KEYBIND_CLIENT_CYCLE_NEXT,          /**< Focus next client */
    KEYBIND_CLIENT_CYCLE_PREV,          /**< Focus previous client */
    KEYBIND_DESKTOP_ICON_NEXT,          /**< Cycle to next iconified client */
    KEYBIND_DESKTOP_ICON_PREV,          /**< Cycle to prev iconified client */

    /* Program launcher */
    KEYBIND_LAUNCH_TERMINAL,            /**< Launch terminal */
    KEYBIND_LAUNCH_LAUNCHER,            /**< Launch process launcher */
    KEYBIND_LAUNCH_FILE_MANAGER,        /**< Launch file manager */
    KEYBIND_LAUNCH_WEB_BROWSER,         /**< Launch web browser */
    KEYBIND_LAUNCH_EDITOR,              /**< Launch editor */

    /* Window movement (fixed step or snap to corner) */
    KEYBIND_CLIENT_MOVE_LEFT,           /**< Move focused client left */
    KEYBIND_CLIENT_MOVE_RIGHT,          /**< Move focused client right */
    KEYBIND_CLIENT_MOVE_UP,             /**< Move focused client up */
    KEYBIND_CLIENT_MOVE_DOWN,           /**< Move focused client down */
    KEYBIND_CLIENT_MOVE_TOP_LEFT,       /**< Snap to top-left corner */
    KEYBIND_CLIENT_MOVE_TOP_RIGHT,      /**< Snap to top-right corner */
    KEYBIND_CLIENT_MOVE_BOTTOM_LEFT,    /**< Snap to bottom-left corner */
    KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT,   /**< Snap to bottom-right corner */

    /* Window resizing (fixed step) */
    KEYBIND_CLIENT_RESIZE_LEFT,         /**< Shrink focused client width */
    KEYBIND_CLIENT_RESIZE_RIGHT,        /**< Grow focused client width */
    KEYBIND_CLIENT_RESIZE_UP,           /**< Shrink focused client height */
    KEYBIND_CLIENT_RESIZE_DOWN,         /**< Grow focused client height */

    /* Show-desktop toggle */
    KEYBIND_DESKTOP_SHOW,               /**< Toggle show-desktop (hide all) */

    /* Direct desktop go-to (indices 0-9) */
    KEYBIND_DESKTOP_GOTO_0,             /**< Switch directly to desktop 0 */
    KEYBIND_DESKTOP_GOTO_1,             /**< Switch directly to desktop 1 */
    KEYBIND_DESKTOP_GOTO_2,             /**< Switch directly to desktop 2 */
    KEYBIND_DESKTOP_GOTO_3,             /**< Switch directly to desktop 3 */
    KEYBIND_DESKTOP_GOTO_4,             /**< Switch directly to desktop 4 */
    KEYBIND_DESKTOP_GOTO_5,             /**< Switch directly to desktop 5 */
    KEYBIND_DESKTOP_GOTO_6,             /**< Switch directly to desktop 6 */
    KEYBIND_DESKTOP_GOTO_7,             /**< Switch directly to desktop 7 */
    KEYBIND_DESKTOP_GOTO_8,             /**< Switch directly to desktop 8 */
    KEYBIND_DESKTOP_GOTO_9,             /**< Switch directly to desktop 9 */

    /* Window manager lifecycle */
    KEYBIND_WM_REDRAW,                  /**< Force on-demand redraw */
    KEYBIND_WM_RELOAD,                  /**< Reload config. in-place */
    KEYBIND_WM_QUIT,                    /**< Quit w/ confirmation dialog */
};


/**
 * @brief Resolved keyboard binding record
 *
 * Associates a keysym, a modifier mask, and an action type loaded from
 * the configuration file.
 */
typedef struct {
    xcb_keysym_t keysym;            /**< X keysym for this binding */
    uint16_t modmask;               /**< Required modifier mask */
    enum wm_keybind_type_e type;    /**< Action triggered by this binding */
} wm_keybinding_td;


/* Public interface */
/**
 * @brief Parse configured key bindings and install passive grabs
 *
 * Reads every keyboard binding from @p config, resolves each one to
 * a keysym and modifier mask, stores it in the internal binding table,
 * and calls @c xcb_grab_key on every root window in @p surfaces
 * (including all lock-modifier variants).
 *
 * @param surfaces Singly-linked list of @c surface_td pointers
 * @param keysyms  Allocated XCB key-symbols table
 * @param config   Configuration from which to read binding strings
 *
 * @note Complexity: @e O(b * s * k), where @e b is the number of
 *       bindings, @e s is the number of surfaces, and @e k is the
 *       number of keycodes per keysym
 */
void keyboard_load(list_td *surfaces, xcb_key_symbols_t *keysyms,
        const config_td *config);

/**
 * @brief Look up the first binding registered for a given action type
 *
 * @param type        Action type to search for
 * @param keysym_out  Receives the matching keysym (set to
 *                    @c XCB_NO_SYMBOL on failure)
 * @param modmask_out Receives the matching modifier mask (set to 0 on
 *                    failure)
 *
 * @return @c true when a matching binding is found, @c false otherwise
 *
 * @note Complexity: @e O(n), where @e n is the number of loaded bindings
 */
bool keyboard_find(enum wm_keybind_type_e type,
        xcb_keysym_t *keysym_out, uint16_t *modmask_out);

/**
 * @brief Translate a raw key-press event into a binding action
 *
 * Converts the keycode in @p event to a keysym, strips locking
 * modifiers from the event state, and scans the binding table for the
 * first match.  On success the matched action type and the raw
 * (unstripped) modifier mask of the binding are written to the output
 * parameters.
 *
 * @param keysyms         Allocated XCB key-symbols table
 * @param event           Incoming key-press event
 * @param type_out        Receives the matched action type
 * @param raw_modmask_out Receives the raw modifier mask of the matched
 *                        binding (useful for passing to the cycle menu)
 *
 * @return @c true when a binding is matched, @c false otherwise
 *
 * @note Complexity: @e O(n), where @e n is the number of loaded
 *       bindings
 */
bool keyboard_find_action(xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event,
        enum wm_keybind_type_e *type_out,
        uint16_t *raw_modmask_out);

/**
 * @brief Test whether a keysym corresponds to a modifier covered by
 *        the given modifier mask
 *
 * Used by the key-release handler to detect when the modifier that
 * opened the cycle menu is released, so the selection can be confirmed
 * automatically.
 *
 * @param keysym Keysym of the released key
 * @param mask   Modifier mask to test against (locking modifiers
 *               already stripped)
 *
 * @return @c true when @p keysym maps to a modifier bit present in
 *         @p mask, @c false otherwise
 *
 * @note Complexity: @e O(1)
 */
bool keyboard_is_modifier_for_mask(xcb_keysym_t keysym, uint16_t mask);

/**
 * @brief Return the number of loaded keyboard bindings
 *
 * @return Number of active bindings in the binding table
 *
 * @note Complexity: @e O(1)
 */
int keyboard_binding_count(void);

/**
 * @brief Access a binding entry by index
 *
 * Retrieves the keysym, modifier mask, and action type of the binding
 * at position @p idx in the binding table.  Writes @c XCB_NO_SYMBOL and
 * @c KEYBIND_NONE when @p idx is out of range.
 *
 * @param idx         Zero-based index into the binding table
 * @param keysym_out  Receives the binding's keysym
 * @param modmask_out Receives the binding's modifier mask
 *
 * @return Action type for that entry, or @c KEYBIND_NONE if out of range
 *
 * @note Complexity: @e O(1)
 */
enum wm_keybind_type_e keyboard_binding_at(int idx,
        xcb_keysym_t *keysym_out, uint16_t *modmask_out);


#endif  /* ! INPUT_KBD_BIND_H */
