/**
 * @file tests/cmds/client/test_workarea.c
 *
 * @brief Test battery for client-relative workarea resolution
 *        (cmds/client/workarea.c)
 *
 * 'ccmd_client_resolve_workarea' is exercised linked against the
 * real 'ccmd_client_monitor' (cmds/client/screen.c), so a client's
 * stage lookup by 'screen_id' and its center-point-to-monitor
 * resolution both run for real, exactly as they would in the window
 * manager itself.  'stage_desktop_get' is a test-controlled
 * stand-in answering from a small table this file fills directly,
 * so a test never has to build a well-formed cdlist just to satisfy
 * a lookup neither function under test is itself the one exercising.
 * 'stage_monitor_for_point' is a test-controlled stand-in too,
 * returning whichever single monitor rectangle a test registers, so
 * each case can pick apart the clip between a desktop's workarea and
 * a monitor's rectangle without any real RandR geometry math running
 * underneath it.  'wm_get_stages', 'xcb_connection_get',
 * 'xcb_get_setup', 'xcb_setup_roots_iterator', and 'xcb_screen_next'
 * are all link-only stand-ins, reached only by 'ccmd_screen_dim',
 * which nothing here calls.
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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Local includes */
#include <client.h>
#include <cmds/client/workarea.h>
#include <desktop.h>
#include <harness/tap.h>
#include <stage.h>


/**
 * @brief Test-controlled stand-in for @a wm_get_stages
 *
 * 'ccmd_client_monitor' walks whatever this answers to find a
 * client's own stage by 'screen_id'; every test here registers
 * its own one-entry list through @a s_make_stages_list before
 * calling the function under test
 *
 * @note Complexity: @e O(1)
 */
static list_td *s_stages_list;

list_td *wm_get_stages(void)
{
    return s_stages_list;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Reached only by @a ccmd_screen_dim, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_get_setup
 * @note Complexity: @e O(1)
 */
const xcb_setup_t *xcb_get_setup(xcb_connection_t *connection)
{
    (void) connection;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_setup_roots_iterator
 * @note Complexity: @e O(1)
 */
xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t *setup)
{
    xcb_screen_iterator_t it;

    (void) setup;
    memset(&it, 0, sizeof(it));
    return it;
}


/**
 * @brief Link-only stand-in for @a xcb_screen_next
 * @note Complexity: @e O(1)
 */
void xcb_screen_next(xcb_screen_iterator_t *iter)
{
    (void) iter;
}


/** The one monitor @a stage_monitor_for_point answers with,
 *  registered by @a s_set_monitor */
static monitor_td s_monitor;


/**
 * @brief Test-controlled stand-in for @a stage_monitor_for_point
 *
 * Answers whichever single monitor rectangle @a s_set_monitor last
 * registered, regardless of @p pos: what each test here picks apart
 * is the clip between a desktop's workarea and a monitor rectangle,
 * not the point-containment search a real multi-monitor stage
 * would need.
 *
 * @note Complexity: @e O(1)
 */
monitor_td stage_monitor_for_point(const stage_td *stage,
        struct position_s pos)
{
    (void) stage;
    (void) pos;

    return s_monitor;
}


static void s_set_monitor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    s_monitor.x = x;
    s_monitor.y = y;
    s_monitor.w = w;
    s_monitor.h = h;
}


/** Desktops this file's own 'stage_desktop_get' stand-in answers
 *  from, registered by @a s_make_desktop */
#define MAX_TEST_DESKTOPS (4)
static desktop_td *s_desktops_by_id[MAX_TEST_DESKTOPS];
static int s_desktops_registered;


/**
 * @brief Test-controlled stand-in for @a stage_desktop_get
 * @note Complexity: @e O(n), where @e n is the number of desktops
 *       registered
 */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;

    for (int i = 0; i < s_desktops_registered; ++i) {
        if (s_desktops_by_id[i] != NULL &&
                s_desktops_by_id[i]->id == desktop_id) {
            return s_desktops_by_id[i];
        }
    }

    return NULL;
}


/** Every client, desktop, and stages list this file allocates,
 *  freed in one place by @a s_teardown rather than at each test's
 *  own end */
#define MAX_TEST_CLIENTS (8)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;
static desktop_td *s_owned_desktops[MAX_TEST_DESKTOPS];
static int s_owned_desktops_used;


static desktop_td *s_make_desktop(uint32_t id, int32_t x, int32_t y,
        uint32_t w, uint32_t h)
{
    desktop_td *desktop = calloc(1, sizeof(*desktop));

    desktop->id = id;
    desktop->workarea.pos.x = x;
    desktop->workarea.pos.y = y;
    desktop->workarea.dim.w = w;
    desktop->workarea.dim.h = h;
    s_owned_desktops[s_owned_desktops_used] = desktop;
    s_owned_desktops_used++;
    s_desktops_by_id[s_desktops_registered] = desktop;
    s_desktops_registered++;

    return desktop;
}


