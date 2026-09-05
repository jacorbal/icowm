/**
 * @file input/kbd/bind.h
 *
 * @brief Keyboard binding and event translation API
 *
 * Declares the key-binding action type, the resolved-binding record,
 * and the public functions for loading key grabs from configuration,
 * translating raw key-press events into action identifiers, and
 * querying the active binding table.
 *
 * This module has no knowledge of the window manager singleton; every
 * function receives explicit parameters for the XCB connection, surface
 * list, and configuration pointer.
 *
 * @ingroup input_kbd
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

/* Type includes */
#include <types/handles.h>

/* Default initial values */
#include <defs/kbd.h>



/**
 * @brief Action types for keyboard bindings
 *
 * Each constant identifies a window-manager action that may be bound to
 * a key combination in the configuration file.
 */
enum wm_keybind_type_e {
    KEYBIND_NONE,

    /* Desktop cycling, each relative to the current desktop */
    KEYBIND_DESKTOP_NORTH,
    KEYBIND_DESKTOP_SOUTH,
    KEYBIND_DESKTOP_EAST,
    KEYBIND_DESKTOP_WEST,

    /* Pan the current desktop's own viewport by one screen, clamped
     * rather than cyclic, and never changing which desktop is
     * current */
    KEYBIND_VIEWPORT_PAN_NORTH,
    KEYBIND_VIEWPORT_PAN_SOUTH,
    KEYBIND_VIEWPORT_PAN_EAST,
    KEYBIND_VIEWPORT_PAN_WEST,

    /* Direct viewport page go-to, keys 1 to 9 addressing the first
     * nine pages of the configured grid, in row-major order; unlike
     * 'KEYBIND_DESKTOP_GOTO_0' through '_9' above, the key itself is
     * one-based (key '1' reaches the very first page, index 0) so
     * every one of the nine digit keys names a real page instead of
     * key '0' being spent on the page already showing most of the
     * time */
    KEYBIND_VIEWPORT_GOTO_1,
    KEYBIND_VIEWPORT_GOTO_2,
    KEYBIND_VIEWPORT_GOTO_3,
    KEYBIND_VIEWPORT_GOTO_4,
    KEYBIND_VIEWPORT_GOTO_5,
    KEYBIND_VIEWPORT_GOTO_6,
    KEYBIND_VIEWPORT_GOTO_7,
    KEYBIND_VIEWPORT_GOTO_8,
    KEYBIND_VIEWPORT_GOTO_9,

    /* Window operations, each on the focused client */
    KEYBIND_CLIENT_ICONIFY,
    /** Hide, which is what other window managers call minimizing */
    KEYBIND_CLIENT_HIDE,
    KEYBIND_CLIENT_CLOSE,
    /** Kill forcibly, without asking the client to close first */
    KEYBIND_CLIENT_KILL,
    KEYBIND_CLIENT_MAXIMIZE,
    KEYBIND_CLIENT_CENTER,

    /* Move to the monitor in a direction, relative to the
     * current one */
    KEYBIND_CLIENT_MOVE_MONITOR_NORTH,
    KEYBIND_CLIENT_MOVE_MONITOR_SOUTH,
    KEYBIND_CLIENT_MOVE_MONITOR_EAST,
    KEYBIND_CLIENT_MOVE_MONITOR_WEST,

    /* Carry the client to a neighboring desktop and follow it there */
    KEYBIND_CLIENT_SEND_TO_DESKTOP_NORTH,
    KEYBIND_CLIENT_SEND_TO_DESKTOP_SOUTH,
    KEYBIND_CLIENT_SEND_TO_DESKTOP_EAST,
    KEYBIND_CLIENT_SEND_TO_DESKTOP_WEST,

