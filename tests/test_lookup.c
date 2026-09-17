/**
 * @file tests/test_lookup.c
 *
 * @brief Test battery for window, client, stage, and desktop lookup
 *
 * lookup.c's own functions never touch the X server themselves; they
 * only search structures already built elsewhere.  The real
 * stage_init/desktop_init/client_init all require a live XCB
 * connection to build one (they read screen setup data from it), so
 * this file builds minimal stage_td/desktop_td/client_td fixtures
 * by hand instead, populating only the fields lookup.c itself
 * actually reads.  The hash/match functions used for each desktop's
 * own 'clients' table are this file's own simple stand-ins (hash by
 * raw id, linear probing), not the project's real ones (those are
 * static to desktop.c) -- lookup.c's own correctness does not depend
 * on which hash scheme populated the table, only on ohtbl's own
 * documented behavior, which test_ohtbl.c already covers on its own.
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

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <client.h>
#include <desktop.h>
#include <harness/tap.h>
#include <lookup.h>
#include <stage.h>


/** Link-only stand-in for stage_desktop_get (stage.c): lookup.c
 *  as a whole references it (from lookup_current_desktop, which this
 *  file deliberately does not test -- see the note further down),
 *  so the linker needs a definition for it somewhere even though
 *  nothing here ever calls it.  Pulling in the real stage.c for
 *  this alone would drag in its own large, uncertain dependency
 *  chain (desktop.c, systray.c, and whatever those pull in further)
 *  just to satisfy the linker for a function this file never
 *  exercises. */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
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


/** Frees a desktop_td and every client_td still stored in its own
 *  clients table; installed as the destroy callback on every test's
 *  own stage->desktops circular list, so a single cdlist_destroy
 *  is enough to tear an entire fixture down */
static void s_destroy_desktop(void *data)
{
    desktop_td *desktop = (desktop_td *) data;
    void *elem;

    ohtbl_foreach(desktop->clients, elem) {
        free(elem);
    }
    ohtbl_destroy(desktop->clients);
    free(desktop);
}


/**
 * @brief Build a desktop_td with the given id and an empty, ready-
 *        to-populate clients table
 */
static desktop_td *s_make_desktop(uint32_t id)
{
    desktop_td *desktop = calloc(1, sizeof(desktop_td));

    desktop->id = id;
    desktop->clients = ohtbl_init(8, 8, s_id_hash1, s_id_hash2,
            s_id_match, NULL);
    return desktop;
}


/**
 * @brief Build a stage_td with the given root window and
 *        'desktop_count' desktops (ids 0..desktop_count-1), current
 *        desktop set to 'desktop_cur'
 */
static stage_td *s_make_stage(xcb_window_t root,
        uint32_t desktop_count, uint32_t desktop_cur)
{
    stage_td *stage = calloc(1, sizeof(stage_td));
    xcb_screen_t *screen = calloc(1, sizeof(xcb_screen_t));

    screen->root = root;
    stage->screen = screen;
    stage->desktop_count = desktop_count;
    stage->desktop_cur = desktop_cur;
    stage->desktops = cdlist_init(s_destroy_desktop);

    for (uint32_t i = 0; i < desktop_count; ++i) {
        cdlist_ins_next(stage->desktops, cdlist_tail(stage->desktops),
                s_make_desktop(i));
    }

    return stage;
}


static client_td *s_make_client(xcb_window_t id, xcb_window_t window,
        xcb_window_t frame, xcb_window_t titlebar,
        xcb_window_t icon_window)
{
    client_td *client = calloc(1, sizeof(client_td));

    client->id = id;
    client->window = window;
    client->frame = frame;
    client->titlebar = titlebar;
    client->icon_window = icon_window;
    return client;
}


/** Tears down everything s_make_stage allocated: every desktop and
 *  every client still in it, the fabricated screen, and the stage
 *  itself */
static void s_destroy_stage(stage_td *stage)
{
    cdlist_destroy(stage->desktops);
    free(stage->screen);
    free(stage);
}


/* lookup_stage_for_root finds the one stage among several whose
 * own screen's root window matches */
static void s_test_stage_for_root_finds_match(void)
{
    list_td *stages = list_init(NULL);
    stage_td *a = s_make_stage(100, 1, 0);
    stage_td *b = s_make_stage(200, 1, 0);
    stage_td *found;

    list_ins_next(stages, NULL, a);
    list_ins_next(stages, NULL, b);

    found = lookup_stage_for_root(stages, 200);
    TAP_OK(found == b, "finds the stage whose own root matches");

    found = lookup_stage_for_root(stages, 999);
    TAP_NULL(found, "no match for a root nobody has");

    found = lookup_stage_for_root(NULL, 100);
    TAP_NULL(found, "a NULL stage list returns NULL, no crash");

    list_destroy(stages);
    s_destroy_stage(a);
    s_destroy_stage(b);
}


