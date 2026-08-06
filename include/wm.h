/**
 * @file wm.h
 *
 * @brief Declaration of window manager structure and main functions
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

/* ADT includes */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <eventq.h>
#include <surface.h>


#ifndef RULES_TD_DECLARED
#define RULES_TD_DECLARED
typedef struct rules_s rules_td;
#endif

#ifndef SESSION_TD_DECLARED
#define SESSION_TD_DECLARED
typedef struct session_s session_td;
#endif


/**
 * @brief Window manager structure
 *
 * Core components of a window manager, maintaining the state of the
 * program as well as the relationships between different screens and
 * their respective windows.  It includes functionality for managing
 * configurations and handling events that affect window behavior and
 * user interactions.
 *
 * The @p is_running flag indicates whether the window manager is
 * currently operational, while the @p surfaces linked list holds
 * references to all surfaces being managed.  The @p config pointer
 * allows for customization of the window manager's settings, and the
 * @p event_handler is responsible for processing user inputs and system
 * events, keeping the window manager responsive and interactive.
 */
typedef struct {
    xcb_connection_t *connection;   /**< Pointer to XCB connection */
    xcb_ewmh_connection_t *ewmh;    /**< EWMH connection */
    xcb_window_t ewmh_support_win;  /**< '_NET_SUPPORTING_WM_CHECK' window */
    list_td *surfaces;              /**< List of surfaces */
    uint32_t screenp;               /**< Preferred screen */
    bool randr_available;           /**< XRandR extension availability */
    uint8_t randr_base_event;       /**< XRandR base event code */
    bool sync_available;            /**< XSync extension availability */
    uint8_t sync_base_event;        /**< XSync base event code */
    config_td *config;              /**< Window manager configuration */
    rules_td *rules;                /**< Window matching rules */
    session_td *session;            /**< Session hooks */
    const char *config_dir_prefix;  /**< Config dir. passed at startup,
                                         for @c NULL if the default
                                         config. dir. is used; kept to
                                         reuse it on config. reload */
    bool is_running;                /**< Running state flag */
    bool is_emergency_exit;         /**< Set when an emergency exit is
                                         requested; suppresses pending
                                         session hooks on shutdown */
} wm_td;


/* Public interface */
/**
 * @brief Initialize window manager instance
 *
 * Allocates memory for a @c wm_td structure, initializes its fields,
 * and opens a connection to the X server.  It also sets up the managed
 * windows array and initializes the current desktop index and running
 * state.
 *
 * @param display_name      Name of the display, or @c NULL for default
 * @param config_dir_prefix Configuration directory, or @c NULL to use
 *                          the default value
 *
 * @return Status of the initialization
 * @retval  0 Success
 * @retval  1 Failed to allocate memory
 * @retval  2 Cannot open X connection
 * @retval  3 Cannot open load configuration
 * @retval >3 Failed to initialize data structures
 * @retval -1 Singleton was already initialized; no action taken
 *
 * @note If @p display_name is @c NULL, the initialization attempts to
 *       get the "DISPLAY" environment variable, if set.
 * @note This function uses a singleton pattern
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       to initialize, and @e m the number of desktops per window, as
 *       for the initialization requires iterate over a list of lists
 */
int wm_start(const char *display_name, const char *config_dir_prefix);

/**
 * @brief Destroy window manager instance
 *
 * Deallocates the memory used by the @p wm_td structure, including the
 * managed windows and closes the connection to the X server.
 *
 * @return Status of the operation
 * @return  0 Success
 * @return  1 No operation has been performed
 *
 * @note Passing a @c NULL pointer has no effect
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
 * @retval 0 Success
 * @retval 1 If the window manager singleton is not initialized
 */
int wm_request_stop(void);

/**
 * @brief Reload the configuration from the configuration files
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the operation
 *
 * @note Complexity: @e O(n), where @e n is the number of parameters
 *       saved because it involves reading from the configuration file
 */
int wm_action_config_reload(void);

/**
 * @brief Perform actions required before destroying the window manager
 *
 * Executes necessary actions required before invoking @a wm_stop, such
 * as sending additional events to the @p eventq priority queue and
 * ensuring it's completely empty by calling the required actions.
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the operation
 */
int wm_action_exit(void);

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
 * and returns the one whose @c id matches @p surface_id.
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
 * Used by @c client_manage to decide whether to create a per-client
 * sync counter/alarm for @c _NET_WM_SYNC_REQUEST, and by
 * @c wcmd_client_resize to decide whether to throttle interactive
 * resize on that client's acknowledgement.
 *
 * @return @c true when @c startup_sync_init found XSync present and
 *         queryable, @c false otherwise (including when the window
 *         manager is not initialized)
 *
 * @note Complexity: @e O(1)
 */
bool wm_sync_available(void);

/**
 * @brief Return the list of surfaces managed by the singleton window
 *        manager instance
 *
 * Used by code outside @c src/wm/ (which cannot include the private
 * @c wm/internal.h singleton pointer directly) that needs the full
 * surface list rather than a single surface by ID (e.g.,
 * @c focus_apply) which needs it to look up and unfocus whichever
 * client was previously active.
 *
 * @return The managed surfaces list, or @c NULL when the window
 *         manager is not initialized
 *
 * @note Complexity: @e O(1)
 */
list_td *wm_get_surfaces(void);

/**
 * @brief Return the configuration directory prefix
 *
 * Returns the value of @c config_dir_prefix passed to @a wm_start, or
 * @c NULL if the default directory is being used.  The returned pointer
 * is valid for the lifetime of the window manager instance.
 *
 * @return Configuration directory prefix, or @c NULL
 *
 * @note Complexity: @e O(1)
 */
const char *wm_get_config_dir(void);

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
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients across all desktops
 */
void wm_ewmh_sync(void);

/**
 * @brief Initialize EWMH root support metadata
 *
 * Creates the supporting window and publishes @c _NET_SUPPORTED and
 * @c _NET_SUPPORTING_WM_CHECK properties.
 *
 * @return 0 on success, or non-zero on failure
 *
 * @note Complexity: @e O(1)
 */
int wm_ewmh_init(void);

/**
 * @brief Run periodic EWMH maintenance tasks
 *
 * Sends @c _NET_WM_PING probes to responsive clients, marks timed out
 * clients as unresponsive, and refreshes EWMH metadata that depends on
 * runtime state.
 */
void wm_ewmh_tick(void);

/**
 * @brief Set the emergency exit flag to @c true
 */
void wm_enable_emergency_exit(void);

/**
 * @brief Macro that evaluates to the number of surfaces handled by the
 *        window manager
 *
 * @note Complexity: @e O(1)
 */
#define wm_surface_count(wm) \
  (((wm) == NULL) || (((wm)->surfaces) == NULL) ? 0 : ((wm)->surfaces)->size)


#endif  /* ! WM_H */
