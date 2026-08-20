/**
 * @file wm.h
 *
 * @brief Declaration of window manager structure and main functions
 *
 * @defgroup wm Window manager
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_H
#define WM_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <surface.h>


#ifndef RULES_TD_DECLARED
#define RULES_TD_DECLARED
/**
 * @brief Opaque window matching rules table
 */
typedef struct rules_s rules_td;
#endif

#ifndef SESSION_TD_DECLARED
#define SESSION_TD_DECLARED
/**
 * @brief Opaque session hooks table
 */
typedef struct session_s session_td;
#endif


/**
 * @brief Window manager structure
 *
 * Core components of a window manager, maintaining the state of the
 * program as well as the relationships between different screens and
 * their respective windows.
 *
 * Opaque outside @c wm.c and @c wm/instance.c.  Every field is reached
 * only through the accessor functions declared below, never through
 * direct member access.  @c wm/internal.h (private to @c wm.c and
 * @c wm/instance.c) holds the real definition.
 *
 * @see @c loop.h
 */
typedef struct wm_s wm_td;


/* Field accessors ('wm/instance.c'); every wm/ sub-module and every
 * external caller reaches wm_td's own fields only through these,
 * never through direct member access */
/**
 * @brief XCB connection handle
 *
 * @param wm Window manager instance
 *
 * @return The connection, or @c NULL before @a wm_start has run
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *wm_connection(const wm_td *wm);

/**
 * @brief EWMH connection handle
 *
 * @param wm Window manager instance
 *
 * @return The EWMH connection, or @c NULL before @a wm_start has run
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *wm_ewmh(const wm_td *wm);

/**
 * @brief List of managed surfaces
 *
 * @param wm Window manager instance
 *
 * @return The surface list, or @c NULL before @a wm_start has run
 *
 * @note Complexity: @e O(1)
 */
list_td *wm_surfaces(const wm_td *wm);

/**
 * @brief Key symbols table used to translate keycodes to keysyms
 *
 * @param wm Window manager instance
 *
 * @return The key symbols table, or @c NULL if none is currently
 *         loaded
 *
 * @note Complexity: @e O(1)
 */
xcb_key_symbols_t *wm_keysyms(const wm_td *wm);

/**
 * @brief XRandR extension availability
 *
 * @param wm Window manager instance
 *
 * @return Whether the XRandR extension was found at startup
 *
 * @note Complexity: @e O(1)
 */
bool wm_randr_available(const wm_td *wm);

/**
 * @brief XRandR base event code
 *
 * @param wm Window manager instance
 *
 * @return The base event code, meaningless if @a wm_randr_available
 *         is @c false
 *
 * @note Complexity: @e O(1)
 */
uint8_t wm_randr_base_event(const wm_td *wm);

/**
 * @brief XSync extension availability
 *
 * @param wm Window manager instance
 *
 * @return Whether the XSync extension was found at startup
 *
 * @note Complexity: @e O(1)
 */
bool wm_sync_available(const wm_td *wm);

/**
 * @brief XSync base event code
 *
 * @param wm Window manager instance
 *
 * @return The base event code, meaningless if @a wm_sync_available
 *         is @c false
 *
 * @note Complexity: @e O(1)
 */
uint8_t wm_sync_base_event(const wm_td *wm);

/**
 * @brief Window manager configuration
 *
 * @param wm Window manager instance
 *
 * @return The active configuration, or @c NULL before @a wm_start has
 *         run
 *
 * @note Complexity: @e O(1)
 */
config_td *wm_config(const wm_td *wm);

/**
 * @brief Window matching rules table
 *
 * @param wm Window manager instance
 *
 * @return The rules table, or @c NULL before @a wm_start has run
 *
 * @note Complexity: @e O(1)
 */
rules_td *wm_rules(const wm_td *wm);

/**
 * @brief Session hooks table
 *
 * @param wm Window manager instance
 *
 * @return The session hooks table, or @c NULL before @a wm_start has
 *         run
 *
 * @note Complexity: @e O(1)
 */
session_td *wm_session(const wm_td *wm);

/**
 * @brief Configuration directory passed at startup
 *
 * @param wm Window manager instance
 *
 * @return The configuration directory, or @c NULL if the default
 *         configuration directory is used
 *
 * @note Complexity: @e O(1)
 */
const char *wm_config_dir_prefix(const wm_td *wm);

/**
 * @brief Running state flag
 *
 * @param wm Window manager instance
 *
 * @return Whether the window manager is currently operational
 *
 * @note Complexity: @e O(1)
 */
