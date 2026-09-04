/**
 * @file tests/ipc/actions/client/test_state.c
 *
 * @brief Test battery for the shade/fullscreen/decoration IPC client
 *        actions
 *
 * ipc/actions/client/state.c is seven one-line wrappers around
 * ipc_dispatch_client_action, each with its own static callback that
 * forwards straight into one enact_client_* function.  This links
 * the real state.c and the real ipc_dispatch_client_action
 * (dispatch.c), resolve.c, args.c, response.c, and lookup.c, matching
 * tests/ipc/test_dispatch.c's own linking for the shared wrapper.
 * Every enact_client_* function these seven callbacks reach is a
 * recording stand-in below, since the real ones (enact/client.c)
 * pull in XCB requests this file has no reason to exercise: every
 * scenario asserts on which stand-in ran, and how many times, never
 * on any X side effect.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>
#include <ipc/actions/client/state.h>


/** Link-only stand-in for surface_desktop_get (surface.c): lookup.c
 *  as a whole references it, though the id-based fast path
 *  lookup_find_client actually takes never reaches it */
desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    return NULL;
}


/** Link-only stand-in for wm_get_surface_by_id (wm.c): ipc/resolve.c
 *  as a whole references it, from ipc_resolve_surface, which none
 *  of these seven actions ever calls (only ipc_resolve_client) */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return NULL;
}


/** Call counters, reset by s_reset before each scenario */
static int s_call_shade;
static int s_call_unshade;
static int s_call_toggle_shade;
static int s_call_fullscreen;
static int s_call_unfullscreen;
static int s_call_toggle_fullscreen;
static int s_call_toggle_decorate;

/** Last client pointer each stand-in below actually received */
static client_td *s_last_client;


/**
 * @brief Recording stand-in for enact_client_shade
 *
 * @note Complexity: O(1)
 */
void enact_client_shade(client_td *client)
{
    s_call_shade++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_unshade
 *
 * @note Complexity: O(1)
 */
void enact_client_unshade(client_td *client)
{
    s_call_unshade++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_toggle_shade
 *
 * @note Complexity: O(1)
 */
void enact_client_toggle_shade(client_td *client)
{
    s_call_toggle_shade++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_fullscreen
 *
 * @note Complexity: O(1)
 */
void enact_client_fullscreen(client_td *client)
{
    s_call_fullscreen++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_unfullscreen
 *
 * @note Complexity: O(1)
 */
void enact_client_unfullscreen(client_td *client)
{
    s_call_unfullscreen++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_toggle_fullscreen
 *
 * @note Complexity: O(1)
 */
void enact_client_toggle_fullscreen(client_td *client)
{
    s_call_toggle_fullscreen++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_toggle_decorate
 *
 * @note Complexity: O(1)
 */
void enact_client_toggle_decorate(client_td *client)
{
    s_call_toggle_decorate++;
    s_last_client = client;
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


/** Every fixture this file needs, wired up fresh by s_build_wm for
 *  each scenario, and torn down by s_teardown_wm right after */
static wm_td s_wm;
static surface_td s_surface;
static desktop_td s_desktop;
static xcb_screen_t s_screen;
static client_td s_client;


/** Build one surface/desktop/client, holding a single client whose
 *  id is 21, and hang it off s_wm.surfaces */
static void s_build_wm(void)
{
    memset(&s_wm, 0, sizeof(s_wm));
    memset(&s_surface, 0, sizeof(s_surface));
    memset(&s_desktop, 0, sizeof(s_desktop));
    memset(&s_screen, 0, sizeof(s_screen));
    memset(&s_client, 0, sizeof(s_client));

    s_client.id = 21u;
    s_desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(s_desktop.clients, &s_client);
    s_surface.screen = &s_screen;
    s_surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(s_surface.desktops, NULL, &s_desktop);
    s_wm.surfaces = list_init(NULL);
    list_ins_next(s_wm.surfaces, NULL, &s_surface);

    s_call_shade = 0;
    s_call_unshade = 0;
    s_call_toggle_shade = 0;
    s_call_fullscreen = 0;
    s_call_unfullscreen = 0;
    s_call_toggle_fullscreen = 0;
    s_call_toggle_decorate = 0;
    s_last_client = NULL;
}


static void s_teardown_wm(void)
{
    list_destroy(s_wm.surfaces);
    cdlist_destroy(s_surface.desktops);
    ohtbl_destroy(s_desktop.clients);
}


/* shade_client resolves the client and calls enact_client_shade
 * once, reporting success */
static void s_test_shade_client_resolvable(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 21);

    resp = ipc_action_shade_client(&s_wm, args);

    TAP_EQ_INT(s_call_shade, 1,
            "shade_client calls enact_client_shade once");
    TAP_OK(s_last_client == &s_client,
            "enact_client_shade received the resolved client");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "shade_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* An unresolvable client_id never calls enact_client_shade */
static void s_test_shade_client_unresolvable(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 999);

    resp = ipc_action_shade_client(&s_wm, args);

    TAP_EQ_INT(s_call_shade, 0,
            "an unresolvable client_id never calls enact_client_shade");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "shade_client reports failure");
    TAP_NOT_NULL(cJSON_GetObjectItem(resp, "error"),
            "the failure carries a reason");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing client_id is rejected before any enact call at all */
static void s_test_shade_client_missing_id(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();

    resp = ipc_action_shade_client(&s_wm, args);

    TAP_EQ_INT(s_call_shade, 0,
            "a missing client_id never calls enact_client_shade");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "shade_client reports failure for a missing client_id");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* unshade_client calls enact_client_unshade, distinct from
 * enact_client_shade */
static void s_test_unshade_client(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 21);

    resp = ipc_action_unshade_client(&s_wm, args);

    TAP_EQ_INT(s_call_unshade, 1,
            "unshade_client calls enact_client_unshade once");
    TAP_EQ_INT(s_call_shade, 0, "and never enact_client_shade");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "unshade_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* toggle_shade_client calls enact_client_toggle_shade, distinct from
 * either plain shade or unshade */
static void s_test_toggle_shade_client(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 21);

    resp = ipc_action_toggle_shade_client(&s_wm, args);

    TAP_EQ_INT(s_call_toggle_shade, 1,
            "toggle_shade_client calls enact_client_toggle_shade once");
    TAP_EQ_INT(s_call_shade + s_call_unshade, 0,
            "and neither plain shade nor unshade");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "toggle_shade_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* fullscreen_client calls enact_client_fullscreen, and an
 * unresolvable id still skips it while leaving the count unchanged */
static void s_test_fullscreen_client(void)
{
    cJSON *args_ok = cJSON_CreateObject();
    cJSON *args_bad = cJSON_CreateObject();
    cJSON *resp_ok;
    cJSON *resp_bad;

    s_build_wm();
    cJSON_AddNumberToObject(args_ok, "client_id", 21);
    cJSON_AddNumberToObject(args_bad, "client_id", 999);

    resp_ok = ipc_action_fullscreen_client(&s_wm, args_ok);
    TAP_EQ_INT(s_call_fullscreen, 1,
            "fullscreen_client calls enact_client_fullscreen once"
            " for a resolvable client");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp_ok, "ok")),
            "fullscreen_client reports success");

    resp_bad = ipc_action_fullscreen_client(&s_wm, args_bad);
    TAP_EQ_INT(s_call_fullscreen, 1,
            "an unresolvable client_id leaves the fullscreen count"
            " unchanged");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp_bad, "ok")),
            "and reports failure instead");

    cJSON_Delete(resp_ok);
    cJSON_Delete(resp_bad);
    cJSON_Delete(args_ok);
    cJSON_Delete(args_bad);
    s_teardown_wm();
}


