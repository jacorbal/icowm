/**
 * @file tests/input/mouse/drag/test_resist.c
 *
 * @brief Test battery for maximized-axis mouse resize resistance math
 *
 * drag_resist_axis_update and drag_resist_axis_finalize (input/mouse/
 * drag/resist.c) read and write only the global drag state, s_drag
 * (input/mouse/drag/internal.h), and call two client-state commands
 * (ccmd_client_demote_axis_state and ccmd_client_promote_axis_state,
 * cmds/client/maximize.c) whenever a resistance threshold is crossed
 * under a solid drag.  Storage for s_drag lives in drag.c, which this
 * file never links, so it is defined once here instead, the same way
 * drag.c itself would define it; both command entry points are
 * recording stand-ins, since what is under test here is the pure
 * resistance arithmetic and which transitions call which command, not
 * the EWMH/geometry side effects those two commands themselves would
 * otherwise trigger.
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
#include <stdint.h>
#include <string.h>

/* Project includes */
#include <client.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/resist.h>


/** Singleton drag state; storage normally lives in drag.c, which this
 *  file never links, so it is defined here instead */
drag_state_td s_drag;


/** Recording log of every ccmd_client_demote_axis_state /
 *  ccmd_client_promote_axis_state call this file's stand-ins observe */
#define LOG_CAP (16)
static int s_demote_log_dir[LOG_CAP];
static int s_demote_log_used;
static int s_promote_log_dir[LOG_CAP];
static int s_promote_log_used;


/**
 * @brief Recording stand-in for @a ccmd_client_demote_axis_state
 * @note Complexity: @e O(1)
 */
void ccmd_client_demote_axis_state(client_td *client, int dir)
{
    (void) client;

    if (s_demote_log_used < LOG_CAP) {
        s_demote_log_dir[s_demote_log_used] = dir;
        s_demote_log_used++;
    }
}


/**
 * @brief Recording stand-in for @a ccmd_client_promote_axis_state
 * @note Complexity: @e O(1)
 */
void ccmd_client_promote_axis_state(client_td *client, int dir)
{
    (void) client;

    if (s_promote_log_used < LOG_CAP) {
        s_promote_log_dir[s_promote_log_used] = dir;
        s_promote_log_used++;
    }
}


static void s_reset(void)
{
    static client_td dummy_client;

    memset(&s_drag, 0, sizeof(s_drag));
    memset(&dummy_client, 0, sizeof(dummy_client));
    s_drag.client = &dummy_client;
    s_demote_log_used = 0;
    s_promote_log_used = 0;
}


/* Neither axis flagged resistant: a call changes nothing, regardless
 * of distance or resistance */
static void s_test_no_axis_resistant_is_a_no_op(void)
{
    s_reset();

    drag_resist_axis_update(999u, 999u, 10u);

    TAP_OK(!s_drag.is_resize_w && !s_drag.is_resize_h,
            "neither axis is resistant: is_resize_w/h stay false");
    TAP_EQ_INT(s_demote_log_used, 0, "and no demote call happens");
    TAP_EQ_INT(s_promote_log_used, 0, "nor a promote one");
}


/* Width resistant, distance short of the threshold: is_resize_w stays
 * false */
static void s_test_below_threshold_stays_frozen(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;

    drag_resist_axis_update(9u, 0u, 10u);

    TAP_OK(!s_drag.is_resize_w,
            "distance 9 under resistance 10: width stays frozen");
}


/* Width resistant, distance exactly at the threshold: the comparison
 * is >=, so it already counts as past it */
static void s_test_exact_threshold_counts_as_past_it(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;

    drag_resist_axis_update(10u, 0u, 10u);

    TAP_OK(s_drag.is_resize_w,
            "distance exactly equal to resistance: already resizing"
            " (>=, not >)");
}


/* Width resistant, distance one past the threshold: past it */
static void s_test_above_threshold_starts_resize(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;

    drag_resist_axis_update(11u, 0u, 10u);

    TAP_OK(s_drag.is_resize_w,
            "distance 11 over resistance 10: resizing");
}