bool wm_is_running(const wm_td *wm);

/**
 * @brief Restricted-memory mode's available-memory ceiling
 *
 * @param wm Window manager instance
 *
 * @return The ceiling in mebibytes, or @c 0 when the mode is not
 *         active
 *
 * @note Complexity: @e O(1)
 */
uint32_t wm_restricted_memory_mib(const wm_td *wm);

/**
 * @brief Set the @c _NET_SUPPORTING_WM_CHECK window
 *
 * @param wm  Window manager instance
 * @param win The support window
 *
 * @note Complexity: @e O(1)
 */
void wm_set_ewmh_support_win(wm_td *wm, xcb_window_t win);

/**
 * @brief Set the key symbols table
 *
 * @param wm      Window manager instance
 * @param keysyms The new key symbols table, or @c NULL to clear it
 *
 * @note Complexity: @e O(1)
 */
void wm_set_keysyms(wm_td *wm, xcb_key_symbols_t *keysyms);

/**
 * @brief Set XRandR availability and its base event code together
 *
 * @param wm          Window manager instance
 * @param available   Whether the XRandR extension was found
 * @param base_event  The base event code; meaningless if @p available
 *                    is @c false
 *
 * @note Complexity: @e O(1)
 */
void wm_set_randr(wm_td *wm, bool available, uint8_t base_event);

/**
 * @brief Set XSync availability and its base event code together
 *
 * @param wm          Window manager instance
 * @param available   Whether the XSync extension was found
 * @param base_event  The base event code; meaningless if @p available
 *                    is @c false
 *
 * @note Complexity: @e O(1)
 */
void wm_set_sync(wm_td *wm, bool available, uint8_t base_event);


/* Public interface */
/**
 * @brief Initialize window manager instance
 *
 * Allocates memory for a @c wm_td structure, initializes its fields,
 * and opens a connection to the X server.  It also sets up the managed
 * windows array and initializes the current desktop index and running
 * state.
 *
 * When @p restricted_memory_mib is non-zero, restricted-memory mode
 * is active.  Available system memory is checked before doing anything
 * else, refusing to start at all if it is already below that many
 * mebibytes; icon pixmaps and modern font rendering are forced off
 * regardless of what the theme itself says; and each screen's desktop
 * count defaults to a smaller number than an ordinary session's when
 * nothing else specifies one, though an explicit @c config.json value
 * is never overridden.  Everything else (theme, window rules, session
 * hooks, screen count) loads exactly as it would without
 * @p restricted_memory_mib at all.
 *
 * @param display_name          Name of the display, or @c NULL for
 *                              default
 * @param config_dir_prefix     Configuration directory, or @c NULL to
 *                              use the default value
 * @param restricted_memory_mib Restricted-memory mode's
 *                              available-memory ceiling in mebibytes,
 *                              or @c 0 to leave the mode off
 * @param ipc_disabled          When @c true, the IPC control socket is
 *                              never brought up at all; every other
 *                              part of IcoWM runs exactly the same
 *                              either way
 *
 * @return Status of the initialization
 * @retval    0 Success
 * @retval    1 Failed to allocate memory
 * @retval    2 Cannot open X connection
 * @retval    3 Cannot open load configuration
 * @retval 4-10 Failed to initialize other internal data structures
 *              (surfaces, RandR, &c.)
 * @retval   11 Restricted-memory mode's ceiling is already below
 *              available system memory; refused to start at all
 * @retval   -1 Singleton was already initialized; no action taken
 *
 * @note If @p display_name is @c NULL, the initialization attempts to
 *       get the "DISPLAY" environment variable, if set.
 * @note This function uses a singleton pattern
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       to initialize, and @e m the number of desktops per window, as
 *       for the initialization requires iterate over a list of lists
 *
 * @see @c main.c for the @c -M option
 * @see @a config_load's own @p restricted_memory_mib parameter for the
 *      precise details.
 */
int wm_start(const char *restrict display_name,
        const char *restrict config_dir_prefix,
        uint32_t restricted_memory_mib, bool ipc_disabled);

