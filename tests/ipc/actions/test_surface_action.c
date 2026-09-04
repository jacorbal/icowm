/**
 * @file tests/ipc/actions/test_surface_action.c
 *
 * @brief Test battery for the IPC commands mirroring enact.h's
 *        enact_surface_desktop_switch* actions
 *
 * Named 'test_surface_action.c', not the shorter 'test_surface.c',
 * to stay unambiguous next to tests/test_surface_lifecycle.c,
 * tests/render/test_surface.c, and tests/surface/ elsewhere in this
 * same tree; none of those test ipc/actions/surface.c at all.  Every
 * enact_surface_* entry point below is a controllable, recording
 * stand-in, since exercising any of them for real needs a live
 * surface's whole desktop list and an X connection, entirely outside
 * this file's own target.  ipc_resolve_surface and ipc_resolve_desktop
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
#include <ipc/actions/surface.h>
#include <surface.h>
#include <wm.h>


/** Controllable stand-in for wm_get_surface_by_id (wm.c) */
static surface_td *s_surface_by_id_result;

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_surface_by_id_result;
}


/** Controllable stand-in for wm_surfaces (wm.c), reached only when a
 *  test's own args carry no explicit "surface_id" */
static list_td s_surfaces_list;

list_td *wm_surfaces(const wm_td *wm)
{
    (void) wm;
    return &s_surfaces_list;
}


/** Controllable stand-in for surface_desktop_get (surface.c), indexed
 *  by desktop_id, used only by ipc_action_goto_desktop's own
 *  ipc_resolve_desktop call */
static desktop_td *s_desktops_by_id[4];

desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
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


/** Call counters and the last surface argument each enact_surface_*
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
static const surface_td *s_last_surface_arg;


/**
 * @brief Controllable stand-in for enact_surface_desktop_switch
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch(surface_td *surface, uint32_t desktop_id)
{
    s_call_switch++;
    s_last_switch_desktop_id = desktop_id;
    s_last_surface_arg = surface;
}


/**
 * @brief Controllable stand-in for enact_surface_desktop_switch_north
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch_north(surface_td *surface)
{
    s_call_switch_north++;
    s_last_surface_arg = surface;
}


/**
 * @brief Controllable stand-in for enact_surface_desktop_switch_south
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch_south(surface_td *surface)
{
    s_call_switch_south++;
    s_last_surface_arg = surface;
}


/**
 * @brief Controllable stand-in for enact_surface_desktop_switch_east
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch_east(surface_td *surface)
{
    s_call_switch_east++;
    s_last_surface_arg = surface;
}


/**
 * @brief Controllable stand-in for enact_surface_desktop_switch_west
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch_west(surface_td *surface)
{
    s_call_switch_west++;
    s_last_surface_arg = surface;
}


/**
 * @brief Controllable stand-in for enact_surface_desktop_add
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_add(surface_td *surface)
{
    s_call_desktop_add++;
    s_last_surface_arg = surface;
}


/**
 * @brief Controllable stand-in for enact_surface_desktop_remove
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_remove(surface_td *surface)
{
    s_call_desktop_remove++;
    s_last_surface_arg = surface;
}


/**
 * @brief Controllable stand-in for
 *        enact_surface_toggle_strutless_maximize
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_toggle_strutless_maximize(surface_td *surface)
{
    s_call_toggle_strutless_maximize++;
    s_last_surface_arg = surface;
}


/** Every fixture this file needs; a non-null opaque handle stands in
 *  for a real wm_td, which this file never builds since the type is
 *  opaque outside wm.c itself, and nothing here ever dereferences
 *  it */
static int s_wm_storage;
static wm_td *const s_wm = (wm_td *) &s_wm_storage;
static surface_td s_surface;
static desktop_td s_desktop;


static void s_reset(void)
{
    size_t i;

    memset(&s_surface, 0, sizeof(s_surface));
    memset(&s_desktop, 0, sizeof(s_desktop));
    for (i = 0u; i < 4u; i++) {
        s_desktops_by_id[i] = NULL;
    }
    s_desktop.id = 0u;
    s_desktops_by_id[0] = &s_desktop;
    s_surface.desktop_count = 1u;
    s_surface.desktop_cur = 0u;
    s_surface_by_id_result = &s_surface;
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
    s_last_surface_arg = NULL;

    list_clear(&s_surfaces_list);
    s_surfaces_list.destroy = NULL;
    list_ins_next(&s_surfaces_list, NULL, &s_surface);
}