/* Both axes resistant, both distances past threshold together: both
 * flip independently in the same call */
static void s_test_both_axes_independent(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_resist_axis_h = true;

    drag_resist_axis_update(50u, 3u, 10u);

    TAP_OK(s_drag.is_resize_w, "width crosses its own threshold");
    TAP_OK(!s_drag.is_resize_h, "height does not cross its own");
}


/* A resistance of zero always counts as past it, since drag distance
 * is unsigned and can never be negative */
static void s_test_zero_resistance_always_past(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;

    drag_resist_axis_update(0u, 0u, 0u);

    TAP_OK(s_drag.is_resize_w,
            "resistance 0: even a 0 drag distance is >= 0");
}


/* Not a solid drag: no command call happens even on a genuine
 * transition, since the real window is off-screen for the whole drag
 * (see the header's doc comment) */
static void s_test_not_solid_drag_never_calls_commands(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_solid_drag = false;

    drag_resist_axis_update(50u, 0u, 10u);

    TAP_OK(s_drag.is_resize_w, "still crosses the threshold itself");
    TAP_EQ_INT(s_demote_log_used, 0,
            "but under !solid_drag no demote call is made live");
}


/* Solid drag, width transitions from frozen to resizing: demotes axis
 * 1 */
static void s_test_solid_drag_demotes_on_width_transition(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_solid_drag = true;
    s_drag.is_resize_w = false;

    drag_resist_axis_update(50u, 0u, 10u);

    TAP_EQ_INT(s_demote_log_used, 1,
            "solid drag, width freezes-to-resizing transition: one"
            " demote call");
    TAP_EQ_INT(s_demote_log_dir[0], 1, "on axis 1 (width)");
}


/* Solid drag, height transitions from frozen to resizing: demotes
 * axis 2 */
static void s_test_solid_drag_demotes_on_height_transition(void)
{
    s_reset();
    s_drag.is_resist_axis_h = true;
    s_drag.is_solid_drag = true;
    s_drag.is_resize_h = false;

    drag_resist_axis_update(0u, 50u, 10u);

    TAP_EQ_INT(s_demote_log_used, 1,
            "solid drag, height freezes-to-resizing transition: one"
            " demote call");
    TAP_EQ_INT(s_demote_log_dir[0], 2, "on axis 2 (height)");
}


/* Solid drag, width transitions back from resizing to frozen:
 * promotes axis 1 */
static void s_test_solid_drag_promotes_on_reverse_transition(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_solid_drag = true;
    s_drag.is_resize_w = true;

    drag_resist_axis_update(0u, 0u, 10u);

    TAP_OK(!s_drag.is_resize_w, "distance under threshold: re-frozen");
    TAP_EQ_INT(s_promote_log_used, 1,
            "solid drag, resizing-to-frozen transition: one promote"
            " call");
    TAP_EQ_INT(s_promote_log_dir[0], 1, "on axis 1 (width)");
}


/* Solid drag, no transition (state stays the same across the call):
 * no command call at all */
static void s_test_solid_drag_no_transition_no_call(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_solid_drag = true;
    s_drag.is_resize_w = true;

    drag_resist_axis_update(50u, 0u, 10u);

    TAP_OK(s_drag.is_resize_w, "still past threshold: still resizing");
    TAP_EQ_INT(s_demote_log_used, 0,
            "already resizing before the call: no repeated demote");
    TAP_EQ_INT(s_promote_log_used, 0, "and no promote either");
}


/* Repeated calls crossing back and forth reflect every single
 * transition, live, reversibly */
static void s_test_repeated_crossings_are_reversible(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_solid_drag = true;

    drag_resist_axis_update(50u, 0u, 10u);
    drag_resist_axis_update(0u, 0u, 10u);
    drag_resist_axis_update(50u, 0u, 10u);

    TAP_OK(s_drag.is_resize_w, "ends past the threshold: resizing");
    TAP_EQ_INT(s_demote_log_used, 2,
            "two freeze-to-resize transitions across the three calls");
    TAP_EQ_INT(s_promote_log_used, 1,
            "one resize-to-freeze transition in between");
}