/**
 * @brief Warn through a message dialog if any JSON file loaded since
 *        the last call to @c json_syntax_errors_reset failed to
 *        parse
 *
 * A JSON file simply not existing is an ordinary, silent reason to fall
 * back to defaults, and produces no entry there at all, only a file
 * that was found but could not actually be parsed does.  One combined
 * warning dialog lists every such file recorded since the last reset,
 * rather than one dialog per file.
 *
 * Once shown, the recorded list is cleared, so calling this again
 * without a fresh @a config_load or @a menujson_load in between finds
 * nothing left to warn about; a menu.json still broken the next time
 * the root menu is opened records and warns about it again on its own.
 *
 * @note A no-op if @a json_syntax_errors_count is @c 0 
 * @note Also a no-op if the singleton window manager instance is not
 *       running, or has no surface to show the dialog on
 * @note Complexity: @e O(n), where @e n is the number of files
 *       recorded
 *
 * @see @a json_syntax_errors_reset, also @c utils/config/json.h
 */
void wm_json_syntax_errors_warn(void);

/**
 * @brief Destroy window manager instance
 *
 * Deallocates the memory used by the @c wm_td structure, including the
 * managed windows and closes the connection to the X server.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 No operation has been performed
 *
 * @note Passing a null pointer has no effect
 * @note Complexity: @e O(m * (1 + n^2)), where @e n is the number of
 *       surfaces, and @e m is the number of desktops per surface, as it
 *       iterates through the array of windows to free each one of them
 */
int wm_stop(void);

/**
 * @brief Request a clean stop of the window manager main loop
 *
 * Sets the running flag to @c false so @a wm_start can return and
 * teardown can happen from the main thread.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 If the window manager singleton is not initialized
 */
int wm_request_stop(void);

/**
 * @brief Request a coordinated stop of the window manager, giving
 *        every managed client a chance to close itself first
 *
 * Unlike @a wm_request_stop, which stops the main loop right away,
 * this asks every managed client to close (ICCCM @c WM_DELETE_WINDOW
 * where supported) and only actually stops once every one of them has
 * closed on its own or a configured timeout elapses, whichever comes
 * first.
 *
 * Meant for the normal quit action; the emergency exit shortcut
 * deliberately calls @a wm_request_stop directly instead, bypassing
 * this entirely.
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients across every surface and desktop
 *
 * @see @c wm/shutdown.h for the full design on asking managed clients
 *      when stopping the window manager
 */
void wm_request_graceful_stop(void);

/**
 * @brief Reload the configuration from the configuration files
 *
 * @param wm Window manager instance
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the operation
 *
 * @note Complexity: @e O(n), where @e n is the number of parameters
 *       saved because it involves reading from the configuration file
 * @note Reloads @p config->randr from @c randr.json but does not call
 *       @a surface_action_apply_randr_profiles; an edited profile
 *       takes effect at the next call to that function (startup, or
 *       the matching output's next @c XCB_RANDR_NOTIFY_OUTPUT_CHANGE),
 *       not from this reload alone
 */
int wm_action_config_reload(const wm_td *wm);

/**
 * @brief Rearrange every visible window on the given surface's own
 *        current desktop
 *
 * A thin wrapper around @a enact_desktop_clients_rearrange.
 *
 * @param wm      Window manager instance
 * @param surface Surface whose current desktop to rearrange
 *
 * @note No-op if @p surface is null or has no current desktop
 */
void wm_action_rearrange(const wm_td *wm, surface_td *surface);

/**
 * @brief Return the desktop that currently contains a specific client
 *
 * Searches all surfaces and desktops managed by the singleton window
 * manager instance.
 *
 * @param client Client whose desktop is requested
 *
 * @return Pointer to the containing @c desktop_td, or @c NULL when the
 *         client is not found or the window manager is not initialized
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across all desktops
 */
desktop_td *wm_get_client_desktop(const client_td *client);

/**
 * @brief Return the managed surface with the given identifier
 *
 * Scans all surfaces handled by the singleton window manager instance
 * and returns the one whose @p id matches @p surface_id.
 *
 * @param surface_id Surface identifier
 *
 * @return Pointer to the matching @c surface_td, or @c NULL when no
 *         surface matches or the window manager is not initialized
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
surface_td *wm_get_surface_by_id(uint32_t surface_id);

/**
 * @brief Query whether the XSync extension is available on this server
 *
 * Used by @a client_init to decide whether to create a per-client
 * sync counter/alarm for @c _NET_WM_SYNC_REQUEST, and by
 * @a ccmd_client_resize to decide whether to throttle interactive
 * resize on that client's acknowledgement.
 *
 * @return Status of the query
 * @retval  true when @a wm_startup_init_sync found XSync present and
 *               queryable
 * @retval false otherwise (including when the window manager is not
 *               initialized)
 *
 * @note Complexity: @e O(1)
 */
bool wm_sync_is_available(void);

