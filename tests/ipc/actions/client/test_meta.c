/**
 * @file tests/ipc/actions/client/test_meta.c
 *
 * @brief Test battery for the rename/reclass/rerole/set-icon IPC
 *        client actions
 *
 * Unlike every other file under ipc/actions/client, meta.c never
 * calls ipc_dispatch_client_action: each of its four public entry
 * points validates its own string argument or arguments first, then
 * calls ipc_resolve_client directly, and only then its one
 * enact_client_* function.  This links the real meta.c together
 * with the real resolve.c, args.c, response.c, and lookup.c it calls
 * into, matching tests/ipc/test_dispatch.c's own linking pattern for
 * the sibling wrapper.  Every enact_client_* function these four
 * entry points reach is a recording stand-in below, since the real
 * ones (enact/client.c) pull in XCB requests this file has no reason
 * to exercise: every scenario asserts on which stand-in ran, with
 * what arguments, and how many times, never on any X side effect.
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
#include <ipc/actions/client/meta.h>


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
 *  of these four actions ever calls (only ipc_resolve_client) */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return NULL;
}


/** Call counters, reset by s_build_wm before each scenario */
static int s_call_rename;
static int s_call_reclass;
static int s_call_rerole;
static int s_call_set_icon;

/** Last client pointer, and last string argument or arguments, each
 *  stand-in below actually received */
static client_td *s_last_client;
static const char *s_last_name;
static const char *s_last_class_name;
static const char *s_last_instance_name;
static const char *s_last_role;
static const char *s_last_icon_name;


/**
 * @brief Recording stand-in for enact_client_rename
 *
 * @note Complexity: O(1)
 */
void enact_client_rename(client_td *client, const char *name)
{
    s_call_rename++;
    s_last_client = client;
    s_last_name = name;
}


/**
 * @brief Recording stand-in for enact_client_reclass
 *
 * @note Complexity: O(1)
 */
void enact_client_reclass(client_td *client,
        const char *restrict class_name,
        const char *restrict instance_name)
{
    s_call_reclass++;
    s_last_client = client;
    s_last_class_name = class_name;
    s_last_instance_name = instance_name;
}


/**
 * @brief Recording stand-in for enact_client_rerole
 *
 * @note Complexity: O(1)
 */
void enact_client_rerole(client_td *client, const char *role)
{
    s_call_rerole++;
    s_last_client = client;
    s_last_role = role;
}


/**
 * @brief Recording stand-in for enact_client_set_icon
 *
 * @note Complexity: O(1)
 */
void enact_client_set_icon(client_td *client, const char *icon_name)
{
    s_call_set_icon++;
    s_last_client = client;
    s_last_icon_name = icon_name;
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
 *  id is 9, and hang it off s_wm.surfaces */
static void s_build_wm(void)
{
    memset(&s_wm, 0, sizeof(s_wm));
    memset(&s_surface, 0, sizeof(s_surface));
    memset(&s_desktop, 0, sizeof(s_desktop));
    memset(&s_screen, 0, sizeof(s_screen));
    memset(&s_client, 0, sizeof(s_client));

    s_client.id = 9u;
    s_desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(s_desktop.clients, &s_client);
    s_surface.screen = &s_screen;
    s_surface.desktops = cdlist_init(NULL);
    cdlist_ins_next(s_surface.desktops, NULL, &s_desktop);
    s_wm.surfaces = list_init(NULL);
    list_ins_next(s_wm.surfaces, NULL, &s_surface);

    s_call_rename = 0;
    s_call_reclass = 0;
    s_call_rerole = 0;
    s_call_set_icon = 0;
    s_last_client = NULL;
    s_last_name = NULL;
    s_last_class_name = NULL;
    s_last_instance_name = NULL;
    s_last_role = NULL;
    s_last_icon_name = NULL;
}


static void s_teardown_wm(void)
{
    list_destroy(s_wm.surfaces);
    cdlist_destroy(s_surface.desktops);
    ohtbl_destroy(s_desktop.clients);
}


/* rename_client resolves the client and calls enact_client_rename
 * with the exact name given, reporting success */
static void s_test_rename_client_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);
    cJSON_AddStringToObject(args, "name", "xterm");

    resp = ipc_action_rename_client(&s_wm, args);

    TAP_EQ_INT(s_call_rename, 1,
            "rename_client calls enact_client_rename once");
    TAP_OK(s_last_client == &s_client,
            "enact_client_rename received the resolved client");
    TAP_EQ_STR(s_last_name, "xterm",
            "enact_client_rename received the given name verbatim");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "rename_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing 'name' is rejected before client resolution is even
 * attempted: an otherwise-resolvable client_id is present, yet
 * enact_client_rename never runs */
static void s_test_rename_client_missing_name(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);

    resp = ipc_action_rename_client(&s_wm, args);

    TAP_EQ_INT(s_call_rename, 0,
            "a missing 'name' never calls enact_client_rename, even"
            " with a resolvable client_id");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "rename_client reports failure");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'name'",
            "the failure names the missing field exactly");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A wrong-typed 'name' (a number, not a string) is rejected the same
 * way as a missing one */
