/**
 * @file tests/ipc/actions/test_desktop.c
 *
 * @brief Test battery for the IPC commands mirroring enact.h's
 *        desktop-scoped actions
 *
 * ipc_resolve_desktop and ipc_resolve_client (ipc/resolve.c) are
 * linked for real, exactly as tests/ipc/test_resolve.c already
 * exercises their own branches directly, on top of the real
 * lookup_find_client (lookup.c), and real adt/list.c, adt/cdlist.c,
 * and adt/ohtbl.c fixtures built the same way tests/test_lookup.c
 * already builds its own surface_td/desktop_td/client_td trees by
 * hand (a live X connection is never needed for a plain lookup by
 * id).  Every enact_desktop_* entry point is a controllable,
 * recording stand-in, since driving any of them for real needs a
 * live X connection this file's own target has nothing to do with.
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
#include <stdlib.h>
#include <string.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* XCB includes */
#include <xcb/xcb.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Local includes */
#include <client.h>
#include <desktop.h>
#include <harness/tap.h>
#include <ipc/actions/desktop.h>
#include <surface.h>
#include <wm.h>


/** Controllable stand-in for wm_get_surface_by_id (wm.c) */
static surface_td *s_surface_by_id_result;

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_surface_by_id_result;
}


/** Controllable stand-in for wm_surfaces (wm.c), reached whenever a
 *  test's own args carry no explicit "surface_id" */
static list_td s_surfaces_list;

list_td *wm_surfaces(const wm_td *wm)
{
    (void) wm;
    return &s_surfaces_list;
}


/** Link-only stand-in for surface_desktop_get (surface.c), used by
 *  ipc_action_send_client_to_desktop's own resolution of its target
 *  desktop; walks the same fixture surface's circular desktop list
 *  by id rather than duplicating a second lookup table */
desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (surface == NULL || surface->desktops == NULL ||
            cdlist_size(surface->desktops) == 0) {
        return NULL;
    }

    node = cdlist_head(surface->desktops);
    initial = node;
    do {
        desktop_td *const desktop = (desktop_td *) cdlist_data(node);

        if (desktop != NULL && desktop->id == desktop_id) {
            return desktop;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/** Call counters and captured arguments for every enact_desktop_*
 *  stand-in below, reset by s_reset before each scenario */
static int s_call_set_background;
static uint32_t s_last_background_color;
static int s_call_show;
static bool s_last_show;
static int s_call_client_send;
static const desktop_td *s_last_send_target;
static int s_call_client_send_front;
static int s_call_client_send_back;
static int s_call_iconify_all;
static int s_call_deiconify_all;
static int s_call_rearrange;


/**
 * @brief Controllable stand-in for enact_desktop_set_background
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_set_background(desktop_td *desktop, uint32_t color)
{
    (void) desktop;
    s_call_set_background++;
    s_last_background_color = color;
}


/**
 * @brief Controllable stand-in for enact_desktop_show
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_show(desktop_td *desktop, bool show)
{
    (void) desktop;
    s_call_show++;
    s_last_show = show;
}


/**
 * @brief Controllable stand-in for enact_desktop_client_send
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_send(const desktop_td *desktop,
        client_td *client, desktop_td *target)
{
    (void) desktop;
    (void) client;
    s_call_client_send++;
    s_last_send_target = target;
}


/**
 * @brief Controllable stand-in for enact_desktop_client_send_front
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_send_front(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    s_call_client_send_front++;
}


/**
 * @brief Controllable stand-in for enact_desktop_client_send_back
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_send_back(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    s_call_client_send_back++;
}


/**
 * @brief Controllable stand-in for enact_desktop_clients_iconify_all
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_clients_iconify_all(desktop_td *desktop)
{
    (void) desktop;
    s_call_iconify_all++;
}


/**
 * @brief Controllable stand-in for enact_desktop_clients_deiconify_all
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_clients_deiconify_all(desktop_td *desktop)
{
    (void) desktop;
    s_call_deiconify_all++;
}


/**
 * @brief Controllable stand-in for enact_desktop_clients_rearrange
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_clients_rearrange(const wm_td *wm, surface_td *surface,
        const desktop_td *desktop)
{
    (void) wm;
    (void) surface;
    (void) desktop;
    s_call_rearrange++;
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


/** Build a desktop_td with the given id and an empty, ready-to-
 *  populate clients table */
static desktop_td *s_make_desktop(uint32_t id)
{
    desktop_td *const desktop = calloc(1, sizeof(desktop_td));

    desktop->id = id;
    desktop->clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    return desktop;
}


/** Build a client_td with the given window id, otherwise zeroed */
static client_td *s_make_client(xcb_window_t id)
{
    client_td *const client = calloc(1, sizeof(client_td));

    client->id = id;
    client->window = id;
    return client;
}


/** Fixtures every scenario shares: one surface with two desktops,
 *  desktop 0 holding one client */
static surface_td *s_surface;
static desktop_td *s_desktop0;
static desktop_td *s_desktop1;
static client_td *s_client;

/** A non-null opaque handle standing in for a real wm_td, which this
 *  file never builds since the type is opaque outside wm.c itself */
static int s_wm_storage;
static wm_td *const s_wm = (wm_td *) &s_wm_storage;


static void s_destroy_desktop(void *data)
{
    desktop_td *const desktop = (desktop_td *) data;
    void *elem;

    ohtbl_foreach(desktop->clients, elem) {
        free(elem);
    }
    ohtbl_destroy(desktop->clients);
    free(desktop);
}


static void s_reset(void)
{
    if (s_surface != NULL) {
        cdlist_destroy(s_surface->desktops);
        free(s_surface);
    }
    list_clear(&s_surfaces_list);
    s_surfaces_list.destroy = NULL;

    s_surface = calloc(1, sizeof(surface_td));
    s_surface->desktop_count = 2u;
    s_surface->desktop_cur = 0u;
    s_surface->desktops = cdlist_init(s_destroy_desktop);

    s_desktop0 = s_make_desktop(0u);
    s_desktop1 = s_make_desktop(1u);
    cdlist_ins_next(s_surface->desktops, cdlist_tail(s_surface->desktops),
            s_desktop0);
    cdlist_ins_next(s_surface->desktops, cdlist_tail(s_surface->desktops),
            s_desktop1);

    s_client = s_make_client(42u);
    ohtbl_insert(s_desktop0->clients, s_client);

    s_surface_by_id_result = s_surface;
    list_ins_next(&s_surfaces_list, NULL, s_surface);

    s_call_set_background = 0;
    s_last_background_color = 0u;
    s_call_show = 0;
    s_last_show = false;
    s_call_client_send = 0;
    s_last_send_target = NULL;
    s_call_client_send_front = 0;
    s_call_client_send_back = 0;
    s_call_iconify_all = 0;
    s_call_deiconify_all = 0;
    s_call_rearrange = 0;
}


/* ipc_action_set_desktop_background: a valid color and a resolvable
 * desktop set it, forwarding the exact color given */
static void s_test_set_background_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    cJSON_AddNumberToObject(args, "desktop_id", 0);
    cJSON_AddNumberToObject(args, "color", 0xFF00FFu);

    resp = ipc_action_set_desktop_background(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "a valid color and a resolvable desktop: ok: true");
    TAP_EQ_INT(s_call_set_background, 1,
            "enact_desktop_set_background is called exactly once");
    TAP_EQ_INT((int) s_last_background_color, (int) 0xFF00FFu,
            "the exact color given is forwarded");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_set_desktop_background: a missing 'color' is an error,
 * before any desktop is even resolved */
static void s_test_set_background_missing_color_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();

    resp = ipc_action_set_desktop_background(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "a missing 'color': an error response");
    TAP_EQ_INT(s_call_set_background, 0,
            "enact_desktop_set_background is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_show_desktop: a valid boolean and a resolvable desktop
 * forward the exact value given, both ways */
static void s_test_show_desktop_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_reset();
    cJSON_AddNumberToObject(args, "desktop_id", 0);
    cJSON_AddBoolToObject(args, "show", 1);

    resp = ipc_action_show_desktop(s_wm, args);

    TAP_EQ_INT(s_call_show, 1,
            "enact_desktop_show is called exactly once");
    TAP_OK(s_last_show == true, "a true 'show' is forwarded as true");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_show_desktop: a missing 'show' is an error */
static void s_test_show_desktop_missing_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();

    resp = ipc_action_show_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "a missing 'show': an error response");
    TAP_EQ_INT(s_call_show, 0,
            "enact_desktop_show is never called on that error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_send_client_to_desktop: a resolvable client and a valid
 * in-range target desktop send it, forwarding that exact target */
static void s_test_send_client_to_desktop_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    cJSON_AddNumberToObject(args, "client_id", 42);
    cJSON_AddNumberToObject(args, "target_desktop_id", 1);

    resp = ipc_action_send_client_to_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "a resolvable client and an in-range target: ok: true");
    TAP_EQ_INT(s_call_client_send, 1,
            "enact_desktop_client_send is called exactly once");
    TAP_OK(s_last_send_target == s_desktop1,
            "the exact resolved target desktop is forwarded");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_send_client_to_desktop: a target_desktop_id past the
 * client's own surface's desktop_count is an error, never sent */
static void s_test_send_client_to_desktop_out_of_range_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    cJSON_AddNumberToObject(args, "client_id", 42);
    cJSON_AddNumberToObject(args, "target_desktop_id", 5);

    resp = ipc_action_send_client_to_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "an out-of-range target_desktop_id: an error response");
    TAP_EQ_INT(s_call_client_send, 0,
            "enact_desktop_client_send is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_send_client_to_desktop: an unresolvable client_id is an
 * error, before any target desktop is even checked */
static void s_test_send_client_to_desktop_no_such_client_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    cJSON_AddNumberToObject(args, "client_id", 999);
    cJSON_AddNumberToObject(args, "target_desktop_id", 1);

    resp = ipc_action_send_client_to_desktop(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "no client with that id anywhere: an error response");
    TAP_EQ_INT(s_call_client_send, 0,
            "enact_desktop_client_send is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_send_client_to_front/back: a resolvable client forwards
 * to its own enact_desktop_client_send_front/back exactly once */
static void s_test_send_client_to_front_and_back_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_reset();
    cJSON_AddNumberToObject(args, "client_id", 42);

    resp = ipc_action_send_client_to_front(s_wm, args);
    TAP_EQ_INT(s_call_client_send_front, 1,
            "send_client_to_front calls enact_desktop_client_send_front"
            " exactly once");
    cJSON_Delete(resp);

    resp = ipc_action_send_client_to_back(s_wm, args);
    TAP_EQ_INT(s_call_client_send_back, 1,
            "send_client_to_back calls enact_desktop_client_send_back"
            " exactly once");
    cJSON_Delete(resp);

    cJSON_Delete(args);
}


/* ipc_action_send_client_to_front: an unresolvable client is an
 * error, never sent */
static void s_test_send_client_to_front_no_such_client_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    cJSON_AddNumberToObject(args, "client_id", 999);

    resp = ipc_action_send_client_to_front(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "no client with that id: an error response");
    TAP_EQ_INT(s_call_client_send_front, 0,
            "enact_desktop_client_send_front is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_iconify_all/deiconify_all: desktop_id is optional here,
 * falling back to the surface's own current desktop */
static void s_test_iconify_and_deiconify_all_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_reset();

    resp = ipc_action_iconify_all(s_wm, args);
    TAP_EQ_INT(s_call_iconify_all, 1,
            "iconify_all calls enact_desktop_clients_iconify_all"
            " exactly once");
    cJSON_Delete(resp);

    resp = ipc_action_deiconify_all(s_wm, args);
    TAP_EQ_INT(s_call_deiconify_all, 1,
            "deiconify_all calls enact_desktop_clients_deiconify_all"
            " exactly once");
    cJSON_Delete(resp);

    cJSON_Delete(args);
}


/* ipc_action_rearrange: a resolvable desktop rearranges its clients
 * exactly once */
static void s_test_rearrange_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();

    resp = ipc_action_rearrange(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "a resolvable desktop: ok: true");
    TAP_EQ_INT(s_call_rearrange, 1,
            "enact_desktop_clients_rearrange is called exactly once");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


/* ipc_action_rearrange: no resolvable surface at all is an error,
 * never rearranged */
static void s_test_rearrange_no_surface_is_error(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    s_reset();
    s_surface_by_id_result = NULL;
    list_clear(&s_surfaces_list);

    resp = ipc_action_rearrange(s_wm, args);

    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && !cJSON_IsTrue(ok_field),
            "no resolvable surface: an error response");
    TAP_EQ_INT(s_call_rearrange, 0,
            "enact_desktop_clients_rearrange is never called on that"
            " error path");

    cJSON_Delete(args);
    cJSON_Delete(resp);
}


int main(void)
{
    TAP_PLAN(26);

    s_test_set_background_ok();
    s_test_set_background_missing_color_is_error();
    s_test_show_desktop_ok();
    s_test_show_desktop_missing_is_error();
    s_test_send_client_to_desktop_ok();
    s_test_send_client_to_desktop_out_of_range_is_error();
    s_test_send_client_to_desktop_no_such_client_is_error();
    s_test_send_client_to_front_and_back_ok();
    s_test_send_client_to_front_no_such_client_is_error();
    s_test_iconify_and_deiconify_all_ok();
    s_test_rearrange_ok();
    s_test_rearrange_no_surface_is_error();

    return TAP_DONE();
}
