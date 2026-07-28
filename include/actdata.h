/**
 * @file actdata.h
 *
 * @brief Object data structures when objects need to update their
 *        properties by events by the execution of an action
 *
 * These data structures are precisely for passing generic data, so the
 * events management is more uniform receiving only "packets of data"
 * and a object (client, desktop, surface,...) that will be "itemized"
 * in a general sense.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ACTDATA_H
#define ACTDATA_H


/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>


/**
 * @brief Window data to store information when updating the client by
 *        an action
 *
 * Flexible container for passing various types of information.  It can
 * hold integers or geometry values for operations such as resizing and
 * moving, as well as strings.  While typically only one string is
 * needed, there are scenarios where two may be required (e.g., when
 * setting the window role for compatibility purposes).  In cases where
 * only one string is needed, use @p str0.
 *
 * @see @c action_client_e
 */
typedef struct action_data_client_s {
    client_td *client;                  /**< Affected client */
    enum action_client_e action_client; /**< Action for this client */

    union {
        struct geometry_s geometry;
/*
        int32_t svalue;
        uint32_t uvalue;
*/
        struct {
            char *str0;
            char *str1;
       } str;
    } new_data;         /**< New values to update */
} action_data_client_td;


/**
 * @brief Desktop data to store information when updating the desktop by
 *        an action
 *
 * @see @c action_desktop_e
 */
typedef struct {
    desktop_td *desktop;                    /**< Affected desktop */
    desktop_td *target;                     /**< Target desktop if needed */
    client_td *client;                      /**< Affected clent, if needed */
    enum action_desktop_e action_desktop;   /**< Action for this desktop */

    union {
        char *str;
        int32_t svalue;
        uint32_t uvalue;
    } new_data;
    /* TODO */
} action_data_desktop_td;


/**
 * @brief Screen data to store information when updating the surface by
 *        an action
 *
 * @see @c action_surface_e
 */
typedef struct {
    surface_td *surface;                  /**< Affected surface */
    enum action_surface_e action_surface; /**< Action for this surface */

    union {
        uint32_t uvalue;    /**< Generic unsigned value (e.g., desktop ID) */
        int32_t  svalue;    /**< Generic signed value */
    } new_data;
} action_data_surface_td;


/**
 * @brief Screen data to store information when updating the surface by
 *        an action
 *
 * @see @c action_surface_e
 */
typedef struct {
    wm_td *wm;                  /**< Pointer to the window manager */
    enum action_wm_e action_wm; /**< Action for this surface */

    /* TODO */
} action_data_wm_td;


/* Public interface */
/**
 * @brief Allocate memory for the client data structure
 *
 * @param client        Pointer to the client that's going to be updated
 * @param action_client Action to perform with this data
 *
 * @return Pointer to the new allocated structure, or @c NULL otherwise
 *
 * @note The values of the structure must be filled manually, not at the
 *       initialization
 * @note Complexity: @e O(1)
 */
action_data_client_td *action_data_client_init(client_td *client,
        enum action_client_e action_client);

/**
 * @brief Deallocate client data structure
 *
 * @param action_data_client Pointer to the data structure to deallocate
 *
 * @note Complexity: @e O(1)
 */
void action_data_client_destroy(action_data_client_td *action_data_client);

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

/**
 * @brief Allocate memory for the window manager data structure
 *
 * @param wm        Pointer to the window manager instance
 * @param action_wm Action type to perform with this data
 *
 * @return Pointer to the new allocated structure, or @c NULL otherwise
 *
 * @note The values of the structure must be filled manually, not at the
 *       initialization
 * @note Complexity: @e O(1)
 */
action_data_wm_td *action_data_wm_init(wm_td *wm,
        enum action_wm_e action_wm);

/**
 * @brief Deallocate window manager data structure
 *
 * @param action_data_wm Pointer to the data structure to deallocate
 *
 * @note Complexity: @e O(1)
 */
void action_data_wm_destroy(action_data_wm_td *action_data_wm);


#endif  /* ! ACTDATA_H */
