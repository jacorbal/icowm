/**
 * @file tests/stage/test_workareas.c
 *
 * @brief Test battery for per-desktop work-area recomputation
 *        (stage/workareas.c)
 *
 * 'stage_workarea_refresh_all' walks every desktop on a stage and
 * calls 'desktop_update_workarea' once per desktop, then always
 * finishes with 'scratchpad_reposition'; this file links the real
 * source under test directly.  'stage_desktop_walk_all' is a link-only
 * stand-in walking a real 'cdlist_td' this file builds itself,
 * exactly the way the real one (stage/desktops.c) would, so the
 * static visitor 's_workarea_update_visit' still runs for real
 * through the function pointer it is handed, without pulling in the
 * rest of 'stage/desktops.c' and its own unrelated dependencies.
 * 'desktop_update_workarea' and 'scratchpad_reposition' are
 * call-counting, argument-recording stand-ins, letting each scenario
 * assert on exactly which desktops were visited, what config/strut/
 * flag combination each visit received, and that repositioning always
 * runs last.
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
#include <string.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Policy includes */
#include <policy/stacking.h>

/* Project includes */
#include <desktop.h>
#include <harness/tap.h>
#include <stage.h>
#include <stage/workarea.h>


/** Link-only stand-in for @a stage_desktop_walk_all (stage/
 *  desktops.c): walks a real 'cdlist_td' this file builds itself, so
 *  the real, static 's_workarea_update_visit' (stage/workareas.c)
 *  still runs through the function pointer handed to it
 *  @note Complexity: @e O(n), where @e n is the number of desktops on
 *        @p stage
 */
void stage_desktop_walk_all(const stage_td *stage,
        stage_desktop_visitor_fn visit, void *data)
{
    cdlist_item_td *node;

    if (stage == NULL || stage->desktops == NULL || visit == NULL) {
        return;
    }

    cdlist_foreach(stage->desktops, node) {
        desktop_td *const desktop = (desktop_td *) cdlist_data(node);

        if (desktop != NULL) {
            visit(desktop, data);
        }
    }
}


/** Desktops visited, and the stage/config/strut/flag combination
 *  each visit received, recorded in visitation order */
#define MAX_RECORDED_VISITS (8)
static desktop_td *s_visited_desktops[MAX_RECORDED_VISITS];
static const stage_td *s_visited_stages[MAX_RECORDED_VISITS];
static const struct config_desktop_s
    *s_visited_config_desktops[MAX_RECORDED_VISITS];
static const struct strut_partial_s
    *s_visited_systray_struts[MAX_RECORDED_VISITS];
static bool s_visited_ignore_struts[MAX_RECORDED_VISITS];
static int s_call_update_workarea;

/** Test-controlled stand-in for @a desktop_update_workarea
 *  (desktop.c)
 *  @note Complexity: @e O(1)
 */
void desktop_update_workarea(desktop_td *desktop,
        const stage_td *stage,
        const struct config_desktop_s *config_desktop,
        const struct strut_partial_s *systray_strut,
        bool ignore_struts)
{
    if (s_call_update_workarea < MAX_RECORDED_VISITS) {
        s_visited_desktops[s_call_update_workarea] = desktop;
        s_visited_stages[s_call_update_workarea] = stage;
        s_visited_config_desktops[s_call_update_workarea] = config_desktop;
        s_visited_systray_struts[s_call_update_workarea] = systray_strut;
        s_visited_ignore_struts[s_call_update_workarea] = ignore_struts;
    }
    s_call_update_workarea++;
}


/** Strut this file's own 'systray_get_reserved_strut' stand-in
 *  answers with, set by each scenario before calling the function
 *  under test */
static const struct strut_partial_s *s_stub_systray_strut;

/** Test-controlled stand-in for @a systray_get_reserved_strut
 *  (systray.c)
 *  @note Complexity: @e O(1)
 */
