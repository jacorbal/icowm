/**
 * @file tests/ipc/test_resolve.c
 *
 * @brief Test battery for turning a request's own numeric IDs into
 *        real pointers
 *
 * wm_get_surface_by_id (wm.c) and surface_desktop_get (surface.c,
 * reached indirectly through lookup_current_desktop) are both
 * stubbed below as controllable stand-ins rather than no-ops: each
 * reads from a small array this file's own tests populate before
 * calling into ipc_resolve_*, so every branch (found, not found,
 * out of range) can be driven directly without either module's own
 * much larger, XCB-dependent real implementation.  lookup_find_client
 * itself is linked for real (lookup.c), exercised the same way
 * test_lookup.c already does, since it needs nothing beyond that.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>
#include <string.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <desktop.h>
#include <client.h>
#include <harness/tap.h>
#include <ipc/resolve.h>
#include <wm/internal.h>


/** Controllable stand-in for wm_get_surface_by_id (wm.c): tests set
 *  this before calling anything that might reach it */
static surface_td *s_surface_by_id_result = NULL;

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_surface_by_id_result;
}


/** Controllable stand-in for surface_desktop_get (surface.c):
 *  indexed by desktop_id, so both ipc_resolve_desktop's own direct
 *  call and its indirect one through lookup_current_desktop can be
 *  driven the same realistic way */
static desktop_td *s_desktops_by_id[8];

desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    if (desktop_id >= 8u) {
        return NULL;
    }
    return s_desktops_by_id[desktop_id];
}


static size_t s_id_hash1(const void *key)
{
    return (size_t) ((const client_td *) key)->id;
}

static size_t s_id_hash2(const void *key)
{
    (void) key;
    return 1u;
}

static bool s_id_match(const void *key1, const void *key2)
{
    return ((const client_td *) key1)->id ==
        ((const client_td *) key2)->id;
}


/* ipc_resolve_surface: an explicit, valid "surface_id" is resolved
 * through wm_get_surface_by_id */
static void s_test_resolve_surface_explicit_id(void)
{
    surface_td surface;
    cJSON *args = cJSON_CreateObject();
    surface_td *found;

    memset(&surface, 0, sizeof(surface));
    s_surface_by_id_result = &surface;
    cJSON_AddNumberToObject(args, "surface_id", 3);

    found = ipc_resolve_surface(NULL, args);
    TAP_OK(found == &surface,
            "an explicit surface_id resolves through wm_get_surface_by_id");

    cJSON_Delete(args);
}


/* ipc_resolve_surface: with no "surface_id" given, falls back to the
 * first surface in wm->surfaces */
static void s_test_resolve_surface_falls_back_to_first(void)
{
    wm_td wm;
    surface_td a;
    surface_td b;
    cJSON *args = cJSON_CreateObject();
    surface_td *found;

    memset(&wm, 0, sizeof(wm));
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    wm.surfaces = list_init(NULL);
    list_ins_next(wm.surfaces, NULL, &b);
    list_ins_next(wm.surfaces, NULL, &a);
    /* list_ins_next(NULL) inserts at the head, so 'a' (inserted
     * last) is the head/first */

    found = ipc_resolve_surface(&wm, args);
    TAP_OK(found == &a, "no surface_id given: falls back to the" \
            " first surface in the list");

    list_destroy(wm.surfaces);
    cJSON_Delete(args);
}


/* ipc_resolve_surface: with no "surface_id" and an empty (or NULL)
 * surface list, there is nothing to fall back to */
static void s_test_resolve_surface_empty_list_fails(void)
{
    wm_td wm;
    cJSON *args = cJSON_CreateObject();

    memset(&wm, 0, sizeof(wm));
    wm.surfaces = list_init(NULL);

    TAP_NULL(ipc_resolve_surface(&wm, args),
            "an empty surface list has nothing to fall back to");
    TAP_NULL(ipc_resolve_surface(&wm, args),
            "(consistent across repeated calls)");

    list_destroy(wm.surfaces);
    cJSON_Delete(args);
}


/* ipc_resolve_desktop: when the underlying surface itself cannot be
 * resolved, fails with "no such surface" */
