/**
 * @file tests/ipc/test_dispatch.c
 *
 * @brief Test battery for the shared "resolve a client, act on it,
 *        report success" wrapper
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

/* Local includes */
#include <desktop.h>
#include <client.h>
#include <harness/tap.h>
#include <ipc/dispatch.h>
#include <wm/internal.h>


/** Link-only stand-in for stage_desktop_get (stage.c): lookup.c
 *  as a whole references it, though nothing here ever reaches that
 *  particular path (see test_lookup.c's own identical note) */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
    return NULL;
}

/** Link-only stand-in for wm_get_stage_by_id (wm.c): ipc/resolve.c
 *  as a whole references it (from ipc_resolve_stage, which this
 *  file never exercises directly), so the linker needs a definition
 *  somewhere */
stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return NULL;
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


/* Records whether the action ran, and with which arguments */
static bool s_action_called;
static const wm_td *s_action_wm;
static client_td *s_action_client;
static stage_td *s_action_stage;
static desktop_td *s_action_desktop;

static void s_recording_action(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop)
{
    s_action_called = true;
    s_action_wm = wm;
    s_action_client = client;
    s_action_stage = stage;
    s_action_desktop = desktop;
}


/* A resolvable client_id runs the action with the correctly resolved
 * client/stage/desktop, and reports success */
static void s_test_dispatch_runs_action_on_success(void)
{
    wm_td wm;
    stage_td stage;
    desktop_td desktop;
    xcb_screen_t screen;
    client_td client;
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    memset(&wm, 0, sizeof(wm));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&screen, 0, sizeof(screen));
    memset(&client, 0, sizeof(client));

    client.id = 55;
    desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(desktop.clients, &client);
    stage.screen = &screen;
    stage.desktops = cdlist_init(NULL);
    cdlist_ins_next(stage.desktops, NULL, &desktop);
    wm.stages = list_init(NULL);
    list_ins_next(wm.stages, NULL, &stage);
    cJSON_AddNumberToObject(args, "client_id", 55);

    s_action_called = false;
    resp = ipc_dispatch_client_action(&wm, args, s_recording_action);

    TAP_OK(s_action_called, "the action ran for a resolvable client");
    TAP_OK(s_action_wm == &wm, "the action received the right wm");
    TAP_OK(s_action_client == &client,
            "the action received the resolved client");
    TAP_OK(s_action_stage == &stage,
            "the action received the resolved stage");
    TAP_OK(s_action_desktop == &desktop,
            "the action received the resolved desktop");

    TAP_NOT_NULL(resp, "a response was built");
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(cJSON_IsTrue(ok_field), "the response reports success");

    cJSON_Delete(resp);
    list_destroy(wm.stages);
    cdlist_destroy(stage.desktops);
    ohtbl_destroy(desktop.clients);
    cJSON_Delete(args);
}


/* An unresolvable client_id never runs the action at all, and
 * reports the resolution failure instead */
static void s_test_dispatch_skips_action_on_failure(void)
{
    wm_td wm;
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;
    cJSON *ok_field;

    memset(&wm, 0, sizeof(wm));
    wm.stages = list_init(NULL);
    cJSON_AddNumberToObject(args, "client_id", 999);

    s_action_called = false;
    resp = ipc_dispatch_client_action(&wm, args, s_recording_action);

    TAP_OK(!s_action_called,
            "the action never runs when the client cannot be resolved");
    TAP_NOT_NULL(resp, "a response was still built");
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(!cJSON_IsTrue(ok_field), "the response reports failure");
    TAP_NOT_NULL(cJSON_GetObjectItem(resp, "error"),
            "the response carries the resolution failure's own reason");

    cJSON_Delete(resp);
    list_destroy(wm.stages);
    cJSON_Delete(args);
}


int main(void)
{
    TAP_PLAN(11);

    s_test_dispatch_runs_action_on_success();
    s_test_dispatch_skips_action_on_failure();

    return TAP_DONE();
}
