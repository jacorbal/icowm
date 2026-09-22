/**
 * @file tests/ipc/actions/test_stage_action.c
 *
 * @brief Test battery for the IPC commands mirroring enact.h's
 *        enact_stage_desktop_switch* actions
 *
 * Named 'test_stage_action.c', not the shorter 'test_stage.c',
 * to stay unambiguous next to tests/test_stage_lifecycle.c,
 * tests/render/test_stage.c, and tests/stage/ elsewhere in this
 * same tree; none of those test ipc/actions/stage.c at all.  Every
 * enact_stage_* entry point below is a controllable, recording
 * stand-in, since exercising any of them for real needs a live
 * stage's whole desktop list and an X connection, entirely outside
 * this file's own target.  ipc_resolve_stage and ipc_resolve_desktop
 * (ipc/resolve.c) are linked for real instead of stubbed, the same
 * way tests/ipc/test_resolve.c already covers their own branches
 * directly, and memguard_max_clients is a one-line controllable
 * stand-in rather than linking the real memguard.c, which drags in
 * a live X connection and message-dialog machinery unrelated to this
 * file's own target.
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
#include <string.h>

/* ADT includes */
#include <adt/list.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <desktop.h>
#include <harness/tap.h>
#include <ipc/actions/stage.h>
#include <stage.h>
#include <wm.h>


/** Controllable stand-in for wm_get_stage_by_id (wm.c) */
static stage_td *s_stage_by_id_result;

stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return s_stage_by_id_result;
}


/** Controllable stand-in for wm_stages (wm.c), reached only when a
 *  test's own args carry no explicit "stage_id" */
static list_td s_stages_list;

list_td *wm_stages(const wm_td *wm)
{
    (void) wm;
    return &s_stages_list;
}


/** Controllable stand-in for stage_desktop_get (stage.c), indexed
 *  by desktop_id, used only by ipc_action_goto_desktop's own
 *  ipc_resolve_desktop call */
static desktop_td *s_desktops_by_id[4];

desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    if (desktop_id >= 4u) {
        return NULL;
    }
    return s_desktops_by_id[desktop_id];
}


/** Controllable stand-in for memguard_max_clients (memguard.c) */
static uint32_t s_memguard_max_clients_result;

uint32_t memguard_max_clients(void)
{
    return s_memguard_max_clients_result;
}


/** Call counters and the last stage argument each enact_stage_*
 *  stand-in below was given, reset by s_reset before each scenario */
static int s_call_switch;
static uint32_t s_last_switch_desktop_id;
static int s_call_switch_north;
static int s_call_switch_south;
static int s_call_switch_east;
static int s_call_switch_west;
static int s_call_desktop_add;
static int s_call_desktop_remove;
static int s_call_toggle_strutless_maximize;
static const stage_td *s_last_stage_arg;


/**
 * @brief Controllable stand-in for enact_stage_desktop_switch
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_switch(stage_td *stage, uint32_t desktop_id)
{
    s_call_switch++;
    s_last_switch_desktop_id = desktop_id;
    s_last_stage_arg = stage;
}


/**
 * @brief Controllable stand-in for enact_stage_desktop_switch_north
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_switch_north(stage_td *stage)
{
    s_call_switch_north++;
    s_last_stage_arg = stage;
}


/**
 * @brief Controllable stand-in for enact_stage_desktop_switch_south
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_switch_south(stage_td *stage)
{
    s_call_switch_south++;
    s_last_stage_arg = stage;
}


/**
 * @brief Controllable stand-in for enact_stage_desktop_switch_east
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_switch_east(stage_td *stage)
{
    s_call_switch_east++;
    s_last_stage_arg = stage;
}


/**
 * @brief Controllable stand-in for enact_stage_desktop_switch_west
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_switch_west(stage_td *stage)
{
    s_call_switch_west++;
    s_last_stage_arg = stage;
}


/**
 * @brief Controllable stand-in for enact_stage_desktop_add
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_add(stage_td *stage)
{
    s_call_desktop_add++;
    s_last_stage_arg = stage;
}


/**
 * @brief Controllable stand-in for enact_stage_desktop_remove
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_desktop_remove(stage_td *stage)
{
    s_call_desktop_remove++;
    s_last_stage_arg = stage;
}


/**
 * @brief Controllable stand-in for
 *        enact_stage_toggle_strutless_maximize
 *
 * @note Complexity: @e O(1)
 */
