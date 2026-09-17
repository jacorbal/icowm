/**
 * @file tests/ipc/actions/test_query.c
 *
 * @brief Test battery for the read-only IPC query commands
 *
 * Unlike the other files in this directory, query.c's own real
 * dependencies (stage_desktop_walk_all, stage_desktop_get,
 * lookup_current_desktop) are cheap, deterministic, and X-free, so
 * this file links every one of them for real rather than stubbing
 * them: src/stage/desktops.c, src/lookup.c, and the real adt/list,
 * adt/cdlist, and adt/ohtbl backing them.  Only wm_stages (wm.c)
 * is a controllable stand-in, the same role tests/ipc/test_resolve.c
 * already gives it, since building a real wm_td here is not possible
 * from outside wm.c (the type is opaque).  Fixture construction
 * follows tests/test_lookup.c's own established pattern for building
 * stage_td/desktop_td/client_td trees by hand.
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
#include <ipc/actions/query.h>
#include <stage.h>
#include <wm.h>


/** Controllable stand-in for wm_stages (wm.c) */
static list_td s_stages_list;

list_td *wm_stages(const wm_td *wm)
{
    (void) wm;
    return &s_stages_list;
}


/** Link-only stand-in for desktop_destroy (desktop.c): only reached
 *  from stage_desktop_rem, a function this file never calls; the
 *  linker still needs a definition for it since it is referenced
 *  from the same translation unit as stage_desktop_walk_all, the one
 *  function here actually under test */
void desktop_destroy(desktop_td *desktop)
{
    (void) desktop;
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


/** Build a desktop_td with the given id and name, and an empty,
 *  ready-to-populate clients table */
static desktop_td *s_make_desktop(uint32_t id, const char *name)
{
    desktop_td *const desktop = calloc(1, sizeof(desktop_td));

    desktop->id = id;
    (void) strncpy(desktop->name, name, sizeof(desktop->name) - 1u);
    desktop->clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    return desktop;
}


/** Build a client_td with the given window id and name, otherwise
 *  zeroed (never iconified, urgent, pinned, or locked unless a test
 *  flips the relevant state bit afterward) */
static client_td *s_make_client(xcb_window_t id, char *name)
{
    client_td *const client = calloc(1, sizeof(client_td));

    client->id = id;
    client->window = id;
    client->info.name = name;
    client->layout.geometry.cur.pos.x = 10;
    client->layout.geometry.cur.pos.y = 20;
    client->layout.geometry.cur.dim.w = 300;
    client->layout.geometry.cur.dim.h = 200;
    return client;
}


/** Fixtures shared by every scenario: one stage, two desktops,
 *  desktop 0 holding a plain client and a locked one, desktop 1
 *  holding an iconified, urgent, pinned client, and set as the
 *  stage's own current desktop */
static stage_td *s_stage;
static desktop_td *s_desktop0;
static desktop_td *s_desktop1;
static client_td *s_plain_client;
static client_td *s_locked_client;
static client_td *s_flagged_client;

static int s_wm_storage;
static wm_td *const s_wm = (wm_td *) &s_wm_storage;


static void s_reset(void)
{
    if (s_stage != NULL) {
        cdlist_destroy(s_stage->desktops);
        free(s_stage->screen);
        free(s_stage);
    }
    list_clear(&s_stages_list);
    s_stages_list.destroy = NULL;

    s_stage = calloc(1, sizeof(stage_td));
    s_stage->id = 7u;
    s_stage->desktop_count = 2u;
    s_stage->desktop_cur = 1u;
    s_stage->screen = calloc(1, sizeof(xcb_screen_t));
    s_stage->desktops = cdlist_init(s_destroy_desktop);

    s_desktop0 = s_make_desktop(0u, "one");
    s_desktop1 = s_make_desktop(1u, "two");
    s_desktop1->client_active_id = 55u;
    cdlist_ins_next(s_stage->desktops, cdlist_tail(s_stage->desktops),
            s_desktop0);
    cdlist_ins_next(s_stage->desktops, cdlist_tail(s_stage->desktops),
            s_desktop1);

    s_plain_client = s_make_client(10u, (char *) "plain");
    ohtbl_insert(s_desktop0->clients, s_plain_client);

    s_locked_client = s_make_client(11u, (char *) "locked");
    s_locked_client->properties.flags |= (uint16_t) CLIENT_FLAG_LOCKED;
    ohtbl_insert(s_desktop0->clients, s_locked_client);

    s_flagged_client = s_make_client(55u, (char *) "flagged");
    s_flagged_client->properties.state |=
        (uint16_t) CLIENT_STATE_ICONIFIED;
    s_flagged_client->properties.flags |= (uint16_t) CLIENT_FLAG_URGENT;
    s_flagged_client->properties.flags |= (uint16_t) CLIENT_FLAG_PIN;
    ohtbl_insert(s_desktop1->clients, s_flagged_client);

    list_ins_next(&s_stages_list, NULL, s_stage);
}


/* "get_version": reports the fixed protocol version */
static void s_test_get_version(void)
{
    cJSON *resp;
    cJSON *version_field;

    s_reset();
    resp = ipc_action_get_version(s_wm, NULL);
    version_field = cJSON_GetObjectItem(resp, "protocol_version");

    TAP_NOT_NULL(version_field,
            "get_version's response carries a protocol_version field");
    TAP_EQ_INT((int) cJSON_GetNumberValue(version_field), 1,
            "protocol_version matches IPC_PROTOCOL_VERSION");

    cJSON_Delete(resp);
}


/* "list_desktops": every desktop on every stage, each carrying its
 * own id, name, stage_id, and current flag */
static void s_test_list_desktops(void)
{
    cJSON *resp;
    cJSON *desktops;
    cJSON *first;
    cJSON *second;

    s_reset();
    resp = ipc_action_list_desktops(s_wm, NULL);
    desktops = cJSON_GetObjectItem(resp, "desktops");

    TAP_NOT_NULL(desktops, "list_desktops returns a desktops array");
    TAP_EQ_INT(cJSON_GetArraySize(desktops), 2,
            "both desktops on the one stage are listed");

    first = cJSON_GetArrayItem(desktops, 0);
    TAP_EQ_STR(cJSON_GetStringValue(cJSON_GetObjectItem(first, "name")),
            "one", "the first desktop's own name is reported");
    TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(first, "current")),
            "desktop 0 is not the stage's current desktop");

    second = cJSON_GetArrayItem(desktops, 1);
    TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(second, "current")),
            "desktop 1 is reported as the stage's current desktop");
    TAP_EQ_INT((int) cJSON_GetNumberValue(
                cJSON_GetObjectItem(second, "stage_id")), 7,
            "each desktop entry carries its own stage's id");

    cJSON_Delete(resp);
}


