/**
 * @file tests/ipc/actions/client/test_visibility.c
 *
 * @brief Test battery for the iconify/hide/unhide IPC client actions
 *
 * ipc/actions/client/visibility.c is three one-line wrappers around
 * ipc_dispatch_client_action, each with its own static callback that
 * forwards straight into one enact_client_* function.  This links
 * the real visibility.c and the real ipc_dispatch_client_action
 * (dispatch.c), resolve.c, args.c, response.c, and lookup.c, matching
 * tests/ipc/test_dispatch.c's own linking for the shared wrapper.
 * Every enact_client_* function these three callbacks reach is a
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
#include <stage.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>
#include <ipc/actions/client/visibility.h>


/** Link-only stand-in for stage_desktop_get (stage.c): lookup.c
 *  as a whole references it, though the id-based fast path
 *  lookup_find_client actually takes never reaches it */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
    return NULL;
}


/** Link-only stand-in for wm_get_stage_by_id (wm.c): ipc/resolve.c
 *  as a whole references it, from ipc_resolve_stage, which none
 *  of these three actions ever calls (only ipc_resolve_client) */
stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return NULL;
}


/** Call counters, reset by s_reset before each scenario */
static int s_call_iconify;
static int s_call_hide;
static int s_call_unhide;

/** Last client pointer each stand-in below actually received */
static client_td *s_last_client;


/**
 * @brief Recording stand-in for enact_client_iconify
 *
 * @note Complexity: O(1)
 */
void enact_client_iconify(client_td *client)
{
    s_call_iconify++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_hide
 *
 * @note Complexity: O(1)
 */
void enact_client_hide(client_td *client)
{
    s_call_hide++;
    s_last_client = client;
}


/**
 * @brief Recording stand-in for enact_client_unhide
 *
 * @note Complexity: O(1)
 */
void enact_client_unhide(client_td *client)
{
    s_call_unhide++;
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
static stage_td s_stage;
static desktop_td s_desktop;
static xcb_screen_t s_screen;
static client_td s_client;


/** Build one stage/desktop/client, holding a single client whose
 *  id is 5, and hang it off s_wm.stages */
static void s_build_wm(void)
{
    memset(&s_wm, 0, sizeof(s_wm));
    memset(&s_stage, 0, sizeof(s_stage));
    memset(&s_desktop, 0, sizeof(s_desktop));
    memset(&s_screen, 0, sizeof(s_screen));
    memset(&s_client, 0, sizeof(s_client));

    s_client.id = 5u;
    s_desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(s_desktop.clients, &s_client);
    s_stage.screen = &s_screen;
    s_stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(s_stage.desktops, NULL, &s_desktop);
    s_wm.stages = list_init(NULL);
    list_ins_next(s_wm.stages, NULL, &s_stage);

    s_call_iconify = 0;
    s_call_hide = 0;
    s_call_unhide = 0;
    s_last_client = NULL;
}


static void s_teardown_wm(void)
{
    list_destroy(s_wm.stages);
    cdlist_destroy(s_stage.desktops);
    ohtbl_destroy(s_desktop.clients);
}


/* iconify_client resolves the client and calls enact_client_iconify
 * once, reporting success */
static void s_test_iconify_client_resolvable(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 5);

    resp = ipc_action_iconify_client(&s_wm, args);

    TAP_EQ_INT(s_call_iconify, 1,
            "iconify_client calls enact_client_iconify once");
    TAP_OK(s_last_client == &s_client,
            "enact_client_iconify received the resolved client");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "iconify_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* An unresolvable client_id never calls enact_client_iconify */
static void s_test_iconify_client_unresolvable(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 999);

    resp = ipc_action_iconify_client(&s_wm, args);

    TAP_EQ_INT(s_call_iconify, 0,
            "an unresolvable client_id never calls"
            " enact_client_iconify");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "iconify_client reports failure");
    TAP_NOT_NULL(cJSON_GetObjectItem(resp, "error"),
            "the failure carries a reason");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing client_id is rejected before any enact call at all */
static void s_test_iconify_client_missing_id(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();

    resp = ipc_action_iconify_client(&s_wm, args);

    TAP_EQ_INT(s_call_iconify, 0,
            "a missing client_id never calls enact_client_iconify");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "iconify_client reports failure for a missing client_id");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* hide_client calls enact_client_hide, distinct from
 * enact_client_iconify */
static void s_test_hide_client(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 5);

    resp = ipc_action_hide_client(&s_wm, args);

    TAP_EQ_INT(s_call_hide, 1, "hide_client calls enact_client_hide once");
    TAP_EQ_INT(s_call_iconify, 0, "and never enact_client_iconify");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "hide_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* unhide_client calls enact_client_unhide, and an unresolvable id
 * still skips it while leaving the count unchanged */
static void s_test_unhide_client(void)
{
    cJSON *args_ok = cJSON_CreateObject();
    cJSON *args_bad = cJSON_CreateObject();
    cJSON *resp_ok;
    cJSON *resp_bad;

    s_build_wm();
    cJSON_AddNumberToObject(args_ok, "client_id", 5);
    cJSON_AddNumberToObject(args_bad, "client_id", 999);

    resp_ok = ipc_action_unhide_client(&s_wm, args_ok);
    TAP_EQ_INT(s_call_unhide, 1,
            "unhide_client calls enact_client_unhide once for a"
            " resolvable client");
    TAP_EQ_INT(s_call_hide, 0, "and never enact_client_hide");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp_ok, "ok")),
            "unhide_client reports success");

    resp_bad = ipc_action_unhide_client(&s_wm, args_bad);
    TAP_EQ_INT(s_call_unhide, 1,
            "an unresolvable client_id leaves the unhide count"
            " unchanged");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp_bad, "ok")),
            "and reports failure instead");

    cJSON_Delete(resp_ok);
    cJSON_Delete(resp_bad);
    cJSON_Delete(args_ok);
    cJSON_Delete(args_bad);
    s_teardown_wm();
}


int main(void)
{
    TAP_PLAN(16);

    s_test_iconify_client_resolvable();
    s_test_iconify_client_unresolvable();
    s_test_iconify_client_missing_id();
    s_test_hide_client();
    s_test_unhide_client();

    return TAP_DONE();
}
