/**
 * @file ipc/commands.c
 *
 * @brief IPC command dispatch table implementation
 *
 * Every handler this table names lives under ipc/actions/, grouped
 * the same way @c cmds/client/ already groups the @c enact_client_*
 * catalog itself (basic, geom, layer, meta, state), plus one file
 * each for desktop-scoped actions, surface (desktop-switching)
 * actions, whole-window-manager actions, and the read-only queries.
 * This file owns only the table itself and the top-level parsing
 * (is it JSON, does it have a string "cmd") that has to happen
 * before any specific handler can run.
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

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <wm.h>

/* Local includes */
#include <ipc.h>
#include <ipc/response.h>
#include <ipc/actions/client/flags.h>
#include <ipc/actions/client/focus.h>
#include <ipc/actions/client/geom.h>
#include <ipc/actions/client/layer.h>
#include <ipc/actions/client/meta.h>
#include <ipc/actions/client/state.h>
#include <ipc/actions/client/visibility.h>
#include <ipc/actions/desktop.h>
#include <ipc/actions/query.h>
#include <ipc/actions/scratchpad.h>
#include <ipc/actions/surface.h>
#include <ipc/actions/wm.h>
#include <ipc/commands.h>


/**
 * @brief One named command's handler
 *
 * @param wm   Window manager instance
 * @param args The request object itself (its other fields besides
 *             @c "cmd" are this command's arguments); never
 *             @c NULL, though it may have no fields of its own for
 *             a command that takes none
 *
 * @return A newly allocated JSON object to use as the full response
 *         (including its @c "ok" field), or @c NULL to have the
 *         caller (see @c ipc_commands_dispatch) fall back to a
 *         generic failure response, when the handler could not build
 *         one of its own (allocation failure)
 */
typedef cJSON *(*s_ipc_cmd_fn)(const wm_td *wm, const cJSON *args);


/** One dispatch table entry: a command's name and handler */
struct s_ipc_cmd_def_s {
    const char *name;
    s_ipc_cmd_fn handler;
};


/** The dispatch table itself, grouped by the same categories as
 *  ipc/actions/ itself
 *
 * @note @c tools/icowm-msg.c keeps a hand-maintained snapshot of every
 *       command name here, @a s_known_commands, used for its offline
 *       @c -L listing alone (see that array's comment for why it is
 *       not queried live instead)
 * @note Adding or removing an entry here means updating that array to
 *       match
 */
static const struct s_ipc_cmd_def_s s_commands[] = {
    /* Queries: ipc/actions/query.h */
    { "get_version",              ipc_action_get_version },
    { "list_desktops",            ipc_action_list_desktops },
    { "list_clients",             ipc_action_list_clients },
    { "get_focused",              ipc_action_get_focused },

    /* Client close/kill/restore/focus: ipc/actions/client/focus.h */
    { "close_client",             ipc_action_close_client },
    { "kill_client",              ipc_action_kill_client },
    { "deiconify_client",         ipc_action_deiconify_client },
    { "focus_client",             ipc_action_focus_client },
    { "unfocus_client",           ipc_action_unfocus_client },

    /* Client iconify/hide: ipc/actions/client/visibility.h */
    { "iconify_client",           ipc_action_iconify_client },
    { "hide_client",              ipc_action_hide_client },
    { "unhide_client",            ipc_action_unhide_client },

    /* Client pin/urgency flags: ipc/actions/client/flags.h */
    { "pin_client",               ipc_action_pin_client },
    { "unpin_client",             ipc_action_unpin_client },
    { "toggle_pin_client",        ipc_action_toggle_pin_client },
    { "urge_client",              ipc_action_urge_client },
    { "unurge_client",            ipc_action_unurge_client },

    /* Client geometry: ipc/actions/client/geom.h */
    { "move_client",              ipc_action_move_client },
    { "center_client",            ipc_action_center_client },
    { "move_client_to_monitor",   ipc_action_move_client_to_monitor },
    { "move_client_to_monitor_north",
        ipc_action_move_client_to_monitor_north },
    { "move_client_to_monitor_south",
        ipc_action_move_client_to_monitor_south },
    { "move_client_to_monitor_east",
        ipc_action_move_client_to_monitor_east },
    { "move_client_to_monitor_west",
        ipc_action_move_client_to_monitor_west },
    { "move_resize_client",        ipc_action_move_resize_client },
    { "resize_client",            ipc_action_resize_client },
    { "maximize_client_horz",     ipc_action_maximize_client_horz },
    { "maximize_client_vert",     ipc_action_maximize_client_vert },
    { "maximize_client",          ipc_action_maximize_client },

    /* Client layering: ipc/actions/client/layer.h */
    { "raise_client",             ipc_action_raise_client },
    { "lower_client",             ipc_action_lower_client },
    { "set_layer_above_client",   ipc_action_set_layer_above_client },
    { "set_layer_normal_client",  ipc_action_set_layer_normal_client },
    { "set_layer_below_client",   ipc_action_set_layer_below_client },
    { "cycle_layer_client",       ipc_action_cycle_layer_client },

    /* Client metadata: ipc/actions/client/meta.h */
    { "rename_client",            ipc_action_rename_client },
    { "reclass_client",           ipc_action_reclass_client },
    { "rerole_client",            ipc_action_rerole_client },
    { "set_client_icon",          ipc_action_set_client_icon },

    /* Client state: ipc/actions/client/state.h */
    { "shade_client",             ipc_action_shade_client },
    { "unshade_client",           ipc_action_unshade_client },
    { "toggle_shade_client",      ipc_action_toggle_shade_client },
    { "fullscreen_client",        ipc_action_fullscreen_client },
    { "unfullscreen_client",      ipc_action_unfullscreen_client },
    { "toggle_fullscreen_client", ipc_action_toggle_fullscreen_client },
    { "toggle_decorate_client",   ipc_action_toggle_decorate_client },

    /* Desktop-scoped: ipc/actions/desktop.h */
    { "set_desktop_background",   ipc_action_set_desktop_background },
    { "show_desktop",             ipc_action_show_desktop },
    { "send_client_to_desktop",   ipc_action_send_client_to_desktop },
    { "send_client_to_front",     ipc_action_send_client_to_front },
    { "send_client_to_back",      ipc_action_send_client_to_back },
    { "iconify_all",              ipc_action_iconify_all },
    { "deiconify_all",            ipc_action_deiconify_all },
    { "rearrange_desktop",        ipc_action_rearrange },

    /* Surface actions, which switch, add and remove desktops, live
     * in 'ipc/actions/surface.h' */
    { "goto_desktop",             ipc_action_goto_desktop },
    { "goto_north_desktop",       ipc_action_goto_north_desktop },
    { "goto_south_desktop",       ipc_action_goto_south_desktop },
    { "goto_east_desktop",        ipc_action_goto_east_desktop },
    { "goto_west_desktop",        ipc_action_goto_west_desktop },
    { "add_desktop",              ipc_action_add_desktop },
    { "remove_desktop",           ipc_action_remove_desktop },
    { "toggle_strutless_maximize",
      ipc_action_toggle_strutless_maximize },

    /* Whole window manager: ipc/actions/wm.h */
    { "exit_wm",                  ipc_action_exit_wm },
    { "restart_wm",               ipc_action_restart_wm },
    { "reload_config",            ipc_action_reload_config },
    { "toggle_scratchpad",        ipc_action_toggle_scratchpad },
};