    KEYBIND_CLIENT_SHADE,
    KEYBIND_CLIENT_FULLSCREEN,
    /** Toggle the pin flag, so the client shows on every desktop */
    KEYBIND_CLIENT_PIN,
    /** Toggle the sticky flag; not to be confused with
     *  @c KEYBIND_CLIENT_PIN above, see @c CLIENT_FLAG_STICKY's
     *  comment in @c client/state.h for the full distinction */
    KEYBIND_CLIENT_STICKY,
    KEYBIND_CLIENT_INFO,
    KEYBIND_CLIENT_INSPECT,
    KEYBIND_CLIENT_TOGGLE_DECORATION,
    /** Cycle the layer between normal, above and below */
    KEYBIND_CLIENT_CYCLE_LAYER,
    /** Focus the next client */
    KEYBIND_CLIENT_CYCLE_NEXT,
    /** Focus the previous client */
    KEYBIND_CLIENT_CYCLE_PREV,

    /**
     * @brief Open this client's context menu
     *
     * Bound to a fixed @c Alt+Space and unrelated to
     * @c KEYBIND_WM_WINDOWS_MENU.
     */
    KEYBIND_CLIENT_WINDOW_MENU,
    /** Cycle to the next iconified client */
    KEYBIND_DESKTOP_ICON_NEXT,
    /** Cycle to the previous iconified client */
    KEYBIND_DESKTOP_ICON_PREV,

    /* Program launcher */
    KEYBIND_LAUNCH_TERMINAL,
    KEYBIND_LAUNCH_LAUNCHER,
    KEYBIND_LAUNCH_FILE_MANAGER,
    KEYBIND_LAUNCH_WEB_BROWSER,
    KEYBIND_LAUNCH_EDITOR,

    /* Window movement by a fixed step */
    KEYBIND_CLIENT_MOVE_LEFT,
    KEYBIND_CLIENT_MOVE_RIGHT,
    KEYBIND_CLIENT_MOVE_UP,
    KEYBIND_CLIENT_MOVE_DOWN,

    /* Window movement snapping to a screen corner */
    KEYBIND_CLIENT_MOVE_TOP_LEFT,
    KEYBIND_CLIENT_MOVE_TOP_RIGHT,
    KEYBIND_CLIENT_MOVE_BOTTOM_LEFT,
    KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT,

    /* Window resizing by a fixed step */
    /** Shrink the width */
    KEYBIND_CLIENT_RESIZE_LEFT,
    /** Grow the width */
    KEYBIND_CLIENT_RESIZE_RIGHT,
    /** Shrink the height */
    KEYBIND_CLIENT_RESIZE_UP,
    /** Grow the height */
    KEYBIND_CLIENT_RESIZE_DOWN,

    /** Toggle show-desktop, which hides every client at once */
    KEYBIND_DESKTOP_SHOW,

    /* Iconify and restore every client on the desktop */
    KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL,
    KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL,
    /** Re-apply the placement policy to every client */
    KEYBIND_DESKTOP_CLIENTS_REARRANGE,

    /* Direct desktop go-to, indices 0 to 9 */
    KEYBIND_DESKTOP_GOTO_0,
    KEYBIND_DESKTOP_GOTO_1,
    KEYBIND_DESKTOP_GOTO_2,
    KEYBIND_DESKTOP_GOTO_3,
    KEYBIND_DESKTOP_GOTO_4,
    KEYBIND_DESKTOP_GOTO_5,
    KEYBIND_DESKTOP_GOTO_6,
    KEYBIND_DESKTOP_GOTO_7,
    KEYBIND_DESKTOP_GOTO_8,
    KEYBIND_DESKTOP_GOTO_9,

    /* Add or remove the surface's last desktop */
    KEYBIND_DESKTOP_ADD,
    KEYBIND_DESKTOP_REMOVE,

    /** Toggle strutless maximization, setting panel and tray struts
     * aside */
    KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE,

    /* Window manager lifecycle */
    /** Open the root desktop menu */
    KEYBIND_WM_ROOT_MENU,
    /** Open the windows desktop menu */
    KEYBIND_WM_WINDOWS_MENU,
    /** Open the fuzzy window search */
    KEYBIND_WM_SEARCH_WINDOWS,
    /** Force an on-demand redraw */
    KEYBIND_WM_REDRAW,
    /** Reload the configuration in place */
    KEYBIND_WM_RELOAD,
    /** Quit, with a confirmation dialog */
    KEYBIND_WM_QUIT,
    /** Show the active keybindings */
    KEYBIND_WM_SHORTCUTS_LIST,
    /** Force an abrupt and quick exit */
    KEYBIND_WM_EMERGENCY_EXIT,
    /** Show a 'fortune' dialog */
    KEYBIND_WM_FORTUNE,
    /** Show or hide the scratchpad */
    KEYBIND_WM_SCRATCHPAD_TOGGLE,
};


