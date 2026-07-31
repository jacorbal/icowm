/**
 * @file input/mouse.h
 *
 * @brief Mouse binding types, parsing, grab and event handling
 *
 * Declares the mouse binding enum, the resolved binding record, and the
 * three public functions that load mouse bindings from configuration,
 * dispatch button-press events, and apply focus-follows-mouse on
 * enter-notify events.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_H
#define INPUT_MOUSE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/**
 * @brief Actions that a mouse binding can trigger
 */
enum wm_mousebind_type_e {
    MOUSEBIND_NONE,
    MOUSEBIND_MOVE,             /**< Move the clicked client */
    MOUSEBIND_RESIZE,           /**< Resize the clicked client */
    MOUSEBIND_LOWER,            /**< Lower the clicked client */
    MOUSEBIND_DESKTOP_NEXT,     /**< Switch to next desktop (wheel 5) */
    MOUSEBIND_DESKTOP_PREV,     /**< Switch to previous desktop (wheel 4) */
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
 * Reads mouse binding strings from @p cfg, parses each one, registers
 * it in the internal binding table, and grabs the corresponding button
 * on all roots in @p surfaces (with lock-modifier variants so that
 * @c Caps_Lock and @c Num_Lock do not interfere).  Replaces any
 * previously loaded bindings.
 *
 * @param surfaces All managed surfaces
 * @param cfg      Active configuration
 *
 * @note Complexity: @e O(s * b * L), where @e s is the number of
 *       surfaces, @e b the number of configured bindings, and @e L is
 *       the number of lock-modifier variants (4)
 */
void mouse_load(list_td *surfaces, const config_td *cfg);

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
 * @param button_out  Receives the binding's button index (may be
 *                    @c NULL)
 * @param modmask_out Receives the binding's modifier mask (may be
 *                    @c NULL)
 *
 * @return Action type for that entry, or @c MOUSEBIND_NONE if out
 *         of range
 *
 * @note Complexity: @e O(1)
 */
enum wm_mousebind_type_e mousebind_at(int idx,
        xcb_button_index_t *button_out, uint16_t *modmask_out);

/**
 * @brief Dispatch a button-press event
 *
 * Handles popup and cycle-menu dismissal, icon-window drag start, plain
 * client focus clicks, titlebar decoration buttons, desktop cycling,
 * and drag-start for configured move/resize/lower bindings.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (for lookup and focus)
 * @param event      Button-press event
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n) for binding lookup; @e O(1) otherwise
 */
void mouse_handle_press(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *cfg);

/**
 * @brief Handle a button-release event to end a drag
 *
 * Finalizes a move or resize drag, decides whether an icon drag was
 * a click or a real drag, ungrab the pointer, and resets drag state.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (for lookup and focus on
 *                   icon-click restore)
 * @param event      Button-release event
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(1)
 */
void mouse_handle_release(xcb_connection_t *connection,
        list_td *surfaces, xcb_button_release_event_t *event,
        const config_td *cfg);

/**
 * @brief Apply focus-follows-mouse on an enter-notify event
 *
 * Focuses the client under the pointer when the configured focus policy
 * is @c follow-mouse.  Normal events on managed client frames raise no
 * stacking change; grab/inferior transitions are silently ignored.
 * The window manager @c client_active_id is preserved so that keyboard
 * shortcuts continue to target the explicitly-focused window, not the
 * hovered one.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces (for lookup and focus)
 * @param event      Enter-notify event
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n)
 */
void mouse_handle_enter(xcb_connection_t *connection,
        list_td *surfaces, xcb_enter_notify_event_t *event,
        const config_td *cfg);

/**
 * @brief Test whether a hover-triggered focus transfer is in progress
 *
 * Returns @c true when @c mouse_handle_enter has initiated
 * a focus-follows-mouse transfer but the corresponding @c FocusIn event
 * has not yet been processed.  Used by the focus-in handler to skip
 * updating @c client_active_id for hover-driven focus changes.
 *
 * @return @c true if a hover-triggered transfer is pending, @c false
 *         otherwise
 *
 * @note Complexity: @e O(1)
 */
bool mouse_enter_focus_is_active(void);

/**
 * @brief Clear the hover-triggered focus flag
 *
 * Resets the internal flag set by @c mouse_handle_enter.  Must be
 * called by the focus-in handler once it has decided to skip the
 * @c client_active_id update for a hover-triggered focus change.
 *
 * @note Complexity: @e O(1)
 */
void mouse_enter_focus_clear(void);


#endif  /* ! INPUT_MOUSE_H */
