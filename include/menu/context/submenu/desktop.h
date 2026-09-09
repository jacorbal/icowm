/**
 * @file menu/context/submenu/desktop.h
 *
 * @brief Shared "Send to desktop" context menu submenu
 *
 * Builds the entries for a "Send to desktop" submenu: one row per
 * desktop on the target client's surface, followed by a separator and
 * an "All desktops"/"Unpin" toggle row.  Shared by every top-level
 * context menu that offers this action on a client (the window menu,
 * and the icon menu), so the entry list, the pin toggle, and the
 * per-row disabled/label logic are written once.
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

#ifndef MENU_CONTEXT_SUBMENU_DESKTOP_H
#define MENU_CONTEXT_SUBMENU_DESKTOP_H


/* Type includes */
#include <types/handles.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Build the "Send to desktop" submenu entries for @p client
 *
 * @param surface     Surface that owns the desktops
 * @param desktop     Desktop @p client is currently on
 * @param client      Target client
 * @param out_entries Set, on success, to this module's own entry
 *                    storage; valid until the next call to this
 *                    function
 * @param out_state   Set, on success, to this module's own submenu
 *                    state; valid until the next call to this function
 *
 * @return Number of entries built, or @c 0 when @p surface has only one
 *         desktop, in which case @p out_entries and @p out_state are
 *         left untouched
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops (a
 *       single walk of the surface's circular desktop list, not one
 *       lookup per index)
 */
int ctxmenu_submenu_desktop_build(surface_td *surface,
        desktop_td *desktop, client_td *client,
        ctxmenu_entry_td **out_entries, ctxmenu_state_td **out_state);


#endif  /* ! MENU_CONTEXT_SUBMENU_DESKTOP_H */