/* unfullscreen_client calls enact_client_unfullscreen, distinct
 * from enact_client_fullscreen */
static void s_test_unfullscreen_client(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 21);

    resp = ipc_action_unfullscreen_client(&s_wm, args);

    TAP_EQ_INT(s_call_unfullscreen, 1,
            "unfullscreen_client calls enact_client_unfullscreen once");
    TAP_EQ_INT(s_call_fullscreen, 0,
            "and never enact_client_fullscreen");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "unfullscreen_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* toggle_fullscreen_client calls enact_client_toggle_fullscreen,
 * distinct from either plain fullscreen or unfullscreen */
static void s_test_toggle_fullscreen_client(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 21);

    resp = ipc_action_toggle_fullscreen_client(&s_wm, args);

    TAP_EQ_INT(s_call_toggle_fullscreen, 1,
            "toggle_fullscreen_client calls"
            " enact_client_toggle_fullscreen once");
    TAP_EQ_INT(s_call_fullscreen + s_call_unfullscreen, 0,
            "and neither plain fullscreen nor unfullscreen");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "toggle_fullscreen_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* toggle_decoration_client calls enact_client_toggle_decorate,
 * unrelated to shading or fullscreen */
static void s_test_toggle_decoration_client(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 21);

    resp = ipc_action_toggle_decorate_client(&s_wm, args);

    TAP_EQ_INT(s_call_toggle_decorate, 1,
            "toggle_decoration_client calls"
            " enact_client_toggle_decorate once");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "toggle_decoration_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


int main(void)
{
    TAP_PLAN(26);

    s_test_shade_client_resolvable();
    s_test_shade_client_unresolvable();
    s_test_shade_client_missing_id();
    s_test_unshade_client();
    s_test_toggle_shade_client();
    s_test_fullscreen_client();
    s_test_unfullscreen_client();
    s_test_toggle_fullscreen_client();
    s_test_toggle_decoration_client();

    return TAP_DONE();
}
