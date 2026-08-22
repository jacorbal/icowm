/**
 * @file input/mouse/bind.h
 *
 * @brief Mouse binding types and configuration loading
 *
 * Declares the mouse binding enum, the resolved binding record, and
 * the functions that load mouse bindings from configuration and look
 * them up by index.
 *
 * @defgroup input_mouse Mouse input
 * @ingroup input
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_BIND_H
#define INPUT_MOUSE_BIND_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>


/**
 * @brief Actions that a mouse binding can trigger
 */
enum wm_mousebind_type_e {
    MOUSEBIND_NONE,
    MOUSEBIND_MOVE,             /**< Move the clicked client */
    MOUSEBIND_RESIZE,           /**< Resize the clicked client */
    MOUSEBIND_LOWER,            /**< Lower the clicked client */
    MOUSEBIND_DESKTOP_NORTH,    /**< Switch to the desktop north of
                                     the current one (wheel) */
    MOUSEBIND_DESKTOP_SOUTH,    /**< Switch to the desktop south of
                                     the current one (wheel) */
    MOUSEBIND_DESKTOP_EAST,     /**< Switch to the desktop east of
                                     the current one (wheel, 5) */
    MOUSEBIND_DESKTOP_WEST,     /**< Switch to the desktop west of
                                     the current one (wheel, 4) */
};


/**
 * @brief Resolved mouse binding record
 *
 * Associates a button index, a modifier mask, and an action type loaded
 * from the configuration file.
 */
typedef struct {
    xcb_button_index_t button;
    uint16_t modmask;
    enum wm_mousebind_type_e type;
} wm_mousebinding_td;


/* Public interface */
/**
 * @brief Parse mouse bindings from configuration and grab buttons
 *
 * Reads mouse binding strings from @p config, parses each one,
 * registers it in the internal binding table, and grabs the
 * corresponding button on all roots in @p surfaces (with lock-modifier
 * variants so that @c Caps_Lock and @c Num_Lock do not interfere).
 *
 * @param surfaces All managed surfaces
 * @param config   Active configuration
 *
 * @note Replaces any * previously loaded bindings.
 * @note Complexity: @e O(s * b * L), where @e s is the number of
 *       surfaces, @e b the number of configured bindings, and @e L is
 *       the number of lock-modifier variants (4)
 */
void mouse_load(list_td *surfaces, const config_td *config);

/**
 * @brief Return the number of loaded mouse bindings
 *
 * @return Number of active bindings in the binding table
 *
 * @note Complexity: @e O(1)
 */
int mousebind_count(void);

/**
 * @brief Access a binding entry by index
 *
 * Retrieves the button index, modifier mask, and action type of the
 * binding at position @p idx in the binding table.
 *
 * @param idx         Zero-based index into the binding table
 * @param button_out  Receives the binding's button index (may be null)
 * @param modmask_out Receives the binding's modifier mask (may be null)
 *
 * @return Action type for that entry, or @c MOUSEBIND_NONE if out of
 *         range
 *
 * @note Complexity: @e O(1)
 */
enum wm_mousebind_type_e mousebind_at(int idx,
        xcb_button_index_t *button_out, uint16_t *modmask_out);


#endif  /* ! INPUT_MOUSE_BIND_H */
