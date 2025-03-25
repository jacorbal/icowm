/**
 * @file wm.h
 *
 * @brief Declaration of window manager structure and main functions
 */

#ifndef WM_H
#define WM_H


/* System includes */
#include <stdbool.h>    /* bool */

/* ADT includes */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
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
    Display *display;   /**< Pointer to X11 display */
    list_td *screens;   /**< List of screens */
    config_td *config;  /**< Window manager configuration details */
    bool is_running;    /**< Running state flag */
} wm_td;


/* Public interface */
/**
 * @brief Initialize window manager instance
 *
 * Allocates memory for a @p wm_td structure, initializes its fields,
 * and opens a connection to the X server.  It sets up the managed
 * windows array and initializes the current desktop index and running
 * state.
 *
 * @param display_name Name of the display
 *
 * @return Status of the initialization
 * @retval  0 Success
 * @retval  1 Failed to allocate memory
 * @retval  2 Cannot open X display
 * @retval  3 Cannot open load configuration
 * @retval  4-7 Failed to initialize data structures
 * @retval -1 Singleton was already initialized; no action taken
 *
 * @note If @p display_name is @c NULL, the inialization tries to get
 *       the environment variable "DISPLAY", if set.
 * @note This function uses a singleton pattern
 * @note Complexity: @e O(n*m), where @e n is the number of screens to
 *       initialize, and @e m the number of desktops per window, as for
 *       the initialization requires iterate over a list of lists
 */
int wm_start(const char *display_name);

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
 * @note Complexity: @e O(n^2 + m*n^2), where @e n is the number of
 *       screens, and @e m is the number of desktops per screen, as it
 *       iterates through the array of windows to free each one of them
 */
int wm_stop(void);


/**
 * @brief Count the screens in the window manager
 *
 * @return Number of screens handled by the window manager
 *
 * @note Complexity: @e O(1)
 */
size_t wm_screen_count(void);


#endif  /* ! WM_H */
