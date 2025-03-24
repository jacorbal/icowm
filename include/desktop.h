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
#include <adt/ohtbl.h>  /* Open-addressed hash table (closed hasing) */

/* Project includes */
#include <config.h>
#include <window.h>


// TODO: Put this in a configuration file!

/**
 * @brief Initial capacity of windows for the desktop
 *
 * Number of windows that the desktop is initialized with.  A higher
 * initial capacity may reduce the need for resizing the underlying data
 * structure as windows are added to the open-addressed hash table.
 */
#define DESKTOP_INITIAL_CAPACITY (256)  /* (512) */

/**
 * @brief Maximum number of characters allowed in the name of the
 *        desktop, including the null terminator
 */
#define DESKTOP_MAX_LENGTH_NAME (64)


/**
 * @brief Desktop structure
 *
 * This structure represents a virtual desktop within an X11 screen,
 * containing specifics about the desktop's properties, including its
 * associated windows, configuration settings, and visual elements.
 *
 * Each desktop can be customized with unique backgrounds and themes,
 * where the background can either be a solid color or an pixmap image.
 * The structure tracks its own active window, facilitating the
 * management of user interactions within that desktop space.
 *
 * The  @p is_outdated flag serves to identify when the desktop's
 * attributes or properties have changed and need to be updated,
 * ensuring that users always have access to the most current
 * information about their environment.
 */
typedef struct {
    unsigned int screen_id;     /**< Screen index */
    unsigned int id;            /**< Desktop index */

    char name[DESKTOP_MAX_LENGTH_NAME]; /** Desktop name*/

    struct background_s {
//        bool is_image;          /** Color or image for background */
        union {
            unsigned long color;    /**< Background color */
//            Pixmap pixmap;           /**< Background image */
        } bg;                   /**< Background information*/
    } background;

    ohtbl_td *windows;          /**< Windows hash table */
    window_td *window_active;   /**< Pointer to active window */

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
 * This function updates the desktop by updating all its windows.
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
 * This function deallocates each and every window of the desktop and
 * resets the window counter to zero.
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
 *
 * @note Complexity: @e O(1)
 */
int desktop_window_add(desktop_td *desktop, window_td *window);

/**
 * @brief Remove a window from the desktop
 *
 * @param desktop Pointer to the desktop where to remove the window
 * @param window  Pointer to the window to be removed from the desktop
 *
 * @return Status of the operation
 * @retval  0 Success on removal
 *
 * @note Complexity: @e O(1)
 */
int desktop_window_rem(desktop_td *desktop, window_td *window);

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