const struct strut_partial_s
    *systray_get_reserved_strut(const stage_td *stage)
{
    (void) stage;
    return s_stub_systray_strut;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_refill_maximized
 *
 * Reached only through 'stacking_walk' below, which this file's own
 * desktop objects never actually hold any clients for.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_refill_maximized(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a stacking_walk
 *
 * This file's own desktop objects never hold any clients to visit, so
 * @p visit is never actually called through it.
 *
 * @note Complexity: @e O(1)
 */
void stacking_walk(const desktop_td *desktop,
        stacking_visitor_fn visit, void *data)
{
    (void) desktop;
    (void) visit;
    (void) data;
}


/** Whether, and with which stage, 'scratchpad_reposition' was
 *  called */
static int s_call_reposition;
static const stage_td *s_last_reposition_stage;

/** Order 'scratchpad_reposition' was called in, relative to
 *  'desktop_update_workarea' calls: recorded as the value
 *  's_call_update_workarea' held at that moment, so a scenario can
 *  confirm repositioning always happens after every desktop visit */
static int s_reposition_seen_after_visits;

/** Call-recording stand-in for @a scratchpad_reposition
 *  (scratchpad.c)
 *  @note Complexity: @e O(1)
 */
void scratchpad_reposition(stage_td *stage)
{
    s_call_reposition++;
    s_last_reposition_stage = stage;
    s_reposition_seen_after_visits = s_call_update_workarea;
}


static void s_reset(void)
{
    memset(s_visited_desktops, 0, sizeof(s_visited_desktops));
    memset(s_visited_stages, 0, sizeof(s_visited_stages));
    memset(s_visited_config_desktops, 0,
            sizeof(s_visited_config_desktops));
    memset(s_visited_systray_struts, 0, sizeof(s_visited_systray_struts));
    memset(s_visited_ignore_struts, 0, sizeof(s_visited_ignore_struts));
    s_call_update_workarea = 0;
    s_stub_systray_strut = NULL;
    s_call_reposition = 0;
    s_last_reposition_stage = NULL;
    s_reposition_seen_after_visits = -1;
}


/* A null stage is refused outright: neither the walk nor the
 * reposition call ever runs */
static void s_test_null_stage_is_noop(void)
{
    s_reset();

    stage_workarea_refresh_all(NULL);
    TAP_EQ_INT(s_call_update_workarea, 0,
            "a null stage never visits any desktop");
    TAP_EQ_INT(s_call_reposition, 0,
            "a null stage never repositions the scratchpad either");
}


/* A stage with no desktops at all still repositions the scratchpad,
 * exactly once, having visited nothing */
static void s_test_empty_desktop_list_still_repositions(void)
{
    stage_td stage;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    stage.desktops = cdlist_init(NULL);

    stage_workarea_refresh_all(&stage);
    TAP_EQ_INT(s_call_update_workarea, 0,
            "an empty desktop list visits no desktop");
    TAP_EQ_INT(s_call_reposition, 1,
            "the scratchpad is still repositioned once, even with no"
            " desktops");

    cdlist_destroy(stage.desktops);
}


/* A single desktop is visited exactly once, with the stage's own
 * config->desktops and the systray's current strut, honoring
 * strutless_maximize as false */
static void s_test_single_desktop_visited_with_config(void)
{
    stage_td stage;
    desktop_td desktop;
    config_td config;
    struct strut_partial_s strut;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    memset(&strut, 0, sizeof(strut));

    stage.desktops = cdlist_init(NULL);
    (void) cdlist_ins_next(stage.desktops, NULL, &desktop);
    stage.config = &config;
    stage.strutless_maximize = false;
    s_stub_systray_strut = &strut;

    stage_workarea_refresh_all(&stage);
    TAP_EQ_INT(s_call_update_workarea, 1,
            "a single desktop is visited exactly once");
    TAP_OK(s_visited_desktops[0] == &desktop,
            "the desktop visited is the one on the stage's list");
    TAP_OK(s_visited_stages[0] == &stage,
            "the stage handed to desktop_update_workarea is the one"
            " being refreshed");
    TAP_OK(s_visited_config_desktops[0] == &config.desktops,
            "the stage's own config.desktops is forwarded");
    TAP_OK(s_visited_systray_struts[0] == &strut,
            "the systray's current reservation is forwarded");
    TAP_OK(!s_visited_ignore_struts[0],
            "ignore_struts reflects strutless_maximize being false");

    cdlist_destroy(stage.desktops);
}


/* A stage with no config forwards a null config_desktop rather than
 * dereferencing a null pointer */
static void s_test_null_config_forwards_null_config_desktop(void)
{
    stage_td stage;
    desktop_td desktop;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));

    stage.desktops = cdlist_init(NULL);
    (void) cdlist_ins_next(stage.desktops, NULL, &desktop);
    stage.config = NULL;

    stage_workarea_refresh_all(&stage);
    TAP_EQ_INT(s_call_update_workarea, 1,
            "the desktop is still visited even with no config at all");
    TAP_NULL(s_visited_config_desktops[0],
            "a null stage config forwards a null config_desktop"
            " rather than dereferencing it");

    cdlist_destroy(stage.desktops);
}