/* lookup_current_desktop is deliberately not covered by this file:
 * it is a thin wrapper around stage_desktop_get (stage.c), and
 * that module's own build-time dependency chain (desktop.c,
 * systray.c, and whatever each of those pull in beyond that) is
 * large enough that linking it in here just for this one function
 * is not worth the added coupling; lookup_stage_for_root and
 * lookup_find_client below cover everything else this file exposes,
 * with no such dependency. */


/* lookup_client_matches_window checks every one of a client's own
 * five possible window IDs, and rejects both a NULL client and the
 * sentinel XCB_WINDOW_NONE.  Not covered here: it is static to
 * lookup.c (s_lookup_client_matches_window), unreachable from this
 * file; lookup_find_client below already exercises it indirectly,
 * through every one of its own test cases. */


/* lookup_find_client's fast path: a window ID equal to a client's
 * own id is found through the O(1) hash lookup */
static void s_test_find_client_fast_path_by_id(void)
{
    list_td *stages = list_init(NULL);
    stage_td *stage = s_make_stage(1, 1, 0);
    desktop_td *desktop = cdlist_data(cdlist_head(stage->desktops));
    client_td *client = s_make_client(42, 42, 0, 0, 0);
    stage_td *out_stage = NULL;
    desktop_td *out_desktop = NULL;
    client_td *found;

    ohtbl_insert(desktop->clients, client);
    list_ins_next(stages, NULL, stage);

    found = lookup_find_client(stages, 42, &out_stage, &out_desktop);

    TAP_OK(found == client, "the fast path finds the client by its own id");
    TAP_OK(out_stage == stage, "out_stage is correctly set");
    TAP_OK(out_desktop == desktop, "out_desktop is correctly set");

    list_destroy(stages);
    s_destroy_stage(stage);
}


/* lookup_find_client's slow path: a window ID matching the client's
 * frame (not its id) is found through the linear scan */
static void s_test_find_client_slow_path_by_frame(void)
{
    list_td *stages = list_init(NULL);
    stage_td *stage = s_make_stage(1, 1, 0);
    desktop_td *desktop = cdlist_data(cdlist_head(stage->desktops));
    client_td *client = s_make_client(42, 42, 77, 0, 0);
    client_td *found;

    ohtbl_insert(desktop->clients, client);
    list_ins_next(stages, NULL, stage);

    /* 77 is the frame, not the id: the fast hash lookup (keyed on
     * id) will miss, forcing the fallback linear scan */
    found = lookup_find_client(stages, 77, NULL, NULL);

    TAP_OK(found == client,
            "the slow path finds the client by its own frame window");

    list_destroy(stages);
    s_destroy_stage(stage);
}


/* lookup_find_client returns NULL (and clears both out-parameters)
 * when nothing anywhere matches */
static void s_test_find_client_not_found(void)
{
    list_td *stages = list_init(NULL);
    stage_td *stage = s_make_stage(1, 1, 0);
    stage_td *out_stage = (stage_td *) 0x1;
    desktop_td *out_desktop = (desktop_td *) 0x1;
    client_td *found;

    list_ins_next(stages, NULL, stage);

    found = lookup_find_client(stages, 999, &out_stage, &out_desktop);

    TAP_NULL(found, "no client anywhere matches an unknown window id");
    TAP_NULL(out_stage, "out_stage is cleared back to NULL");
    TAP_NULL(out_desktop, "out_desktop is cleared back to NULL");

    TAP_NULL(lookup_find_client(NULL, 42, NULL, NULL),
            "a NULL stage list returns NULL, no crash");
    TAP_NULL(lookup_find_client(stages, XCB_WINDOW_NONE, NULL, NULL),
            "XCB_WINDOW_NONE as the search window returns NULL");

    list_destroy(stages);
    s_destroy_stage(stage);
}


/* lookup_find_client searches every desktop on a stage, not just
 * the first one, correctly stopping the circular walk after
 * desktop_count iterations */
static void s_test_find_client_searches_every_desktop(void)
{
    list_td *stages = list_init(NULL);
    stage_td *stage = s_make_stage(1, 3, 0);
    cdlist_item_td *item = cdlist_head(stage->desktops);
    desktop_td *second_desktop;
    client_td *client = s_make_client(55, 55, 0, 0, 0);
    client_td *found;

    /* Advance to the second of the three desktops and put the
     * client only there */
    item = cdlist_next(item);
    second_desktop = cdlist_data(item);
    ohtbl_insert(second_desktop->clients, client);

    list_ins_next(stages, NULL, stage);
    found = lookup_find_client(stages, 55, NULL, NULL);

    TAP_OK(found == client,
            "a client on a desktop other than the first is still found");

    list_destroy(stages);
    s_destroy_stage(stage);
}


int main(void)
{
    TAP_PLAN(13);

    s_test_stage_for_root_finds_match();
    s_test_find_client_fast_path_by_id();
    s_test_find_client_slow_path_by_frame();
    s_test_find_client_not_found();
    s_test_find_client_searches_every_desktop();

    return TAP_DONE();
}
