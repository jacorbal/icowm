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

/* Project includes */
#include <surface.h>


/* Shared layout constants */
/** Bottom padding below buttons (pixels) */
#define DIALOG_PAD_BOTTOM (8u)

/** Minimum dialog width (pixels) */
#define DIALOG_MIN_W (220u)

/** Minimum dialog height (pixels) */
#define DIALOG_MIN_H (90u)

/** Minimum button width (pixels) */
#define DIALOG_BTN_MIN_W (60u)

/** Prompt baseline position from dialog top (pixels) */
#define DIALOG_PROMPT_BASELINE_Y (22u)

/** Vertical gap between prompt baseline and button top (pixels) */
#define DIALOG_PROMPT_TO_BTN_GAP (34u)

/** Maximum text length for dialogs (prompt + level prefix) */
#define DIALOG_TEXT_MAX_LEN (256u)


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
 * @param x          Left edge of the button fill rectangle
 * @param y          Top edge of the button fill rectangle
 * @param w          Width of the button fill rectangle (pixels)
 * @param h          Height of the button fill rectangle (pixels)
 *
 * @note Complexity: @e O(1)
 */
void dlgutil_draw_button_border(xcb_connection_t *connection,
        xcb_window_t window, uint32_t color, uint32_t width,
        int16_t x, int16_t y, uint16_t w, uint16_t h);

/**
 * @brief Resolve the monitor a dialog should size and center itself
 *        against
 *
 * Whichever monitor the pointer currently sits on, the same default
 * @c CONFIG_PLACEMENT_MONITOR_POINTER uses for window placement, since
 * a dialog has no window of its own to anchor a monitor resolution to
 * the way client placement does.  Falls back to a monitor spanning the
 * whole surface if the pointer query fails or @p surface has no
 * monitors of its own.
 *
 * @param connection XCB connection, for the pointer query
 * @param surface    Surface to resolve a monitor on
 *
 * @return The resolved monitor
 *
 * @note Complexity: @e O(n), where @e n is @p surface->monitor_count
 */
monitor_td dlgutil_resolve_monitor(xcb_connection_t *connection,
        const surface_td *surface);

/**
 * @brief Center a dialog of the given size on its target monitor
 *
 * @param connection XCB connection, for the pointer query
 * @param surface    Surface to center within
 * @param width      Dialog width in pixels
 * @param height     Dialog height in pixels
 * @param out_x      Receives the dialog's left edge
 * @param out_y      Receives the dialog's top edge
 *
 * @note Complexity: @e O(n), where @e n is @p surface's monitor count
 */
void menu_dialog_center(xcb_connection_t *connection,
        const surface_td *surface,
        uint16_t width, uint16_t height,
        int16_t *restrict out_x, int16_t *restrict out_y);


#endif  /* ! MENU_DIALOG_H */
