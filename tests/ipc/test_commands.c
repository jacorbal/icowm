/**
 * @file tests/ipc/test_commands.c
 *
 * @brief Test battery for the top-level IPC command dispatch table
 *
 * ipc/commands.c's own real value is entirely in its top-level
 * parsing (malformed JSON, a missing/non-string "cmd") and its own
 * routing logic (an unmatched name, "internal error" when a matched
 * handler itself returns NULL) -- none of which needs any handler's
 * own real behavior to run.  Every one of the ~57 handlers the
 * dispatch table references is stubbed below purely so the table
 * itself (and the linker) has something to point to; all but two are
 * generic no-op stand-ins.  ipc_action_get_version and ipc_action_
 * rearrange are the two exceptions, each returning an identifiable
 * marker instead, used only to confirm the dispatcher actually
 * routes to the specific handler a given "cmd" names, not some other
 * one.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>
#include <ipc/commands.h>
#include <ipc/response.h>


/** Link-only stand-ins for subscribe/unsubscribe (ipc.c): handled
 *  ahead of the ordinary table, so the linker needs these too */
cJSON *ipc_client_subscribe(int client_idx, const cJSON *args)
{
    (void) client_idx;
    (void) args;
    return NULL;
}

cJSON *ipc_client_unsubscribe(int client_idx, const cJSON *args)
{
    (void) client_idx;
    (void) args;
    return NULL;
}


/** The two identifiable handlers, used to confirm correct routing */
cJSON *ipc_action_get_version(const wm_td *wm, const cJSON *args)
{
    cJSON *resp = ipc_response_ok();

    (void) wm;
    (void) args;
    cJSON_AddStringToObject(resp, "marker", "get_version_called");
    return resp;
}

cJSON *ipc_action_rearrange(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    /* Simulates an allocation failure inside a matched handler */
    return NULL;
}