/**
 * @brief Return the list of surfaces managed by the singleton window
 *        manager instance
 *
 * Used by code outside @c src/wm/ (which cannot include the private
 * @c wm/internal.h singleton pointer directly) that needs the full
 * surface list rather than a single surface by ID (e.g.,
 * @a focus_apply) which needs it to look up and unfocus whichever
 * client was previously active.
 *
 * @return The managed surfaces list, or @c NULL when the window manager
 *         is not initialized
 *
 * @note Complexity: @e O(1)
 */
list_td *wm_get_surfaces(void);

/**
 * @brief Find which managed surface a given desktop belongs to
 *
 * @c desktop_td itself keeps no back-pointer to its own owning
 * @c surface_td (each surface's own @p desktops list points one way
 * only, surface to desktop); this is the reverse lookup, used by
 * @a desktop_action_recompute_urgent (@c desktop/dclient.c) to find the
 * surface a desktop's own cross-desktop urgency notification popup
 * needs to center on and compare @p desktop_cur against, without that
 * function's own signature having to grow a @c surface_td parameter
 * every one of its own several unrelated callers would then also have
 * to obtain and pass through.
 *
 * @param desktop Desktop to find the owning surface of
 *
 * @return The surface @p desktop belongs to, or @c NULL when
 *         @p desktop is @c NULL, no managed surface contains it, or
 *         the window manager is not initialized
 *
 * @note Complexity: @e O(s * d), where @e s is the number of managed
 *       surfaces and @e d the number of desktops per surface
 */
surface_td *wm_get_desktop_surface(const desktop_td *desktop);

/**
 * @brief Return the active configuration of the singleton window
 *        manager instance
 *
 * Used by code outside @c src/wm/ (which cannot include the private
 * @c wm/internal.h singleton pointer directly) that needs to read
 * configuration without already having a @c client_td / @c desktop_td
 * of its own that caches the specific sub-section it needs.
 *
 * @return The active configuration, or @c NULL when the window manager
 *         is not initialized
 *
 * @note @a desktop_action_recompute_urgent (@c desktop/dclient.c) is
 *       the first such caller, reading @p desktops.notify-activity
 * @note Complexity: @e O(1)
 *
 * @see @p client_td.config_base and @p desktop_td.config_base
 */
config_td *wm_get_config(void);

/**
 * @brief Return the key symbols table of the singleton window
 *        manager instance
 *
 * For a module needing @c xcb_key_symbols_t to resolve or re-resolve
 * a key binding (@a keyboard_load, @c input/kbd/bind.c, is the first
 * such caller outside @c wm.c itself) without already holding one of
 * its own, the same reasoning @a wm_get_config already documents for
 * itself above.
 *
 * @return The active key symbols table, or @c NULL when the window
 *         manager is not initialized or has not yet resolved one
 *
 * @note Complexity: @e O(1)
 */
xcb_key_symbols_t *wm_get_keysyms(void);

/**
 * @brief Mark the client owner desktop and surface as outdated
 *
 * Locates the desktop currently owning @p client and marks that desktop
 * and its surface for redraw on the next update cycle.
 *
 * @param client Client whose owner context should be redrawn
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces and desktops
 */
void wm_request_client_redraw(client_td *client);

/**
 * @brief Mark all surfaces and desktops as outdated
 *
 * Requests a full redraw on demand by setting the outdated flags across
 * every managed surface and desktop.
 *
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m is the number of desktops per surface
 */
void wm_request_full_redraw(void);

/**
 * @brief Recompute and publish EWMH root properties
 *
 * Synchronizes core EWMH metadata for each managed screen, including
 * desktop counts, current desktop, workarea, client lists, and active
 * window.
 *
 * @param wm Window manager instance
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients across all desktops
 */
void wm_ewmh_sync(wm_td *wm);

/**
 * @brief Initialize EWMH root support metadata
 *
 * Creates the supporting window and publishes @c _NET_SUPPORTED and
 * @c _NET_SUPPORTING_WM_CHECK properties.
 *
 * @param wm Window manager instance
 *
 * @return 0 on success, or non-zero on failure
 *
 * @note Complexity: @e O(1)
 */
int wm_ewmh_init(wm_td *wm);

/**
 * @brief Set the emergency exit flag to @c true
 */
void wm_emergency_exit_enable(void);

/**
 * @brief Macro that evaluates to the number of surfaces handled by the
 *        window manager
 *
 * @note Complexity: @e O(1)
 */
#define wm_surface_count(wm) \
  (((wm) == NULL) || (((wm)->surfaces) == NULL) ? 0 : ((wm)->surfaces)->size)


#endif  /* ! WM_H */
