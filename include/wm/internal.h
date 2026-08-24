/**
 * @file wm/internal.h
 *
 * @brief Private state and helpers shared across @c wm sub-modules
 *
 * Defines the real @c wm_td structure (opaque everywhere else; see
 * @c wm.h's own accessor functions), and declares helpers shared by
 * more than one of @c wm/shutdown.c, @c wm/ewmhinit.c, and
 * @c wm/action.c.
 *
 * @note This header is private to the @c wm subsystem and must not be
 *       included outside of @c src/wm.c and @c src/wm/, for it is NOT
 *       part of the public API
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_INTERNAL_H
#define WM_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <wm.h>


/**
 * @brief Window manager structure: real definition
 *
 * Core components of a window manager, maintaining the state of the
 * program as well as the relationships between different screens and
 * their respective windows.
 *
 * The @p is_running flag indicates whether the window manager is
 * currently operational, while the @p surfaces linked list holds
 * references to all surfaces being managed.  The @p config pointer
 * allows for customization of the window manager's settings; events
 * that affect window behavior and user interactions are dispatched
 * from @a loop_run instead, not tracked as a field here.
 *
 * Reached from outside @c wm.c and @c wm/instance.c only through
 * @c wm.h's own accessor functions, never through direct member
 * access.
 *
 * @see @c loop.h
 */
struct wm_s {
    xcb_connection_t *connection;   /**< Pointer to XCB connection */
    xcb_ewmh_connection_t *ewmh;    /**< EWMH connection */
    list_td *surfaces;              /**< List of surfaces */

    /**
     * Key symbols table used to translate keycodes to keysyms for
     * keyboard binding grabs; owned and freed by @a loop_run, which
     * allocates it once at startup.
     *
     * Stored here so @a wm_action_config_reload can re-run @a
     * keyboard_load with the current bindings after every reload
     * trigger (@c SIGHUP, the reload keybinding, and the root menu's
     * "Reload configuration" entry) without each of those three call
     * sites needing its own copy of this pointer. */
    xcb_key_symbols_t *keysyms;

    config_td *config;              /**< Window manager configuration */
    rules_td *rules;                /**< Window matching rules */
    session_td *session;            /**< Session hooks */

    const char *config_dir_prefix;  /**< Config dir. passed at startup,
                                         for @c NULL if the default
                                         config. dir. is used; kept to
                                         reuse it on config. reload */

    xcb_window_t ewmh_support_win;  /**< '_NET_SUPPORTING_WM_CHECK' window */
    uint32_t screenp;               /**< Preferred screen */

    /**
     * @brief Restricted-memory mode's available-memory ceiling, in
     *        mebibytes, or @c 0 when the mode is not active
     *
     * Set once at startup from the @c -M command-line option and never
     * changed afterward.  Consulted by @a loop_run's periodic
     * low-memory check, which warns once system-wide available memory
     * drops below it.
     *
     * @see @c main.c for the @c -M option, and @c utils/sysmem.h
     * @see @a wm_start
     */
    uint32_t restricted_memory_mib;

    bool is_randr_available;        /**< XRandR extension availability */
    uint8_t randr_base_event;       /**< XRandR base event code */
    bool is_sync_available;         /**< XSync extension availability */
    uint8_t sync_base_event;        /**< XSync base event code */
    bool is_running;                /**< Running state flag */

    bool is_emergency_exit;         /**< Set when an emergency exit is
                                         requested; suppresses pending
                                         session hooks on shutdown */
};


/**
 * @brief Visit every currently managed client across every surface and
 *        desktop, optionally applying an action to each
 *
 * Shared by @c wm/shutdown.c and @c wm/ewmhinit.c, so both walk the
 * exact same enumeration instead of each keeping its own separate copy
 * of this traversal.
 *
 * @param wm       Window manager instance
 * @param action   Called once per client found, with @p userdata passed
 *                 through unchanged, or @c NULL to only count clients
 *                 without acting on any
 * @param userdata Passed through to @p action on every call, untouched
 *                 otherwise
 *
 * @return Number of managed clients found
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across every surface and desktop
 */
uint32_t wm_for_each_client(const wm_td *wm,
        void (*action)(client_td *client,
            void *userdata), void *userdata);


#endif  /* ! WM_INTERNAL_H */
