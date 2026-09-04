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
 * the same wm_get_surface_by_id and surface_desktop_get controllable
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
#include <surface.h>
#include <wm.h>


/** Controllable stand-in for wm_get_surface_by_id (wm.c) */
static surface_td *s_surface_by_id_result;

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_surface_by_id_result;
}


/** Controllable stand-in for wm_surfaces (wm.c): reached only when a
 *  test's own args carry no explicit "surface_id" at all, in which
 *  case ipc_resolve_surface falls back to the first surface on this
 *  list */
static list_td s_surfaces_list;

list_td *wm_surfaces(const wm_td *wm)
{
    (void) wm;
    return &s_surfaces_list;
}


/** Controllable stand-in for surface_desktop_get (surface.c), indexed
 *  by desktop_id */
static desktop_td *s_desktops_by_id[8];

desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
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
static surface_td s_surface;
static desktop_td s_desktop;


static void s_reset(void)
{
    size_t i;

    memset(&s_surface, 0, sizeof(s_surface));
    memset(&s_desktop, 0, sizeof(s_desktop));
    for (i = 0u; i < 8u; i++) {
        s_desktops_by_id[i] = NULL;
    }
    s_desktop.id = 0u;
    s_desktops_by_id[0] = &s_desktop;
    s_surface.desktop_count = 1u;
    s_surface.desktop_cur = 0u;
    s_surface_by_id_result = &s_surface;
    s_call_scratchpad_toggle = 0;
    s_last_toggle_desktop = NULL;

    list_clear(&s_surfaces_list);
    s_surfaces_list.destroy = NULL;
    list_ins_next(&s_surfaces_list, NULL, &s_surface);
}


/* No surface at all (surface_id explicit but unresolved, and no
 * fallback surface either): an error response, never toggled */
static void s_test_no_surface_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_surface_by_id_result = NULL;
    cJSON_AddNumberToObject(args, "surface_id", 9);

    resp = ipc_action_toggle_scratchpad(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "no resolvable surface: an error response");
    TAP_EQ_INT(s_call_scratchpad_toggle, 0,
            "scratchpad_toggle is never called on that error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* desktop_id omitted: falls back to the surface's own current
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
 * toggled, not the surface's current one */
static void s_test_explicit_desktop_id_is_used(void)
{
    desktop_td other;
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_reset();
    memset(&other, 0, sizeof(other));
    other.id = 1u;
    s_desktops_by_id[1] = &other;
    s_surface.desktop_count = 2u;
    cJSON_AddNumberToObject(args, "desktop_id", 1);

    resp = ipc_action_toggle_scratchpad(s_wm, args);

    TAP_OK(s_last_toggle_desktop == &other,
            "an explicit in-range desktop_id is toggled instead of"
            " the current one");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* desktop_id given but out of range for the resolved surface: an
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

    s_test_no_surface_is_error();
    s_test_missing_desktop_id_falls_back_and_toggles();
    s_test_explicit_desktop_id_is_used();
    s_test_out_of_range_desktop_id_is_error();

    return TAP_DONE();
}
