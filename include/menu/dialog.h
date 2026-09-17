/**
 * @file menu/dialog.h
 *
 * @brief Layout constants and drawing helpers shared by every modal
 *        dialog type
 *
 * Nothing here is a dialog on its own; @c menu/dialog/confirm.h and
 * @c menu/dialog/message.h are the two dialog types that use it.
 *
 * @defgroup menu_dialog Modal dialogs
 * @ingroup menu
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_DIALOG_H
#define MENU_DIALOG_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Initial definitions */
#include <defs/dialog.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <stage.h>


/**
 * @brief Return the greater of two @c uint16_t values
 *
 * @param a First value
 * @param b Second value
 *
 * @return Whichever of @p a or @p b is greater
 *
 * @note Complexity: @e O(1)
 */
uint16_t dlgutil_u16max(uint16_t a, uint16_t b);

/**
 * @brief Draw a solid border outline around a button, if its theme
 *        style gives it a non-zero border width
 *
 * @param connection XCB connection
 * @param window     Target drawable window
 * @param color      Border color
 * @param width      Border width in pixels; a no-op when 0
 * @param geom       Button fill rectangle
 *
 * @note Complexity: @e O(1)
 */
void dlgutil_button_border_draw(xcb_connection_t *connection,
        xcb_window_t window, uint32_t color, uint32_t width,
        struct geometry_s geom);

/**
 * @brief Resolve the monitor a dialog should size and center itself
 *        against
 *
 * Whichever monitor the pointer currently sits on, the same default
 * @c CONFIG_PLACEMENT_MONITOR_POINTER uses for window placement, since
 * a dialog has no window of its own to anchor a monitor resolution to
 * the way client placement does.  Falls back to a monitor spanning the
 * whole stage if the pointer query fails or @p stage has no
 * monitors of its own.
 *
 * @param connection XCB connection, for the pointer query
 * @param stage      Stage to resolve a monitor on
 *
 * @return The resolved monitor
 *
 * @note Complexity: @e O(n), where @e n is @p stage->monitor_count
 */
monitor_td dlgutil_resolve_monitor(xcb_connection_t *connection,
        const stage_td *stage);

/**
 * @brief Center a dialog of the given size on its target monitor
 *
 * @param connection XCB connection, for the pointer query
 * @param stage      Stage to center within
 * @param width      Dialog width in pixels
 * @param height     Dialog height in pixels
 * @param out_x      Receives the dialog's left edge
 * @param out_y      Receives the dialog's top edge
 *
 * @note Complexity: @e O(n), where @e n is @p stage's monitor count
 */
void menu_dialog_center(xcb_connection_t *connection,
        const stage_td *stage,
        uint16_t width, uint16_t height,
        int16_t *restrict out_x, int16_t *restrict out_y);


#endif  /* ! MENU_DIALOG_H */
