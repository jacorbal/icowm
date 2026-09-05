/**
 * @file enact/surface.c
 *
 * @brief Every surface-level action this window manager can carry
 *        out, one typed function per action
 *
 * One of the files @c enact/ is made of; see
 * @c enact/internal.h for why.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* NULL */

/* Project includes */
#include <surface.h>
#include <wm.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Command includes */
#include <cmds/surface.h>

/* Handler includes */
#include <input/kbd/bind.h>

/* Local includes */
#include <enact.h>


/* 'action_surface_e' */

static void s_broadcast_desktop_switched(const surface_td *surface)
{
    cJSON *const fields = cJSON_CreateObject();

    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) surface->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) surface->desktop_cur);
    }
    ipc_broadcast_event(IPC_EVENT_DESKTOP_SWITCHED, fields);
}


/**
 * @brief Refresh keyboard grabs after any surface's desktop count
 *        changes
 *
 * @c keyboard_load (@c input/kbd/bind.c) only grabs the desktop-cycle
 * and go-to-desktop-@e N keys when at least one managed surface
 * currently has more than one desktop, decided fresh every time it
 * runs.  Nothing else re-runs it after @a surface_action_desktop_add
 * or @a surface_action_desktop_remove change a surface's desktop
 * count, so without this, those grabs could silently drift out of
 * sync with the desktop count they were meant to reflect: stuck in
 * whichever state happened to be true the last time some unrelated
 * event (a keyboard mapping change, a RandR change, a configuration
 * reload) last triggered a refresh, e.g., correctly ungrabbed while
 * every desktop but one was removed, then never regrabbed once new
 * ones were added back, silently leaving every desktop-switch key
 * combination unresponsive until the next unrelated refresh happens
 * to fall due.
 *
 * @note No-op if @a wm_get_keysyms (@c wm.h) has nothing to return
 *       yet, the same guard @a wm_action_config_reload (wm/actions.c)
 *       already applies to its @c keyboard_load call
 * @note Complexity: same as @a keyboard_load itself
 */
static void s_refresh_keyboard_grabs(void)
{
    xcb_key_symbols_t *const keysyms = wm_get_keysyms();

    if (keysyms != NULL) {
        keyboard_load(wm_get_surfaces(), keysyms, wm_get_config());
    }
}


void enact_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id)
{
    scmd_surface_desktop_switch(surface, desktop_id);
    s_broadcast_desktop_switched(surface);
}


/* Switch the surface to the desktop north of the current one, in
 * cyclic order */
void enact_surface_desktop_switch_north(surface_td *surface)
{
    scmd_surface_desktop_switch_north(surface);
    s_broadcast_desktop_switched(surface);
}


/* Switch the surface to the desktop south of the current one, in
 * cyclic order */
void enact_surface_desktop_switch_south(surface_td *surface)
{
    scmd_surface_desktop_switch_south(surface);
    s_broadcast_desktop_switched(surface);
}


/* Switch the surface to the desktop east of the current one, in
 * cyclic order */
void enact_surface_desktop_switch_east(surface_td *surface)
{
    scmd_surface_desktop_switch_east(surface);
    s_broadcast_desktop_switched(surface);
}


/* Switch the surface to the desktop west of the current one, in
 * cyclic order */
void enact_surface_desktop_switch_west(surface_td *surface)
{
    scmd_surface_desktop_switch_west(surface);
    s_broadcast_desktop_switched(surface);
}


/* Add a new, empty desktop to the end of the surface's list */
void enact_surface_desktop_add(surface_td *surface)
{
    if (surface_action_desktop_add(surface) == 0) {
        s_refresh_keyboard_grabs();
        s_broadcast_desktop_switched(surface);
    }
}


/* Remove the surface's last desktop */
void enact_surface_desktop_remove(surface_td *surface)
{
    if (surface_action_desktop_remove(surface) == 0) {
        s_refresh_keyboard_grabs();
        s_broadcast_desktop_switched(surface);
    }
}


/* Toggle whether panel/tray struts are set aside on this surface */
void enact_surface_toggle_strutless_maximize(surface_td *surface)
{
    if (surface_action_toggle_strutless_maximize(surface) == 0) {
        s_broadcast_desktop_switched(surface);
    }
}


/* Pan the surface's current desktop viewport one screen north */
void enact_surface_viewport_pan_north(surface_td *surface)
{
    scmd_surface_viewport_pan_north(surface);
}


/* Pan the surface's current desktop viewport one screen south */
void enact_surface_viewport_pan_south(surface_td *surface)
{
    scmd_surface_viewport_pan_south(surface);
}


/* Pan the surface's current desktop viewport one screen east */
void enact_surface_viewport_pan_east(surface_td *surface)
{
    scmd_surface_viewport_pan_east(surface);
}


/* Pan the surface's current desktop viewport one screen west */
void enact_surface_viewport_pan_west(surface_td *surface)
{
    scmd_surface_viewport_pan_west(surface);
}


/* Jump the surface's current desktop viewport straight to one of its
 * configured pages */
void enact_surface_viewport_goto(surface_td *surface, uint32_t page)
{
    scmd_surface_viewport_goto(surface, page);
}
