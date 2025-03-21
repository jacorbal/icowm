/**
 * @file desktop.h
 *
 * @brief Desktop structure declaration
 */

#ifndef DESKTOP_H
#define DESKTOP_H


/* ADT */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <window.h>


/**
 * @brief Desktop structure
 */
typedef struct {
    unsigned int screen_id;     /**< Screen index */
    unsigned int id;            /**< Desktop index */

    struct background_s {
//        bool is_image;          /** Color or image for background */
        union {
            unsigned long color;    /**< Background color */
//            Pixmap pixmap;           /**< Background image */
        } bg;                   /**< Background information*/
    } background;

    list_td *windows;           /**< Windows array for this desktop */
    window_td *window_active;   /**< Pointer to active window */

    struct config_base_s *config_base;
    struct config_theme_s *config_theme;
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
 * @brief Clear a desktop by removing all its windows
 *
 * This function deallocates each and every window of the desktop and
 * resets the window counter to zero.
 *
 * @param desktop Desktop to be cleared from windows
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
 * @retval 0 Success on removal
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
 * @retval 0 Success on removal
 *
 * @note Complexity: @e O(1)
 */
int desktop_window_rem(desktop_td *desktop, window_td *window);

/**
 * @brief Remove a window from the desktop searching by id
 *
 * @param desktop   Pointer to the desktop where to remove the window
 * @param window_id Identifier of the window to be removed
 *
 * @return Status of the operation
 * @retval 0 Success on removal
 *
 * @note Complexity: @e O(1)
 */
int desktop_window_rem_by_id(desktop_td *desktop,
        unsigned int window_id);

/**
 * @brief Macro that evaluates to the window count of the desktop
 *
 * @note Complexity: @e O(1)
 */
#define desktop_window_count(d) ((d) ? d->windows->size : 0)


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


#endif  /* ! DESKTOP_H */
