/**
 * @file tests/wm/test_clients.c
 *
 * @brief Test battery for whole-window-manager client traversal
 *
 * surface_desktop_get (surface.c) is stubbed below as a controllable
 * stand-in, indexed by desktop_id, the same way test_lookup.c and
 * test_resolve.c already do: this file's own tests populate it
 * before each call, so both the "found" and "gap" (NULL desktop)
 * cases can be driven directly without surface.c's own much larger,
 * XCB-dependent real implementation.
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
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Local includes */
#include <harness/tap.h>
#include <wm/internal.h>


/** This file owns the one real wm_td instance, passed explicitly to
 *  every wm_for_each_client call below */
wm_td *wm = NULL;


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


/* A NULL singleton, or one with a NULL surface list, visits nothing */
static void s_test_null_wm_or_surfaces(void)
{
    wm_td local_wm;

    wm = NULL;
    TAP_EQ_INT((long) wm_for_each_client(wm, NULL, NULL), 0,
            "a NULL wm singleton visits nothing");

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.surfaces = NULL;
    wm = &local_wm;
    TAP_EQ_INT((long) wm_for_each_client(wm, NULL, NULL), 0,
            "a NULL surfaces list visits nothing");

    wm = NULL;
}


/* An empty surface list visits nothing */
static void s_test_empty_surface_list(void)
{
    wm_td local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.surfaces = list_init(NULL);
    wm = &local_wm;

    TAP_EQ_INT((long) wm_for_each_client(wm, NULL, NULL), 0,
            "an empty surface list visits nothing");

    list_destroy(local_wm.surfaces);
    wm = NULL;
}


/* Every client across a single surface's several desktops is
 * visited, with the action called on each and passed the given
 * userdata through unchanged */
static bool s_visited[3];
static void *s_seen_userdata;

static void s_recording_action(client_td *client, void *userdata)
{
    uint32_t id = client->id;

    if (id < 3u) {
        s_visited[id] = true;
    }
    s_seen_userdata = userdata;
}

static void s_test_visits_every_client_across_desktops(void)
{
    wm_td local_wm;
    surface_td surface;
    desktop_td desktop_a;
    desktop_td desktop_b;
    client_td client0;
    client_td client1;
    client_td client2;
    int marker = 42;
    uint32_t count;

    memset(&local_wm, 0, sizeof(local_wm));
    memset(&surface, 0, sizeof(surface));
    memset(&desktop_a, 0, sizeof(desktop_a));
    memset(&desktop_b, 0, sizeof(desktop_b));
    memset(&client0, 0, sizeof(client0));
    memset(&client1, 0, sizeof(client1));
    memset(&client2, 0, sizeof(client2));
    client0.id = 0;
    client1.id = 1;
    client2.id = 2;

    desktop_a.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    desktop_b.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(desktop_a.clients, &client0);
    ohtbl_insert(desktop_a.clients, &client1);
    ohtbl_insert(desktop_b.clients, &client2);

    surface.desktop_count = 2u;
    s_desktops_by_id[0] = &desktop_a;
    s_desktops_by_id[1] = &desktop_b;

    local_wm.surfaces = list_init(NULL);
    list_ins_next(local_wm.surfaces, NULL, &surface);
    wm = &local_wm;

    s_visited[0] = s_visited[1] = s_visited[2] = false;
    s_seen_userdata = NULL;
    count = wm_for_each_client(wm, s_recording_action, &marker);

    TAP_EQ_INT((long) count, 3, "all three clients across both" \
            " desktops are counted");
    TAP_OK(s_visited[0] && s_visited[1] && s_visited[2],
            "the action ran for every one of them");
    TAP_OK(s_seen_userdata == &marker,
            "userdata is passed through to the action unchanged");

    s_desktops_by_id[0] = NULL;
    s_desktops_by_id[1] = NULL;
    ohtbl_destroy(desktop_a.clients);
    ohtbl_destroy(desktop_b.clients);
    list_destroy(local_wm.surfaces);
    wm = NULL;
}


/* A NULL action still counts every client, without calling anything */
static void s_test_null_action_only_counts(void)
{
    wm_td local_wm;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    uint32_t count;

    memset(&local_wm, 0, sizeof(local_wm));
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    client.id = 0;

    desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(desktop.clients, &client);
    surface.desktop_count = 1u;
    s_desktops_by_id[0] = &desktop;

    local_wm.surfaces = list_init(NULL);
    list_ins_next(local_wm.surfaces, NULL, &surface);
    wm = &local_wm;

    count = wm_for_each_client(wm, NULL, NULL);
    TAP_EQ_INT((long) count, 1,
            "a NULL action still counts every client, no crash");

    s_desktops_by_id[0] = NULL;
    ohtbl_destroy(desktop.clients);
    list_destroy(local_wm.surfaces);
    wm = NULL;
}


/* A gap where surface_desktop_get returns NULL for one desktop_id is
 * skipped cleanly, without stopping the walk over the rest */
static void s_test_skips_null_desktop_gap(void)
{
    wm_td local_wm;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    uint32_t count;

    memset(&local_wm, 0, sizeof(local_wm));
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    client.id = 0;

    desktop.clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    ohtbl_insert(desktop.clients, &client);
    /* desktop_count is 2, but only slot 1 is populated: slot 0 is a
     * gap surface_desktop_get reports as NULL */
    surface.desktop_count = 2u;
    s_desktops_by_id[0] = NULL;
    s_desktops_by_id[1] = &desktop;

    local_wm.surfaces = list_init(NULL);
    list_ins_next(local_wm.surfaces, NULL, &surface);
    wm = &local_wm;

    count = wm_for_each_client(wm, NULL, NULL);
    TAP_EQ_INT((long) count, 1,
            "a NULL-desktop gap is skipped, the rest still counted");

    s_desktops_by_id[1] = NULL;
    ohtbl_destroy(desktop.clients);
    list_destroy(local_wm.surfaces);
    wm = NULL;
}


int main(void)
{
    TAP_PLAN(8);

    s_test_null_wm_or_surfaces();
    s_test_empty_surface_list();
    s_test_visits_every_client_across_desktops();
    s_test_null_action_only_counts();
    s_test_skips_null_desktop_gap();

    return TAP_DONE();
}
