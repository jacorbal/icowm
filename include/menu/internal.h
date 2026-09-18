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
    stage_td *stage;
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
    int visible_rows;

    int last_drawn_selected;        /**< @p selected as of @a cycle_draw's
                                         own most recent call, so it can
                                         redraw only the rows that
                                         actually changed selection
                                         instead of every visible row
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
                                         always paints every visible
                                         row regardless */

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
 * Stacks the currently selected client right below the cycle menu and
 * moves the cycle outline onto it.  A window's own border is left as it
 * is, the outline alone marking the selection; in the icon menu, the
 * selected and deselected icons' borders are recolored.
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
 * @brief Apply the icon cycle menu's border to an icon window
 *
 * Gives @p icon_window the icon theme's active border width, whether
 * selected or not, and @p border_color.  The window cycle menu has no
 * counterpart: it marks its selection with the cycle outline alone.
 *
 * @param connection   Active XCB connection
 * @param icon_window  Icon window to restyle
 * @param cfg          Active configuration
 * @param border_color Border color to apply
 *
 * @note A no-op if @p connection or @p cfg is @c NULL, or if
 *       @p icon_window is @c XCB_WINDOW_NONE
 * @note Implemented in @c menu/cycle/draw.c
 * @note Complexity: @e O(1)
 */
void mi_cycle_preview_style_icon(xcb_connection_t *connection,
        xcb_window_t icon_window, const config_td *cfg,
        uint32_t border_color);


#endif  /* ! MENU_INTERNAL_H */
