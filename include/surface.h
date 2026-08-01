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
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SURFACE_H
#define SURFACE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>     /* size_t */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Type includes */
#include <types/pair.h> /* dimensions_s, size_s */

/* Project includes */
#include <config.h>
#include <desktop.h>


/**
 * @brief Structure to hold properties of the visual
 */
struct visual_properties_s {
    xcb_colormap_t colormap;    /**< Color maps for the visual */
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
        xcb_visualid_t visual_id;               /**< Associated visual */
        struct visual_properties_s properties;  /**< Visual properties */
    } visual_info;
};


/**
 * @brief Structure for a surface in an XCB environment
 *
 * Maintains a circular list of desktops to support multiple virtual
 * desktops on the surface, allowing for more organized and flexible
 * user interfaces.  The structure also includes parameters for color
 * depth, DPI settings, and a reference to configuration data for
 * dynamic management and customization.
 *
 * The @p is_outdated flag indicates whether the surface data needs to
 * be refreshed, ensuring the surface information remains synchronized
 * with underlying changes in the XCB environment or user preferences.
 */
typedef struct surface_s {
    uint32_t id;                    /**< Screen unique identifier or index */

    xcb_connection_t *connection;   /**< Pointer to XCB connection */
    xcb_screen_t *screen;           /**< Pointer to XCB screen */
    xcb_ewmh_connection_t *ewmh;    /**< Pointer to EWMH connection */

    /* Properties */
    struct surface_properties_s properties;

    uint32_t desktop_count;         /**< No. of desktops for this surface */
    uint32_t desktop_cur;           /**< Index of current desktop */
    cdlist_td *desktops;            /**< Circular list of desktops */

    config_td *config;              /**< Configuration */

    bool fullsurface;               /**< Full surface or not */
    bool showing_desktop;           /**< EWMH '_NET_SHOWING_DESKTOP' state */
    bool is_outdated;               /**< Flag if data needs to be updated */
} surface_td;


/* Public interface */
/**
 * @brief Initialize a new surface
 *
 * @param connection    Pointer to XCB connection
 * @param screen_id     Screen identifier
 * @param desktop_count Number of desktops on this surface
 *
 * @return Pointer to new surface, or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
surface_td *surface_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        const uint32_t screen_id, uint32_t desktop_count,
        config_td *config);

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
 * @param width   New width for the surface in pixels
 * @param height  New height for the surface in pixels
 *
 * @note Complexity: @e O(1)
 */
void surface_resize(surface_td *surface,
        uint32_t width, uint32_t height);

/**
 * @brief Add a new desktop to the list
 *
 * @param surface Pointer to the surface structure
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
 * @param surface    Pointer to the surface structure.
 * @param desktop_id ID of the desktop to be removed
 *
 * @return Status of the removal operation
 * @retval  0 Success
 * @retval  1 Failed to remove the desktop from the list
 * @retval  2 Could not find the desktop matching that ID
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_rem(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Get a desktop from the list by its ID
 *
 * Retrieves a pointer to a desktop with the specified ID from the
 * surface.  If the desktop is found, its pointer is returned;
 * otherwise, @c NULL is returned.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the desktop to retrieve
 *
 * @return Pointer to the desktop if found, or @c NULL otherwise
 */
desktop_td *surface_desktop_get(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Get the previous desktop in the list, optionally cycling
 *
 * Searches for the desktop with the given ID and returns the previous
 * desktop in the circular list of desktops.  If the current desktop is
 * the first in the list, and cycling is enabled, it will return the
 * last desktop.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, the function will cycle back to the
 *                   last desktop if the current is the first
 *
 * @return Pointer to the previous desktop or @c NULL if not
 *         found or not invalid
 */
desktop_td *surface_desktop_prev(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Get the next desktop in the list, optionally cycling
 *
 * Searches for the desktop with the given ID and returns the next
 * desktop in the circular list of desktops.  If the current desktop is
 * the last in the list, and cycling is enabled, it will return the
 * first desktop.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, the function will cycle to the first
 *                   desktop if the current is the last
 *
 * @return Pointer to the next desktop or @c NULL if not found or not
 *         valid
 */
desktop_td *surface_desktop_next(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Select the previous desktop, optionally cycling
 *
 * Attempts to select the desktop that precedes the current one in the
 * list.  If the current desktop is the first and cycle mode is enabled,
 * it will select the last desktop updating @p desktop_cur.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, will cycle to the last desktop if the
 *                current is the first
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
 * @param cycle   If @c true, will cycle to the first desktop if the
 *                current is the last
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
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the desktop to select
 *
 * @return Status of the selection
 * @retval  0 Sucess
 * @retval  1 No next desktop found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select(surface_td *surface,
        uint32_t desktop_id);

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
 * @param surface    Pointer to the surface to receive the action
 * @param desktop_id The identifier of the desktop to switch to
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_desktop_switch(surface_td *surface,
        uint32_t desktop_id);

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
 * @brief Toggle the current application into or out of full surface
 *        mode
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
 * @param surface    Pointer to the surface to receive the action
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
 * @param surface     Pointer to the surface to receive the action
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
 * @param surface    Pointer to the surface to receive the action
 * @param brightness New brightness level [0-100]
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_set_brightness(surface_td *surface,
        uint16_t brightness);

/**
 * @brief Update the contrast level of the surface
 *
 * @param surface  Pointer to the surface to receive the action
 * @param contrast New contrast level [0-100]
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int surface_action_set_contrast(surface_td *surface, uint16_t contrast);

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
 * @brief Unmap all non-sticky client windows belonging to a desktop
 *
 * Iterates the stacking list of the specified desktop and calls
 * @c xcb_unmap_window for each client that does not have the
 * @c CLIENT_FLAG_STICKY flag set.  Used when switching away from a
 * desktop to hide its windows.
 *
 * @param surface    Pointer to the surface that owns the desktop
 * @param desktop_id ID of the desktop whose clients should be hidden
 *
 * @note Complexity: @e O(n), where @e n is the number of stacked
 *       clients on the desktop
 */
void surface_clients_hide(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Map all visible client windows belonging to a desktop
 *
 * Iterates the stacking list of the specified desktop and calls
 * @c xcb_map_window for each client that is neither hidden
 * (@c CLIENT_FLAG_HIDDEN) nor iconified
 * (@c CLIENT_STATE_ICONIFIED).  Used when switching to a desktop to
 * reveal its windows.
 *
 * @param surface    Pointer to the surface that owns the desktop
 * @param desktop_id ID of the desktop whose clients should be shown
 *
 * @note Complexity: @e O(n), where @e n is the number of stacked
 *       clients on the desktop
 */
void surface_clients_show(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Move all sticky clients from every other desktop to @p to_id
 *
 * Iterates all desktops on the surface and relocates any client that
 * carries the @c CLIENT_FLAG_STICKY flag to the desktop identified by
 * @p to_id.  Called during desktop switches so that pinned windows are
 * present in the new desktop's stacking list and therefore respond to
 * keyboard shortcuts and focus management on the destination desktop.
 *
 * @param surface Pointer to the surface that owns all desktops
 * @param to_id   ID of the desktop to which sticky clients are moved
 *
 * @note Complexity: @e O(d * n), where @e d is the number of desktops
 *       and @e n is the average number of clients per desktop
 */
void surface_clients_sticky_transfer_all(surface_td *surface,
        uint32_t to_id);

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