/* strutless_maximize being true is forwarded as ignore_struts true */
static void s_test_strutless_maximize_forwards_ignore_struts(void)
{
    stage_td stage;
    desktop_td desktop;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));

    stage.desktops = cdlist_init(NULL);
    (void) cdlist_ins_next(stage.desktops, NULL, &desktop);
    stage.strutless_maximize = true;

    stage_workarea_refresh_all(&stage);
    TAP_OK(s_visited_ignore_struts[0],
            "strutless_maximize true is forwarded as ignore_struts"
            " true");

    cdlist_destroy(stage.desktops);
}


/* A null systray reservation is forwarded as-is, rather than
 * substituted for anything */
static void s_test_null_systray_strut_forwarded_as_null(void)
{
    stage_td stage;
    desktop_td desktop;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));

    stage.desktops = cdlist_init(NULL);
    (void) cdlist_ins_next(stage.desktops, NULL, &desktop);
    s_stub_systray_strut = NULL;

    stage_workarea_refresh_all(&stage);
    TAP_NULL(s_visited_systray_struts[0],
            "no systray reservation is forwarded as a null strut,"
            " unchanged");

    cdlist_destroy(stage.desktops);
}


/* Multiple desktops are all visited, each in the stage's own list
 * order, and the scratchpad is repositioned exactly once, after every
 * one of them */
static void s_test_multiple_desktops_all_visited_then_repositioned(void)
{
    stage_td stage;
    desktop_td desktop_a;
    desktop_td desktop_b;
    desktop_td desktop_c;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop_a, 0, sizeof(desktop_a));
    memset(&desktop_b, 0, sizeof(desktop_b));
    memset(&desktop_c, 0, sizeof(desktop_c));

    /* Each 'cdlist_ins_next' call with a null 'item' inserts at the
     * head, so the three are inserted in reverse to land in a, b, c
     * order when walked from the head afterward */
    stage.desktops = cdlist_init(NULL);
    (void) cdlist_ins_next(stage.desktops, NULL, &desktop_c);
    (void) cdlist_ins_next(stage.desktops, NULL, &desktop_b);
    (void) cdlist_ins_next(stage.desktops, NULL, &desktop_a);

    stage_workarea_refresh_all(&stage);
    TAP_EQ_INT(s_call_update_workarea, 3,
            "every desktop on the stage's list is visited");
    TAP_OK(s_visited_desktops[0] == &desktop_a,
            "the first desktop visited is the list's own first");
    TAP_OK(s_visited_desktops[1] == &desktop_b,
            "the second desktop visited is the list's own second");
    TAP_OK(s_visited_desktops[2] == &desktop_c,
            "the third desktop visited is the list's own third");
    TAP_EQ_INT(s_call_reposition, 1,
            "the scratchpad is repositioned exactly once regardless of"
            " how many desktops were visited");
    TAP_EQ_INT(s_reposition_seen_after_visits, 3,
            "repositioning happens only after every desktop has"
            " already been visited");
    TAP_OK(s_last_reposition_stage == &stage,
            "the scratchpad is repositioned for the same stage being"
            " refreshed");

    cdlist_destroy(stage.desktops);
}


int main(void)
{
    TAP_PLAN(21);

    s_test_null_stage_is_noop();
    s_test_empty_desktop_list_still_repositions();
    s_test_single_desktop_visited_with_config();
    s_test_null_config_forwards_null_config_desktop();
    s_test_strutless_maximize_forwards_ignore_struts();
    s_test_null_systray_strut_forwarded_as_null();
    s_test_multiple_desktops_all_visited_then_repositioned();

    return TAP_DONE();
}
