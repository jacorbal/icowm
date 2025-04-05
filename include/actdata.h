/**
 * @file actdata.h
 *
 * @brief Object data structures when objects need to update their
 *        properties by events by the execution of an action
 */

#ifndef ACTDATA_H
#define ACTDATA_H

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <action.h>
#include <desktop.h>
#include <surface.h>
#include <window.h>


/**
 * @brief Window data to store information when updating the window by
 *        an action
 *
 * @see action_window_e
 */
typedef struct action_data_window_s {
    window_td *window;                  /**< Window affected */
    enum action_window_e action_window; /**< Action for this window */

    union {
        char *name;
        char *visible_name;
        char *icon_name;
        char *visible_icon_name;
        char *class_name;
        char *role_name;
        struct geometry_s geometry;
    } new_data;                         /** New values to update */
} action_data_window_td;


/**
 * @brief Desktop data to store information when updating the desktop by
 *        an action
 *
 * @see action_window_e
 */
typedef struct {
    desktop_td *desktop;                    /**< Desktop affected */
    enum action_desktop_e action_desktop;   /**< Action for this desktop */

    /* TODO */
} action_data_desktop_td;


/**
 * @brief Screen data to store information when updating the surface by
 *        an action
 *
 * @see action_window_e
 */
typedef struct {
    surface_td *surface;                  /**< Surface affected */
    enum action_surface_e action_surface; /**< Action for this surface */

    /* TODO */
} action_data_surface_td;


/* Public interface */
/**
 * @brief Allocate memory for the window data structure
 *
 * @param window        Pointer to the window that's going to be updated
 * @param action_window Action to perform with this data
 *
 * @return Pointer to the new allocated structure, or @c NULL otherwise
 *
 * @note The values of the structure must be filled manually, not at the
 *       initialization
 * @note Complexity: @e O(1)
 */
action_data_window_td *action_data_window_init(window_td *window,
        enum action_window_e action_window);

/**
 * @brief Deallocate window data structure
 *
 * @param action_data_window Pointer to the data structure to deallocate
 *
 * @note Complexity: @e O(1)
 */
void action_data_window_destroy(action_data_window_td *action_data_window);

/**
 * @brief Allocate memory for the desktop data structure
 *
 * @param desktop        Pointer to the desktop that's going to be updated
 * @param action_desktop Action type to perform with this data
 *
 * @return Pointer to the new allocated structure, or @c NULL otherwise
 *
 * @note The values of the structure must be filled manually, not at the
 *       initialization
 * @note Complexity: @e O(1)
 */
action_data_desktop_td *action_data_desktop_init(desktop_td *desktop,
        enum action_desktop_e action_desktop);

/**
 * @brief Deallocate desktop data structure
 *
 * @param action_data_desktop Pointer to the data structure to deallocate
 *
 * @note Complexity: @e O(1)
 */
void action_data_desktop_destroy(action_data_desktop_td *action_data_desktop);

/**
 * @brief Allocate memory for the surface data structure
 *
 * @param surface        Pointer to the surface that's going to be updated
 * @param action_surface Action type to perform with this data
 *
 * @return Pointer to the new allocated structure, or @c NULL otherwise
 *
 * @note The values of the structure must be filled manually, not at the
 *       initialization
 * @note Complexity: @e O(1)
 */
action_data_surface_td *action_data_surface_init(surface_td *surface,
        enum action_surface_e action_surface);

/**
 * @brief Deallocate surface data structure
 *
 * @param action_data_surface Pointer to the data structure to deallocate
 *
 * @note Complexity: @e O(1)
 */
void action_data_surface_destroy(action_data_surface_td *action_data_surface);


#endif  /* ! ACTDATA_H */