/* "list_clients": every unlocked client across every desktop, each
 * carrying its own geometry and state flags; a locked client is
 * skipped entirely */
static void s_test_list_clients(void)
{
    cJSON *resp;
    cJSON *clients;
    int found_plain;
    int found_locked;
    int found_flagged;
    int i;

    s_reset();
    resp = ipc_action_list_clients(s_wm, NULL);
    clients = cJSON_GetObjectItem(resp, "clients");

    TAP_EQ_INT(cJSON_GetArraySize(clients), 2,
            "the locked client is excluded, leaving exactly two");

    found_plain = 0;
    found_locked = 0;
    found_flagged = 0;
    for (i = 0; i < cJSON_GetArraySize(clients); i++) {
        cJSON *const entry = cJSON_GetArrayItem(clients, i);
        const char *const name =
            cJSON_GetStringValue(cJSON_GetObjectItem(entry, "name"));

        if (name != NULL && strcmp(name, "plain") == 0) {
            found_plain = 1;
            TAP_EQ_INT((int) cJSON_GetNumberValue(
                        cJSON_GetObjectItem(entry, "x")), 10,
                    "the plain client's own x coordinate is reported");
            TAP_OK(!cJSON_IsTrue(cJSON_GetObjectItem(entry, "iconified")),
                    "the plain client is reported as not iconified");
        } else if (name != NULL && strcmp(name, "locked") == 0) {
            found_locked = 1;
        } else if (name != NULL && strcmp(name, "flagged") == 0) {
            found_flagged = 1;
            TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(entry, "iconified")),
                    "the flagged client is reported as iconified");
            TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(entry, "urgent")),
                    "the flagged client is reported as urgent");
            TAP_OK(cJSON_IsTrue(cJSON_GetObjectItem(entry, "pinned")),
                    "the flagged client is reported as pinned");
        }
    }
    TAP_OK(found_plain == 1, "the plain client is present");
    TAP_OK(found_locked == 0, "the locked client never appears");
    TAP_OK(found_flagged == 1, "the flagged client is present");

    cJSON_Delete(resp);
}


/* "get_focused": one entry per stage, with a real client_id when
 * the current desktop has an active client */
static void s_test_get_focused_with_active_client(void)
{
    cJSON *resp;
    cJSON *focused;
    cJSON *entry;

    s_reset();
    resp = ipc_action_get_focused(s_wm, NULL);
    focused = cJSON_GetObjectItem(resp, "focused");

    TAP_EQ_INT(cJSON_GetArraySize(focused), 1,
            "one entry for the one stage");
    entry = cJSON_GetArrayItem(focused, 0);
    TAP_EQ_INT((int) cJSON_GetNumberValue(
                cJSON_GetObjectItem(entry, "client_id")), 55,
            "the current desktop's own active client id is reported");

    cJSON_Delete(resp);
}


/* "get_focused": a current desktop with no active client reports
 * client_id as JSON null, not zero or an omitted field */
static void s_test_get_focused_with_no_active_client(void)
{
    cJSON *resp;
    cJSON *focused;
    cJSON *entry;
    cJSON *client_id_field;

    s_reset();
    s_stage->desktop_cur = 0u;
    s_desktop0->client_active_id = XCB_WINDOW_NONE;

    resp = ipc_action_get_focused(s_wm, NULL);
    focused = cJSON_GetObjectItem(resp, "focused");
    entry = cJSON_GetArrayItem(focused, 0);
    client_id_field = cJSON_GetObjectItem(entry, "client_id");

    TAP_OK(cJSON_IsNull(client_id_field),
            "no active client on the current desktop: client_id is"
            " JSON null");

    cJSON_Delete(resp);
}


int main(void)
{
    TAP_PLAN(20);

    s_test_get_version();
    s_test_list_desktops();
    s_test_list_clients();
    s_test_get_focused_with_active_client();
    s_test_get_focused_with_no_active_client();

    return TAP_DONE();
}
