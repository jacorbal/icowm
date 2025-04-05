/**
 * @file surface.h
 *
 * @brief Surface structure declaration
 *
 * Defines the structure that represents a screen within the window
 * manager environment that includes a list of workspaces associated
 * with that surface and an index indicating which workspace is
 * currently active.
 */

#ifndef SURFACE_H
#define SURFACE_H

/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* size_t */

/* X11 includes */
#include <X11/Xlib.h>   /* Window, Colormap */

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Type includes */
#include <types/pair.h> /* dimensions_s, size_s */

/* Project includes */
#include <config.h>
#include <desktop.h>


/* '<desktop.h>': Forward declaration of the type 'desktop_td', allowing
 *                it to be referenced without a complete definition,
 *                which helps to prevent circular dependencies and
 *                reduces compilation dependencies */
//typedef struct desktop_s desktop_td;


/**
 * @brief Structure to hold properties of the visual
 */
struct visual_properties_s {
    Colormap colormaps;         /**< Color maps for the visual */
    int depth;                  /**< Color depth (bits per pixel) */
};


/**
 * @brief Structure to hold properties of the surface
 */
struct surface_properties_s {
    struct dimensions_s dim;    /**< Screen dimensions (px) */
    struct dimensions_s dim_mm; /**< Screen dimensions (mm) */
    struct dpi_s dpi;           /**< Dots per pixel */

    struct {
        Visual *visual;                         /**< Associated visual */
        struct visual_properties_s properties;  /**< Visual properties */
    } visual_info;
};


/**
 * @brief Structure for a surface in an X11 environment
 *
 * Maintains a circular list of desktops to support multiple virtual
 * desktops on the surface, allowing for more organized and flexible
 * user interfaces.  The structure also includes parameters for color
 * depth, DPI settings, and a reference to configuration data for
 * dynamic management and customization.
 *
 * The @p is_outdated flag indicates whether the surface data needs to
 * be refreshed, ensuring the surface information remains synchronized
 * with underlying changes in the X11 environment or user preferences.
 */
typedef struct surface_s {
    XID id;            /**< Screen unique identifier or index */

    Display *display;           /**< Pointer to X11 display */
    Screen *xsurface;            /**< Pointer to X11 surface */
    Window root;                /**< Root window for this surface */

    /* Properties */
    struct surface_properties_s properties;

    size_t desktop_count;       /**< No. of desktops for this surface */
    XID desktop_cur;   /**< Index of current desktop */
    cdlist_td *desktops;        /**< Circular list of desktops */

    config_td *config;          /**< Configuration */

    bool fullsurface;            /**< Full surface or not */
    bool is_outdated;           /**< Flag if data needs to be updated */
} surface_td;


/* Public interface */
/**
 * @brief Initialize a new surface
 *
 * @param display       Pointer to X11 display
 * @param surface_id     Screen identifier
 * @param desktop_count Number of desktops on this surface
 *
 * @return Pointer to new surface, or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
surface_td *surface_init(Display *display, const XID surface_id,
        unsigned int desktop_count, config_td *config);

/**
 * @brief Free allocated memory for a surface
 *
 * @param surface Pointer to the surface to deallocate
 *
 * @note Complexity: @e O(n), where @e n is the number of desktop, as it
 *       iterates through the array of windows to free each one of them
 */
void surface_destroy(surface_td *surface);

/**
 * @brief Soft surface update
 *
 * @param surface Pointer to the surface to soft update
 *
 * @note Complexity: @e O(1)
 */
void surface_update(surface_td *surface);

/**
 * @brief Full surface update
 *
 * Updates the surface by updating every window of every desktop.
 *
 * @param surface Pointer to the surface to full update
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
void surface_update_full(surface_td *surface);

/**
 * @brief Resize the specified surface to the new dimensions
 *
 * @param surface Pointer to the surface to be resized
 * @param width  New width for the surface in pixels
 * @param height New height for the surface in pixels
 *
 * @note Complexity: @e O(1)
 */
void surface_resize(surface_td *surface,
        unsigned int width, unsigned int height);

/**
 * @brief Add a new desktop to the list
 *
 * @param surface  Pointer to the surface structure
 * @param desktop Pointer to the desktop to be added
 *
 * @return Status of the adding operation
 * @retval  0 Success
 * @retval  1 Failed to insert the desktop to the list
 * @retval -1 Invalid surface
 */
int surface_desktop_add(surface_td *surface, desktop_td *desktop);

/**
 * @brief Remove a desktop from the list by its ID
 *
 * @param surface Pointer to the surface structure.
 * @param desktop_id ID of the desktop to be removed
 *
 * @return Status of the remova operation
 * @retval  0 Success
 * @retval  1 Failed to remove the desktop from the list
 * @retval  2 Could not find the desktop matching that ID
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_rem(surface_td *surface, XID desktop_id);

/**
 * @brief Get a desktop from the list by its ID
 *
 * Retrieves a pointer to a desktop with the specified ID from the
 * surface.  If the desktop is found, its pointer is returned; otherwise,
 * @c NULL is returned.
 *
 * @param surface     Pointer to the surface structure
 * @param desktop_id ID of the desktop to retrieve
 *
 * @return Pointer to the desktop if found, or @c NULL otherwise
 */