void enact_stage_toggle_strutless_maximize(stage_td *stage)
{
    s_call_toggle_strutless_maximize++;
    s_last_stage_arg = stage;
}


/** Every fixture this file needs; a non-null opaque handle stands in
 *  for a real wm_td, which this file never builds since the type is
 *  opaque outside wm.c itself, and nothing here ever dereferences
 *  it */
static int s_wm_storage;
static wm_td *const s_wm = (wm_td *) &s_wm_storage;
static stage_td s_stage;
static desktop_td s_desktop;


static void s_reset(void)
{
    size_t i;

    memset(&s_stage, 0, sizeof(s_stage));
    memset(&s_desktop, 0, sizeof(s_desktop));
    for (i = 0u; i < 4u; i++) {
        s_desktops_by_id[i] = NULL;
    }
    s_desktop.id = 0u;
    s_desktops_by_id[0] = &s_desktop;
    s_stage.desktop_count = 1u;
    s_stage.desktop_cur = 0u;
    s_stage_by_id_result = &s_stage;
    s_memguard_max_clients_result = 0u;

    s_call_switch = 0;
    s_last_switch_desktop_id = 0xFFFFFFFFu;
    s_call_switch_north = 0;
    s_call_switch_south = 0;
    s_call_switch_east = 0;
    s_call_switch_west = 0;
    s_call_desktop_add = 0;
    s_call_desktop_remove = 0;
    s_call_toggle_strutless_maximize = 0;
    s_last_stage_arg = NULL;

    list_clear(&s_stages_list);
    s_stages_list.destroy = NULL;
    list_ins_next(&s_stages_list, NULL, &s_stage);
}


/* ipc_action_goto_desktop: a resolvable desktop switches the stage
 * to it and forwards the exact desktop id resolved */
static void s_test_goto_desktop_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    cJSON_AddNumberToObject(args, "desktop_id", 0);

    resp = ipc_action_goto_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "a resolvable desktop_id: ok: true");
    TAP_EQ_INT(s_call_switch, 1,
            "enact_stage_desktop_switch is called exactly once");
    TAP_EQ_INT((int) s_last_switch_desktop_id, 0,
            "the resolved desktop's own id is forwarded");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_goto_desktop: desktop_id is required here, so omitting
 * it is an error and never switches */
static void s_test_goto_desktop_missing_id_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();

    resp = ipc_action_goto_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "a missing desktop_id: an error response");
    TAP_EQ_INT(s_call_switch, 0,
            "enact_stage_desktop_switch is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* Each compass direction forwards to its own enact_stage_* call
 * when the stage resolves */
static void s_test_goto_compass_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_reset();
    resp = ipc_action_goto_desktop_north(s_wm, args);
    TAP_EQ_INT(s_call_switch_north, 1,
            "goto_desktop_north calls enact_stage_desktop_switch_north"
            " exactly once");
    cJSON_Delete(resp);

    s_reset();
    resp = ipc_action_goto_desktop_south(s_wm, args);
    TAP_EQ_INT(s_call_switch_south, 1,
            "goto_desktop_south calls enact_stage_desktop_switch_south"
            " exactly once");
    cJSON_Delete(resp);

    s_reset();
    resp = ipc_action_goto_desktop_east(s_wm, args);
    TAP_EQ_INT(s_call_switch_east, 1,
            "goto_desktop_east calls enact_stage_desktop_switch_east"
            " exactly once");
    cJSON_Delete(resp);

    s_reset();
    resp = ipc_action_goto_desktop_west(s_wm, args);
    TAP_EQ_INT(s_call_switch_west, 1,
            "goto_desktop_west calls enact_stage_desktop_switch_west"
            " exactly once");
    cJSON_Delete(resp);

    cJSON_Delete(args);
}