/** Number of entries in 's_commands' */
#define S_IPC_COMMAND_COUNT \
    (sizeof(s_commands) / sizeof(s_commands[0]))


/* Handle one complete IPC request line and produce a response */
char *ipc_commands_dispatch(wm_td *wm, const char *request, int client_idx)
{
    cJSON *parsed;
    cJSON *cmd_item;
    cJSON *resp = NULL;
    bool found = false;
    char *out;

    parsed = cJSON_Parse(request);
    if (parsed == NULL || !cJSON_IsObject(parsed)) {
        cJSON_Delete(parsed);
        resp = ipc_response_error("request is not a JSON object");
        out = (resp != NULL) ? cJSON_PrintUnformatted(resp) : NULL;
        cJSON_Delete(resp);
        return out;
    }

    cmd_item = cJSON_GetObjectItem(parsed, "cmd");
    if (cmd_item == NULL || !cJSON_IsString(cmd_item)) {
        cJSON_Delete(parsed);
        resp = ipc_response_error("request is missing a string 'cmd'");
        out = (resp != NULL) ? cJSON_PrintUnformatted(resp) : NULL;
        cJSON_Delete(resp);
        return out;
    }

    /* 'subscribe'/'unsubscribe' ahead of the ordinary table: the
     * only two commands whose effect belongs to this specific
     * connection (see 'ipc.h''s comment on each) rather than to 'wm',
     * so neither one fits the table's handler shape at all */
    if (safe_strcmp(cmd_item->valuestring, "subscribe") == 0) {
        found = true;
        resp = ipc_client_subscribe(client_idx, parsed);
    } else if (safe_strcmp(cmd_item->valuestring, "unsubscribe") == 0) {
        found = true;
        resp = ipc_client_unsubscribe(client_idx, parsed);
    } else {
        for (size_t i = 0; i < S_IPC_COMMAND_COUNT; ++i) {
            if (safe_strcmp(s_commands[i].name,
                        cmd_item->valuestring) == 0) {
                found = true;
                resp = s_commands[i].handler(wm, parsed);
                break;
            }
        }
    }

    if (!found) {
        /* No entry in 's_commands' matched 'cmd' at all, as opposed
         * to matching one whose handler returned null from an
         * allocation failure (the 'resp == NULL' case just below).
         * That second case keeps a more specific message, since a
         * handler whose request DID match a real command is a
         * different failure than the request never matching one at
         * all. */
        resp = ipc_response_error("unknown command");
    } else if (resp == NULL) {
        resp = ipc_response_error("internal error building the response");
    }

    cJSON_Delete(parsed);

    out = (resp != NULL) ? cJSON_PrintUnformatted(resp) : NULL;
    cJSON_Delete(resp);
    return out;
}
