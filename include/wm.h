/**
 * @file wm.h
 *
 * @brief Declaration of window manager structure and main functions
 */

#ifndef WM_H
#define WM_H


/* System includes */
#include <stdbool.h>    /* bool */
#include <stdio.h>      /* FILE */

/* ADT */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <screen.h>


/**
 * @brief Window manager structure with information about display,
 *        screens and windows to manage, as well as the configuration
 *        used and logging behaviour
 *
 * It stores the essential components of a window manager, keeping track
 * of both the screens' arrangements and the windows that inhabit them.
 */
typedef struct {
    bool is_running;                /**< Running state flag */
    list_td *screens;               /**< Screens list */
    config_td *config;              /**< Window manager configuration */

    void (*event_handler)(XEvent*); /**< Pointer to the event handler */

    /* Callbacks */
} wm_td;


/* Public interface */
/**
 * @brief Initialize window manager instance
 *
 * This function allocates memory for a @c wm_td structure, initializes
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
 * This function deallocates the memory used by the @c wm_td structure,
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
 * @brief Enters the main event handling loop of the window manager
 *
 * This function runs continuously while the window manager is active,
 * listening for X11 events and passing them to the event handler for
 * processing.  It uses @c XNextEvent to wait for incoming events from
 * the X server, enabling responsive behavior in window management.
 * The condition to end the loop is by setting @c is_running to @c false.
 *
 * @param wm Pointer to the initialized @c wm_td instance that contains
 *           the necessary state and configuration for the window manager
 *
 * @note The event loop will stop when the @c is_running flag is set to
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