/* Each compass direction is an error, and never switches, when no
 * stage resolves at all */
static void s_test_goto_compass_no_stage_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_stage_by_id_result = NULL;
    list_clear(&s_stages_list);

    resp = ipc_action_goto_desktop_north(s_wm, args);
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "goto_desktop_north with no stage: an error response");
    TAP_EQ_INT(s_call_switch_north, 0,
            "and enact_stage_desktop_switch_north is never called");
    cJSON_Delete(resp);

    cJSON_Delete(args);
}


/* ipc_action_add_desktop: below the configured maximum, adds a
 * desktop */
static void s_test_add_desktop_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_stage.desktop_count = 3u;

    resp = ipc_action_add_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "below the configured maximum: ok: true");
    TAP_EQ_INT(s_call_desktop_add, 1,
            "enact_stage_desktop_add is called exactly once");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_add_desktop: at the configured maximum, refuses and
 * never adds */
static void s_test_add_desktop_at_maximum_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_stage.desktop_count = (uint32_t) CONFIG_MAX_DESKTOPS;

    resp = ipc_action_add_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "at the configured maximum: an error response");
    TAP_EQ_INT(s_call_desktop_add, 0,
            "enact_stage_desktop_add is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_add_desktop: restricted-memory mode locks to a single
 * desktop regardless of the configured maximum */
static void s_test_add_desktop_restricted_memory_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_memguard_max_clients_result = 32u;
    s_stage.desktop_count = 1u;

    resp = ipc_action_add_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "restricted-memory mode: an error response even with"
            " room left under the maximum");
    TAP_EQ_INT(s_call_desktop_add, 0,
            "enact_stage_desktop_add is never called under"
            " restricted-memory mode");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_remove_desktop: with more than one desktop, removes
 * one */
static void s_test_remove_desktop_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_stage.desktop_count = 2u;

    resp = ipc_action_remove_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "more than one desktop: ok: true");
    TAP_EQ_INT(s_call_desktop_remove, 1,
            "enact_stage_desktop_remove is called exactly once");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_remove_desktop: the very last desktop refuses removal */
static void s_test_remove_last_desktop_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_stage.desktop_count = 1u;

    resp = ipc_action_remove_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "the last remaining desktop: an error response");
    TAP_EQ_INT(s_call_desktop_remove, 0,
            "enact_stage_desktop_remove is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_toggle_strutless_maximize: a resolvable stage toggles
 * it exactly once */
static void s_test_toggle_strutless_maximize_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();

    resp = ipc_action_toggle_strutless_maximize(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "a resolvable stage: ok: true");
    TAP_EQ_INT(s_call_toggle_strutless_maximize, 1,
            "enact_stage_toggle_strutless_maximize is called"
            " exactly once");
    TAP_OK(s_last_stage_arg == &s_stage,
            "the exact resolved stage is forwarded");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_toggle_strutless_maximize: no resolvable stage is an
 * error, never toggled */
static void s_test_toggle_strutless_maximize_no_stage_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_stage_by_id_result = NULL;
    list_clear(&s_stages_list);

    resp = ipc_action_toggle_strutless_maximize(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "no resolvable stage: an error response");
    TAP_EQ_INT(s_call_toggle_strutless_maximize, 0,
            "enact_stage_toggle_strutless_maximize is never called"
            " on that error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


int main(void)
{
    TAP_PLAN(26);

    s_test_goto_desktop_ok();
    s_test_goto_desktop_missing_id_is_error();
    s_test_goto_compass_ok();
    s_test_goto_compass_no_stage_is_error();
    s_test_add_desktop_ok();
    s_test_add_desktop_at_maximum_is_error();
    s_test_add_desktop_restricted_memory_is_error();
    s_test_remove_desktop_ok();
    s_test_remove_last_desktop_is_error();
    s_test_toggle_strutless_maximize_ok();
    s_test_toggle_strutless_maximize_no_stage_is_error();

    return TAP_DONE();
}
