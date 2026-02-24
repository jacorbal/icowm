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


/**
 * @brief Window manager structure
 *
 * Core components of a window manager, maintaining the state of the
 * application as well as the relationships between different screens
 * and their respective windows.  It includes functionality for managing
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
    list_td *surfaces;              /**< List of surfaces */
    uint32_t screenp;               /**< Preferred screen */
    config_td *config;              /**< Window manager configuration */
    bool is_running;                /**< Running state flag */
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
 * @retval  4-7 Failed to initialize data structures
 * @retval -1 Singleton was already initialized; no action taken
 *
 * @note If @p display_name is @c NULL, the initialization attempts to
 *       get the "DISPLAY" environment variable, if set.
 * @note This function uses a singleton pattern
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces to
 *       initialize, and @e m the number of desktops per window, as for
 *       the initialization requires iterate over a list of lists
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
 * @note Complexity: @e O(n^2 + m * n^2), where @e n is the number of
 *       surfaces, and @e m is the number of desktops per surface, as it
 *       iterates through the array of windows to free each one of them
 */
int wm_stop(void);

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
 * @brief Save the configuration, overwriting the existing one
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the operation
 *
 * @note Complexity: @e O(n), where @e n is the number of parameters
 *       saved because it involves writing to the configuration file
 */
int wm_action_config_save(void);

/**
 * @brief Insert a surface into the window manager's surface list
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the operation
 */
int wm_action_surface_ins(void);

/**
 * @brief Remove a surface from the window manager's surface list
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the operation
 */
int wm_action_surface_rem(void);

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
 * @brief Macro that evaluates to the number of surfaces handled by the
 *        window manager
 *
 * @note Complexity: @e O(1)
 */
#define wm_surface_count(wm) \
  (((wm) == NULL) || (((wm)->surfaces) == NULL) ? 0 : ((wm)->surfaces)->size)


#endif  /* ! WM_H */
