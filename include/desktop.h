/**
 * @file desktop.h
 *
 * @brief Desktop structure declaration
 */

#ifndef DESKTOP_H
#define DESKTOP_H

/* System includes */
#include <stdbool.h>    /* bool */

/* ADT includes */
#include <adt/ohtbl.h>  /* Open-addressed hash table (closed hashing) */

/* Definitions and inclusions */
#include <defs/wm.h>

/* Project includes */
#include <config.h>
#include <window.h>


/**
 * @brief Structure for a virtual desktop within an X11 screen
 *
 * Each desktop can be customized with unique backgrounds and themes,
 * where the background can either be a solid color or an pixmap image.
 * The structure tracks its own active window, facilitating the
 * management of user interactions within that desktop space.
 *
 * The @p is_outdated flag serves to identify when the desktop's
 * attributes or properties have changed and need to be updated,
 * ensuring that users always have access to the most current
 * information about their environment.
 */
typedef struct {
    unsigned int screen_id;                 /**< Screen index */
    unsigned int id;                        /**< Desktop index */

    char name[DESKTOP_MAX_LENGTH_NAME];     /**< Desktop name */

    struct background_s {
//        bool is_image;                      /**< BG color or image? */
        union {
            unsigned long color;            /**< Background color */
//            Pixmap pixmap;                  /**< Background image */
        } bg;                               /**< Background information */
    } background;

    ohtbl_td *windows;                      /**< Windows hash table */
    window_td *window_active;               /**< Pointer to active window */

    struct config_base_s *config_base;      /**< Base configuration */
    struct config_theme_s *config_theme;    /**< Theme configuration */

    bool is_outdated;   /**< Flag when data needs to be updated */
} desktop_td;


/* Public interface */
/**
 * @brief Initialize a new desktop
 *
 * @param screen_id    Screen identifier where this desktop belongs
 * @param desktop_id   Desktop identifier
 * @param config_base  Pointer to base configuration
 * @param config_theme Pointer to theme configuration
 *
 * @return Pointer to new desktop or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
desktop_td *desktop_init(unsigned int screen_id,
        unsigned int desktop_id,
        struct config_base_s *config_base,
        struct config_theme_s *config_theme);

/**
 * @brief Free memory for allocated desktop
 *
 * @param desktop Desktop to deallocate
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       windows, as it iterates through the array of windows to free
 *       each one of them
 */
void desktop_destroy(desktop_td *desktop);

/**
 * @brief Soft desktop update
 *
 * @param desktop Pointer to the desktop to update softly
 *
 * @note Complexity: @e O(1)
 */
void desktop_update(desktop_td *desktop);

/**
 * @brief Full desktop update
 *
 * Updates the desktop by updating all its windows.
 *
 * @param desktop Pointer to the desktop to update fully
 *
 * @note Complexity: @e O(1) because that's the order of the access to
 *       a hash table of windows
 */
void desktop_update_full(desktop_td *desktop);

/**
 * @brief Clear a desktop by removing all its windows
 *
 * Deallocates each and every window of the desktop and resets the
 * window counter to zero.
 *
 * @param desktop Pointer to the desktop to be cleared from windows
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       windows, as it iterates through the array of windows to free
 *       each one of them
 */
void desktop_clear(desktop_td *desktop);

/**
 * @brief Add a previously allocated window to the desktop
 *
 * @param desktop Pointer to the desktop where to add the new window
 * @param window  Pointer to the window to be added to the desktop
 *
 * @return Status of the operation
 * @retval  0 Success on removal
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_window_add(desktop_td *desktop, window_td *window);

/**
 * @brief Remove a window from the desktop
 *
 * @param desktop Pointer to the desktop where to remove the window
 * @param window  Pointer to the window to be removed from the desktop
 *
 * @return Status of the operation
 * @retval  0 Success on removal
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_window_rem(desktop_td *desktop, window_td *window);

/**
 * @brief Rename the desktop
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param name    New name for the desktop
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_rename(desktop_td *desktop, const char *name);

/**
 * @brief Send a window to another desktop
 *
 * @param desktop    Pointer to the desktop to receive the action
 * @param window     Pointer to the window to be sent
 * @param desktop_id Destination desktop identifier
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_send_window(desktop_td *desktop, window_td *window,
        unsigned int desktop_id);

/**
 * @brief Update the desktop background color
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param color   New color
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_background_update(desktop_td *desktop,
        unsigned int color);

/**
 * @brief Set a window to the front
 *
 * Brings the specified window to the top of the stacking order.
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param window  Pointer to the window to be sent to the front
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_window_send_front(desktop_td *desktop,
        window_td *window);

/**
 * @brief Set a window to the back
 *
 * Sends the specified window to the bottom of the stacking order.
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param window  Pointer to the window to be sent to the back
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_window_send_back(desktop_td *desktop,
        window_td *window);

/**
 * @brief Rearrange windows on the current desktop
 *
 * Alters the positions of windows on the current desktop.
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of windows to
 *       rearrange
 */
int desktop_action_windows_rearrange(desktop_td *desktop);

/**
 * @brief Iconify (minimize) all windows on the current desktop
 *
 * Set all visible windows on the current desktop to an iconified state
 * (also, technically, minimized).
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of windows on the
 *       desktop
 */
int desktop_action_windows_iconify_all(desktop_td *desktop);

/**
 * @brief Cycle through active windows on the current desktop
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of active windows
 */
int desktop_action_cycle_windows_active(desktop_td *desktop);

/**
 * @brief Cycle through iconified windows on the current desktop
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of iconified
 *       windows
 */
int desktop_action_cycle_windows_icons(desktop_td *desktop);

/**
 * @brief Lock the current desktop session,  preventing unauthorized
 *        access
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_lock(desktop_td *desktop);

/**
 * @brief Unlock the current desktop session, allowing user access
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_unlock(desktop_td *desktop);

/**
 * @brief Change the layout of the current desktop
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param layout New layout configuration
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_set_layout(desktop_td *desktop, const char *layout);

/**
 * @brief Launch a new application
 *
 * @param desktop          Pointer to the desktop to receive the action
 * @param application_path Path to the executable of the application
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_application_launch(desktop_td *desktop,
        const char *application_path);

/**
 * @brief Terminate an application
 *
 * Stops the specified application that is running in the desktop
 * session by killing it.
 *
 * @param desktop        Pointer to the desktop to receive the action
 * @param application_id Identifier of the application to be terminated
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_application_kill(desktop_td *desktop,
        unsigned int application_id);

/**
 * @brief Macro that evaluates to the active window of the desktop
 *
 * @note Complexity: @e O(1)
 */
#define desktop_window_active(d) ((d)->window_active)

/**
 * @brief Macro that sets the active window of the desktop
 *
 * @note If no window show be focused/active, @c NULL is the right value
 * @note Complexity: @e O(1)
 */
#define desktop_set_window_active(d, w) (((d)->window_active) = (w))

/**
 * @brief Macro that evaluates to the window count of the desktop
 *
 * @note Complexity: @e O(1)
 */
#define desktop_window_count(d) ((d) ? d->windows->size : 0)


#endif  /* ! DESKTOP_H */