cJSON *ipc_action_center_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_close_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_cycle_layer_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_deiconify_all(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_deiconify_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_exit_wm(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_focus_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_fullscreen_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_get_focused(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_goto_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_goto_north_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_goto_south_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_goto_east_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_goto_west_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_add_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_remove_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_restart_wm(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_toggle_strutless_maximize(const wm_td *wm,
        const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_hide_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_iconify_all(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_iconify_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_kill_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_list_clients(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_list_desktops(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_lower_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_maximize_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_maximize_client_horz(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_maximize_client_vert(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_move_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_move_client_to_monitor(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_move_client_to_monitor_north(const wm_td *wm,
        const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_move_client_to_monitor_south(const wm_td *wm,
        const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_move_client_to_monitor_east(const wm_td *wm,
        const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_move_client_to_monitor_west(const wm_td *wm,
        const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_move_resize_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_pin_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_raise_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_reclass_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_reload_config(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_rename_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_rerole_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_resize_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_send_client_to_back(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_send_client_to_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_send_client_to_front(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_set_client_icon(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_set_desktop_background(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_set_layer_above_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_set_layer_below_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_set_layer_normal_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_shade_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_show_desktop(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_toggle_decorate_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_toggle_fullscreen_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_toggle_pin_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_toggle_scratchpad(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_toggle_shade_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_unfocus_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_unfullscreen_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_unhide_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_unpin_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_unshade_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_unurge_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


cJSON *ipc_action_urge_client(const wm_td *wm, const cJSON *args)
{
    (void) wm;
    (void) args;
    return NULL;
}


/* Malformed (non-JSON) input is reported cleanly, never crashes */
static void s_test_malformed_json(void)
{
    wm_td wm;
    char *response;
    cJSON *parsed;

    memset(&wm, 0, sizeof(wm));
    response = ipc_commands_dispatch(&wm, "not valid json {{{", 0);

    TAP_NOT_NULL(response, "a response is still produced");
    parsed = cJSON_Parse(response);
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(parsed, "ok")),
            "malformed JSON is reported as a failure");
    TAP_EQ_STR(cJSON_GetObjectItem(parsed, "error")->valuestring,
            "request is not a JSON object",
            "the error message names the real cause");

    cJSON_Delete(parsed);
    free(response);
}


/* Valid JSON that is not an object (e.g., a bare array) is rejected
 * the same way malformed JSON is */
static void s_test_json_not_an_object(void)
{
    wm_td wm;
    char *response;
    cJSON *parsed;

    memset(&wm, 0, sizeof(wm));
    response = ipc_commands_dispatch(&wm, "[1, 2, 3]", 0);

    parsed = cJSON_Parse(response);
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(parsed, "ok")),
            "a JSON array (not an object) is rejected");

    cJSON_Delete(parsed);
    free(response);
}


/* A JSON object with no "cmd" field at all is reported cleanly */
static void s_test_missing_cmd_field(void)
{
    wm_td wm;
    char *response;
    cJSON *parsed;

    memset(&wm, 0, sizeof(wm));
    response = ipc_commands_dispatch(&wm, "{}", 0);

    parsed = cJSON_Parse(response);
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(parsed, "ok")),
            "a missing 'cmd' field is a failure");
    TAP_EQ_STR(cJSON_GetObjectItem(parsed, "error")->valuestring,
            "request is missing a string 'cmd'",
            "the error message names the real cause");

    cJSON_Delete(parsed);
    free(response);
}


/* A "cmd" field that is present but not a string is rejected the
 * same way a missing one is */
static void s_test_cmd_field_not_a_string(void)
{
    wm_td wm;
    char *response;
    cJSON *parsed;

    memset(&wm, 0, sizeof(wm));
    response = ipc_commands_dispatch(&wm, "{\"cmd\": 42}", 0);

    parsed = cJSON_Parse(response);
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(parsed, "ok")),
            "a non-string 'cmd' field is a failure");

    cJSON_Delete(parsed);
    free(response);
}


/* A syntactically valid request naming a command that is not in the
 * table at all is reported as "unknown command" */
static void s_test_unknown_command(void)
{
    wm_td wm;
    char *response;
    cJSON *parsed;

    memset(&wm, 0, sizeof(wm));
    response = ipc_commands_dispatch(&wm, "{\"cmd\": \"no_such_thing\"}", 0);

    parsed = cJSON_Parse(response);
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(parsed, "ok")),
            "an unmatched command name is a failure");
    TAP_EQ_STR(cJSON_GetObjectItem(parsed, "error")->valuestring,
            "unknown command",
            "the error message names the real cause");

    cJSON_Delete(parsed);
    free(response);
}


/* A known command routes to its own specific handler, not some
 * other one, and that handler's own response passes through intact */
static void s_test_known_command_routes_correctly(void)
{
    wm_td wm;
    char *response;
    cJSON *parsed;

    memset(&wm, 0, sizeof(wm));
    response = ipc_commands_dispatch(&wm, "{\"cmd\": \"get_version\"}", 0);

    parsed = cJSON_Parse(response);
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(parsed, "ok")),
            "a known command's own handler response passes through");
    TAP_EQ_STR(cJSON_GetObjectItem(parsed, "marker")->valuestring,
            "get_version_called",
            "the dispatcher routed to the exact handler the command" \
            " names, not another one");

    cJSON_Delete(parsed);
    free(response);
}


/* A matched handler that itself returns NULL (an allocation
 * failure building its own response) is reported as an internal
 * error, not confused with an unmatched command */
static void s_test_handler_returning_null_is_internal_error(void)
{
    wm_td wm;
    char *response;
    cJSON *parsed;

    memset(&wm, 0, sizeof(wm));
    response = ipc_commands_dispatch(&wm, "{\"cmd\": \"rearrange_desktop\"}",
            0);

    parsed = cJSON_Parse(response);
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(parsed, "ok")),
            "a handler returning NULL is a failure");
    TAP_EQ_STR(cJSON_GetObjectItem(parsed, "error")->valuestring,
            "internal error building the response",
            "distinct from 'unknown command': this command WAS matched");

    cJSON_Delete(parsed);
    free(response);
}


int main(void)
{
    TAP_PLAN(13);

    s_test_malformed_json();
    s_test_json_not_an_object();
    s_test_missing_cmd_field();
    s_test_cmd_field_not_a_string();
    s_test_unknown_command();
    s_test_known_command_routes_correctly();
    s_test_handler_returning_null_is_internal_error();

    return TAP_DONE();
}
