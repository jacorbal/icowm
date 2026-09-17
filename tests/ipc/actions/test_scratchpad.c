/**
 * @file tests/ipc/actions/test_scratchpad.c
 *
 * @brief Test battery for the IPC command mirroring scratchpad.h's
 *        toggle action
 *
 * ipc_action_toggle_scratchpad is a thin wrapper around
 * ipc_resolve_desktop, itself already covered directly by
 * tests/ipc/test_resolve.c, so this file links the real
 * ipc/resolve.c, ipc/args.c, ipc/response.c, and lookup.c, following
 * the same wm_get_stage_by_id and stage_desktop_get controllable
 * stand-ins tests/ipc/test_resolve.c already established, rather
 * than duplicating ipc_resolve_desktop's own branch coverage here.
 * scratchpad_toggle (scratchpad.c) is a recording stand-in below,
 * since driving its own real behavior needs a live X connection and
 * a spawned process, entirely outside this file's own target.
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
#include <ipc/actions/scratchpad.h>
#include <stage.h>
#include <wm.h>


/** Controllable stand-in for wm_get_stage_by_id (wm.c) */
static stage_td *s_stage_by_id_result;

stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return s_stage_by_id_result;
}


/** Controllable stand-in for wm_stages (wm.c): reached only when a
 *  test's own args carry no explicit "stage_id" at all, in which
 *  case ipc_resolve_stage falls back to the first stage on this
 *  list */
static list_td s_stages_list;

list_td *wm_stages(const wm_td *wm)
{
    (void) wm;
    return &s_stages_list;
}


/** Controllable stand-in for stage_desktop_get (stage.c), indexed
 *  by desktop_id */
static desktop_td *s_desktops_by_id[8];

desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    if (desktop_id >= 8u) {
        return NULL;
    }
    return s_desktops_by_id[desktop_id];
}


/** Recording stand-in for scratchpad_toggle (scratchpad.c) */
static int s_call_scratchpad_toggle;
static const desktop_td *s_last_toggle_desktop;

void scratchpad_toggle(const wm_td *wm, desktop_td *desktop)
{
    (void) wm;
    s_call_scratchpad_toggle++;
    s_last_toggle_desktop = desktop;
}


/** A non-null opaque handle standing in for a real wm_td, which this
 *  file never builds since the type is opaque outside wm.c itself;
 *  nothing here ever dereferences it, only passes it through to
 *  scratchpad_toggle's own stand-in above */
static int s_wm_storage;
static wm_td *const s_wm = (wm_td *) &s_wm_storage;
static stage_td s_stage;
static desktop_td s_desktop;


static void s_reset(void)
{
    size_t i;

    memset(&s_stage, 0, sizeof(s_stage));
    memset(&s_desktop, 0, sizeof(s_desktop));
    for (i = 0u; i < 8u; i++) {
        s_desktops_by_id[i] = NULL;
    }
    s_desktop.id = 0u;
    s_desktops_by_id[0] = &s_desktop;
    s_stage.desktop_count = 1u;
    s_stage.desktop_cur = 0u;
    s_stage_by_id_result = &s_stage;
    s_call_scratchpad_toggle = 0;
    s_last_toggle_desktop = NULL;

    list_clear(&s_stages_list);
    s_stages_list.destroy = NULL;
    list_ins_next(&s_stages_list, NULL, &s_stage);
}


/* No stage at all (stage_id explicit but unresolved, and no
 * fallback stage either): an error response, never toggled */
static void s_test_no_stage_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_stage_by_id_result = NULL;
    cJSON_AddNumberToObject(args, "stage_id", 9);

    resp = ipc_action_toggle_scratchpad(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "no resolvable stage: an error response");
    TAP_EQ_INT(s_call_scratchpad_toggle, 0,
            "scratchpad_toggle is never called on that error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* desktop_id omitted: falls back to the stage's own current
 * desktop, and toggles it */
static void s_test_missing_desktop_id_falls_back_and_toggles(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();

    resp = ipc_action_toggle_scratchpad(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "a resolvable fallback desktop: ok: true");
    TAP_EQ_INT(s_call_scratchpad_toggle, 1,
            "scratchpad_toggle is called exactly once");
    TAP_OK(s_last_toggle_desktop == &s_desktop,
            "the fallback current desktop is the one toggled");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* desktop_id given and in range: that exact desktop is resolved and
 * toggled, not the stage's current one */
static void s_test_explicit_desktop_id_is_used(void)
{
    desktop_td other;
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_reset();
    memset(&other, 0, sizeof(other));
    other.id = 1u;
    s_desktops_by_id[1] = &other;
    s_stage.desktop_count = 2u;
    cJSON_AddNumberToObject(args, "desktop_id", 1);

    resp = ipc_action_toggle_scratchpad(s_wm, args);

    TAP_OK(s_last_toggle_desktop == &other,
            "an explicit in-range desktop_id is toggled instead of"
            " the current one");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* desktop_id given but out of range for the resolved stage: an
 * error, never toggled */
static void s_test_out_of_range_desktop_id_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    cJSON_AddNumberToObject(args, "desktop_id", 5);

    resp = ipc_action_toggle_scratchpad(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "an out-of-range desktop_id: an error response");
    TAP_EQ_INT(s_call_scratchpad_toggle, 0,
            "scratchpad_toggle is never called on that error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


int main(void)
{
    TAP_PLAN(8);

    s_test_no_stage_is_error();
    s_test_missing_desktop_id_falls_back_and_toggles();
    s_test_explicit_desktop_id_is_used();
    s_test_out_of_range_desktop_id_is_error();

    return TAP_DONE();
}