static client_td *s_make_client(uint32_t screen_id, uint32_t desktop_id,
        int32_t cx, int32_t cy, uint32_t cw, uint32_t ch)
{
    client_td *client = calloc(1, sizeof(*client));

    client->screen_id = screen_id;
    client->desktop_id = desktop_id;
    client->layout.geometry.cur.pos.x = cx;
    client->layout.geometry.cur.pos.y = cy;
    client->layout.geometry.cur.dim.w = cw;
    client->layout.geometry.cur.dim.h = ch;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


/* Build a one-entry stages list holding 'stage', matched by
 * 'ccmd_client_monitor' on its 'id' field against a client's own
 * 'screen_id', and register it as what 'wm_get_stages' answers */
static void s_make_stages_list(stage_td *stage)
{
    s_stages_list = list_init(NULL);
    (void) list_ins_next(s_stages_list, NULL, stage);
}


static void s_reset(void)
{
    s_desktops_registered = 0;
    memset(s_desktops_by_id, 0, sizeof(s_desktops_by_id));
    memset(&s_monitor, 0, sizeof(s_monitor));
}


static void s_teardown(void)
{
    if (s_stages_list != NULL) {
        list_destroy(s_stages_list);
        s_stages_list = NULL;
    }

    for (int i = 0; i < s_owned_desktops_used; ++i) {
        free(s_owned_desktops[i]);
    }
    s_owned_desktops_used = 0;

    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* A null client, or missing output pointers, is refused outright */
static void s_test_null_arguments_refused(void)
{
    client_td client;
    int32_t x = 0;
    int32_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    s_reset();
    memset(&client, 0, sizeof(client));

    TAP_OK(!ccmd_client_resolve_workarea(NULL, &x, &y, &w, &h),
            "a null client is refused outright");
    TAP_OK(!ccmd_client_resolve_workarea(&client, &x, &y, NULL, &h),
            "a null width output is refused outright");
    TAP_OK(!ccmd_client_resolve_workarea(&client, &x, &y, &w, NULL),
            "a null height output is refused outright");

    s_teardown();
}


/* A client whose 'screen_id' matches no stage in the list fails to
 * resolve, since 'ccmd_client_monitor' cannot place it anywhere */
static void s_test_unknown_screen_fails(void)
{
    stage_td stage;
    client_td *client;
    int32_t x = 0;
    int32_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    bool ok;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    s_make_stages_list(&stage);

    client = s_make_client(99u, 0u, 0, 0, 100u, 100u);

    ok = ccmd_client_resolve_workarea(client, &x, &y, &w, &h);
    TAP_OK(!ok, "a client on an unregistered screen id fails to"
            " resolve a workarea");

    s_teardown();
}


/* A desktop with no workarea known yet (width or height still zero)
 * fails to resolve, rather than handing back a bogus empty rectangle */
static void s_test_zero_workarea_fails(void)
{
    stage_td stage;
    client_td *client;
    int32_t x = 0;
    int32_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_cur = 0u;
    s_make_stages_list(&stage);
    (void) s_make_desktop(0u, 0, 0, 0u, 0u);
    s_set_monitor(0, 0, 1920u, 1080u);

    client = s_make_client(0u, 0u, 500, 500, 100u, 100u);

    TAP_OK(!ccmd_client_resolve_workarea(client, &x, &y, &w, &h),
            "a desktop whose workarea is still all zeroes fails to"
            " resolve");

    s_teardown();
}


/* A desktop id that resolves to no desktop at all (an unregistered
 * one) fails outright */
static void s_test_unknown_desktop_fails(void)
{
    stage_td stage;
    client_td *client;
    int32_t x = 0;
    int32_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_cur = 0u;
    s_make_stages_list(&stage);
    s_set_monitor(0, 0, 1920u, 1080u);

    /* No desktop registered at all */
    client = s_make_client(0u, 7u, 500, 500, 100u, 100u);

    TAP_OK(!ccmd_client_resolve_workarea(client, &x, &y, &w, &h),
            "a client whose desktop id resolves to nothing fails to"
            " resolve");

    s_teardown();
}


/* A desktop workarea fully inside its monitor is returned unclipped */
static void s_test_workarea_inside_monitor_unclipped(void)
{
    stage_td stage;
    client_td *client;
    int32_t x = -1;
    int32_t y = -1;
    uint16_t w = 0;
    uint16_t h = 0;
    bool ok;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_cur = 0u;
    s_make_stages_list(&stage);
    (void) s_make_desktop(0u, 10, 20, 800u, 600u);
    s_set_monitor(0, 0, 1920u, 1080u);

    client = s_make_client(0u, 0u, 100, 100, 100u, 100u);

    ok = ccmd_client_resolve_workarea(client, &x, &y, &w, &h);
    TAP_OK(ok, "a workarea fully inside its monitor resolves");
    TAP_EQ_INT(x, 10, "left edge is the workarea's own, unclipped");
    TAP_EQ_INT(y, 20, "top edge is the workarea's own, unclipped");
    TAP_EQ_INT(w, 800, "width is the workarea's own, unclipped");
    TAP_EQ_INT(h, 600, "height is the workarea's own, unclipped");

    s_teardown();
}


/* A desktop workarea that spills past its monitor's own edge is
 * clipped down to the overlap, not handed back in full */
static void s_test_workarea_clipped_to_monitor(void)
{
    stage_td stage;
    client_td *client;
    int32_t x = 0;
    int32_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    bool ok;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_cur = 0u;
    s_make_stages_list(&stage);
    /* Workarea spans two monitors' worth of width; the monitor below
     * only covers the left half of it */
    (void) s_make_desktop(0u, 0, 0, 2000u, 1000u);
    s_set_monitor(0, 0, 1000u, 900u);

    client = s_make_client(0u, 0u, 100, 100, 100u, 100u);

    ok = ccmd_client_resolve_workarea(client, &x, &y, &w, &h);
    TAP_OK(ok, "an overlapping but larger workarea still resolves");
    TAP_EQ_INT(w, 1000, "width is clipped down to the monitor's own");
    TAP_EQ_INT(h, 900, "height is clipped down to the monitor's own");

    s_teardown();
}


/* A workarea with no overlap against the monitor at all clips down to
 * an empty rectangle and is refused */
static void s_test_no_overlap_fails(void)
{
    stage_td stage;
    client_td *client;
    int32_t x = 0;
    int32_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_cur = 0u;
    s_make_stages_list(&stage);
    (void) s_make_desktop(0u, 0, 0, 100u, 100u);
    /* Monitor sits entirely to the right, no overlap at all */
    s_set_monitor(500, 500, 400u, 400u);

    client = s_make_client(0u, 0u, 10, 10, 10u, 10u);

    TAP_OK(!ccmd_client_resolve_workarea(client, &x, &y, &w, &h),
            "a workarea with no overlap against the monitor fails to"
            " resolve");

    s_teardown();
}


/* A client pinned to every desktop (WM_DESKTOP_ID_ALL) resolves
 * against its stage's currently shown desktop instead of its own
 * (nonexistent) one */
static void s_test_pinned_client_uses_current_desktop(void)
{
    stage_td stage;
    client_td *client;
    int32_t x = 0;
    int32_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    bool ok;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_cur = 2u;
    s_make_stages_list(&stage);
    (void) s_make_desktop(0u, 0, 0, 100u, 100u);
    (void) s_make_desktop(2u, 40, 50, 300u, 200u);
    s_set_monitor(0, 0, 1920u, 1080u);

    client = s_make_client(0u, WM_DESKTOP_ID_ALL, 10, 10, 10u, 10u);

    ok = ccmd_client_resolve_workarea(client, &x, &y, &w, &h);
    TAP_OK(ok, "a pinned client resolves against its stage's"
            " current desktop");
    TAP_EQ_INT(x, 40, "using desktop 2's workarea, not desktop 0's");
    TAP_EQ_INT(w, 300, "width comes from desktop 2, not desktop 0");

    s_teardown();
}


/* Output pointers for x/y may each be individually NULL; only w/h
 * are mandatory */
static void s_test_optional_position_outputs(void)
{
    stage_td stage;
    client_td *client;
    uint16_t w = 0;
    uint16_t h = 0;
    bool ok;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.id = 0u;
    stage.desktop_cur = 0u;
    s_make_stages_list(&stage);
    (void) s_make_desktop(0u, 5, 6, 640u, 480u);
    s_set_monitor(0, 0, 1920u, 1080u);

    client = s_make_client(0u, 0u, 10, 10, 10u, 10u);

    ok = ccmd_client_resolve_workarea(client, NULL, NULL, &w, &h);
    TAP_OK(ok, "position outputs may both be omitted");
    TAP_EQ_INT(w, 640, "width is still filled in without them");
    TAP_EQ_INT(h, 480, "height is still filled in without them");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(21);

    s_test_null_arguments_refused();
    s_test_unknown_screen_fails();
    s_test_zero_workarea_fails();
    s_test_unknown_desktop_fails();
    s_test_workarea_inside_monitor_unclipped();
    s_test_workarea_clipped_to_monitor();
    s_test_no_overlap_fails();
    s_test_pinned_client_uses_current_desktop();
    s_test_optional_position_outputs();

    return TAP_DONE();
}
