/**
 * @file screen.h
 *
 * @brief Screen structure declaration
 */

#ifndef SCREEN_H
#define SCREEN_H

/* System includes */
#include <stdbool.h>    /* bool */

/* External libraries */
#include <X11/Xlib.h>   /* Window, Colormap */

/* ADT */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Common type structures */
#include <types/pair.h> /* dimensions_s, size_s */

/* Project includes */
#include <config.h>
#include <desktop.h>


/**
 * @brief Screen structure
 *
 * This structure represents a screen in an X11 environment,
 * encapsulating attributes and configurations necessary for managing
 * display properties, screen size, and associated visual objects.
 *
 * It maintains a circular list of desktops to support multiple virtual
 * desktops on the screen, allowing for more organized and flexible user
 * interfaces.  The structure also includes parameters for color depth,
 * DPI settings, and a reference to configuration data for dynamic
 * management and customization.
 *
 * The @p is_outdated flag provides a mechanism for tracking when screen
 * data needs to be refreshed, ensuring the screen information remains
 * synchronized with underlying changes in the X11 environment or user
 * preferences.
 */
typedef struct {
    unsigned int id;            /**< Screen unique identifier or index */

    Display *display;           /**< Pointer to X11 display */
    Screen *screen;             /**< Pointer to X11 screen */
    Visual *visual;             /**< Pointer to X11 associated visual */
    Window root;                /**< Root window for this screen */

    int depth;                  /**< Color depth */
    struct size_s dpi;          /**< Screen DPI (dots per inch) */
    Colormap colormaps;         /**< Color maps */

    struct dimensions_s dim;    /**< Screen dimensions (px) */

    unsigned int desktop_count; /**< Number of desktops */
    unsigned int desktop_cur;   /**< Index of current desktop */
    cdlist_td *desktops;        /**< Circular list of desktops */

    config_td *config;          /**< Configuration */

    bool is_outdated;           /**< Flag when data needs to be updated */
} screen_td;


/* Public interface */
/**
 * @brief Initialize a new screen
 *
 * @param display       Pointer to X11 display
 * @param screen_id     Screen identifier
 * @param desktop_count Number of desktops on this screen
 *
 * @return Pointer to new screen, or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
screen_td *screen_init(Display *display, const unsigned int screen_id,
        unsigned int desktop_count, config_td *config);

/**
 * @brief Free allocated memory for a screen
 *
 * @param screen Pointer to the screen to deallocate
 *
 * @note Complexity: @e O(n), where @e n is the number of desktop, as it
 *       iterates through the array of windows to free each one of them
 */
void screen_destroy(screen_td *screen);

/**
 * @brief Soft screen update
 *
 * @param screen Pointer to the screen to soft update
 *
 * @note Complexity: @e O(1)
 */
void screen_update(screen_td *screen);

/**
 * @brief Full screen update
 *
 * This function updates the screen by updating every window of every
 * desktop
 *
 * @param screen Pointer to the screen to full update
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
void screen_update_full(screen_td *screen);

/**
 * @brief Resize the specified screen to the new dimensions
 *
 * @param screen Pointer to the screen to be resized
 * @param width  New width for the screen in pixels
 * @param height New height for the screen in pixels
 *
 * @note Complexity: @e O(1)
 */
void screen_resize(screen_td *screen,
        unsigned int width, unsigned int height);

/**
 * @brief Add a new desktop to the list
 *
 * @param screen  Pointer to the screen structure
 * @param desktop Pointer to the desktop to be added
 *
 * @return Status of the adding operation
 * @retval  0 Success
 * @retval  1 Failed to insert the desktop to the list
 * @retval -1 Invalid screen
 */
int screen_desktop_add(screen_td *screen, desktop_td *desktop);

/**
 * @brief Remove a desktop from the list by its ID
 *
 * @param screen Pointer to the screen structure.
 * @param desktop_id ID of the desktop to be removed
 *
 * @return Status of the remova operation
 * @retval  0 Success
 * @retval  1 Failed to remove the desktop from the list
 * @retval  2 Could not find the desktop matching that ID
 * @retval -1 Invalid screen or no desktops
 */
