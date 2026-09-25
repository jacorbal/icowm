/**
 * @file enact/stage.c
 *
 * @brief Every stage-level action this window manager can carry out,
 *        one typed function per action
 *
 * One of the files @c enact/ is made of; see @c enact/internal.h for
 * why.
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

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Command includes */
#include <cmds/stage.h>

/* Handler includes */
#include <input/kbd/bind.h>

/* Stage includes */
#include <stage/action.h>

/* Project includes */
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <enact.h>
#include <enact/stage.h>


/**
 * @brief Broadcast that the stage's current desktop just changed
 *
 * @param stage Stage whose current desktop just changed; must not be
 *              null
 *
 * @note Complexity: @e O(1)
 */
static void s_broadcast_desktop_switched(const stage_td *stage)
{
    cJSON *const fields = cJSON_CreateObject();

    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "stage_id",
                (double) stage->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) stage->desktop_cur);
    }
    ipc_broadcast_event(IPC_EVENT_DESKTOP_SWITCHED, fields);
}


/**
 * @brief Tell IPC subscribers a stage gained or lost a desktop
 *
 * @param stage      Stage whose desktop list changed
 * @param type       @c IPC_EVENT_DESKTOP_ADDED or
 *                   @c IPC_EVENT_DESKTOP_REMOVED
 * @param desktop_id Desktop added or removed
 *
 * @note Complexity: @e O(1)
 */
static void s_broadcast_desktop_counted(const stage_td *stage,
        uint64_t type, uint32_t desktop_id)
{
    cJSON *const fields = cJSON_CreateObject();

    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "stage_id",
                (double) stage->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) desktop_id);
    }
    ipc_broadcast_event(type, fields);
}


/**
 * @brief Refresh keyboard grabs after any stage's desktop count
 *        changes
 *
 * @a keyboard_load (@c input/kbd/bind.c) only grabs the desktop-cycle
 * and @a go-to-desktop-@e N keys when at least one managed stage
 * currently has more than one desktop, decided fresh every time it
 * runs.  Nothing else re-runs it after @a stage_action_desktop_add or
 * @a stage_action_desktop_remove change a stage's desktop count, so
 * without this, those grabs could silently drift out of sync with the
 * desktop count they were meant to reflect.
 *
 * Stuck in whichever state happened to be true the last time some
 * unrelated event (a keyboard mapping change, a RandR change,
 * a configuration reload) last triggered a refresh, e.g., correctly
 * ungrabbed while every desktop but one was removed, then never
 * regrabbed once new ones were added back, silently leaving every
 * desktop-switch key combination unresponsive until the next unrelated
 * refresh happens to fall due.
 *
 * @note No-op if @a wm_get_keysyms (@c wm.h) has nothing to return yet,
 *       the same guard @a wm_action_config_reload (in @c wm/actions.c)
 *       already applies to its @a keyboard_load call
 * @note Complexity: same as @a keyboard_load itself
 */
static void s_refresh_keyboard_grabs(void)
{
    xcb_key_symbols_t *const keysyms = wm_get_keysyms();

    if (keysyms != NULL) {
        keyboard_load(wm_get_stages(), keysyms, wm_get_config());
    }
}


/* Switch stage to desktop */
void enact_stage_desktop_switch(stage_td *stage,
        uint32_t desktop_id)
{
    scmd_stage_desktop_switch(stage, desktop_id);
    s_broadcast_desktop_switched(stage);
}


/* Switch the stage to the desktop north of the current one, in cyclic
 * order */
void enact_stage_desktop_switch_north(stage_td *stage)
{
    scmd_stage_desktop_switch_north(stage);
    s_broadcast_desktop_switched(stage);
}


/* Switch the stage to the desktop south of the current one, in cyclic
 * order */
void enact_stage_desktop_switch_south(stage_td *stage)
{
    scmd_stage_desktop_switch_south(stage);
    s_broadcast_desktop_switched(stage);
}


/* Switch the stage to the desktop east of the current one, in cyclic
 * order */
void enact_stage_desktop_switch_east(stage_td *stage)
{
    scmd_stage_desktop_switch_east(stage);
    s_broadcast_desktop_switched(stage);
}


/* Switch the stage to the desktop west of the current one, in cyclic
 * order */
void enact_stage_desktop_switch_west(stage_td *stage)
{
    scmd_stage_desktop_switch_west(stage);
    s_broadcast_desktop_switched(stage);
}


/* Add a new, empty desktop to the end of the stage's list */
void enact_stage_desktop_add(stage_td *stage)
{
    if (stage_action_desktop_add(stage) == 0) {
        s_refresh_keyboard_grabs();
        s_broadcast_desktop_counted(stage, IPC_EVENT_DESKTOP_ADDED,
                stage->desktop_count - 1u);
    }
}


/* Remove the stage's last desktop */
void enact_stage_desktop_remove(stage_td *stage)
{
    uint32_t removed_id;
    uint32_t cur_before;

    if (stage == NULL) {
        return;
    }

    removed_id = stage->desktop_count - 1u;
    cur_before = stage->desktop_cur;
    if (stage_action_desktop_remove(stage) == 0) {
        s_refresh_keyboard_grabs();
        s_broadcast_desktop_counted(stage, IPC_EVENT_DESKTOP_REMOVED,
                removed_id);
        if (stage->desktop_cur != cur_before) {
            s_broadcast_desktop_switched(stage);
        }
    }
}


/* Toggle whether panel/tray struts are set aside on this stage */
void enact_stage_toggle_strutless_maximize(stage_td *stage)
{
    if (stage_action_maximize_toggle_strutless(stage) == 0) {
        s_broadcast_desktop_switched(stage);
    }
}


/* Move the stage's current desktop viewport a whole page north */
void enact_stage_viewport_switch_north(stage_td *stage)
{
    scmd_stage_viewport_pan_north(stage);
}


/* Move the stage's current desktop viewport a whole page south */
void enact_stage_viewport_switch_south(stage_td *stage)
{
    scmd_stage_viewport_pan_south(stage);
}


/* Move the stage's current desktop viewport a whole page east */
void enact_stage_viewport_switch_east(stage_td *stage)
{
    scmd_stage_viewport_pan_east(stage);
}


/* Move the stage's current desktop viewport a whole page west */
void enact_stage_viewport_switch_west(stage_td *stage)
{
    scmd_stage_viewport_pan_west(stage);
}


/* Pan the stage's current desktop viewport north by one
 * 'viewport.pan-step' */
void enact_stage_viewport_pan_north(stage_td *stage)
{
    scmd_stage_viewport_pan_step(stage, COMPASS_NORTH);
}


/* Pan the stage's current desktop viewport south by one
 * 'viewport.pan-step' */
void enact_stage_viewport_pan_south(stage_td *stage)
{
    scmd_stage_viewport_pan_step(stage, COMPASS_SOUTH);
}


/* Pan the stage's current desktop viewport east by one
 * 'viewport.pan-step' */
void enact_stage_viewport_pan_east(stage_td *stage)
{
    scmd_stage_viewport_pan_step(stage, COMPASS_EAST);
}


/* Pan the stage's current desktop viewport west by one
 * 'viewport.pan-step' */
void enact_stage_viewport_pan_west(stage_td *stage)
{
    scmd_stage_viewport_pan_step(stage, COMPASS_WEST);
}


/* Jump the stage's current desktop viewport straight to one of its
 * configured pages */
void enact_stage_viewport_goto(stage_td *stage, uint32_t page)
{
    scmd_stage_viewport_goto(stage, page);
}