static void s_test_resolve_desktop_no_surface(void)
{
    wm_td wm;
    cJSON *args = cJSON_CreateObject();
    surface_td *out_surface = NULL;
    cJSON *out_error = NULL;
    desktop_td *found;

    memset(&wm, 0, sizeof(wm));
    wm.surfaces = list_init(NULL);

    found = ipc_resolve_desktop(&wm, args, false, &out_surface,
            &out_error);

    TAP_NULL(found, "no resolvable surface: desktop resolution fails");
    TAP_NOT_NULL(out_error, "an error response is built");
    TAP_EQ_STR(cJSON_GetObjectItem(out_error, "error")->valuestring,
            "no such surface", "the error message names the real cause");

    cJSON_Delete(out_error);
    list_destroy(wm.surfaces);
    cJSON_Delete(args);
}


/* ipc_resolve_desktop: an explicit, in-range "desktop_id" resolves
 * through surface_desktop_get directly */
static void s_test_resolve_desktop_explicit_id(void)
{
    surface_td surface;
    desktop_td desktop;
    cJSON *args = cJSON_CreateObject();
    surface_td *out_surface = NULL;
    cJSON *out_error = NULL;
    desktop_td *found;

    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    surface.desktop_count = 4u;
    s_surface_by_id_result = &surface;
    s_desktops_by_id[2] = &desktop;
    cJSON_AddNumberToObject(args, "surface_id", 0);
    cJSON_AddNumberToObject(args, "desktop_id", 2);

    found = ipc_resolve_desktop(NULL, args, true, &out_surface, &out_error);

    TAP_OK(found == &desktop, "an explicit, in-range desktop_id resolves");
    TAP_OK(out_surface == &surface, "out_surface is set to the" \
            " resolved surface");

    s_desktops_by_id[2] = NULL;
    cJSON_Delete(args);
}


/* ipc_resolve_desktop: an explicit "desktop_id" at or beyond the
 * surface's own desktop_count fails, without ever consulting
 * surface_desktop_get for it */
static void s_test_resolve_desktop_id_out_of_range(void)
{
    surface_td surface;
    cJSON *args = cJSON_CreateObject();
    surface_td *out_surface = NULL;
    cJSON *out_error = NULL;
    desktop_td *found;

    memset(&surface, 0, sizeof(surface));
    surface.desktop_count = 2u;
    s_surface_by_id_result = &surface;
    cJSON_AddNumberToObject(args, "surface_id", 0);
    cJSON_AddNumberToObject(args, "desktop_id", 5);

    found = ipc_resolve_desktop(NULL, args, true, &out_surface, &out_error);

    TAP_NULL(found, "an out-of-range desktop_id fails");
    TAP_EQ_STR(cJSON_GetObjectItem(out_error, "error")->valuestring,
            "no such desktop on that surface",
            "the error message names the real cause");

    cJSON_Delete(out_error);
    cJSON_Delete(args);
}


/* ipc_resolve_desktop: a missing "desktop_id" fails only when the
 * caller marked it required */
static void s_test_resolve_desktop_missing_id_required(void)
{
    surface_td surface;
    cJSON *args = cJSON_CreateObject();
    surface_td *out_surface = NULL;
    cJSON *out_error = NULL;
    desktop_td *found;

    memset(&surface, 0, sizeof(surface));
    surface.desktop_count = 2u;
    s_surface_by_id_result = &surface;
    cJSON_AddNumberToObject(args, "surface_id", 0);

    found = ipc_resolve_desktop(NULL, args, true, &out_surface, &out_error);

    TAP_NULL(found, "a missing desktop_id fails when required");
    TAP_EQ_STR(cJSON_GetObjectItem(out_error, "error")->valuestring,
            "missing or invalid 'desktop_id'",
            "the error message names the real cause");

    cJSON_Delete(out_error);
    cJSON_Delete(args);
}


/* ipc_resolve_desktop: a missing "desktop_id" that is not required
 * falls back to the surface's own current desktop */
