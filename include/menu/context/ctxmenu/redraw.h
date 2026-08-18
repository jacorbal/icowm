/**
 * @file menu/context/ctxmenu/redraw.h
 *
 * @brief Painting a context menu's own rows
 *
 * @ingroup menu_context
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONTEXT_CTXMENU_REDRAW_H
#define MENU_CONTEXT_CTXMENU_REDRAW_H


/* Local includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Repaint the context menu window
 *
 * Called from the expose handler when @p state->window receives an
 * expose event.  Redraws all entries using the cached configuration.
 *
 * @param state Menu state to repaint
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
void ctxmenu_redraw(ctxmenu_state_td *state);

/**
 * @brief Repaint only the given one or two entry indices, not the
 *        whole menu
 *
 * A single deselect (e.g., the pointer leaving every entry) passes
 * @c -1 for @p idx_b.
 *
 * @param state Menu state the entries belong to
 * @param idx_a First index to redraw, or @c -1 for none
 * @param idx_b Second index to redraw, or @c -1 for none; skipped if
 *              equal to @p idx_a
 *
 * @note Complexity: @e O(1)
 */
void ctxmenu_redraw_entries(ctxmenu_state_td *state,
        int idx_a, int idx_b);


#endif  /* ! MENU_CONTEXT_CTXMENU_REDRAW_H */
