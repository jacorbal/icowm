/**
 * @file menu/context/submenu/page.h
 *
 * @brief Shared "Send to page" context menu submenu
 *
 * Builds the entries for a "Send to page" submenu: one row per page of
 * the surface's configured viewport grid, followed by a separator and
 * an "All pages"/"Unstick" toggle row.  Parallels @c submenu/desktop.h,
 * which sends a client to a different desktop entirely; this only
 * moves it within the current desktop's own pannable canvas.  Shared
 * by every top-level context menu that offers this action on a client
 * (the window menu, and the icon menu).
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

#ifndef MENU_CONTEXT_SUBMENU_PAGE_H
#define MENU_CONTEXT_SUBMENU_PAGE_H


/* Type includes */
#include <types/handles.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Build the "Send to page" submenu entries for @p client
 *
 * @param surface     Surface whose viewport grid to enumerate
 * @param desktop     Desktop @p client is currently on
 * @param client      Target client
 * @param out_entries Set, on success, to this module's own entry
 *                     storage; valid until the next call to this
 *                     function
 * @param out_state   Set, on success, to this module's own submenu
 *                     state; valid until the next call to this
 *                     function
 *
 * @return Number of entries built, or 0 on a viewport that cannot pan,
 *         or for a sticky client, which belongs to no one page; either
 *         way @p out_entries and @p out_state are left untouched
 *
 * @note Complexity: @e O(n), where @e n is the number of pages
 */
int ctxmenu_submenu_page_build(surface_td *surface, desktop_td *desktop,
        client_td *client, ctxmenu_entry_td **out_entries,
        ctxmenu_state_td **out_state);


#endif  /* ! MENU_CONTEXT_SUBMENU_PAGE_H */