/**
 * @brief Resolved keyboard binding record
 *
 * Associates a keysym, a modifier mask, and an action type loaded from
 * the configuration file.
 */
typedef struct {
    xcb_keysym_t keysym;            /**< X keysym for this binding */
    enum wm_keybind_type_e type;    /**< Action this binding triggers */
    uint16_t modmask;               /**< Required modifier mask */
} wm_keybinding_td;


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
 * @return @c true when a matching binding is found
 *
 * @note Complexity: @e O(n), where @e n is the number of loaded
 *       bindings
 */
bool keyboard_find(enum wm_keybind_type_e type,
        xcb_keysym_t *keysym_out, uint16_t *modmask_out);

/**
 * @brief Test whether a keysym corresponds to a modifier covered by the
 *        given modifier mask
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
 *         @p mask
 *
 * @note Complexity: @e O(1)
 */
bool keyboard_is_modifier_for_mask(xcb_keysym_t keysym, uint16_t mask);

/**
 * @brief The symbol a key actually produces, modifiers included
 *
 * A keycode names a physical key, and what that key produces depends on
 * which modifiers are held: the same key gives @c 7 on its own and @c
 * slash with @c Shift, and @c 4, @c dollar or @c asciitilde depending
 * on @c Shift and @c AltGr on some layouts.  X arranges those in
 * columns of the keyboard mapping, and asking for column @c 0 alone, as
 * the binding lookup does, always answers with the unmodified key.
 *
 * That is right for bindings, which are defined against the key rather
 * than against what it types, and wrong for anything reading text.
 * A dialog asking for a command could not be given an uppercase letter,
 * a slash, an asterisk, a dollar sign or a tilde at all.
 *
 * @param keysyms Keyboard mapping to consult; may be null
 * @param keycode Physical key that was pressed
 * @param state   Modifier state the press carried
 *
 * @return The symbol that key and those modifiers produce, or
 *         @c XCB_NO_SYMBOL when it produces none
 *
 * @note Caps Lock counts as @c Shift for letters only, which is what
 *       X itself defines: it uppercases @c a, and leaves @c 7 alone
 * @note Complexity: @e O(1)
 */
xcb_keysym_t keyboard_keysym_for_state(xcb_key_symbols_t *keysyms,
        xcb_keycode_t keycode, uint16_t state);

/**
 * @brief Test whether a keysym is any modifier key at all
 *
 * Unlike @a keyboard_is_modifier_for_mask, this checks every known
 * modifier keysym (@c Shift, @c Control, @c Meta/Alt, @c Num_Lock,
 * @c Super, @c Hyper) regardless of which specific one, rather than
 * restricting the check to bits set in a caller-supplied mask.
 *
 * Used by @a s_handle_cycle_key (@c input/kbd/event.c) so that tapping
 * a bare modifier such as @c Shift, to switch cycle direction, does not
 * close the cycle menu; see @a cycle_init's @p g_cycle_menu @p modifier
 * assignment (in @c menu/cycle.c) for the other half of that same fix.
 *
 * @param keysym Keysym to test
 *
 * @return @c true when @p keysym is any modifier key
 *
 * @note Complexity: @e O(1)
 */
bool keyboard_keysym_is_modifier(xcb_keysym_t keysym);

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
 * @return Action type for that entry, or @c KEYBIND_NONE if out of
 *         range
 *
 * @note Complexity: @e O(1)
 */
enum wm_keybind_type_e keyboard_binding_at(int idx,
        xcb_keysym_t *keysym_out, uint16_t *modmask_out);


#endif  /* ! INPUT_KBD_BIND_H */
