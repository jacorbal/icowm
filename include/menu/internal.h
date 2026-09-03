/**
 * @file menu/internal.h
 *
 * @brief Private types and declarations shared across menu modules
 *
 * Defines the @c cycle_menu_state_s struct and declares the helper
 * functions shared between @c menu/cycle.c (state and input handling)
 * and @c menu/cycle/draw.c (visual rendering).
 *
 * @note This header is private to the menu subsystem and must not be
 *       included outside of @c src/menu/
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_INTERNAL_H
#define MENU_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xproto.h>

/* Type includes */
#include <types/handles.h>

/* Default initial values */
#include <defs/cycle.h>


/**
 * @brief Private cycle-menu state
 *
 * Owns the window, client list, labels, selection, geometry, and key
 * bindings for the current cycle-menu session.  Shared between
 * @c menu/cycle.c (state management) and @c menu/cycle/draw.c
 * (rendering).
 */
struct cycle_menu_state_s {
    surface_td *surface;
    desktop_td *desktop;
    client_td *preview_client;
    const config_td *config;
    client_td *clients[WM_CYCLE_MENU_MAX_ENTRIES];
    xcb_window_t window;
    int count;
    int selected;
    xcb_window_t prev_focus;
    xcb_keysym_t next_keysym;
    xcb_keysym_t prev_keysym;
    int scroll_offset;
    int viewport_rows;

    int last_drawn_selected;        /**< @p selected as of @a cycle_draw's
                                         own most recent call, so it can
                                         redraw only the rows that
                                         actually changed selection
                                         instead of the whole viewport
                                         when @p scroll_offset did not
                                         also change; meaningless until
                                         @p has_drawn_once */

    int last_drawn_scroll_offset;   /**< See @p last_drawn_selected */

    xcb_window_t outline_windows[4];/**< The 4 strip windows (see
                                         @c render/outline.h) outlining
                                         whichever client is
                                         currently selected;
                                         @c XCB_WINDOW_NONE in all
                                         4 slots until the first
                                         selection is applied */

    uint16_t width;
    uint16_t modifier;
    uint16_t next_modmask;
    uint16_t prev_modmask;
    bool is_icon_menu;

    bool has_drawn_once;            /**< Whether @p last_drawn_selected /
                                         @p last_drawn_scroll_offset
                                         hold a real prior draw yet;
                                         false right after @a cycle_init
                                         so its first @a cycle_draw
                                         always paints the whole
                                         viewport regardless */

    char labels[WM_CYCLE_MENU_MAX_ENTRIES][WM_CYCLE_MENU_ENTRY_LENGTH];
};


/**
 * @brief Cycle menu singleton instance (defined in @c menu/cycle.c)
 */
extern struct cycle_menu_state_s g_cycle_menu;


/**
 * @brief Resolve the X window used as the visual target for cycle
 *        preview
 *
 * @param client       Client to evaluate
 * @param is_icon_menu Whether the cycle preview is in icon menu mode
 *
 * @return Target window ID, or @c XCB_WINDOW_NONE if unavailable
 *
 * @note Implemented in @c menu/cycle/draw.c
 * @note Complexity: @e O(1)
 */
xcb_window_t mi_cycle_preview_target(const client_td *client,
        bool is_icon_menu);

/**
 * @brief Apply cycle preview highlighting and stacking
 *
 * Updates border color and stacking of the currently selected client in
 * the preview.  Restores the previous preview client's border color.
 *
 * @param connection XCB connection
 * @param cfg        Active configuration (theme data)
 *
 * @note Implemented in @c menu/cycle/draw.c
 * @note Complexity: @e O(1)
 */
void mi_cycle_preview_apply(xcb_connection_t *connection,
        const config_td *cfg);

/**
 * @brief Apply preview border color and width to a target window
 *
 * @param connection   Active XCB connection
 * @param target       Target window
 * @param client       Client associated with @p target
 * @param cfg          Active configuration
 * @param is_icon_menu Whether the cycle menu shows icons
 * @param border_color Border color to apply
 *
 * @note Implemented in @c menu/cycle/draw.c
 * @note Complexity: @e O(1)
 */
void mi_cycle_preview_style_target(xcb_connection_t *connection,
        xcb_window_t target, const client_td *client,
        const config_td *cfg, bool is_icon_menu,
        uint32_t border_color);


#endif  /* ! MENU_INTERNAL_H */
