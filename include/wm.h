/**
 * @file wm.h
 *
 * @brief Declaration of window manager structure and main functions
 */

#ifndef WM_H
#define WM_H


/* System includes */
#include <stdbool.h>    /* bool */

/* ADT */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <event.h>
#include <screen.h>


/**
 * @brief Window manager structure
 *
 * This structure represents the core components of a window manager,
 * maintaining the state of the application as well as the relationships
 * between different screens and their respective windows.  It includes
 * functionality for managing configurations and handling events that
 * affect window behavior and user interactions.
 *
 * The @p is_running flag indicates whether the window manager is
 * currently operational, while the @p screens linked list holds
 * references to all screens being managed.  The @p config pointer
 * allows for customization of the window manager's settings, and the
 * @p event_handler is responsible for processing user inputs and system
 * events, keeping the window manager responsive and interactive.
 */
typedef struct {
    bool is_running;                    /**< Running state flag */
    list_td *screens;                   /**< Screens list */
    config_td *config;                  /**< Window manager config. */

    event_handler_td *event_handler;    /**< Pointer to event handler */

    /* Callbacks? */
} wm_td;


/* Public interface */
/**
 * @brief Initialize window manager instance
 *
 * This function allocates memory for a @p wm_td structure, initializes
 * its fields, and opens a connection to the X server.  It sets up the
 * managed windows array and initializes the current desktop index and
 * running state.
 *
 * @param config Pointer to window manager configuration
 *
 * @return Pointer to newly created window manager instance, or @c NULL
 *
 * @note Is responsibility of the caller to free the memory allocated
 *       for the @c wm_td instance using the @e wm_destroy function
 * @note This function uses a singleton pattern
 * @note Complexity: @e O(1)
 */
wm_td *wm_init(config_td *config);

/**
 * @brief Destroy window manager instance
 *
 * This function deallocates the memory used by the @p wm_td structure,
 * including the managed windows and closes the connection to the
 * X server.
 *
 * @param wm Pointer to the window manager instance to free its memory
 *
 * @note Passing a @c NULL pointer has no effect
 * @note Complexity: @e O(n), where @e n is the number of screens, as it
 *       iterates through the array of windows to free each one of them
 */
void wm_destroy(wm_td *wm);

/**
 * @brief Soft window manager update
 *
 * @param wm Pointer to the window manager instance to update it
 *
 * @note Complexity: @e O(1)
 */
void wm_update(wm_td *wm);

/**
 * @brief Full window manager update
 *
 * This function updates the window manager by updating every window on
 * every desktop of every screen.
 *
 * @param wm Pointer to the window manager instance to update it fully
 *
 * @note Complexity: @e O(n*m), where @e n is the number of screens and
 *       @e m is the number of desktops on the screen
 */
void wm_update_full(wm_td *wm);

/**
 * @brief Enters the main event handling loop of the window manager
 *
 * This function runs continuously while the window manager is active,
 * listening for X11 events and passing them to the event handler for
 * processing.  It uses @p XNextEvent to wait for incoming events from
 * the X server, enabling responsive behavior in window management.
 * The condition to end the loop is by setting @p is_running to @c false.
 *
 * @param wm Pointer to the initialized @p wm_td instance that contains
 *           the necessary state and configuration for the window manager
 *
 * @note The event loop will stop when the @p is_running flag is set to
 *       @c false, which should be handled in response to user actions
 *       or when the window manager is terminating
 * @note Complexity: @e O(1) for each event processed; however, the
 *       overall time complexity depends on the number of events
 *       processed, so each call to @e event_handle may have a different
 *       complexity based on the event type and operations performed
 */
void wm_loop(wm_td *wm);

/**
 * @brief Macro that evaluates to the screen count of the window manager
 *
 * @note Complexity: @e O(1)
 */
#define wm_screen_count(wm) ((wm) ? wm->screens->size : 0)


#endif  /* ! WM_H */
