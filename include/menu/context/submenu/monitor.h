/**
 * @file menu/context/submenu/monitor.h
 *
 * @brief Shared "Send to monitor" context menu submenu
 *
 * Builds the entries for a "Send to monitor" submenu: one row per
 * monitor on the target client's stage.  Parallels
 * @c submenu/desktop.h and @c submenu/page.h, which send a client to
 * a different desktop or a different page of its own desktop
 * respectively; this instead moves it to a different physical screen,
 * leaving desktop and page untouched.  Shared by every top-level
 * context menu that offers this action on a client (the window menu,
 * and the icon menu).
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

#ifndef MENU_CONTEXT_SUBMENU_MONITOR_H
#define MENU_CONTEXT_SUBMENU_MONITOR_H


/* Type includes */
#include <types/handles.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Build the "Send to monitor" submenu entries for @p client
 *
 * @param stage   Stage that owns the monitors
 * @param desktop Unused; kept only so every "Send to" submenu
 *                    builder shares one identical call signature
 * @param client      Target client
 * @param out_entries Set, on success, to this module's own entry
 *                    storage; valid until the next call to this
 *                    function
 * @param out_state Set, on success, to this module's own submenu
 *                    state; valid until the next call to this function
 *
 * @return Number of entries built, or @c 0 when @p stage has only one
 *         monitor, in which case @p out_entries and @p out_state are
 *         left untouched
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors
 */
int ctxmenu_submenu_monitor_build(stage_td *stage,
        desktop_td *desktop, client_td *client,
        ctxmenu_entry_td **out_entries, ctxmenu_state_td **out_state);


#endif  /* ! MENU_CONTEXT_SUBMENU_MONITOR_H */
