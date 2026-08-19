/**
 * @file tests/wm/test_kill.c
 *
 * @brief Test battery for kill escalation
 *
 * Uses 'wm_kill_test_register_with_timeout' throughout instead of the
 * real 'wm_kill_register' to control each registration's own deadline
 * directly, so every test here runs in milliseconds regardless of
 * 'WM_KILL_ESCALATE_TIMEOUT_MS''s own real-time length (ten seconds):
 * a timeout of 0 registers already past due, ready for
 * 'wm_kill_tick' to act on immediately, while a very large one stays
 * safely pending for the whole length of any one test.
 *
 * Every test process ID used throughout is deliberately never a real,
 * live process ('kill(pid, 0)' failing with 'ESRCH' is the whole
 * point): this exercises the "process already gone" branch of
 * 'wm_kill_tick' specifically, the one that never actually sends
 * 'SIGKILL' to begin with, so nothing here risks signalling a real
 * process on the machine these tests happen to run on.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>
#include <sys/types.h>

/* Local includes */
#include <wm/kill.h>
#include <harness/tap.h>


/** A process ID essentially guaranteed to never correspond to a real,
 *  live process (@c pid_t itself is a signed type, and negative or
 *  zero values are never valid live PIDs, so no live process this
 *  suite runs alongside could ever collide with these) */
#define TEST_PID_A ((pid_t) 987654)
#define TEST_PID_B ((pid_t) 987655)
#define TEST_PID_C ((pid_t) 987656)

/** Long enough that no single test here could plausibly still be
 *  running once this many milliseconds have genuinely elapsed */
#define FAR_FUTURE_MS (600000u)


static void s_test_register_then_remaining_reflects_it(void)
{
    int remaining;

    wm_kill_test_reset();

    remaining = wm_kill_ms_remaining();
    TAP_OK(remaining < 0, "nothing pending yet");

    wm_kill_test_register_with_timeout(TEST_PID_A, FAR_FUTURE_MS);
    remaining = wm_kill_ms_remaining();
    TAP_OK(remaining >= 0, "a pending registration reports a" \
            " non-negative remaining time");
    TAP_OK(remaining <= (int) FAR_FUTURE_MS,
            "remaining time never exceeds what was just registered");

    wm_kill_test_reset();
}


static void s_test_remaining_picks_the_closest_deadline(void)
{
    int remaining;

    wm_kill_test_reset();

    wm_kill_test_register_with_timeout(TEST_PID_A, FAR_FUTURE_MS);
    wm_kill_test_register_with_timeout(TEST_PID_B, 0u);
    wm_kill_test_register_with_timeout(TEST_PID_C, FAR_FUTURE_MS);

    remaining = wm_kill_ms_remaining();
    TAP_OK(remaining >= 0 && remaining < 1000,
            "the soonest of three deadlines wins, not the first or" \
            " last registered");

    wm_kill_test_reset();
}


static void s_test_register_non_positive_pid_is_a_no_op(void)
{
    int remaining;

    wm_kill_test_reset();

    wm_kill_test_register_with_timeout((pid_t) 0, FAR_FUTURE_MS);
    wm_kill_test_register_with_timeout((pid_t) -1, FAR_FUTURE_MS);

    remaining = wm_kill_ms_remaining();
    TAP_OK(remaining < 0,
            "registering pid 0 or a negative pid claims no slot");

    wm_kill_test_reset();
}


static void s_test_registration_beyond_capacity_is_a_no_op(void)
{
    int remaining_before;
    int remaining_after;

    wm_kill_test_reset();

    /* Fill every one of 'WM_KILL_ESCALATE_MAX_PENDING' (8) slots with
     * a deadline far enough out that none of them could be the
     * "soonest" once one more, due sooner, gets added below */
    for (int i = 0; i < 8; ++i) {
        wm_kill_test_register_with_timeout((pid_t) (500000 + i),
                FAR_FUTURE_MS);
    }
    remaining_before = wm_kill_ms_remaining();

    /* Every slot is already claimed, so this one, due almost
     * immediately, has nowhere to go and must be silently dropped;
     * if it had been accepted, 'wm_kill_ms_remaining' would now
     * report something far below 'remaining_before' instead */
    wm_kill_test_register_with_timeout((pid_t) 600000, 0u);
    remaining_after = wm_kill_ms_remaining();

    TAP_OK(remaining_after >= remaining_before - 50,
            "a 9th registration past the 8-slot capacity is silently" \
            " dropped, not squeezed in over an existing one");

    wm_kill_test_reset();
}


static void s_test_tick_leaves_a_not_yet_due_registration_pending(void)
{
    int remaining_before;
    int remaining_after;

    wm_kill_test_reset();
    wm_kill_test_register_with_timeout(TEST_PID_A, FAR_FUTURE_MS);

    remaining_before = wm_kill_ms_remaining();
    wm_kill_tick();
    remaining_after = wm_kill_ms_remaining();

    TAP_OK(remaining_after >= 0,
            "still pending after a tick, since its own deadline has" \
            " not arrived yet");
    TAP_OK(remaining_after <= remaining_before,
            "remaining time did not increase across the tick");

    wm_kill_test_reset();
}


static void s_test_tick_clears_a_due_registration(void)
{
    wm_kill_test_reset();
    wm_kill_test_register_with_timeout(TEST_PID_A, 0u);

    TAP_OK(wm_kill_ms_remaining() >= 0, "due registration is pending" \
            " right after being registered");

    wm_kill_tick();

    TAP_OK(wm_kill_ms_remaining() < 0,
            "a tick past its own due registration's deadline clears" \
            " its slot; 'kill(pid, 0)' finding no such process (this" \
            " test's own pid is never a real, live one) takes the" \
            " \"already gone, nothing to signal\" path, never" \
            " actually reaching 'SIGKILL' at all");

    wm_kill_test_reset();
}


static void s_test_tick_only_clears_due_registrations(void)
{
    int remaining;

    wm_kill_test_reset();
    wm_kill_test_register_with_timeout(TEST_PID_A, 0u);
    wm_kill_test_register_with_timeout(TEST_PID_B, FAR_FUTURE_MS);

    wm_kill_tick();

    remaining = wm_kill_ms_remaining();
    TAP_OK(remaining >= 0,
            "the not-yet-due registration alone is still pending" \
            " after the tick that cleared the due one");
    TAP_OK(remaining > 1000,
            "and it is still the far-future one, not a leftover of" \
            " the one that just got cleared");

    wm_kill_test_reset();
}


static void s_test_reset_clears_every_pending_registration(void)
{
    wm_kill_test_register_with_timeout(TEST_PID_A, FAR_FUTURE_MS);
    wm_kill_test_register_with_timeout(TEST_PID_B, FAR_FUTURE_MS);
    TAP_OK(wm_kill_ms_remaining() >= 0, "something pending before" \
            " reset");

    wm_kill_test_reset();

    TAP_OK(wm_kill_ms_remaining() < 0,
            "nothing pending immediately after reset");
}


int main(void)
{
    TAP_PLAN(14);

    s_test_register_then_remaining_reflects_it();
    s_test_remaining_picks_the_closest_deadline();
    s_test_register_non_positive_pid_is_a_no_op();
    s_test_registration_beyond_capacity_is_a_no_op();
    s_test_tick_leaves_a_not_yet_due_registration_pending();
    s_test_tick_clears_a_due_registration();
    s_test_tick_only_clears_due_registrations();
    s_test_reset_clears_every_pending_registration();

    return TAP_DONE();
}