desktop_td *surface_desktop_get(surface_td *surface, XID desktop_id);

/**
 * @brief Get the previous desktop in the list, optionally cycling
 *
 * Searches for the desktop with the given ID and returns the previous
 * desktop in the circular list of desktops.  If the current desktop is
 * the first in the list, and cycling is enabled, it will return the
 * last desktop.
 *
 * @param surface     Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, the function will cycle back to the
 *                   last desktop if the current is the first
 *
 * @return Pointer to the previous desktop or @c NULL if not
 *         found or not invalid
 */
desktop_td *surface_desktop_prev(surface_td *surface, XID desktop_id,
        bool cycle);

/**
 * @brief Get the next desktop in the list, optionally cycling
 *
 * Searches for the desktop with the given ID and returns the next
 * desktop in the circular list of desktops.  If the current desktop is
 * the last in the list, and cycling is enabled, it will return the
 * first desktop.
 *
 * @param surface     Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, the function will cycle to the first
 *                   desktop if the current is the last
 *
 * @return Pointer to the next desktop or @c NULL if not found or not
 *         valid
 */
desktop_td *surface_desktop_next(surface_td *surface, XID desktop_id,
        bool cycle);

/**
 * @brief Select the previous desktop, optionally cycling
 *
 * Attempts to select the desktop that precedes the current one in the
 * list.  If the current desktop is the first and cycle mode is enabled,
 * it will select the last desktop updating @p desktop_cur.
 *
 * @param surface Pointer to the surface structure
 * @param cycle  If @c true, will cycle to the last desktop if the
 *               current is the first
 *
 * @return Status of the selection
 * @retval  0 Sucess
 * @retval  1 No previous desktop found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select_prev(surface_td *surface, bool cycle);

/**
 * @brief Select the next desktop, optionally cycling
 *
 * Attempts to select the desktop that follows the current one in the
 * list.  If the current desktop is the last and cycle mode is enabled,
 * it will select the first desktop updating @p desktop_cur.
 *
 * @param surface Pointer to the surface structure
 * @param cycle  If @c true, will cycle to the first desktop if the
 *               current is the last
 *
 * @return Status of the selection
 * @retval  0 Sucess
 * @retval  1 No next desktop found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select_next(surface_td *surface, bool cycle);

/**
 * @brief Select a specific desktop by its ID
 *
 * Selects a desktop by its ID.  If the ID is valid, it changes the
 * current desktop to the specified ID updating @p desktop_cur.  Returns
 * an error and updates nothing if the ID is not found in the list of
 * desktops.
 *
 * @param surface     Pointer to the surface structure
 * @param desktop_id ID of the desktop to select
 *
 * @return Status of the selection
 * @retval  0 Sucess
 * @retval  1 No next desktop found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select(surface_td *surface, XID desktop_id);

/**
 * @brief Add a new desktop associated with the surface
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_desktop_add(surface_td *surface);

/**
 * @brief Remove the specified desktop associated with the surface
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_desktop_remove(surface_td *surface);

/**
 * @brief Switch the current view to another specified desktop
 *
 * @param surface     Pointer to the surface to receive the action
 * @param desktop_id The identifier of the desktop to switch to
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_desktop_switch(surface_td *surface, XID desktop_id);

/**
 * @brief Switch the current view to the next desktop in sequence
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_desktop_switch_next(surface_td *surface);

/**
 * @brief Switch to the current view to the previous desktop in sequence
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_desktop_switch_prev(surface_td *surface);

/**
 * @brief Toggle the current application into or out of full surface mode
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_toggle_fullsurface(surface_td *surface);

/**
 * @brief Update the surface resolution to the specified dimensions
 *
 * @param surface     Pointer to the surface to receive the action
 * @param resolution Structure for new dimensions of new resolution
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 *
 * @see @c dimensions_s
 */
int surface_action_set_resolution(surface_td *surface,
        struct dimensions_s resolution);

/**
 * @brief Update the orientation of the surface
 *
 * @param surface      Pointer to the surface to receive the action
 * @param orientation New orientation for the surface (e.g., portrait or
 *                    landscape)
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_set_orientation(surface_td *surface, int orientation);

/**
 * @brief Updates the brightness level of the surface
 *
 * @param surface     Pointer to the surface to receive the action
 * @param brightness New brightness level [0-100]
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_set_brightness(surface_td *surface, int brightness);

/**
 * @brief Update the contrast level of the surface
 *
 * @param surface   Pointer to the surface to receive the action
 * @param contrast New contrast level [0-100]
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_set_contrast(surface_td *surface, int contrast);

/**
 * @brief Apply the current surface configuration settings
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_configure_settings(surface_td *surface);

/**
 * @brief Macro that evaluates to the surface width
 *
 * @note Complexity: @e O(1)
 */
#define surface_width(s) ((s) ? (s).properties.dim.w : 0)

/**
 * @brief Macro that evaluates to the surface height
 *
 * @note Complexity: @e O(1)
 */
#define surface_height(s) ((s) ? (s).properties.dim.h : 0)

/**
 * @brief Macro that evaluates to the desktop count of the surface
 *
 * @note Complexity: @e O(1)
 */
#define surface_desktop_count(s) ((s) ? (s)->desktops->size : 0)


#endif  /* ! SURFACE_H */