/* finalize is a no-op when finalize_resize is false, regardless of
 * any other state */
static void s_test_finalize_no_op_when_not_finalizing(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_resize_w = true;
    s_drag.is_solid_drag = false;

    drag_resist_axis_finalize(false);

    TAP_EQ_INT(s_demote_log_used, 0,
            "finalize_resize false: never calls demote, even with a"
            " resistant axis mid-resize");
}


/* finalize is a no-op under solid_drag, since every transition was
 * already settled live by drag_resist_axis_update */
static void s_test_finalize_no_op_under_solid_drag(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_resize_w = true;
    s_drag.is_solid_drag = true;

    drag_resist_axis_finalize(true);

    TAP_EQ_INT(s_demote_log_used, 0,
            "solid_drag true: finalize defers entirely to the live"
            " sync, no demote call of its own");
}


/* finalize under !solid_drag demotes width when it ended past its
 * resistance threshold */
static void s_test_finalize_demotes_width_when_past_threshold(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_resize_w = true;
    s_drag.is_solid_drag = false;

    drag_resist_axis_finalize(true);

    TAP_EQ_INT(s_demote_log_used, 1,
            "!solid_drag, width ended past threshold: one demote call");
    TAP_EQ_INT(s_demote_log_dir[0], 1, "on axis 1 (width)");
}


/* finalize under !solid_drag demotes height too, independently of
 * width, when both ended past their own thresholds */
static void s_test_finalize_demotes_both_axes(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_resize_w = true;
    s_drag.is_resist_axis_h = true;
    s_drag.is_resize_h = true;
    s_drag.is_solid_drag = false;

    drag_resist_axis_finalize(true);

    TAP_EQ_INT(s_demote_log_used, 2,
            "both resistant axes ended past their threshold: two"
            " demote calls");
    TAP_OK((s_demote_log_dir[0] == 1 && s_demote_log_dir[1] == 2),
            "width (axis 1) is demoted before height (axis 2)");
}


/* finalize under !solid_drag does not demote an axis that ended back
 * under its own threshold, even if it is flagged resistant */
static void s_test_finalize_skips_axis_still_frozen(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_resize_w = false;
    s_drag.is_solid_drag = false;

    drag_resist_axis_finalize(true);

    TAP_EQ_INT(s_demote_log_used, 0,
            "width ended back under its threshold: no demote call");
}


/* finalize never calls promote at all, only demote: settling a
 * maximize-locked axis at drag end is one-directional, unlike the
 * live, reversible sync drag_resist_axis_update performs mid-drag */
static void s_test_finalize_never_calls_promote(void)
{
    s_reset();
    s_drag.is_resist_axis_w = true;
    s_drag.is_resize_w = true;
    s_drag.is_solid_drag = false;

    drag_resist_axis_finalize(true);

    TAP_EQ_INT(s_promote_log_used, 0,
            "finalize is one-directional: never a promote call");
}


int main(void)
{
    TAP_PLAN(32);

    s_test_no_axis_resistant_is_a_no_op();
    s_test_below_threshold_stays_frozen();
    s_test_exact_threshold_counts_as_past_it();
    s_test_above_threshold_starts_resize();
    s_test_both_axes_independent();
    s_test_zero_resistance_always_past();
    s_test_not_solid_drag_never_calls_commands();
    s_test_solid_drag_demotes_on_width_transition();
    s_test_solid_drag_demotes_on_height_transition();
    s_test_solid_drag_promotes_on_reverse_transition();
    s_test_solid_drag_no_transition_no_call();
    s_test_repeated_crossings_are_reversible();
    s_test_finalize_no_op_when_not_finalizing();
    s_test_finalize_no_op_under_solid_drag();
    s_test_finalize_demotes_width_when_past_threshold();
    s_test_finalize_demotes_both_axes();
    s_test_finalize_skips_axis_still_frozen();
    s_test_finalize_never_calls_promote();

    return TAP_DONE();
}