static void s_test_resolve_desktop_missing_id_falls_back(void)
{
    surface_td surface;
    desktop_td desktop;
    cJSON *args = cJSON_CreateObject();
    surface_td *out_surface = NULL;
    cJSON *out_error = NULL;
    desktop_td *found;

    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    surface.desktop_count = 3u;
    surface.desktop_cur = 1u;
    s_surface_by_id_result = &surface;
    s_desktops_by_id[1] = &desktop;
    cJSON_AddNumberToObject(args, "surface_id", 0);

    found = ipc_resolve_desktop(NULL, args, false, &out_surface,
            &out_error);

    TAP_OK(found == &desktop,
            "a missing, non-required desktop_id falls back to the" \
            " surface's own current desktop");

    s_desktops_by_id[1] = NULL;
    cJSON_Delete(args);
}


/* ipc_resolve_client: a missing "client_id" fails immediately,
 * without ever searching anything */
static void s_test_resolve_client_missing_id(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *out_error = NULL;
    client_td *found;

    found = ipc_resolve_client(NULL, args, NULL, NULL, &out_error);

    TAP_NULL(found, "a missing client_id fails");
    TAP_EQ_STR(cJSON_GetObjectItem(out_error, "error")->valuestring,
            "missing or invalid 'client_id'",
            "the error message names the real cause");

    cJSON_Delete(out_error);
    cJSON_Delete(args);
}


/* ipc_resolve_client: a valid client_id that matches a real,
 * managed client resolves it, through the real lookup_find_client */
static void s_test_resolve_client_found(void)
{
    wm_td wm;
    surface_td surface;
    desktop_td desktop;
    xcb_screen_t screen;
    client_td client;
    cJSON *args = cJSON_CreateObject();
    surface_td *out_surface = NULL;
    desktop_td *out_desktop = NULL;
    cJSON *out_error = NULL;
    client_td *found;

    memset(&wm, 0, sizeof(wm));
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&screen, 0, sizeof(screen));
    memset(&client, 0, sizeof(client));

    client.id = 77;
    desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(desktop.clients, &client);

    surface.screen = &screen;
    surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(surface.desktops, NULL, &desktop);

    wm.surfaces = list_init(NULL);
    list_ins_next(wm.surfaces, NULL, &surface);

    cJSON_AddNumberToObject(args, "client_id", 77);

    found = ipc_resolve_client(&wm, args, &out_surface, &out_desktop,
            &out_error);

    TAP_OK(found == &client, "a real, managed client_id resolves");
    TAP_OK(out_surface == &surface, "out_surface is correctly set");
    TAP_OK(out_desktop == &desktop, "out_desktop is correctly set");

    list_destroy(wm.surfaces);
    cdlist_destroy(surface.desktops);
    ohtbl_destroy(desktop.clients);
    cJSON_Delete(args);
}


/* ipc_resolve_client: a valid client_id that matches nothing fails
 * with "no such client" */
static void s_test_resolve_client_not_found(void)
{
    wm_td wm;
    cJSON *args = cJSON_CreateObject();
    cJSON *out_error = NULL;
    client_td *found;

    memset(&wm, 0, sizeof(wm));
    wm.surfaces = list_init(NULL);
    cJSON_AddNumberToObject(args, "client_id", 999);

    found = ipc_resolve_client(&wm, args, NULL, NULL, &out_error);

    TAP_NULL(found, "an unmatched client_id fails");
    TAP_EQ_STR(cJSON_GetObjectItem(out_error, "error")->valuestring,
            "no such client", "the error message names the real cause");

    cJSON_Delete(out_error);
    list_destroy(wm.surfaces);
    cJSON_Delete(args);
}


int main(void)
{
    TAP_PLAN(21);

    s_test_resolve_surface_explicit_id();
    s_test_resolve_surface_falls_back_to_first();
    s_test_resolve_surface_empty_list_fails();
    s_test_resolve_desktop_no_surface();
    s_test_resolve_desktop_explicit_id();
    s_test_resolve_desktop_id_out_of_range();
    s_test_resolve_desktop_missing_id_required();
    s_test_resolve_desktop_missing_id_falls_back();
    s_test_resolve_client_missing_id();
    s_test_resolve_client_found();
    s_test_resolve_client_not_found();

    return TAP_DONE();
}