int screen_desktop_rem(screen_td *screen, unsigned int desktop_id);

/**
 * @brief Get a desktop from the list by its ID
 *
 * This function retrieves a pointer to a desktop with the specified ID
 * from the screen.  If the desktop is found, its pointer is returned;
 * otherwise, @c NULL is returned.
 *
 * @param screen     Pointer to the screen structure
 * @param desktop_id ID of the desktop to retrieve
 *
 * @return Pointer to the desktop if found, or @c NULL otherwise
 */
desktop_td *screen_desktop_get(screen_td *screen,
        unsigned int desktop_id);

/**
 * @brief Get the previous desktop in the list, optionally cycling
 *
 * This function searches for the desktop with the given ID and returns
 * the previous desktop in the circular list of desktops.  If the
 * current desktop is the first in the list, and cycling is enabled, it
 * will return the last desktop.
 *
 * @param screen     Pointer to the screen structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, the function will cycle back to the
 *                   last desktop if the current is the first
 *
 * @return Pointer to the previous desktop or @c NULL if not
 *         found or not invalid
 */
desktop_td *screen_desktop_prev(screen_td *screen,
        unsigned int desktop_id, bool cycle);

/**
 * @brief Get the next desktop in the list, optionally cycling
 *
 * This function searches for the desktop with the given ID and returns
 * the next desktop in the circular list of desktops.  If the current
 * desktop is the last in the list, and cycling is enabled, it will
 * return the first desktop.
 *
 * @param screen     Pointer to the screen structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, the function will cycle to the first
 *                   desktop if the current is the last
 *
 * @return Pointer to the next desktop or @c NULL if not found or not
 *         valid
 */
desktop_td *screen_desktop_next(screen_td *screen,
        unsigned int desktop_id, bool cycle);

/**
 * @brief Select the previous desktop, optionally cycling
 *
 * This function attempts to select the desktop that precedes the
 * current one in the list.  If the current desktop is the first and
 * cycle mode is enabled, it will select the last desktop updating
 * @p desktop_cur.
 *
 * @param screen Pointer to the screen structure
 * @param cycle  If @c true, will cycle to the last desktop if the
 *               current is the first
 *
 * @return Status of the selection
 * @retval  0 Sucess
 * @retval  1 No previous desktop found
 * @retval -1 Invalid screen or no desktops
 */
int screen_desktop_select_prev(screen_td *screen, bool cycle);

/**
 * @brief Select the next desktop, optionally cycling
 *
 * This function attempts to select the desktop that follows the current
 * one in the list.  If the current desktop is the last and cycle mode
 * is enabled, it will select the first desktop updating @p desktop_cur.
 *
 * @param screen Pointer to the screen structure
 * @param cycle  If @c true, will cycle to the first desktop if the
 *               current is the last
 *
 * @return Status of the selection
 * @retval  0 Sucess
 * @retval  1 No next desktop found
 * @retval -1 Invalid screen or no desktops
 */
int screen_desktop_select_next(screen_td *screen, bool cycle);

/**
 * @brief Select a specific desktop by its ID
 *
 * This function selects a desktop by its ID.  If the ID is valid, it
 * changes the current desktop to the specified ID updating
 * @p desktop_cur.  Returns an error and updates nothing if the ID is
 * not found in the list of desktops.
 *
 * @param screen     Pointer to the screen structure
 * @param desktop_id ID of the desktop to select
 *
 * @return Status of the selection
 * @retval  0 Sucess
 * @retval  1 No next desktop found
 * @retval -1 Invalid screen or no desktops
 */
int screen_desktop_select(screen_td *screen, unsigned int desktop_id);

/**
 * @brief Macro that evaluates to the desktop count of the screen
 *
 * @note Complexity: @e O(1)
 */
#define screen_desktop_count(s) ((s) ? s->desktops->size : 0)


#endif  /* ! SCREEN_H */