/* ipc_action_goto_desktop: a resolvable desktop switches the surface
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
            "enact_surface_desktop_switch is called exactly once");
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
            "enact_surface_desktop_switch is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* Each compass direction forwards to its own enact_surface_* call
 * when the surface resolves */
static void s_test_goto_compass_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_reset();
    resp = ipc_action_goto_north_desktop(s_wm, args);
    TAP_EQ_INT(s_call_switch_north, 1,
            "goto_north_desktop calls enact_surface_desktop_switch_north"
            " exactly once");
    cJSON_Delete(resp);

    s_reset();
    resp = ipc_action_goto_south_desktop(s_wm, args);
    TAP_EQ_INT(s_call_switch_south, 1,
            "goto_south_desktop calls enact_surface_desktop_switch_south"
            " exactly once");
    cJSON_Delete(resp);

    s_reset();
    resp = ipc_action_goto_east_desktop(s_wm, args);
    TAP_EQ_INT(s_call_switch_east, 1,
            "goto_east_desktop calls enact_surface_desktop_switch_east"
            " exactly once");
    cJSON_Delete(resp);

    s_reset();
    resp = ipc_action_goto_west_desktop(s_wm, args);
    TAP_EQ_INT(s_call_switch_west, 1,
            "goto_west_desktop calls enact_surface_desktop_switch_west"
            " exactly once");
    cJSON_Delete(resp);

    cJSON_Delete(args);
}


/* Each compass direction is an error, and never switches, when no
 * surface resolves at all */
static void s_test_goto_compass_no_surface_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_surface_by_id_result = NULL;
    list_clear(&s_surfaces_list);

    resp = ipc_action_goto_north_desktop(s_wm, args);
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "goto_north_desktop with no surface: an error response");
    TAP_EQ_INT(s_call_switch_north, 0,
            "and enact_surface_desktop_switch_north is never called");
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
    s_surface.desktop_count = 3u;

    resp = ipc_action_add_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "below the configured maximum: ok: true");
    TAP_EQ_INT(s_call_desktop_add, 1,
            "enact_surface_desktop_add is called exactly once");

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
    s_surface.desktop_count = (uint32_t) CONFIG_MAX_DESKTOPS;

    resp = ipc_action_add_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "at the configured maximum: an error response");
    TAP_EQ_INT(s_call_desktop_add, 0,
            "enact_surface_desktop_add is never called on that"
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
    s_surface.desktop_count = 1u;

    resp = ipc_action_add_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "restricted-memory mode: an error response even with"
            " room left under the maximum");
    TAP_EQ_INT(s_call_desktop_add, 0,
            "enact_surface_desktop_add is never called under"
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
    s_surface.desktop_count = 2u;

    resp = ipc_action_remove_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "more than one desktop: ok: true");
    TAP_EQ_INT(s_call_desktop_remove, 1,
            "enact_surface_desktop_remove is called exactly once");

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
    s_surface.desktop_count = 1u;

    resp = ipc_action_remove_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "the last remaining desktop: an error response");
    TAP_EQ_INT(s_call_desktop_remove, 0,
            "enact_surface_desktop_remove is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_toggle_strutless_maximize: a resolvable surface toggles
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
            "a resolvable surface: ok: true");
    TAP_EQ_INT(s_call_toggle_strutless_maximize, 1,
            "enact_surface_toggle_strutless_maximize is called"
            " exactly once");
    TAP_OK(s_last_surface_arg == &s_surface,
            "the exact resolved surface is forwarded");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_toggle_strutless_maximize: no resolvable surface is an
 * error, never toggled */
static void s_test_toggle_strutless_maximize_no_surface_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_surface_by_id_result = NULL;
    list_clear(&s_surfaces_list);

    resp = ipc_action_toggle_strutless_maximize(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "no resolvable surface: an error response");
    TAP_EQ_INT(s_call_toggle_strutless_maximize, 0,
            "enact_surface_toggle_strutless_maximize is never called"
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
    s_test_goto_compass_no_surface_is_error();
    s_test_add_desktop_ok();
    s_test_add_desktop_at_maximum_is_error();
    s_test_add_desktop_restricted_memory_is_error();
    s_test_remove_desktop_ok();
    s_test_remove_last_desktop_is_error();
    s_test_toggle_strutless_maximize_ok();
    s_test_toggle_strutless_maximize_no_surface_is_error();

    return TAP_DONE();
}