static void s_test_rename_client_wrong_type_name(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);
    cJSON_AddNumberToObject(args, "name", 123);

    resp = ipc_action_rename_client(&s_wm, args);

    TAP_EQ_INT(s_call_rename, 0,
            "a numeric 'name' never calls enact_client_rename");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "rename_client reports failure for a wrong-typed 'name'");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A valid 'name' but an unresolvable client_id calls
 * enact_client_rename zero times and reports the resolution error */
static void s_test_rename_client_unresolvable(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 999);
    cJSON_AddStringToObject(args, "name", "xterm");

    resp = ipc_action_rename_client(&s_wm, args);

    TAP_EQ_INT(s_call_rename, 0,
            "an unresolvable client_id never calls"
            " enact_client_rename, even with a valid 'name'");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "rename_client reports failure");
    TAP_NOT_NULL(cJSON_GetObjectItem(resp, "error"),
            "the failure carries a reason");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* reclass_client resolves the client and calls enact_client_reclass
 * with both strings given, reporting success */
static void s_test_reclass_client_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);
    cJSON_AddStringToObject(args, "class_name", "XTerm");
    cJSON_AddStringToObject(args, "instance_name", "xterm");

    resp = ipc_action_reclass_client(&s_wm, args);

    TAP_EQ_INT(s_call_reclass, 1,
            "reclass_client calls enact_client_reclass once");
    TAP_EQ_STR(s_last_class_name, "XTerm",
            "enact_client_reclass received the given class_name"
            " verbatim");
    TAP_EQ_STR(s_last_instance_name, "xterm",
            "enact_client_reclass received the given instance_name"
            " verbatim");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "reclass_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing 'class_name' is rejected before 'instance_name' is even
 * looked at, and before client resolution */
static void s_test_reclass_client_missing_class_name(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);
    cJSON_AddStringToObject(args, "instance_name", "xterm");

    resp = ipc_action_reclass_client(&s_wm, args);

    TAP_EQ_INT(s_call_reclass, 0,
            "a missing 'class_name' never calls enact_client_reclass");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'class_name'",
            "the failure names 'class_name' rather than"
            " 'instance_name'");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A present 'class_name' but a missing 'instance_name' is rejected
 * with its own distinct message */
static void s_test_reclass_client_missing_instance_name(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);
    cJSON_AddStringToObject(args, "class_name", "XTerm");

    resp = ipc_action_reclass_client(&s_wm, args);

    TAP_EQ_INT(s_call_reclass, 0,
            "a missing 'instance_name' never calls"
            " enact_client_reclass");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'instance_name'",
            "the failure names 'instance_name' exactly");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* rerole_client resolves the client and calls enact_client_rerole
 * with the exact role given */
static void s_test_rerole_client_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);
    cJSON_AddStringToObject(args, "role", "browser");

    resp = ipc_action_rerole_client(&s_wm, args);

    TAP_EQ_INT(s_call_rerole, 1,
            "rerole_client calls enact_client_rerole once");
    TAP_EQ_STR(s_last_role, "browser",
            "enact_client_rerole received the given role verbatim");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "rerole_client reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing 'role' is rejected before client resolution */
static void s_test_rerole_client_missing_role(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);

    resp = ipc_action_rerole_client(&s_wm, args);

    TAP_EQ_INT(s_call_rerole, 0,
            "a missing 'role' never calls enact_client_rerole");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'role'",
            "the failure names 'role' exactly");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* set_client_icon resolves the client and calls
 * enact_client_set_icon with the exact icon_name given */
static void s_test_set_client_icon_ok(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);
    cJSON_AddStringToObject(args, "icon_name", "terminal");

    resp = ipc_action_set_client_icon(&s_wm, args);

    TAP_EQ_INT(s_call_set_icon, 1,
            "set_client_icon calls enact_client_set_icon once");
    TAP_EQ_STR(s_last_icon_name, "terminal",
            "enact_client_set_icon received the given icon_name"
            " verbatim");
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(resp, "ok")),
            "set_client_icon reports success");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


/* A missing 'icon_name' is rejected before client resolution */
static void s_test_set_client_icon_missing_icon_name(void)
{
    cJSON *args = cJSON_CreateObject();
    cJSON *resp;

    s_build_wm();
    cJSON_AddNumberToObject(args, "client_id", 9);

    resp = ipc_action_set_client_icon(&s_wm, args);

    TAP_EQ_INT(s_call_set_icon, 0,
            "a missing 'icon_name' never calls enact_client_set_icon");
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(resp, "error")),
            "missing or invalid 'icon_name'",
            "the failure names 'icon_name' exactly");

    cJSON_Delete(resp);
    cJSON_Delete(args);
    s_teardown_wm();
}


int main(void)
{
    TAP_PLAN(30);

    s_test_rename_client_ok();
    s_test_rename_client_missing_name();
    s_test_rename_client_wrong_type_name();
    s_test_rename_client_unresolvable();
    s_test_reclass_client_ok();
    s_test_reclass_client_missing_class_name();
    s_test_reclass_client_missing_instance_name();
    s_test_rerole_client_ok();
    s_test_rerole_client_missing_role();
    s_test_set_client_icon_ok();
    s_test_set_client_icon_missing_icon_name();

    return TAP_DONE();
}
